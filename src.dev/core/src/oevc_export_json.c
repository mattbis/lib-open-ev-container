#include "oevc/sym_il.h"

#include <inttypes.h>

static void write_json_escaped(FILE *out, const char *s) {
    fputc('"', out);
    if (s) {
        for (const unsigned char *p = (const unsigned char *)s; *p; ++p) {
            switch (*p) {
                case '"':  fputs("\\\"", out); break;
                case '\\': fputs("\\\\", out); break;
                case '\b': fputs("\\b", out); break;
                case '\f': fputs("\\f", out); break;
                case '\n': fputs("\\n", out); break;
                case '\r': fputs("\\r", out); break;
                case '\t': fputs("\\t", out); break;
                default:
                    if (*p < 0x20) {
                        fprintf(out, "\\u%04x", *p);
                    } else {
                        fputc(*p, out);
                    }
                    break;
            }
        }
    }
    fputc('"', out);
}

static const char *track_type_str(uint8_t t) {
    switch (t) {
        case OEV_TRACK_MIDI:  return "midi";
        case OEV_TRACK_AUDIO: return "audio";
        case OEV_TRACK_BUS:   return "bus";
        default:              return "unknown";
    }
}

static const char *clip_type_str(uint8_t t) {
    switch (t) {
        case OEV_CLIP_MIDI:  return "midi";
        case OEV_CLIP_AUDIO: return "audio";
        default:             return "unknown";
    }
}

oevc_status_t oevc_export_json_stream(const oevc_container_t *cont, FILE *out) {
    if (!cont || !out) return OEVC_ERR_INVALID_ARG;

    fprintf(out, "{\n");
    fprintf(out, "  \"container\": {\n");
    fprintf(out, "    \"version\": %u,\n", cont->header.version);
    fprintf(out, "    \"flags\": %u,\n", cont->header.flags);
    fprintf(out, "    \"qrate\": %u,\n", cont->header.qrate);
    fprintf(out, "    \"chunk_count\": %zu\n", cont->chunk_count);
    fprintf(out, "  },\n");

    /* Meta */
    int has_meta = 0;
    for (size_t i = 0; i < cont->chunk_count; ++i) {
        const oevc_chunk_t *c = &cont->chunks[i];
        if ((c->flags & OEV_CHUNK_DELETED) || c->kind != OEV_KIND_META) continue;
        oevc_sym_meta_t m;
        if (oevc_sym_decode_meta(c, &m) == OEVC_OK) {
            has_meta = 1;
            fprintf(out, "  \"meta\": {\n");
            fprintf(out, "    \"project_name\": "); write_json_escaped(out, m.project_name); fprintf(out, ",\n");
            fprintf(out, "    \"author\": ");       write_json_escaped(out, m.author);       fprintf(out, ",\n");
            fprintf(out, "    \"software\": ");     write_json_escaped(out, m.software);     fprintf(out, ",\n");
            fprintf(out, "    \"bpm\": %.3f,\n", (double)m.bpm_milli / 1000.0);
            fprintf(out, "    \"ppq\": %u\n", m.ppq);
            fprintf(out, "  },\n");
            break;
        }
    }
    if (!has_meta) {
        fprintf(out, "  \"meta\": null,\n");
    }

    /* Tracks */
    fprintf(out, "  \"tracks\": [\n");
    int first = 1;
    for (size_t i = 0; i < cont->chunk_count; ++i) {
        const oevc_chunk_t *c = &cont->chunks[i];
        if ((c->flags & OEV_CHUNK_DELETED) || c->kind != OEV_KIND_SYM_TRACK) continue;
        oevc_sym_track_t t;
        if (oevc_sym_decode_track(c, &t) != OEVC_OK) continue;
        if (!first) fprintf(out, ",\n");
        first = 0;
        fprintf(out, "    {\"track_id\": %u, \"type\": \"%s\", \"name\": ",
                t.track_id, track_type_str(t.track_type));
        write_json_escaped(out, t.name);
        fprintf(out, ", \"gain_db\": %.3f, \"pan\": %.3f}",
                (double)t.gain_db_milli / 1000.0, (double)t.pan_milli / 1000.0);
    }
    fprintf(out, "\n  ],\n");

    /* Clips */
    fprintf(out, "  \"clips\": [\n");
    first = 1;
    for (size_t i = 0; i < cont->chunk_count; ++i) {
        const oevc_chunk_t *c = &cont->chunks[i];
        if ((c->flags & OEV_CHUNK_DELETED) || c->kind != OEV_KIND_SYM_CLIP) continue;
        oevc_sym_clip_t cl;
        if (oevc_sym_decode_clip(c, &cl) != OEVC_OK) continue;
        if (!first) fprintf(out, ",\n");
        first = 0;
        fprintf(out, "    {\n");
        fprintf(out, "      \"clip_id\": %u, \"track_id\": %u, \"type\": \"%s\", \"start_tick\": %" PRIu64 ", \"length_ticks\": %" PRIu64 ",\n",
                cl.clip_id, cl.track_id, clip_type_str(cl.clip_type), cl.start_tick, cl.length_ticks);
        fprintf(out, "      \"asset_path\": "); write_json_escaped(out, cl.asset_path); fprintf(out, ",\n");
        fprintf(out, "      \"notes\": [");
        for (uint32_t n = 0; n < cl.note_count; ++n) {
            if (n > 0) fprintf(out, ", ");
            fprintf(out, "{\"offset\": %u, \"dur\": %u, \"pitch\": %u, \"vel\": %u, \"ch\": %u}",
                    cl.notes[n].offset_tick, cl.notes[n].duration_tick,
                    cl.notes[n].pitch, cl.notes[n].velocity, cl.notes[n].channel);
        }
        fprintf(out, "]\n    }");
        oevc_sym_free_clip(&cl);
    }
    fprintf(out, "\n  ],\n");

    /* Routes */
    fprintf(out, "  \"routes\": [\n");
    first = 1;
    for (size_t i = 0; i < cont->chunk_count; ++i) {
        const oevc_chunk_t *c = &cont->chunks[i];
        if ((c->flags & OEV_CHUNK_DELETED) || c->kind != OEV_KIND_SYM_ROUTE) continue;
        oevc_sym_route_t rt;
        if (oevc_sym_decode_route(c, &rt) != OEVC_OK) continue;
        if (!first) fprintf(out, ",\n");
        first = 0;
        fprintf(out, "    {\"src_track_id\": %u, \"dst_track_id\": %u, \"send_db\": %.3f}",
                rt.src_track_id, rt.dst_track_id, (double)rt.send_db_milli / 1000.0);
    }
    fprintf(out, "\n  ],\n");

    /* Plugins */
    fprintf(out, "  \"plugins\": [\n");
    first = 1;
    for (size_t i = 0; i < cont->chunk_count; ++i) {
        const oevc_chunk_t *c = &cont->chunks[i];
        if ((c->flags & OEV_CHUNK_DELETED) || c->kind != OEV_KIND_SYM_PLUGIN) continue;
        oevc_sym_plugin_t pl;
        if (oevc_sym_decode_plugin(c, &pl) != OEVC_OK) continue;
        if (!first) fprintf(out, ",\n");
        first = 0;
        fprintf(out, "    {\"plugin_id\": %u, \"track_id\": %u, \"vendor_chunk_id\": %u, \"name\": ",
                pl.plugin_id, pl.track_id, pl.vendor_chunk_id);
        write_json_escaped(out, pl.name);
        fprintf(out, ", \"format\": ");
        write_json_escaped(out, pl.format);
        fprintf(out, ", \"params\": [");
        for (uint32_t p = 0; p < pl.param_count; ++p) {
            if (p > 0) fprintf(out, ", ");
            fprintf(out, "{\"id\": %u, \"name\": ", pl.params[p].param_id);
            write_json_escaped(out, pl.params[p].name);
            fprintf(out, ", \"value\": %.3f}", (double)pl.params[p].value_milli / 1000.0);
        }
        fprintf(out, "]}");
        oevc_sym_free_plugin(&pl);
    }
    fprintf(out, "\n  ],\n");

    /* Protected vendor_void & raw buffers summary */
    fprintf(out, "  \"opaque_chunks\": [\n");
    first = 1;
    for (size_t i = 0; i < cont->chunk_count; ++i) {
        const oevc_chunk_t *c = &cont->chunks[i];
        if (c->flags & OEV_CHUNK_DELETED) continue;
        if (c->kind != OEV_KIND_VENDOR_BLOB && c->kind != OEV_KIND_RAW_BUF && c->tier != OEV_TIER_VENDOR_VOID) {
            continue;
        }
        if (!first) fprintf(out, ",\n");
        first = 0;
        fprintf(out, "    {\"chunk_id\": %u, \"tier\": \"%s\", \"kind\": \"%s\", \"bytes\": %u, \"crc32\": \"0x%08X\"}",
                c->chunk_id, oevc_tier_name(c->tier), oevc_kind_name(c->kind), c->payload_len, c->crc32);
    }
    fprintf(out, "\n  ]\n");
    fprintf(out, "}\n");

    return ferror(out) ? OEVC_ERR_IO : OEVC_OK;
}

oevc_status_t oevc_export_json_file(const oevc_container_t *cont, const char *json_path) {
    if (!cont || !json_path) return OEVC_ERR_INVALID_ARG;
    FILE *fp = fopen(json_path, "wb");
    if (!fp) return OEVC_ERR_IO;
    oevc_status_t st = oevc_export_json_stream(cont, fp);
    if (fclose(fp) != 0 && st == OEVC_OK) st = OEVC_ERR_IO;
    return st;
}
