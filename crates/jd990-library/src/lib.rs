//! Librarian (M5): Patch-Bibliothek auf SQLite.
//!
//! `.syx` bleibt die Wahrheit (ADR-5) — die Bibliothek speichert die 486
//! Rohbytes unverändert und legt Komfort darüber: Herkunft, Tags, Notizen,
//! Favoriten, Suche. Dubletten werden am Inhalts-Hash erkannt (gleiche Bytes =
//! gleicher Patch, egal aus welcher Datei).

use jd990_core::sysex::Address;
use jd990_core::{Patch, Tone};
use rusqlite::{Connection, OptionalExtension, params};
use std::path::Path;

pub use rusqlite::Error as DbError;

const SCHEMA_VERSION: i32 = 1;

pub struct Library {
    db: Connection,
}

/// Ein Bibliothekseintrag (ohne Patch-Daten).
#[derive(Debug, Clone, PartialEq)]
pub struct Entry {
    pub id: i64,
    pub name: String,
    pub source: String,
    pub added: String,
    pub notes: String,
    pub favorite: bool,
    pub tags: Vec<String>,
}

#[derive(Debug, Default, Clone, PartialEq)]
pub struct ImportReport {
    pub added: Vec<i64>,
    pub duplicates: Vec<i64>,
}

#[derive(Debug)]
pub enum Error {
    Db(rusqlite::Error),
    Parse(String),
    NotFound(i64),
    Duplicate(i64),
}

impl std::fmt::Display for Error {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        match self {
            Error::Db(e) => write!(f, "Datenbank: {e}"),
            Error::Parse(e) => write!(f, "Keine Patch-Daten: {e}"),
            Error::NotFound(id) => write!(f, "Eintrag {id} nicht gefunden"),
            Error::Duplicate(id) => write!(f, "Gleicher Patch existiert bereits (Eintrag {id})"),
        }
    }
}

impl std::error::Error for Error {}

impl From<rusqlite::Error> for Error {
    fn from(e: rusqlite::Error) -> Self {
        Error::Db(e)
    }
}

pub type Result<T> = std::result::Result<T, Error>;

impl Library {
    /// Öffnet (oder erzeugt) die Bibliothek.
    pub fn open(path: impl AsRef<Path>) -> Result<Library> {
        if let Some(dir) = path.as_ref().parent() {
            let _ = std::fs::create_dir_all(dir);
        }
        let db = Connection::open(path)?;
        db.pragma_update(None, "foreign_keys", true)?;
        let version: i32 = db.pragma_query_value(None, "user_version", |r| r.get(0))?;
        if version < 1 {
            db.execute_batch(
                "CREATE TABLE IF NOT EXISTS patch (
                    id       INTEGER PRIMARY KEY,
                    hash     TEXT    NOT NULL UNIQUE,
                    name     TEXT    NOT NULL,
                    data     BLOB    NOT NULL,
                    source   TEXT    NOT NULL DEFAULT '',
                    added    TEXT    NOT NULL DEFAULT (datetime('now', 'localtime')),
                    notes    TEXT    NOT NULL DEFAULT '',
                    favorite INTEGER NOT NULL DEFAULT 0
                 );
                 CREATE TABLE IF NOT EXISTS tag (
                    patch_id INTEGER NOT NULL REFERENCES patch(id) ON DELETE CASCADE,
                    tag      TEXT    NOT NULL,
                    PRIMARY KEY (patch_id, tag)
                 );",
            )?;
            db.pragma_update(None, "user_version", SCHEMA_VERSION)?;
        }
        Ok(Library { db })
    }

    /// Standardort: ~/Library/Application Support/JD-990 Editor/library.sqlite
    /// (überschreibbar mit JD990_LIBRARY).
    pub fn default_path() -> std::path::PathBuf {
        if let Ok(p) = std::env::var("JD990_LIBRARY") {
            return p.into();
        }
        let home = std::env::var("HOME").unwrap_or_else(|_| ".".into());
        Path::new(&home).join("Library/Application Support/JD-990 Editor/library.sqlite")
    }

    fn hash(p: &Patch) -> String {
        format!("{:016x}", p.content_hash())
    }

    /// Fügt einen Patch hinzu. Existiert derselbe Inhalt schon: `Err(Duplicate(id))`.
    pub fn add(&self, patch: &Patch, source: &str) -> Result<i64> {
        let hash = Self::hash(patch);
        if let Some(id) = self.db.query_row("SELECT id FROM patch WHERE hash = ?1", [&hash], |r| r.get(0)).optional()? {
            return Err(Error::Duplicate(id));
        }
        self.db.execute(
            "INSERT INTO patch (hash, name, data, source) VALUES (?1, ?2, ?3, ?4)",
            params![hash, patch.name().trim_end(), &patch.as_bytes()[..], source],
        )?;
        Ok(self.db.last_insert_rowid())
    }

    /// Importiert alle Patches aus `.syx`/`.mid`-Bytes (Einzelpatch oder Bank).
    /// Quelle = `origin` + Platz (z. B. „bank.syx · I:23“).
    pub fn import_bytes(&self, bytes: &[u8], origin: &str) -> Result<ImportReport> {
        let native = Patch::all_from_bytes(bytes).map_err(|e| Error::Parse(e.to_string()))?;
        // JD-990-Patches und — zusätzlich — JD-800-Patches (konvertiert, M5)
        let mut all: Vec<(Patch, String)> = native
            .iter()
            .map(|(p, at)| {
                let src = if native.len() > 1 || at.area() != 0x03 { format!("{origin} · {}", at.slot_label()) } else { origin.to_string() };
                (p.clone(), src)
            })
            .collect();
        for (p8, label) in jd990_core::jd800::patches_from_bytes(bytes) {
            all.push((jd990_core::jd800::convert(&p8), format!("{origin} · {label} (konvertiert)")));
        }
        if all.is_empty() {
            return Err(Error::Parse("keine Patches gefunden".into()));
        }
        let mut report = ImportReport::default();
        let tx = self.db.unchecked_transaction()?;
        for (p, source) in &all {
            match self.add(p, source) {
                Ok(id) => report.added.push(id),
                Err(Error::Duplicate(id)) => report.duplicates.push(id),
                Err(e) => return Err(e),
            }
        }
        tx.commit()?;
        Ok(report)
    }

    pub fn import_file(&self, path: impl AsRef<Path>) -> Result<ImportReport> {
        let path = path.as_ref();
        let bytes = std::fs::read(path).map_err(|e| Error::Parse(format!("{}: {e}", path.display())))?;
        let name = path.file_name().map(|n| n.to_string_lossy().to_string()).unwrap_or_default();
        self.import_bytes(&bytes, &name)
    }

    pub fn get(&self, id: i64) -> Result<Patch> {
        let data: Vec<u8> = self
            .db
            .query_row("SELECT data FROM patch WHERE id = ?1", [id], |r| r.get(0))
            .optional()?
            .ok_or(Error::NotFound(id))?;
        let arr: [u8; 486] = data.try_into().map_err(|_| Error::Parse(format!("Eintrag {id}: Länge ≠ 486")))?;
        Ok(Patch::from_bytes(arr))
    }

    /// Suche in Name, Quelle, Notizen und Tags (Groß/Klein egal). Leere Suche = alle.
    pub fn list(&self, query: &str, favorites_only: bool) -> Result<Vec<Entry>> {
        let like = format!("%{}%", query.trim());
        let mut stmt = self.db.prepare(
            "SELECT p.id, p.name, p.source, p.added, p.notes, p.favorite,
                    COALESCE((SELECT group_concat(tag, ',') FROM (SELECT tag FROM tag WHERE patch_id = p.id ORDER BY tag)), '')
             FROM patch p
             WHERE (?1 = '%%' OR p.name LIKE ?1 OR p.source LIKE ?1 OR p.notes LIKE ?1
                    OR EXISTS (SELECT 1 FROM tag t WHERE t.patch_id = p.id AND t.tag LIKE ?1))
               AND (?2 = 0 OR p.favorite = 1)
             ORDER BY p.name COLLATE NOCASE, p.id",
        )?;
        let rows = stmt.query_map(params![like, favorites_only as i32], |r| {
            let tags: String = r.get(6)?;
            Ok(Entry {
                id: r.get(0)?,
                name: r.get(1)?,
                source: r.get(2)?,
                added: r.get(3)?,
                notes: r.get(4)?,
                favorite: r.get::<_, i32>(5)? != 0,
                tags: tags.split(',').filter(|t| !t.is_empty()).map(String::from).collect(),
            })
        })?;
        Ok(rows.collect::<std::result::Result<Vec<_>, _>>()?)
    }

    /// Die `limit` ähnlichsten Einträge zu `p` (Ähnlichkeit 0…1, absteigend).
    pub fn similar(&self, p: &Patch, limit: usize) -> Result<Vec<(i64, f32)>> {
        let mut stmt = self.db.prepare("SELECT id, data FROM patch")?;
        let rows = stmt.query_map([], |r| Ok((r.get::<_, i64>(0)?, r.get::<_, Vec<u8>>(1)?)))?;
        let mut out = Vec::new();
        for row in rows {
            let (id, data) = row?;
            if let Ok(arr) = <[u8; 486]>::try_from(data) {
                out.push((id, jd990_core::morph::similarity(p, &Patch::from_bytes(arr))));
            }
        }
        out.sort_by(|a, b| b.1.total_cmp(&a.1));
        out.truncate(limit);
        Ok(out)
    }

    pub fn count(&self) -> Result<i64> {
        Ok(self.db.query_row("SELECT count(*) FROM patch", [], |r| r.get(0))?)
    }

    pub fn set_notes(&self, id: i64, notes: &str) -> Result<()> {
        self.touch(self.db.execute("UPDATE patch SET notes = ?2 WHERE id = ?1", params![id, notes])?, id)
    }

    pub fn set_favorite(&self, id: i64, on: bool) -> Result<()> {
        self.touch(self.db.execute("UPDATE patch SET favorite = ?2 WHERE id = ?1", params![id, on as i32])?, id)
    }

    /// Ersetzt die Tags (kommagetrennt oder Liste); leere Einträge werden ignoriert.
    pub fn set_tags<S: AsRef<str>>(&self, id: i64, tags: &[S]) -> Result<()> {
        self.get(id)?;
        let tx = self.db.unchecked_transaction()?;
        tx.execute("DELETE FROM tag WHERE patch_id = ?1", [id])?;
        for t in tags {
            let t = t.as_ref().trim().to_lowercase();
            if !t.is_empty() {
                tx.execute("INSERT OR IGNORE INTO tag (patch_id, tag) VALUES (?1, ?2)", params![id, t])?;
            }
        }
        tx.commit()?;
        Ok(())
    }

    /// Entfernt einen Eintrag aus der Bibliothek (nur auf ausdrückliche Nutzeraktion).
    pub fn remove(&self, id: i64) -> Result<()> {
        self.touch(self.db.execute("DELETE FROM patch WHERE id = ?1", [id])?, id)
    }

    fn touch(&self, changed: usize, id: i64) -> Result<()> {
        if changed == 0 { Err(Error::NotFound(id)) } else { Ok(()) }
    }

    /// Export: ein Eintrag → `.syx` für den Temp-Bereich; mehrere → Bank-Dump an
    /// den internen Plätzen I:11 … (max. 64). Schreiben ins Gerät ist Sache des
    /// Nutzers (ADR-7) — die Datei ist nur eine Datei.
    pub fn export_syx(&self, ids: &[i64]) -> Result<Vec<u8>> {
        let mut out = Vec::new();
        if ids.len() == 1 {
            out = self.get(ids[0])?.to_syx(0x10, Address::TEMP_PATCH);
        } else {
            for (n, id) in ids.iter().take(64).enumerate() {
                out.extend(self.get(*id)?.to_syx(0x10, Address::internal_patch(n as u8)));
            }
        }
        Ok(out)
    }
}

/// Lesbarer Parameter-Diff zweier Patches: (ID mit Tone, Anzeige a, Anzeige b).
pub fn diff(a: &Patch, b: &Patch) -> Vec<(String, String, String)> {
    let mut out = Vec::new();
    if a.name() != b.name() {
        out.push(("patch.name".into(), a.name().trim_end().into(), b.name().trim_end().into()));
    }
    for (def, tone, x, y) in a.diff_params(b) {
        let id = match tone {
            Some(t) => def.id.replacen("tone.", &format!("tone{}.", Tone::letter(t)), 1),
            None => def.id.to_string(),
        };
        out.push((id, jd990_core::display::format(def, x), jd990_core::display::format(def, y)));
    }
    out
}

#[cfg(test)]
mod tests {
    use super::*;
    use jd990_core::params::find;

    fn lib() -> (tempfile::TempDir, Library) {
        let dir = tempfile::tempdir().unwrap();
        let l = Library::open(dir.path().join("lib.sqlite")).unwrap();
        (dir, l)
    }

    #[test]
    fn hinzufuegen_dubletten_suchen() {
        let (_d, l) = lib();
        let a = Patch::init();
        let mut b = Patch::init();
        b.set(find("tone.tvf.cutoff").unwrap(), Some(Tone::A), 40).unwrap();
        let ia = l.add(&a, "Init").unwrap();
        let ib = l.add(&b, "Test").unwrap();
        assert!(matches!(l.add(&a, "nochmal"), Err(Error::Duplicate(id)) if id == ia));
        assert_eq!(l.count().unwrap(), 2);
        l.set_tags(ib, &["Bass", " dunkel ", ""]).unwrap();
        l.set_notes(ia, "trockener Startpunkt").unwrap();
        l.set_favorite(ib, true).unwrap();
        assert_eq!(l.list("bass", false).unwrap().iter().map(|e| e.id).collect::<Vec<_>>(), vec![ib]);
        assert_eq!(l.list("trocken", false).unwrap().len(), 1);
        assert_eq!(l.list("", true).unwrap().len(), 1);
        assert_eq!(l.list("", false).unwrap()[1].tags, vec!["bass", "dunkel"]);
        assert_eq!(l.get(ib).unwrap(), b);
        l.remove(ia).unwrap();
        assert!(matches!(l.get(ia), Err(Error::NotFound(_))));
    }

    #[test]
    fn bank_import_und_export() {
        let (_d, l) = lib();
        let path = Path::new(env!("CARGO_MANIFEST_DIR")).join("../../corpus/2026-09-27-backup/patches-internal.syx");
        let Ok(bytes) = std::fs::read(&path) else { return };
        let r = l.import_bytes(&bytes, "backup").unwrap();
        assert_eq!(r.added.len() + r.duplicates.len(), 64);
        assert!(l.list("I:11", false).unwrap().iter().any(|e| e.source == "backup · I:11"));
        // Zweiter Import: alles Dubletten
        let r2 = l.import_bytes(&bytes, "backup").unwrap();
        assert!(r2.added.is_empty() && r2.duplicates.len() == 64);
        // Export einzeln (Temp) und als Bank; Bank ist wieder einlesbar
        let one = l.export_syx(&r.added[..1]).unwrap();
        assert_eq!(Patch::all_from_bytes(&one).unwrap()[0].1, Address::TEMP_PATCH);
        let bank = l.export_syx(&r.added).unwrap();
        assert_eq!(Patch::all_from_bytes(&bank).unwrap().len(), r.added.len());
    }

    #[test]
    fn jd800_bank_wird_konvertiert_importiert() {
        let (_d, l) = lib();
        // JD-800-Temp-Patch (Model 3DH): Standardpatch aus dem Kern-Test, hier minimal nachgebaut
        let mut p8 = [0x32u8; 384];
        p8[..16].copy_from_slice(b"JD800 Brass     ");
        for t in 0..4 { p8[96 + t * 72 + 5] = 1; p8[96 + t * 72 + 11] = 1; p8[96 + t * 72 + 36] = 2; p8[96 + t * 72 + 14] = 0; p8[96 + t * 72 + 15] = 0; }
        p8[33] = 1; p8[34] = 1; p8[50] = 0; p8[51] = 0; p8[60] = 0; p8[89] = 0; p8[92] = 16; p8[31] = 0; p8[30] = 0;
        let mut syx = Vec::new();
        for (i, chunk) in p8.chunks(256).enumerate() {
            let a = (i * 256) as u32;
            let mut body = vec![(a >> 14) as u8, (a >> 7) as u8 & 0x7F, a as u8 & 0x7F];
            body.extend_from_slice(chunk);
            syx.extend([0xF0, 0x41, 0x10, 0x3D, 0x12]);
            syx.extend(&body);
            syx.push(jd990_core::sysex::checksum(&body));
            syx.push(0xF7);
        }
        let r = l.import_bytes(&syx, "jd800.syx").unwrap();
        assert_eq!(r.added.len(), 1);
        let e = &l.list("brass", false).unwrap()[0];
        assert_eq!(e.source, "jd800.syx · JD-800 Temp (konvertiert)");
        // LFO-Wellenform SAW (800: 1) → 990: 2
        assert_eq!(l.get(e.id).unwrap().get(find("tone.lfo1.waveform").unwrap(), Some(Tone::A)), 2);
    }

    #[test]
    fn diff_zeigt_anzeigewerte() {
        let a = Patch::init();
        let mut b = Patch::init();
        b.set(find("tone.tvf.cutoff").unwrap(), Some(Tone::C), 40).unwrap();
        b.set(find("patch.fx.rv_type").unwrap(), None, 6).unwrap();
        let d = diff(&a, &b);
        assert!(d.contains(&("toneC.tvf.cutoff".into(), "100".into(), "40".into())));
        assert!(d.contains(&("patch.fx.rv_type".into(), "HALL1".into(), "GATE".into())));
    }
}
