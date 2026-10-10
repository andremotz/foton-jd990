/*
 * jd990.h — C-Schnittstelle zu jd990-core (Rust). Konventionen siehe src/lib.rs.
 * Von Hand gepflegt; der Smoke-Test tests/c_smoke.sh prüft Header und ABI gemeinsam.
 */
#ifndef JD990_H
#define JD990_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define JD990_E_NULL  (-1)
#define JD990_E_PARSE (-2)
#define JD990_E_INDEX (-3)
#define JD990_E_RANGE (-4)
#define JD990_E_TONE  (-5)

typedef struct Jd990Patch Jd990Patch;

typedef struct {
    uint16_t min, max;   /* Rohbereich */
    uint16_t offset;     /* im Patch Common bzw. im Tone */
    uint8_t  bytes;      /* 1 oder 2 (MSB/LSB) */
    uint8_t  is_tone;    /* 1 = Tone-Parameter (tone 0–3), 0 = Patch (tone -1) */
    uint8_t  verified;
    uint16_t group_rank; /* Reihenfolge der Gruppe im UI (group_order der Spec) */
    uint8_t  kind;       /* 0 linear, 1 Enum, 2 Tabelle, 3 Pan, 4 Note, 5 Wellenform */
} Jd990ParamInfo;

uint32_t    jd990_spec_version(void);
/* SysEx-Device-ID aller erzeugten Nachrichten: 0x10–0x1F (am Gerät „Unit #“ 17–32), Standard 0x10 */
int32_t     jd990_set_device_id(uint8_t id);
uint8_t     jd990_device_id(void);

Jd990Patch *jd990_patch_new(void);
Jd990Patch *jd990_patch_init(void);                                   /* Init-Patch aus der Spec */
Jd990Patch *jd990_patch_clone(const Jd990Patch *p);
int32_t     jd990_patch_equal(const Jd990Patch *a, const Jd990Patch *b);
int32_t     jd990_patch_diff_dt1(const Jd990Patch *from, const Jd990Patch *to, uint8_t *out, size_t cap);
Jd990Patch *jd990_patch_from_bytes(const uint8_t *data, size_t len);   /* .syx oder .mid */
void        jd990_patch_free(Jd990Patch *p);
int32_t     jd990_patch_name(const Jd990Patch *p, char *out, size_t cap);
int32_t     jd990_patch_to_syx(const Jd990Patch *p, uint8_t area, uint8_t number, uint8_t *out, size_t cap);

uint32_t    jd990_param_count(void);
int32_t     jd990_param_info(uint32_t index, Jd990ParamInfo *info);
int32_t     jd990_param_text(uint32_t index, uint32_t what /*0=id,1=label,2=group*/, char *out, size_t cap);
int32_t     jd990_param_find(const char *id);
int32_t     jd990_param_init(uint32_t index);                         /* Init-Wert (Doppelklick) */
int32_t     jd990_param_locked(uint32_t index, uint16_t raw);   /* 1 = UI sperrt (out of scope) */
int32_t     jd990_param_format(uint32_t index, uint16_t raw, char *out, size_t cap);  /* ohne Patch-Kontext */

int32_t     jd990_patch_get(const Jd990Patch *p, uint32_t index, int32_t tone);
int32_t     jd990_patch_set(Jd990Patch *p, uint32_t index, int32_t tone, uint16_t value, uint8_t *out_dt1, size_t cap);
int32_t     jd990_patch_display(const Jd990Patch *p, uint32_t index, int32_t tone, char *out, size_t cap);

int32_t     jd990_rq1_temp_patch(uint8_t *out, size_t cap);
int32_t     jd990_rq1_patch(uint8_t area /*3 Temp, 2 Performance-Part 0–6, 6 Internal*/, uint8_t number, uint8_t *out, size_t cap);

/* ── Bibliothek (M5) — Listen als JSON (UTF-8) ─────────────────────────────── */
typedef struct Jd990Library Jd990Library;
Jd990Library *jd990_lib_open(const char *path);                       /* NULL = Standardort */
void        jd990_lib_close(Jd990Library *l);
int32_t     jd990_lib_default_path(char *out, size_t cap);
int32_t     jd990_lib_import_file(const Jd990Library *l, const char *path, int32_t *added, int32_t *dupes);
int32_t     jd990_lib_list_json(const Jd990Library *l, const char *query, int32_t favorites_only, char *out, size_t cap);
Jd990Patch *jd990_lib_get(const Jd990Library *l, int64_t id);
int64_t     jd990_lib_add(const Jd990Library *l, const Jd990Patch *p, const char *source);   /* >0 neu, <0 −ID Dublette, 0 Fehler */
int32_t     jd990_lib_set_notes(const Jd990Library *l, int64_t id, const char *text);
int32_t     jd990_lib_set_favorite(const Jd990Library *l, int64_t id, int32_t on);
int32_t     jd990_lib_set_tags(const Jd990Library *l, int64_t id, const char *tags_csv);
int32_t     jd990_lib_remove(const Jd990Library *l, int64_t id);
int32_t     jd990_lib_similar_json(const Jd990Library *l, const Jd990Patch *p, size_t limit, char *out, size_t cap);   /* [[id, 0…1], …] */
int32_t     jd990_lib_export(const Jd990Library *l, const int64_t *ids, size_t n, const char *path);
int32_t     jd990_patch_diff_json(const Jd990Patch *a, const Jd990Patch *b, char *out, size_t cap);

/* ── Morph / Variation (M5) ──────────────────────────────────────────────── */
Jd990Patch *jd990_patch_morph(const Jd990Patch *a, const Jd990Patch *b, float t);          /* t 0 = a, 1 = b */
Jd990Patch *jd990_patch_variation(const Jd990Patch *base, float amount, uint64_t seed);  /* amount 0…1 */

/* ── Tones kopieren / tauschen — liefern einen neuen Patch ───────────────── */
Jd990Patch *jd990_patch_tone_copy(const Jd990Patch *p, int32_t from, int32_t to, const char *group /* NULL = ganzer Tone */);
Jd990Patch *jd990_patch_tone_swap(const Jd990Patch *p, int32_t a, int32_t b);

/* ── Performance-Modus (ADR-22) ──────────────────────────────────────────── */
/* Patch-DT1 (Temp 03) auf Temp (area 3) oder Performance-Part (area 2, number 0–6) umadressieren */
int32_t     jd990_dt1_retarget(const uint8_t *msgs, size_t len, uint8_t area, uint8_t number, uint8_t *out, size_t cap);
int32_t     jd990_system_mode_dt1(uint8_t mode /*0 PATCH, 1 PERFORMANCE*/, uint8_t *out, size_t cap);
int32_t     jd990_rq1_system(uint8_t *out, size_t cap);                      /* 38 Byte, Byte 0 = Modus */
int32_t     jd990_rq1_performance(uint8_t area /*1 Temp, 5 Internal*/, uint8_t number, uint8_t *out, size_t cap);
Jd990Patch *jd990_patch_from_bytes_at(const uint8_t *data, size_t len, uint8_t area, uint8_t number);

typedef struct Jd990Performance Jd990Performance;
uint32_t    jd990_perf_param_count(void);                                   /* Common, dann Part-Parameter */
int32_t     jd990_perf_param_info(uint32_t index, Jd990ParamInfo *info);    /* is_tone = 1: je Part */
int32_t     jd990_perf_param_text(uint32_t index, uint32_t what, char *out, size_t cap);
int32_t     jd990_perf_param_init(uint32_t index);
int32_t     jd990_perf_param_format(uint32_t index, uint16_t raw, char *out, size_t cap);
Jd990Performance *jd990_perf_init(void);
Jd990Performance *jd990_perf_clone(const Jd990Performance *p);
void        jd990_perf_free(Jd990Performance *p);
Jd990Performance *jd990_perf_from_bytes(const uint8_t *data, size_t len);
int32_t     jd990_perf_name(const Jd990Performance *p, char *out, size_t cap);
int32_t     jd990_perf_set_name(Jd990Performance *p, const char *name, uint8_t *out_dt1, size_t cap);
int32_t     jd990_perf_get(const Jd990Performance *p, uint32_t index, int32_t part /*-1 Common, 0–7*/);
int32_t     jd990_perf_set(Jd990Performance *p, uint32_t index, int32_t part, uint16_t value, uint8_t *out_dt1, size_t cap);
int32_t     jd990_perf_display(const Jd990Performance *p, uint32_t index, int32_t part, char *out, size_t cap);
int32_t     jd990_perf_voices(const Jd990Performance *p);                   /* Summe reserviert, ≤ 24 */
int32_t     jd990_perf_patch_slot(const Jd990Performance *p, int32_t part, char *out, size_t cap);
int32_t     jd990_perf_to_syx(const Jd990Performance *p, uint8_t area /*1 Temp, 5 Internal*/, uint8_t number, uint8_t *out, size_t cap);
int32_t     jd990_perf_diff_dt1(const Jd990Performance *from, const Jd990Performance *to, uint8_t *out, size_t cap);

#ifdef __cplusplus
}
#endif
#endif
