/* Smoke-Test der C-ABI: lädt einen Patch, liest/setzt Parameter, formatiert. */
#include "jd990.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(c) do { if (!(c)) { fprintf(stderr, "FEHLER Zeile %d: %s\n", __LINE__, #c); return 1; } } while (0)

int main(int argc, char **argv) {
    char buf[64];
    uint8_t msg[600];
    CHECK(jd990_spec_version() == 2);
    CHECK(jd990_param_count() == 99 + 91);

    int32_t cutoff = jd990_param_find("tone.tvf.cutoff");
    CHECK(cutoff >= 0);
    Jd990ParamInfo info;
    CHECK(jd990_param_info((uint32_t)cutoff, &info) == 0);
    CHECK(info.min == 0 && info.max == 100 && info.is_tone == 1 && info.kind == 0);
    int32_t mode = jd990_param_find("tone.tvf.filter_mode");
    CHECK(jd990_param_info((uint32_t)mode, &info) == 0 && info.kind == 1);
    CHECK(jd990_param_format((uint32_t)mode, 0, buf, sizeof buf) == 4 && strcmp(buf, "HPF") == 0);
    int32_t src = jd990_param_find("tone.wg.wave_source");
    CHECK(jd990_param_locked((uint32_t)src, 0) == 0 && jd990_param_locked((uint32_t)src, 1) == 1);

    Jd990Patch *p = NULL;
    if (argc > 1) {                       /* echter Patch aus dem Korpus */
        FILE *f = fopen(argv[1], "rb");
        CHECK(f);
        uint8_t data[4096];
        size_t n = fread(data, 1, sizeof data, f);
        fclose(f);
        p = jd990_patch_from_bytes(data, n);
        CHECK(p);
        CHECK(jd990_patch_name(p, buf, sizeof buf) == 17);
        printf("Patch: \"%s\"\n", buf);
    } else {
        p = jd990_patch_new();
    }

    /* Puffer zu klein → Länge, Patch bleibt unverändert */
    int32_t before = jd990_patch_get(p, (uint32_t)cutoff, 2);
    CHECK(jd990_patch_set(p, (uint32_t)cutoff, 2, 77, msg, 3) == 12);
    CHECK(jd990_patch_get(p, (uint32_t)cutoff, 2) == before);

    int32_t len = jd990_patch_set(p, (uint32_t)cutoff, 2, 77, msg, sizeof msg);
    CHECK(len == 12);
    CHECK(msg[0] == 0xF0 && msg[4] == 0x12 && msg[5] == 0x03 && msg[len - 1] == 0xF7);
    CHECK(jd990_patch_get(p, (uint32_t)cutoff, 2) == 77);
    CHECK(jd990_patch_set(p, (uint32_t)cutoff, 2, 101, msg, sizeof msg) == JD990_E_RANGE);
    CHECK(jd990_patch_set(p, (uint32_t)cutoff, -1, 50, msg, sizeof msg) == JD990_E_TONE);

    int32_t coarse = jd990_param_find("tone.wg.pitch_coarse");
    jd990_patch_set(p, (uint32_t)coarse, 0, 60, msg, sizeof msg);
    jd990_patch_display(p, (uint32_t)coarse, 0, buf, sizeof buf);
    CHECK(strcmp(buf, "+12 st") == 0);

    CHECK(jd990_patch_to_syx(p, 3, 0, NULL, 0) == 486 + 3 * 11);
    CHECK(jd990_patch_to_syx(p, 5, 0, NULL, 0) == JD990_E_RANGE);  /* nur Temp/Internal */
    CHECK(jd990_rq1_temp_patch(msg, sizeof msg) == 15);

    /* Init, Clone, Diff */
    Jd990Patch *init = jd990_patch_init();
    jd990_patch_name(init, buf, sizeof buf);
    CHECK(strcmp(buf, "INIT PATCH      ") == 0);
    CHECK(jd990_param_init((uint32_t)cutoff) == 100);
    Jd990Patch *c = jd990_patch_clone(init);
    CHECK(jd990_patch_equal(init, c) == 1);
    CHECK(jd990_patch_diff_dt1(init, c, NULL, 0) == 0);
    jd990_patch_set(c, (uint32_t)cutoff, 1, 12, msg, sizeof msg);
    CHECK(jd990_patch_equal(init, c) == 0);
    CHECK(jd990_patch_diff_dt1(init, c, msg, sizeof msg) == 12);
    jd990_patch_free(c);
    jd990_patch_free(init);

    /* Bibliothek in einer Wegwerf-Datei */
    remove("target/debug/jd990-smoke.sqlite");
    Jd990Library *lib = jd990_lib_open("target/debug/jd990-smoke.sqlite");
    CHECK(lib);
    Jd990Patch *i1 = jd990_patch_init();
    int64_t id = jd990_lib_add(lib, i1, "Smoke");
    CHECK(id > 0);
    CHECK(jd990_lib_add(lib, i1, "nochmal") == -id);
    CHECK(jd990_lib_set_tags(lib, id, "init,test") == 0);
    char json[4096];
    CHECK(jd990_lib_list_json(lib, "test", 0, json, sizeof json) > 0 && strstr(json, "\"INIT PATCH\"") != NULL);
    Jd990Patch *g = jd990_lib_get(lib, id);
    CHECK(g && jd990_patch_equal(g, i1) == 1);
    jd990_patch_set(g, (uint32_t)cutoff, 0, 40, msg, sizeof msg);
    CHECK(jd990_patch_diff_json(i1, g, json, sizeof json) > 0 && strstr(json, "toneA.tvf.cutoff") != NULL);
    Jd990Patch *half = jd990_patch_morph(i1, g, 0.5f);
    CHECK(half && jd990_patch_get(half, (uint32_t)cutoff, 0) == 70);
    Jd990Patch *var = jd990_patch_variation(i1, 0.3f, 42);
    CHECK(var && jd990_patch_equal(var, i1) == 0);
    jd990_patch_free(var);
    Jd990Patch *cp = jd990_patch_tone_copy(g, 0, 3, "TVF");
    CHECK(cp && jd990_patch_get(cp, (uint32_t)cutoff, 3) == 40);
    Jd990Patch *sw = jd990_patch_tone_swap(g, 0, 1);
    CHECK(sw && jd990_patch_get(sw, (uint32_t)cutoff, 1) == 40 && jd990_patch_get(sw, (uint32_t)cutoff, 0) == 100);
    jd990_patch_free(sw);
    jd990_patch_free(cp);
    jd990_patch_free(half);
    jd990_patch_free(g);
    jd990_patch_free(i1);
    jd990_lib_close(lib);
    remove("target/debug/jd990-smoke.sqlite");

    /* Performance-Modus (ADR-22) */
    CHECK(jd990_perf_param_count() == 28 + 11);
    Jd990Performance *pf = jd990_perf_init();
    CHECK(pf && jd990_perf_voices(pf) == 24);
    CHECK(jd990_perf_name(pf, buf, sizeof buf) > 0 && strncmp(buf, "INIT PERFORM", 12) == 0);
    int32_t level = -1, ch = -1;
    for (uint32_t i = 0; i < jd990_perf_param_count(); ++i) {
        jd990_perf_param_text(i, 0, buf, sizeof buf);
        if (strcmp(buf, "part.level") == 0) level = (int32_t)i;
        if (strcmp(buf, "part.rx_ch") == 0) ch = (int32_t)i;
    }
    CHECK(level >= 0 && ch >= 0);
    CHECK(jd990_perf_param_info((uint32_t)level, &info) == 0 && info.is_tone == 1);
    CHECK(jd990_perf_display(pf, (uint32_t)ch, 2, buf, sizeof buf) > 0 && strcmp(buf, "3") == 0);
    int32_t n = jd990_perf_set(pf, (uint32_t)level, 4, 50, msg, sizeof msg);   /* Part 5 Level */
    CHECK(n == 12 && msg[5] == 0x01 && msg[8] == 0x64 && msg[9] == 50);
    CHECK(jd990_perf_get(pf, (uint32_t)level, 4) == 50);
    CHECK(jd990_perf_get(pf, (uint32_t)level, -1) < 0);                        /* Part-Parameter ohne Part */
    CHECK(jd990_perf_patch_slot(pf, 6, buf, sizeof buf) > 0 && strcmp(buf, "I:17") == 0);
    /* Patch-DT1 auf Part 3 umadressieren */
    Jd990Patch *tp = jd990_patch_init();
    n = jd990_patch_set(tp, (uint32_t)cutoff, 0, 40, msg, sizeof msg);
    uint8_t moved[64];
    CHECK(jd990_dt1_retarget(msg, (size_t)n, 2, 2, moved, sizeof moved) == n);
    CHECK(moved[5] == 0x02 && moved[6] == 0x02 && moved[7] == msg[7] && moved[8] == msg[8]);
    CHECK(jd990_dt1_retarget(msg, (size_t)n, 6, 0, moved, sizeof moved) < 0);    /* nie in den Speicher */
    CHECK(jd990_system_mode_dt1(1, moved, sizeof moved) == 12 && moved[5] == 0 && moved[9] == 1);
    CHECK(jd990_system_mode_dt1(2, moved, sizeof moved) < 0);                   /* RHYTHM nie */
    jd990_patch_free(tp);
    jd990_perf_free(pf);

    jd990_patch_free(p);
    printf("C-ABI Smoke-Test: OK\n");
    return 0;
}
