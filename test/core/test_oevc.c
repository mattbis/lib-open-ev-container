#include "oevc/oevc.h"
#include "oevc/sym_il.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void test_crc32_known_vector(void) {
    const char *msg = "123456789";
    uint32_t crc = oevc_crc32(0u, msg, 9);
    assert(crc == 0xCBF43926u);
}

static void test_create_save_load_roundtrip(void) {
    const char *path = "var/build/t_roundtrip.oevc";
    oevc_container_t cont;
    assert(oevc_init(&cont, OEV_CONT_COMPLETE, 0u) == OEVC_OK);

    const char *open_data = "open-buf";
    const uint8_t vendor_data[4] = { 1, 2, 3, 4 };
    uint32_t id1 = 0, id2 = 0;

    assert(oevc_add_chunk(&cont, OEV_TIER_OPEN, OEV_KIND_RAW_BUF, 0, open_data, 8, &id1) == OEVC_OK);
    assert(oevc_add_chunk(&cont, OEV_TIER_VENDOR_VOID, OEV_KIND_VENDOR_BLOB, 0, vendor_data, 4, &id2) == OEVC_OK);
    assert(oevc_save_file(&cont, path) == OEVC_OK);
    oevc_free(&cont);

    oevc_container_t loaded;
    assert(oevc_load_file(&loaded, path) == OEVC_OK);
    assert(loaded.header.version == OEVC_FORMAT_VERSION);
    assert(loaded.header.qrate == 0u);
    assert(loaded.chunk_count == 2);
    assert(oevc_count_chunks(&loaded, OEV_TIER_OPEN, -1, 0) == 1);
    assert(oevc_count_chunks(&loaded, OEV_TIER_VENDOR_VOID, -1, 0) == 1);

    const oevc_chunk_t *c1 = oevc_find_chunk(&loaded, id1);
    assert(c1 != NULL && c1->payload_len == 8 && memcmp(c1->payload, open_data, 8) == 0);
    oevc_free(&loaded);
}

static void test_append_and_consolidate(void) {
    const char *path = "var/build/t_append.oevc";
    const char *cons_path = "var/build/t_cons.oevc";

    oevc_container_t cont;
    assert(oevc_init(&cont, OEV_CONT_FAST_SAVE, 1u) == OEVC_OK);
    uint32_t id1 = 0, id2 = 0;
    assert(oevc_add_chunk(&cont, OEV_TIER_OPEN, OEV_KIND_RAW_BUF, 0, "aaa", 3, &id1) == OEVC_OK);
    assert(oevc_add_chunk(&cont, OEV_TIER_OPEN, OEV_KIND_RAW_BUF, 0, "bbb", 3, &id2) == OEVC_OK);
    assert(oevc_mark_deleted(&cont, id1) == OEVC_OK);
    assert(oevc_save_file(&cont, path) == OEVC_OK);
    oevc_free(&cont);

    assert(oevc_append_chunk_file(path, OEV_TIER_OPEN, OEV_KIND_RAW_BUF, 0, 99, "ccc", 3) == OEVC_OK);
    assert(oevc_consolidate_file(path, cons_path) == OEVC_OK);

    oevc_container_t cons;
    assert(oevc_load_file(&cons, cons_path) == OEVC_OK);
    assert(cons.chunk_count == 2);
    assert(oevc_find_chunk(&cons, id1) == NULL);
    assert(oevc_find_chunk(&cons, id2) != NULL);
    assert(oevc_find_chunk(&cons, 99) != NULL);
    oevc_free(&cons);
}

static void test_rtest_and_repair_corrupt_chunk(void) {
    const char *bad_path = "var/build/t_corrupt.oevc";
    const char *rep_path = "var/build/t_repaired.oevc";

    oevc_container_t cont;
    assert(oevc_init(&cont, OEV_CONT_COMPLETE, 0u) == OEVC_OK);
    uint32_t id1 = 0, id2 = 0, id3 = 0;
    assert(oevc_add_chunk(&cont, OEV_TIER_OPEN, OEV_KIND_RAW_BUF, 0, "first_ok", 8, &id1) == OEVC_OK);
    assert(oevc_add_chunk(&cont, OEV_TIER_OPEN, OEV_KIND_RAW_BUF, 0, "bad_data", 8, &id2) == OEVC_OK);
    assert(oevc_add_chunk(&cont, OEV_TIER_OPEN, OEV_KIND_RAW_BUF, 0, "third_ok", 8, &id3) == OEVC_OK);
    assert(oevc_save_file(&cont, bad_path) == OEVC_OK);
    oevc_free(&cont);

    /* Load offsets, then flip a byte inside chunk 2's payload on disk */
    oevc_container_t tmp;
    assert(oevc_load_file(&tmp, bad_path) == OEVC_OK);
    uint64_t flip_offset = tmp.chunks[1].file_offset + OEVC_CHUNK_HEADER_SIZE + 2;
    oevc_free(&tmp);

    FILE *fp = fopen(bad_path, "r+b");
    assert(fp != NULL);
    assert(fseek(fp, (long)flip_offset, SEEK_SET) == 0);
    fputc('X', fp);
    fclose(fp);

    oevc_rtest_report_t report;
    assert(oevc_rtest_file(bad_path, &report) == OEVC_OK);
    assert(report.header_valid == 1);
    assert(report.needs_repair == 1);
    assert(report.valid_chunks == 2);
    assert(report.corrupt_chunks == 1);

    assert(oevc_repair_file(bad_path, rep_path, &report) == OEVC_OK);
    assert(report.valid_chunks == 2);

    oevc_container_t fixed;
    assert(oevc_load_file(&fixed, rep_path) == OEVC_OK);
    assert(fixed.chunk_count == 2);
    assert(oevc_find_chunk(&fixed, id1) != NULL);
    assert(oevc_find_chunk(&fixed, id2) == NULL);
    assert(oevc_find_chunk(&fixed, id3) != NULL);
    oevc_free(&fixed);
}

static void test_sym_il_and_json_export(void) {
    const char *json_path = "var/build/t_export.json";
    oevc_container_t cont;
    assert(oevc_init(&cont, OEV_CONT_COMPLETE, 0u) == OEVC_OK);

    oevc_sym_meta_t meta = { 120000, 960, "UnitTest", "Tester", "TestDAW" };
    assert(oevc_sym_add_meta(&cont, &meta, NULL) == OEVC_OK);

    oevc_sym_track_t tr = { 1, OEV_TRACK_MIDI, 0, 0, "Keys" };
    assert(oevc_sym_add_track(&cont, &tr, NULL) == OEVC_OK);

    oevc_sym_note_t notes[1] = { { 0, 480, 60, 100, 0 } };
    oevc_sym_clip_t cl = { 1, 1, OEV_CLIP_MIDI, 0, 960, "", notes, 1 };
    assert(oevc_sym_add_clip(&cont, &cl, NULL) == OEVC_OK);

    assert(oevc_export_json_file(&cont, json_path) == OEVC_OK);

    FILE *fp = fopen(json_path, "rb");
    assert(fp != NULL);
    char buf[1024];
    size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
    fclose(fp);
    buf[n] = '\0';
    assert(strstr(buf, "\"project_name\": \"UnitTest\"") != NULL);
    assert(strstr(buf, "\"name\": \"Keys\"") != NULL);

    oevc_free(&cont);
}

int main(void) {
    test_crc32_known_vector();
    test_create_save_load_roundtrip();
    test_append_and_consolidate();
    test_rtest_and_repair_corrupt_chunk();
    test_sym_il_and_json_export();
    printf("all oevc tests passed!\n");
    return 0;
}
