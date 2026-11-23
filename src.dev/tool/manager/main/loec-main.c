#include "oevc/oevc.h"
#include "oevc/sym_il.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_usage(const char *prog) {
    fprintf(stderr,
        "loec - lib-open-ev-container manager CLI (v0)\n"
        "Usage:\n"
        "  %s create <file> [--fast]     Create an empty container\n"
        "  %s q <file>                   Query container header and chunks\n"
        "  %s rtest <file>               Verify header and all chunk CRCs\n"
        "  %s repair <src> <dst>         Salvage valid chunks into <dst>\n"
        "  %s consolidate <src> <dst>    Strip deleted chunks into <dst>\n"
        "  %s export <file> [out.json]   Export open tier + opaque summary to JSON\n",
        prog, prog, prog, prog, prog, prog);
}

static int cmd_create(int argc, char **argv) {
    if (argc < 3) return 1;
    const char *path = argv[2];
    int fast = (argc >= 4 && strcmp(argv[3], "--fast") == 0);

    oevc_container_t cont;
    oevc_status_t st = oevc_init(&cont, fast ? OEV_CONT_FAST_SAVE : OEV_CONT_COMPLETE, fast ? 1u : 0u);
    if (st == OEVC_OK) st = oevc_save_file(&cont, path);
    oevc_free(&cont);

    if (st != OEVC_OK) {
        fprintf(stderr, "error: create failed: %s\n", oevc_status_str(st));
        return 1;
    }
    printf("created %s (%s, qrate=%u)\n", path, fast ? "fast_save" : "complete", fast ? 1u : 0u);
    return 0;
}

static int cmd_query(int argc, char **argv) {
    if (argc < 3) return 1;
    oevc_container_t cont;
    oevc_status_t st = oevc_load_file(&cont, argv[2]);
    if (st != OEVC_OK) {
        fprintf(stderr, "error: load failed: %s\n", oevc_status_str(st));
        return 1;
    }

    printf("version=%u flags=0x%04x qrate=%u chunks=%zu\n",
           cont.header.version, cont.header.flags, cont.header.qrate, cont.chunk_count);
    for (size_t i = 0; i < cont.chunk_count; ++i) {
        const oevc_chunk_t *c = &cont.chunks[i];
        printf("  id=%-4u tier=%-11s kind=%-11s len=%-6u crc=0x%08X%s\n",
               c->chunk_id,
               oevc_tier_name(c->tier),
               oevc_kind_name(c->kind),
               c->payload_len,
               c->crc32,
               (c->flags & OEV_CHUNK_DELETED) ? " [DELETED]" : "");
    }
    oevc_free(&cont);
    return 0;
}

static int cmd_rtest(int argc, char **argv) {
    if (argc < 3) return 1;
    oevc_rtest_report_t rep;
    oevc_status_t st = oevc_rtest_file(argv[2], &rep);
    if (st != OEVC_OK) {
        fprintf(stderr, "error: rtest failed: %s\n", oevc_status_str(st));
        return 1;
    }
    printf("header=%s scanned=%zu valid=%zu corrupt=%zu needs_repair=%s\n",
           rep.header_valid ? "ok" : "corrupt",
           rep.total_chunks_scanned,
           rep.valid_chunks,
           rep.corrupt_chunks,
           rep.needs_repair ? "yes" : "no");
    return rep.needs_repair ? 2 : 0;
}

static int cmd_repair(int argc, char **argv) {
    if (argc < 4) return 1;
    oevc_rtest_report_t rep;
    oevc_status_t st = oevc_repair_file(argv[2], argv[3], &rep);
    if (st != OEVC_OK) {
        fprintf(stderr, "error: repair failed: %s\n", oevc_status_str(st));
        return 1;
    }
    printf("repaired -> %s (salvaged=%zu corrupt_skipped=%zu)\n",
           argv[3], rep.valid_chunks, rep.corrupt_chunks);
    return 0;
}

static int cmd_consolidate(int argc, char **argv) {
    if (argc < 4) return 1;
    oevc_status_t st = oevc_consolidate_file(argv[2], argv[3]);
    if (st != OEVC_OK) {
        fprintf(stderr, "error: consolidate failed: %s\n", oevc_status_str(st));
        return 1;
    }
    printf("consolidated -> %s\n", argv[3]);
    return 0;
}

static int cmd_export(int argc, char **argv) {
    if (argc < 3) return 1;
    oevc_container_t cont;
    oevc_status_t st = oevc_load_file(&cont, argv[2]);
    if (st != OEVC_OK) {
        fprintf(stderr, "error: load failed: %s\n", oevc_status_str(st));
        return 1;
    }
    if (argc >= 4) {
        st = oevc_export_json_file(&cont, argv[3]);
        if (st == OEVC_OK) printf("exported -> %s\n", argv[3]);
    } else {
        st = oevc_export_json_stream(&cont, stdout);
    }
    oevc_free(&cont);
    if (st != OEVC_OK) {
        fprintf(stderr, "error: export failed: %s\n", oevc_status_str(st));
        return 1;
    }
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }
    const char *cmd = argv[1];
    if (strcmp(cmd, "create") == 0)      return cmd_create(argc, argv);
    if (strcmp(cmd, "q") == 0)           return cmd_query(argc, argv);
    if (strcmp(cmd, "rtest") == 0)       return cmd_rtest(argc, argv);
    if (strcmp(cmd, "repair") == 0)      return cmd_repair(argc, argv);
    if (strcmp(cmd, "consolidate") == 0) return cmd_consolidate(argc, argv);
    if (strcmp(cmd, "export") == 0)      return cmd_export(argc, argv);

    print_usage(argv[0]);
    return 1;
}
