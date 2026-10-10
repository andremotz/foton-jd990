#include "Model.h"

namespace
{
    juce::String paramText (uint32_t index, uint32_t what)
    {
        char buf[128];
        return jd990_param_text (index, what, buf, sizeof buf) > 0 ? juce::String::fromUTF8 (buf) : juce::String();
    }
    juce::String now() { return Model::testMode ? juce::String ("12:00:00") : juce::Time::getCurrentTime().toString (false, true); }
    juce::String u (const char* s) { return juce::String::fromUTF8 (s); }
    juce::String perfText (uint32_t index, uint32_t what)
    {
        char buf[128];
        return jd990_perf_param_text (index, what, buf, sizeof buf) > 0 ? juce::String::fromUTF8 (buf) : juce::String();
    }
    template <typename F> std::vector<uint8_t> bytes (F&& f)
    {
        std::vector<uint8_t> v ((size_t) std::max (0, (int) f (nullptr, 0)));
        if (! v.empty()) f (v.data(), v.size());
        return v;
    }
    /** Einzelne DT1-Nachrichten eines Bytestroms an `fn`. */
    template <typename F> void eachMessage (const std::vector<uint8_t>& b, F&& fn)
    {
        for (size_t i = 0, start = 0; i < b.size(); ++i)
        {
            if (b[i] == 0xF0) start = i;
            if (b[i] == 0xF7) fn (b.data() + start, i + 1 - start);
        }
    }
}

Model::Model() : current (jd990_patch_new())
{
    for (uint32_t i = 0; i < jd990_param_count(); ++i)
    {
        Param p { i, {}, paramText (i, 0), paramText (i, 1), paramText (i, 2) };
        jd990_param_info (i, &p.info);
        all.push_back (p);
    }
    for (uint32_t i = 0; i < jd990_perf_param_count(); ++i)
    {
        PerfParam p { i, {}, perfText (i, 0), perfText (i, 1), perfText (i, 2) };
        jd990_perf_param_info (i, &p.info);
        perfAll.push_back (p);
    }
    link.onPatch = [this] (const std::vector<uint8_t>& syx) {
        PatchPtr p (jd990_patch_from_bytes (syx.data(), syx.size()));
        if (bankIndex >= 0)                                     // Bank lesen: nächster Patch
        {
            int n = bankIndex;
            if (p) { ++bankRead; if (bankEach) bankEach (n, std::move (p)); }
            if (++bankIndex < 64) { status (u ("Lese I:") + juce::String (bankIndex / 8 + 1) + juce::String (bankIndex % 8 + 1) + u (" …")); link.requestInternalPatch (bankIndex); }
            else { bankIndex = -1; if (bankDone) bankDone (bankRead); }
            return;
        }
        if (! p) { status (u ("Antwort des Geräts nicht lesbar")); return; }
        if (! checking) { setPatch (std::move (p), u ("Gerät")); return; }
        checking = false;
        if (bytesOf (p.get()) == bytesOf (current.get()))
            setSync (u ("Gerät = Editor (abgeglichen ") + now() + ")", juce::Colours::lightgreen);
        else
            setSync (u ("⚠ Gerät weicht ab (") + now() + u (") — „Vom Gerät lesen“"), juce::Colours::orange);
        changed();
    };
    link.onStatus = [this] (const juce::String& s) {
        if (bankIndex >= 0 && s.startsWith ("Keine Antwort")) { bankIndex = -1; if (bankDone) bankDone (bankRead); }
        status (s);
    };
    link.onSent = [this] (const std::vector<uint8_t>& m) { lastSent = m; changed(); };
    setSync (u ("kein Patch"), juce::Colours::grey);
}

Model::~Model() { stopTimer(); }

juce::PropertiesFile* Model::settings()
{
    static std::unique_ptr<juce::PropertiesFile> file;
    if (testMode) return nullptr;
    if (file == nullptr)
    {
        juce::PropertiesFile::Options o;
        // Ordner/Dateiname bleiben beim alten Namen (vor „FotonJD990“) — vorhandene
        // Einstellungen und die Bibliothek im selben Ordner werden weiter gefunden.
        o.applicationName = "JD-990 Editor";
        o.folderName = "JD-990 Editor";
        o.filenameSuffix = ".settings";
        o.osxLibrarySubFolder = "Application Support";
        file = std::make_unique<juce::PropertiesFile> (o);
    }
    return file.get();
}

const Model::Param* Model::find (const juce::String& id) const
{
    for (auto& p : all) if (p.id == id) return &p;
    return nullptr;
}

juce::String Model::patchName() const
{
    if (! loaded) return u ("— kein Patch —");
    char buf[32];
    jd990_patch_name (current.get(), buf, sizeof buf);
    return juce::String::fromUTF8 (buf).trimEnd();
}

int Model::get (uint32_t index, int t) const { return jd990_patch_get (current.get(), index, t); }

juce::String Model::display (uint32_t index, int t) const
{
    char buf[64];
    jd990_patch_display (current.get(), index, t, buf, sizeof buf);
    return juce::String::fromUTF8 (buf);
}

std::vector<uint8_t> Model::bytesOf (const Jd990Patch* p) const
{
    std::vector<uint8_t> v ((size_t) jd990_patch_to_syx (p, 3, 0, nullptr, 0));
    jd990_patch_to_syx (p, 3, 0, v.data(), v.size());
    return v;
}

std::vector<uint8_t> Model::routed (const uint8_t* msg, size_t len) const
{
    if (editPart < 0) return std::vector<uint8_t> (msg, msg + len);
    return bytes ([&] (uint8_t* o, size_t c) { return jd990_dt1_retarget (msg, len, 2, (uint8_t) editPart, o, c); });
}

void Model::queueDt1 (const uint8_t* msg, size_t len)
{
    if (! link.isOpen()) return;
    auto m = routed (msg, len);
    eachMessage (m, [this] (const uint8_t* d, size_t n) { link.queueParam (d, n); });
}

const Model::PerfParam* Model::findPerf (const juce::String& id) const
{
    for (auto& p : perfAll) if (p.id == id) return &p;
    return nullptr;
}

void Model::set (uint32_t index, int t, int value)
{
    if (! loaded || index >= all.size()) return;
    auto& p = all[index];
    // Undo-Schritt: neuer Parameter oder Pause > 800 ms (ein Fader-Zug, nicht 100)
    auto ms = juce::Time::getMillisecondCounterHiRes();
    int64_t key = (int64_t) index * 8 + (t + 1);
    if (gesture) { if (gesturePending) { pushUndo ({}, (int) index, t); redoStack.clear(); gesturePending = false; } }
    else if (key != lastEditKey || ms - lastEditMs > 800.0) { pushUndo ({}, (int) index, t); redoStack.clear(); }
    lastEditKey = key;
    lastEditMs = ms;

    // Link: Tone-Parameter relativ in allen gelinkten Tones (Auswahllisten absolut)
    std::vector<int> tones { t };
    int delta = 0;
    bool relative = p.isTone() && linkMode && linked.size() > 1;
    if (relative)
    {
        tones.assign (linked.begin(), linked.end());
        delta = value - get (index, t);
        relative = p.info.kind == 0 || p.info.kind == 3;   // linear, Pan: relativ; sonst gleicher Wert
    }
    for (int tt : tones)
    {
        int v = relative ? juce::jlimit ((int) p.info.min, (int) p.info.max, get (index, tt) + delta) : value;
        uint8_t msg[32];
        auto n = jd990_patch_set (current.get(), index, tt, (uint16_t) v, msg, sizeof msg);
        if (n > 0) queueDt1 (msg, (size_t) n);
    }
    lastIndex = (int) index;
    lastTone = t;
    if (! link.isOpen()) setSync (u ("Editor ≠ Gerät (nicht verbunden)"), juce::Colours::grey);
    changed();
}

void Model::pushUndo (const juce::String& what, int param, int t)
{
    undoStack.push_back ({ PatchPtr (jd990_patch_clone (current.get())), what, param, t, nullptr, -1, -1 });
    if (undoStack.size() > 200) undoStack.erase (undoStack.begin());
}

void Model::pushPerfUndo (int param, int part)
{
    undoStack.push_back ({ PatchPtr (jd990_patch_clone (current.get())), {}, -1, -1, PerfPtr (jd990_perf_clone (perf.get())), param, part });
    if (undoStack.size() > 200) undoStack.erase (undoStack.begin());
    redoStack.clear();
}

juce::String Model::stepText (size_t i, bool redo) const
{
    auto& stack = redo ? redoStack : undoStack;
    auto& s = stack[i];
    if (s.perfState != nullptr && s.perfParam >= 0 && s.perfParam < (int) perfAll.size())
    {
        // Performance-Schritt: Zustand davor/danach wie bei Patch-Schritten
        auto perfOf = [&] (size_t k) -> const Jd990Performance* { return stack[k].perfState ? stack[k].perfState.get() : perf.get(); };
        const Jd990Performance* before = s.perfState.get();
        const Jd990Performance* after = ! redo && i + 1 < stack.size() ? perfOf (i + 1) : perf.get();
        if (redo) { after = s.perfState.get(); before = i + 1 < stack.size() ? perfOf (i + 1) : perf.get(); }
        auto& p = perfAll[(size_t) s.perfParam];
        auto disp = [&] (const Jd990Performance* x) { char buf[64]; jd990_perf_display (x, p.index, s.perfPart, buf, sizeof buf); return juce::String::fromUTF8 (buf); };
        juce::String where = p.isPart() ? "Part " + juce::String (s.perfPart + 1) + u (" · ") : u ("Performance · ");
        return where + p.label + u (": ") + disp (before) + u (" → ") + disp (after);
    }
    if (s.param < 0 || s.param >= (int) all.size()) return s.what.isNotEmpty() ? s.what : u ("Änderung");
    // Zustand davor/danach: Undo-Eintrag i → danach ist Eintrag i+1 bzw. der aktuelle Patch
    const Jd990Patch* before = s.state.get();
    const Jd990Patch* after = current.get();
    if (! redo && i + 1 < stack.size()) after = stack[i + 1].state.get();
    if (redo) { after = s.state.get(); before = i + 1 < stack.size() ? stack[i + 1].state.get() : current.get(); }
    auto& p = all[(size_t) s.param];
    auto disp = [&] (const Jd990Patch* x) { char buf[64]; jd990_patch_display (x, p.index, s.tone, buf, sizeof buf); return juce::String::fromUTF8 (buf); };
    juce::String where = p.isTone() && s.tone >= 0 ? u ("Tone ") + juce::String::charToString ((juce::juce_wchar) ('A' + s.tone)) + u (" · ") : juce::String();
    return where + p.group + u (" · ") + p.label + u (": ") + disp (before) + u (" → ") + disp (after);
}

juce::StringArray Model::undoHistory (int max) const
{
    juce::StringArray out;
    for (int i = (int) undoStack.size() - 1; i >= 0 && out.size() < max; --i) out.add (stepText ((size_t) i, false));
    return out;
}

juce::StringArray Model::redoHistory (int max) const
{
    juce::StringArray out;
    for (int i = (int) redoStack.size() - 1; i >= 0 && out.size() < max; --i) out.add (stepText ((size_t) i, true));
    return out;
}

void Model::undoSteps (int n)
{
    if (n <= 0 || undoStack.empty()) return;
    PatchPtr now (jd990_patch_clone (current.get()));
    PerfPtr nowPerf (perf ? jd990_perf_clone (perf.get()) : nullptr);
    bool perfTouched = false;
    for (int k = 0; k < n && ! undoStack.empty(); ++k)
    {
        auto s = std::move (undoStack.back());
        undoStack.pop_back();
        bool isPerf = s.perfState != nullptr;
        redoStack.push_back ({ std::move (now), s.what, s.param, s.tone, isPerf ? PerfPtr (jd990_perf_clone (nowPerf.get())) : nullptr, s.perfParam, s.perfPart });
        now = std::move (s.state);
        if (isPerf) { nowPerf = std::move (s.perfState); perfTouched = true; }
    }
    if (perfTouched) applyPerf (std::move (nowPerf));
    applyPatch (std::move (now), n == 1 ? u ("Rückgängig") : juce::String (n) + u (" Schritte rückgängig"));
}

void Model::redoSteps (int n)
{
    if (n <= 0 || redoStack.empty()) return;
    PatchPtr now (jd990_patch_clone (current.get()));
    PerfPtr nowPerf (perf ? jd990_perf_clone (perf.get()) : nullptr);
    bool perfTouched = false;
    for (int k = 0; k < n && ! redoStack.empty(); ++k)
    {
        auto s = std::move (redoStack.back());
        redoStack.pop_back();
        bool isPerf = s.perfState != nullptr;
        undoStack.push_back ({ std::move (now), s.what, s.param, s.tone, isPerf ? PerfPtr (jd990_perf_clone (nowPerf.get())) : nullptr, s.perfParam, s.perfPart });
        now = std::move (s.state);
        if (isPerf) { nowPerf = std::move (s.perfState); perfTouched = true; }
    }
    if (perfTouched) applyPerf (std::move (nowPerf));
    applyPatch (std::move (now), n == 1 ? juce::String ("Wiederholt") : juce::String (n) + u (" Schritte wiederholt"));
}

void Model::toggleLock (const juce::String& group)
{
    if (lockedGroups.count (group)) lockedGroups.erase (group); else lockedGroups.insert (group);
    status (isLocked (group) ? u ("🔒 ") + group + u (" gesperrt — Variation und Morph lassen die Gruppe unverändert")
                             : group + u (" entsperrt"));
    changed();
}

void Model::applyLocks (Jd990Patch* target, const Jd990Patch* source) const
{
    if (lockedGroups.empty() || target == nullptr || source == nullptr) return;
    uint8_t msg[32];
    for (auto& p : all)
        if (lockedGroups.count (p.group))
            for (int t = p.isTone() ? 0 : -1; t < (p.isTone() ? 4 : 0); ++t)
                jd990_patch_set (target, p.index, t, (uint16_t) jd990_patch_get (source, p.index, t), msg, sizeof msg);
}

void Model::applyPatch (PatchPtr target, const juce::String& what)
{
    // Nur den Unterschied senden — ein Undo über einen Fader ist eine Nachricht, kein Patch.
    if (link.isOpen() && loaded)
    {
        auto diff = bytes ([&] (uint8_t* o, size_t c) { return jd990_patch_diff_dt1 (current.get(), target.get(), o, c); });
        eachMessage (diff, [this] (const uint8_t* d, size_t n) { queueDt1 (d, n); });
    }
    current = std::move (target);
    lastEditKey = -1;
    status (what);
    changed();
}

void Model::undo() { undoSteps (1); }
void Model::redo() { redoSteps (1); }

void Model::setPatch (PatchPtr p, const juce::String& origin)
{
    if (loaded) { pushUndo (u ("Geladen: ") + origin); redoStack.clear(); }
    current = std::move (p);
    loaded = true;
    lastEditKey = -1;
    status ("Geladen aus " + origin + " um " + now());
    if (origin == u ("Gerät"))
        setSync (u ("Gerät = Editor (gelesen ") + now() + ")", juce::Colours::lightgreen);
    else
        setSync (u ("Editor ≠ Gerät (") + origin + u (", noch nicht gesendet)"), juce::Colours::orange);
    changed();
}

bool Model::loadFile (const juce::File& f)
{
    juce::MemoryBlock mb;
    if (! f.loadFileAsData (mb)) return false;
    auto* data = (const uint8_t*) mb.getData();
    // Setup-Datei (Temp-Performance + Part-Patches)? → als Performance laden
    if (PerfPtr pf (jd990_perf_from_bytes (data, mb.getSize())); pf != nullptr)
    {
        std::array<PatchPtr, 7> ps;
        bool complete = true;
        for (int n = 0; n < 7; ++n)
        {
            ps[(size_t) n].reset (jd990_patch_from_bytes_at (data, mb.getSize(), 2, (uint8_t) n));
            complete &= ps[(size_t) n] != nullptr;
        }
        if (complete)
        {
            enterPerformance (std::move (pf), std::move (ps), f.getFileName());
            if (link.isOpen() && deviceMode == 1) sendSetupToDevice();
            return true;
        }
    }
    PatchPtr p (jd990_patch_from_bytes (data, mb.getSize()));
    if (! p) { status ("Keine Patch-Daten in " + f.getFileName()); return false; }
    setPatch (std::move (p), f.getFileName());
    if (link.isOpen()) sendToDevice();
    return true;
}

bool Model::saveFile (const juce::File& f)
{
    std::vector<uint8_t> syx;
    if (perfMode())   // Setup: Performance + alle Part-Patches in die Temp-Bereiche
    {
        syx = bytes ([&] (uint8_t* o, size_t c) { return jd990_perf_to_syx (perf.get(), 1, 0, o, c); });
        for (int n = 0; n < 7; ++n)
        {
            auto* pp = n == editPart ? current.get() : parts[(size_t) n].get();
            auto b = bytes ([&] (uint8_t* o, size_t c) { return jd990_patch_to_syx (pp, 2, (uint8_t) n, o, c); });
            syx.insert (syx.end(), b.begin(), b.end());
        }
    }
    else
        syx = bytesOf (current.get());
    bool ok = f != juce::File() && f.replaceWithData (syx.data(), syx.size());
    if (ok) status ("Gespeichert: " + f.getFileName());
    return ok;
}

// Der Editor ist Owner des Temp-Bereichs (ADR-8): Was er lädt, geht bei bestehender
// Verbindung sofort in den Temp-Patch des Geräts — sonst hört man nicht, was man sieht
// (Abendtest 2026-09-28: „Init geht nicht“). Der interne Speicher bleibt unberührt.
void Model::loadInit()
{
    setPatch (PatchPtr (jd990_patch_init()), "Init");
    if (link.isOpen()) sendToDevice();
}

void Model::selectTone (int t)
{
    if (t < 0 || t > 3) return;
    tone = t;
    if (! linkMode) linked = { t };
    else linked.insert (t);
    changed();
}

void Model::toggleLinked (int t)
{
    if (linked.count (t) && linked.size() > 1 && t != tone) linked.erase (t);
    else linked.insert (t);
    changed();
}

bool Model::connect (const juce::String& port, bool quiet)
{
    if (! link.open (port))
    {
        if (! quiet) status (u ("„") + port + u ("“ nicht vorhanden — Interface aus? Verbinde automatisch, sobald der Port auftaucht."));
        return false;
    }
    status ("Verbunden: " + port);
    checkDeviceMode();             // erst den Modus des Geräts, dann das Passende lesen
    setPolling (5000);
    return true;
}

void Model::readBank (std::function<void (int, PatchPtr)> onEach, std::function<void (int)> onDone)
{
    if (! link.isOpen()) { status ("Nicht verbunden"); return; }
    if (bankIndex >= 0) return;
    bankEach = std::move (onEach);
    bankDone = std::move (onDone);
    bankIndex = 0;
    bankRead = 0;
    checking = false;
    status (u ("Lese I:11 …"));
    link.requestInternalPatch (0);
}

void Model::readFromDevice()
{
    if (! link.isOpen()) { status ("Nicht verbunden"); return; }
    checking = false;
    if (perfMode()) { readSetupFromDevice(); return; }
    link.requestTempPatch();
}

void Model::sendToDevice()
{
    if (! link.isOpen()) { status ("Nicht verbunden"); return; }
    auto b = bytesOf (current.get());
    link.sendPatch (routed (b.data(), b.size()));   // Patch-Modus: 03 · Performance: Patch des gewählten Parts
    setSync (u ("Gerät = Editor (gesendet ") + now() + ")", juce::Colours::lightgreen);
    changed();
}

void Model::setPolling (int ms)
{
    if (ms > 0) startTimer (ms); else stopTimer();
    changed();
}

void Model::timerCallback()
{
    if (! link.isOpen() || link.busy() || ! loaded || bankIndex >= 0) return;   // nie in eine laufende Übertragung hinein
    if (perfMode())
    {
        // Performance-Modus: die Performance leise vergleichen (am Gerät gewechselt? → Abweichung)
        uint8_t rq[32];
        auto n = jd990_rq1_performance (1, 0, rq, sizeof rq);
        link.request (std::vector<uint8_t> (rq, rq + n), 144, [this] (const std::vector<uint8_t>& b) {
            PerfPtr dev (b.empty() ? nullptr : jd990_perf_from_bytes (b.data(), b.size()));
            if (! dev || ! perf) return;
            bool same = jd990_perf_diff_dt1 (perf.get(), dev.get(), nullptr, 0) == 0;
            if (same) setSync (u ("Gerät = Editor (abgeglichen ") + now() + ")", juce::Colours::lightgreen);
            else setSync (u ("⚠ Performance am Gerät weicht ab (") + now() + u (") — „Vom Gerät lesen“"), juce::Colours::orange);
            changed();
        });
        return;
    }
    checking = true;
    link.requestTempPatch();
}

// ── Performance-Modus (ADR-22) ──────────────────────────────────────────────

int Model::perfGet (uint32_t index, int p) const { return perf ? jd990_perf_get (perf.get(), index, p) : 0; }

juce::String Model::perfDisplay (uint32_t index, int p) const
{
    if (! perf) return {};
    char buf[64];
    jd990_perf_display (perf.get(), index, p, buf, sizeof buf);
    return juce::String::fromUTF8 (buf);
}

juce::String Model::perfName() const
{
    if (! perf) return {};
    char buf[32];
    jd990_perf_name (perf.get(), buf, sizeof buf);
    return juce::String::fromUTF8 (buf).trimEnd();
}

juce::String Model::partPatchName (int p) const
{
    if (! perfMode() || p < 0 || p > 6) return {};
    auto* pp = p == editPart ? current.get() : parts[(size_t) p].get();
    if (pp == nullptr) return {};
    char buf[32];
    jd990_patch_name (pp, buf, sizeof buf);
    return juce::String::fromUTF8 (buf).trimEnd();
}

juce::String Model::partSlot (int p) const
{
    if (! perf) return {};
    char buf[16];
    jd990_perf_patch_slot (perf.get(), p, buf, sizeof buf);
    return juce::String::fromUTF8 (buf);
}

int Model::partChannel (int p) const
{
    auto* ch = findPerf ("part.rx_ch");
    return ch != nullptr && perf ? perfGet (ch->index, p) + 1 : 1;
}

void Model::perfSet (uint32_t index, int p, int value)
{
    if (! perf || index >= perfAll.size()) return;
    auto& prm = perfAll[index];
    if (perfGet (index, p) == value) return;
    bool reload = prm.id == "part.patch";              // Gerät lädt den Speicher-Patch in den Part
    bool undoable = ! reload && prm.id != "part.bank"; // Patchwahl ist ein Laden, kein umkehrbarer Schritt
    auto ms = juce::Time::getMillisecondCounterHiRes();
    int64_t key = 100000 + (int64_t) index * 16 + (p + 1);
    if (undoable && (key != lastEditKey || ms - lastEditMs > 800.0)) pushPerfUndo ((int) index, p);
    lastEditKey = key;
    lastEditMs = ms;
    uint8_t msg[32];
    auto n = jd990_perf_set (perf.get(), index, p, (uint16_t) value, msg, sizeof msg);
    if (n > 0 && link.isOpen())
    {
        if (reload) link.sendPatch (std::vector<uint8_t> (msg, msg + n));   // Bulk: 700 ms Pause danach
        else link.queueParam (msg, (size_t) n);
    }
    if (reload)
    {
        if (link.isOpen()) readPartFromDevice (p);
        else status (u ("Part ") + juce::String (p + 1) + u (": Patch ") + partSlot (p) + u (" — offline, Patchdaten kommen erst vom Gerät"));
    }
    if (! link.isOpen()) setSync (u ("Editor ≠ Gerät (nicht verbunden)"), juce::Colours::grey);
    changed();
}

void Model::choosePartPatch (int p, int bank, int number)
{
    auto* b = findPerf ("part.bank");
    auto* pn = findPerf ("part.patch");
    if (! perf || b == nullptr || pn == nullptr || p < 0 || p > 6) return;
    // Bank und Nummer als EINE Folge: erst die Bank (lädt nichts), dann die Nummer (lädt) —
    // auch wenn die Nummer gleich bleibt, sonst wechselt der Inhalt nicht.
    std::vector<uint8_t> seq;
    uint8_t msg[32];
    auto n = jd990_perf_set (perf.get(), b->index, p, (uint16_t) bank, msg, sizeof msg);
    if (n > 0) seq.insert (seq.end(), msg, msg + n);
    n = jd990_perf_set (perf.get(), pn->index, p, (uint16_t) number, msg, sizeof msg);
    if (n > 0) seq.insert (seq.end(), msg, msg + n);
    if (link.isOpen())
    {
        link.sendPatch (seq);
        readPartFromDevice (p);
        status (u ("Part ") + juce::String (p + 1) + u (": lade ") + partSlot (p) + u (" …"));
    }
    else status (u ("Part ") + juce::String (p + 1) + u (" → ") + partSlot (p) + u (" — offline, Patchdaten kommen erst vom Gerät"));
    changed();
}

void Model::setPerfName (const juce::String& name)
{
    if (! perf || name == perfName()) return;
    pushPerfUndo (-1, -1);
    undoStack.back().what = u ("Performance-Name");
    uint8_t msg[40];
    auto n = jd990_perf_set_name (perf.get(), name.toRawUTF8(), msg, sizeof msg);
    if (n > 0 && link.isOpen()) link.queueParam (msg, (size_t) n);
    changed();
}

void Model::applyPerf (PerfPtr target)
{
    if (! target) return;
    if (perf && link.isOpen())
    {
        auto diff = bytes ([&] (uint8_t* o, size_t c) { return jd990_perf_diff_dt1 (perf.get(), target.get(), o, c); });
        if (! diff.empty()) link.sendPatch (diff);
    }
    perf = std::move (target);
}

void Model::selectPart (int p)
{
    if (! perfMode() || p < 0 || p > 6 || p == editPart) return;
    parts[(size_t) editPart] = std::move (current);
    slots[(size_t) editPart] = { std::move (undoStack), std::move (redoStack) };
    editPart = p;
    current = std::move (parts[(size_t) p]);
    undoStack = std::move (slots[(size_t) p].undo);
    redoStack = std::move (slots[(size_t) p].redo);
    lastEditKey = -1;
    lastIndex = -1;
    status (u ("Part ") + juce::String (p + 1) + u (" bearbeiten: „") + partPatchName (p) + u ("“"));
    changed();
}

void Model::enterPerformance (PerfPtr p, std::array<PatchPtr, 7> ps, const juce::String& origin)
{
    if (! perfMode())
    {
        patchModePatch = std::move (current);
        slots[7] = { std::move (undoStack), std::move (redoStack) };
    }
    perf = std::move (p);
    parts = std::move (ps);
    for (int n = 0; n < 7; ++n) slots[(size_t) n] = {};      // neues Setup: Undo je Part beginnt neu
    undoStack.clear();
    redoStack.clear();
    editPart = juce::jlimit (0, 6, editPart < 0 ? 0 : editPart);
    current = std::move (parts[(size_t) editPart]);
    loaded = true;
    lastEditKey = -1;
    lastIndex = -1;
    status (u ("Performance „") + perfName() + u ("“ aus ") + origin + u (" um ") + now());
    if (origin == u ("Gerät")) setSync (u ("Gerät = Editor (gelesen ") + now() + ")", juce::Colours::lightgreen);
    else setSync (u ("Editor ≠ Gerät (") + origin + u (", noch nicht gesendet)"), juce::Colours::orange);
    changed();
}

void Model::leavePerformance (PatchPtr p, const juce::String& origin)
{
    if (! perfMode()) return;
    parts[(size_t) editPart] = std::move (current);
    editPart = -1;
    current = p ? std::move (p) : (patchModePatch ? std::move (patchModePatch) : PatchPtr (jd990_patch_init()));
    undoStack = std::move (slots[7].undo);
    redoStack = std::move (slots[7].redo);
    lastEditKey = -1;
    lastIndex = -1;
    status (u ("Patch-Modus — ") + origin);
    changed();
}

void Model::readPartFromDevice (int p)
{
    uint8_t rq[32];
    auto n = jd990_rq1_patch (2, (uint8_t) p, rq, sizeof rq);
    link.request (std::vector<uint8_t> (rq, rq + n), 486, [this, p] (const std::vector<uint8_t>& b) {
        PatchPtr got (b.empty() ? nullptr : jd990_patch_from_bytes_at (b.data(), b.size(), 2, (uint8_t) p));
        if (! got || ! perfMode()) return;
        if (p == editPart) { current = std::move (got); lastEditKey = -1; }
        else parts[(size_t) p] = std::move (got);
        status (u ("Part ") + juce::String (p + 1) + u (": „") + partPatchName (p) + u ("“ vom Gerät geladen (") + partSlot (p) + ")");
        changed();
    });
}

void Model::readSetupFromDevice()
{
    if (! link.isOpen()) { status ("Nicht verbunden"); return; }
    struct Pending { PerfPtr perf; std::array<PatchPtr, 7> patches; int got = 0; bool failed = false; };
    auto acc = std::make_shared<Pending>();
    uint8_t rq[32];
    auto n = jd990_rq1_performance (1, 0, rq, sizeof rq);
    status (u ("Lese Performance und Parts 1–7 …"));
    link.request (std::vector<uint8_t> (rq, rq + n), 144, [acc] (const std::vector<uint8_t>& b) {
        acc->perf.reset (b.empty() ? nullptr : jd990_perf_from_bytes (b.data(), b.size()));
        acc->failed |= acc->perf == nullptr;
    });
    for (int k = 0; k < 7; ++k)
    {
        n = jd990_rq1_patch (2, (uint8_t) k, rq, sizeof rq);
        link.request (std::vector<uint8_t> (rq, rq + n), 486, [this, acc, k] (const std::vector<uint8_t>& b) {
            acc->patches[(size_t) k].reset (b.empty() ? nullptr : jd990_patch_from_bytes_at (b.data(), b.size(), 2, (uint8_t) k));
            acc->failed |= acc->patches[(size_t) k] == nullptr;
            if (++acc->got < 7) return;
            if (acc->failed) { status (u ("Performance konnte nicht vollständig gelesen werden")); return; }
            enterPerformance (std::move (acc->perf), std::move (acc->patches), u ("Gerät"));
        });
    }
}

void Model::sendSetupToDevice()
{
    if (! link.isOpen()) { status ("Nicht verbunden"); return; }
    if (! perfMode() || ! perf) return;
    if (deviceMode != 1) { status (u ("Das Gerät ist nicht im Performance-Modus — erst umschalten")); return; }
    // Reihenfolge Pflicht (P0): erst die Performance (lädt die Part-Patches aus dem Speicher), dann die Patches
    auto syx = bytes ([&] (uint8_t* o, size_t c) { return jd990_perf_to_syx (perf.get(), 1, 0, o, c); });
    for (int k = 0; k < 7; ++k)
    {
        auto* pp = k == editPart ? current.get() : parts[(size_t) k].get();
        auto b = bytes ([&] (uint8_t* o, size_t c) { return jd990_patch_to_syx (pp, 2, (uint8_t) k, o, c); });
        syx.insert (syx.end(), b.begin(), b.end());
    }
    link.sendPatch (syx);
    setSync (u ("Gerät = Editor (Setup gesendet ") + now() + ")", juce::Colours::lightgreen);
    changed();
}

void Model::checkDeviceMode()
{
    uint8_t rq[32];
    auto n = jd990_rq1_system (rq, sizeof rq);
    link.request (std::vector<uint8_t> (rq, rq + n), 38, [this] (const std::vector<uint8_t>& b) {
        if (b.size() < 10) { readFromDevice(); return; }
        deviceMode = b[9];                                  // erstes Datenbyte: System mode
        if (deviceMode == 1) readSetupFromDevice();
        else
        {
            if (perfMode()) leavePerformance (nullptr, u ("Gerät ist im Patch-Modus"));
            readFromDevice();
        }
        changed();
    });
}

void Model::setMode (int mode)
{
    if (mode != 0 && mode != 1) return;
    if (! link.isOpen())
    {
        // Offline: nur der Editor wechselt (zum Vorbereiten und für Tests)
        if (mode == 1 && ! perfMode())
        {
            std::array<PatchPtr, 7> ps;
            for (auto& x : ps) x.reset (jd990_patch_init());
            enterPerformance (perf ? PerfPtr (jd990_perf_clone (perf.get())) : PerfPtr (jd990_perf_init()), std::move (ps), u ("Init (offline)"));
        }
        else if (mode == 0) leavePerformance (nullptr, u ("offline"));
        return;
    }
    uint8_t msg[16];
    auto n = jd990_system_mode_dt1 ((uint8_t) mode, msg, sizeof msg);
    if (n <= 0) return;
    link.sendPatch (std::vector<uint8_t> (msg, msg + n));   // Bulk: danach 700 ms Pause (P0: ≈ 0,5 s)
    status (mode == 1 ? u ("Schalte das Gerät in den Performance-Modus …") : u ("Schalte das Gerät in den Patch-Modus …"));
    juce::Timer::callAfterDelay (900, [this, mode] {
        deviceMode = mode;
        if (mode == 1) readSetupFromDevice();   // das Gerät hat Performance + Parts aus dem Speicher geladen
        else { leavePerformance (nullptr, u ("Gerät umgeschaltet")); checking = false; link.requestTempPatch(); }
        changed();
    });
}
