#include "oevc/sym_il.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    uint8_t *buf;
    size_t   len;
    size_t   cap;
} byte_writer_t;

static int bw_reserve(byte_writer_t *w, size_t extra) {
    if (w->len + extra <= w->cap) return 1;
    size_t new_cap = w->cap == 0 ? 128 : w->cap;
    while (new_cap < w->len + extra) new_cap *= 2;
    uint8_t *p = (uint8_t *)realloc(w->buf, new_cap);
    if (!p) return 0;
    w->buf = p;
    w->cap = new_cap;
    return 1;
}

static int bw_u8(byte_writer_t *w, uint8_t v) {
    if (!bw_reserve(w, 1)) return 0;
    w->buf[w->len++] = v;
    return 1;
}

static int bw_u32(byte_writer_t *w, uint32_t v) {
    if (!bw_reserve(w, 4)) return 0;
    w->buf[w->len++] = (uint8_t)(v & 0xFFu);
    w->buf[w->len++] = (uint8_t)((v >> 8) & 0xFFu);
    w->buf[w->len++] = (uint8_t)((v >> 16) & 0xFFu);
    w->buf[w->len++] = (uint8_t)((v >> 24) & 0xFFu);
    return 1;
}

static int bw_i32(byte_writer_t *w, int32_t v) {
    return bw_u32(w, (uint32_t)v);
}

static int bw_u64(byte_writer_t *w, uint64_t v) {
    if (!bw_reserve(w, 8)) return 0;
    for (int i = 0; i < 8; ++i) {
        w->buf[w->len++] = (uint8_t)((v >> (i * 8)) & 0xFFu);
    }
    return 1;
}

static int bw_str(byte_writer_t *w, const char *s) {
    size_t slen = s ? strlen(s) : 0;
    if (slen > 0xFFFFu) slen = 0xFFFFu;
    if (!bw_reserve(w, 2 + slen)) return 0;
    w->buf[w->len++] = (uint8_t)(slen & 0xFFu);
    w->buf[w->len++] = (uint8_t)((slen >> 8) & 0xFFu);
    if (slen > 0) {
        memcpy(w->buf + w->len, s, slen);
        w->len += slen;
    }
    return 1;
}

typedef struct {
    const uint8_t *buf;
    size_t         len;
    size_t         pos;
} byte_reader_t;

static int br_u8(byte_reader_t *r, uint8_t *out) {
    if (r->pos + 1 > r->len) return 0;
    *out = r->buf[r->pos++];
    return 1;
}

static int br_u32(byte_reader_t *r, uint32_t *out) {
    if (r->pos + 4 > r->len) return 0;
    const uint8_t *p = r->buf + r->pos;
    *out = (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
    r->pos += 4;
    return 1;
}

static int br_i32(byte_reader_t *r, int32_t *out) {
    uint32_t u = 0;
    if (!br_u32(r, &u)) return 0;
    *out = (int32_t)u;
    return 1;
}

static int br_u64(byte_reader_t *r, uint64_t *out) {
    if (r->pos + 8 > r->len) return 0;
    uint64_t v = 0;
    for (int i = 0; i < 8; ++i) {
        v |= ((uint64_t)r->buf[r->pos + i]) << (i * 8);
    }
    r->pos += 8;
    *out = v;
    return 1;
}

static int br_str(byte_reader_t *r, char *dst, size_t dst_cap) {
    if (r->pos + 2 > r->len) return 0;
    uint16_t slen = (uint16_t)r->buf[r->pos] | ((uint16_t)r->buf[r->pos + 1] << 8);
    r->pos += 2;
    if (r->pos + slen > r->len) return 0;
    size_t copy_len = slen < dst_cap - 1 ? slen : dst_cap - 1;
    if (copy_len > 0) memcpy(dst, r->buf + r->pos, copy_len);
    dst[copy_len] = '\0';
    r->pos += slen;
    return 1;
}

oevc_status_t oevc_sym_add_meta(oevc_container_t *cont, const oevc_sym_meta_t *meta, uint32_t *out_id) {
    if (!cont || !meta) return OEVC_ERR_INVALID_ARG;
    byte_writer_t w = {0};
    int ok = bw_u32(&w, meta->bpm_milli) &&
             bw_u32(&w, meta->ppq) &&
             bw_str(&w, meta->project_name) &&
             bw_str(&w, meta->author) &&
             bw_str(&w, meta->software);
    oevc_status_t st = ok ? oevc_add_chunk(cont, OEV_TIER_OPEN, OEV_KIND_META, 0, w.buf, (uint32_t)w.len, out_id)
                          : OEVC_ERR_NOMEM;
    free(w.buf);
    return st;
}

oevc_status_t oevc_sym_add_track(oevc_container_t *cont, const oevc_sym_track_t *track, uint32_t *out_id) {
    if (!cont || !track) return OEVC_ERR_INVALID_ARG;
    byte_writer_t w = {0};
    int ok = bw_u32(&w, track->track_id) &&
             bw_u8(&w, track->track_type) &&
             bw_i32(&w, track->gain_db_milli) &&
             bw_i32(&w, track->pan_milli) &&
             bw_str(&w, track->name);
    oevc_status_t st = ok ? oevc_add_chunk(cont, OEV_TIER_OPEN, OEV_KIND_SYM_TRACK, 0, w.buf, (uint32_t)w.len, out_id)
                          : OEVC_ERR_NOMEM;
    free(w.buf);
    return st;
}

oevc_status_t oevc_sym_add_clip(oevc_container_t *cont, const oevc_sym_clip_t *clip, uint32_t *out_id) {
    if (!cont || !clip) return OEVC_ERR_INVALID_ARG;
    if (clip->note_count > 0 && !clip->notes) return OEVC_ERR_INVALID_ARG;
    byte_writer_t w = {0};
    int ok = bw_u32(&w, clip->clip_id) &&
             bw_u32(&w, clip->track_id) &&
             bw_u8(&w, clip->clip_type) &&
             bw_u64(&w, clip->start_tick) &&
             bw_u64(&w, clip->length_ticks) &&
             bw_str(&w, clip->asset_path) &&
             bw_u32(&w, clip->note_count);
    for (uint32_t i = 0; ok && i < clip->note_count; ++i) {
        const oevc_sym_note_t *n = &clip->notes[i];
        ok = bw_u32(&w, n->offset_tick) &&
             bw_u32(&w, n->duration_tick) &&
             bw_u8(&w, n->pitch) &&
             bw_u8(&w, n->velocity) &&
             bw_u8(&w, n->channel);
    }
    oevc_status_t st = ok ? oevc_add_chunk(cont, OEV_TIER_OPEN, OEV_KIND_SYM_CLIP, 0, w.buf, (uint32_t)w.len, out_id)
                          : OEVC_ERR_NOMEM;
    free(w.buf);
    return st;
}

oevc_status_t oevc_sym_add_route(oevc_container_t *cont, const oevc_sym_route_t *route, uint32_t *out_id) {
    if (!cont || !route) return OEVC_ERR_INVALID_ARG;
    byte_writer_t w = {0};
    int ok = bw_u32(&w, route->src_track_id) &&
             bw_u32(&w, route->dst_track_id) &&
             bw_i32(&w, route->send_db_milli);
    oevc_status_t st = ok ? oevc_add_chunk(cont, OEV_TIER_OPEN, OEV_KIND_SYM_ROUTE, 0, w.buf, (uint32_t)w.len, out_id)
                          : OEVC_ERR_NOMEM;
    free(w.buf);
    return st;
}

oevc_status_t oevc_sym_add_plugin(oevc_container_t *cont, const oevc_sym_plugin_t *plugin, uint32_t *out_id) {
    if (!cont || !plugin) return OEVC_ERR_INVALID_ARG;
    if (plugin->param_count > 0 && !plugin->params) return OEVC_ERR_INVALID_ARG;
    byte_writer_t w = {0};
    int ok = bw_u32(&w, plugin->plugin_id) &&
             bw_u32(&w, plugin->track_id) &&
             bw_u32(&w, plugin->vendor_chunk_id) &&
             bw_str(&w, plugin->name) &&
             bw_str(&w, plugin->format) &&
             bw_u32(&w, plugin->param_count);
    for (uint32_t i = 0; ok && i < plugin->param_count; ++i) {
        const oevc_sym_param_t *p = &plugin->params[i];
        ok = bw_u32(&w, p->param_id) &&
             bw_i32(&w, p->value_milli) &&
             bw_str(&w, p->name);
    }
    oevc_status_t st = ok ? oevc_add_chunk(cont, OEV_TIER_OPEN, OEV_KIND_SYM_PLUGIN, 0, w.buf, (uint32_t)w.len, out_id)
                          : OEVC_ERR_NOMEM;
    free(w.buf);
    return st;
}

oevc_status_t oevc_sym_decode_meta(const oevc_chunk_t *chunk, oevc_sym_meta_t *out) {
    if (!chunk || !out || chunk->kind != OEV_KIND_META) return OEVC_ERR_INVALID_ARG;
    memset(out, 0, sizeof(*out));
    byte_reader_t r = { chunk->payload, chunk->payload_len, 0 };
    if (!br_u32(&r, &out->bpm_milli) ||
        !br_u32(&r, &out->ppq) ||
        !br_str(&r, out->project_name, sizeof(out->project_name)) ||
        !br_str(&r, out->author, sizeof(out->author)) ||
        !br_str(&r, out->software, sizeof(out->software))) {
        return OEVC_ERR_CORRUPT;
    }
    return OEVC_OK;
}

oevc_status_t oevc_sym_decode_track(const oevc_chunk_t *chunk, oevc_sym_track_t *out) {
    if (!chunk || !out || chunk->kind != OEV_KIND_SYM_TRACK) return OEVC_ERR_INVALID_ARG;
    memset(out, 0, sizeof(*out));
    byte_reader_t r = { chunk->payload, chunk->payload_len, 0 };
    if (!br_u32(&r, &out->track_id) ||
        !br_u8(&r, &out->track_type) ||
        !br_i32(&r, &out->gain_db_milli) ||
        !br_i32(&r, &out->pan_milli) ||
        !br_str(&r, out->name, sizeof(out->name))) {
        return OEVC_ERR_CORRUPT;
    }
    return OEVC_OK;
}

oevc_status_t oevc_sym_decode_clip(const oevc_chunk_t *chunk, oevc_sym_clip_t *out) {
    if (!chunk || !out || chunk->kind != OEV_KIND_SYM_CLIP) return OEVC_ERR_INVALID_ARG;
    memset(out, 0, sizeof(*out));
    byte_reader_t r = { chunk->payload, chunk->payload_len, 0 };
    if (!br_u32(&r, &out->clip_id) ||
        !br_u32(&r, &out->track_id) ||
        !br_u8(&r, &out->clip_type) ||
        !br_u64(&r, &out->start_tick) ||
        !br_u64(&r, &out->length_ticks) ||
        !br_str(&r, out->asset_path, sizeof(out->asset_path)) ||
        !br_u32(&r, &out->note_count)) {
        return OEVC_ERR_CORRUPT;
    }
    if (out->note_count > 0) {
        out->notes = (oevc_sym_note_t *)calloc(out->note_count, sizeof(oevc_sym_note_t));
        if (!out->notes) return OEVC_ERR_NOMEM;
        for (uint32_t i = 0; i < out->note_count; ++i) {
            oevc_sym_note_t *n = &out->notes[i];
            if (!br_u32(&r, &n->offset_tick) ||
                !br_u32(&r, &n->duration_tick) ||
                !br_u8(&r, &n->pitch) ||
                !br_u8(&r, &n->velocity) ||
                !br_u8(&r, &n->channel)) {
                oevc_sym_free_clip(out);
                return OEVC_ERR_CORRUPT;
            }
        }
    }
    return OEVC_OK;
}

void oevc_sym_free_clip(oevc_sym_clip_t *clip) {
    if (!clip) return;
    free(clip->notes);
    clip->notes = NULL;
    clip->note_count = 0;
}

oevc_status_t oevc_sym_decode_route(const oevc_chunk_t *chunk, oevc_sym_route_t *out) {
    if (!chunk || !out || chunk->kind != OEV_KIND_SYM_ROUTE) return OEVC_ERR_INVALID_ARG;
    memset(out, 0, sizeof(*out));
    byte_reader_t r = { chunk->payload, chunk->payload_len, 0 };
    if (!br_u32(&r, &out->src_track_id) ||
        !br_u32(&r, &out->dst_track_id) ||
        !br_i32(&r, &out->send_db_milli)) {
        return OEVC_ERR_CORRUPT;
    }
    return OEVC_OK;
}

oevc_status_t oevc_sym_decode_plugin(const oevc_chunk_t *chunk, oevc_sym_plugin_t *out) {
    if (!chunk || !out || chunk->kind != OEV_KIND_SYM_PLUGIN) return OEVC_ERR_INVALID_ARG;
    memset(out, 0, sizeof(*out));
    byte_reader_t r = { chunk->payload, chunk->payload_len, 0 };
    if (!br_u32(&r, &out->plugin_id) ||
        !br_u32(&r, &out->track_id) ||
        !br_u32(&r, &out->vendor_chunk_id) ||
        !br_str(&r, out->name, sizeof(out->name)) ||
        !br_str(&r, out->format, sizeof(out->format)) ||
        !br_u32(&r, &out->param_count)) {
        return OEVC_ERR_CORRUPT;
    }
    if (out->param_count > 0) {
        out->params = (oevc_sym_param_t *)calloc(out->param_count, sizeof(oevc_sym_param_t));
        if (!out->params) return OEVC_ERR_NOMEM;
        for (uint32_t i = 0; i < out->param_count; ++i) {
            oevc_sym_param_t *p = &out->params[i];
            if (!br_u32(&r, &p->param_id) ||
                !br_i32(&r, &p->value_milli) ||
                !br_str(&r, p->name, sizeof(p->name))) {
                oevc_sym_free_plugin(out);
                return OEVC_ERR_CORRUPT;
            }
        }
    }
    return OEVC_OK;
}

void oevc_sym_free_plugin(oevc_sym_plugin_t *plugin) {
    if (!plugin) return;
    free(plugin->params);
    plugin->params = NULL;
    plugin->param_count = 0;
}
