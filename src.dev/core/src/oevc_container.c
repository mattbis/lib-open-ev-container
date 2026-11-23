#include "oevc/oevc.h"

#include <stdlib.h>
#include <string.h>

static void put_u16_le(uint8_t *dst, uint16_t v) {
    dst[0] = (uint8_t)(v & 0xFFu);
    dst[1] = (uint8_t)((v >> 8) & 0xFFu);
}

static void put_u32_le(uint8_t *dst, uint32_t v) {
    dst[0] = (uint8_t)(v & 0xFFu);
    dst[1] = (uint8_t)((v >> 8) & 0xFFu);
    dst[2] = (uint8_t)((v >> 16) & 0xFFu);
    dst[3] = (uint8_t)((v >> 24) & 0xFFu);
}

static uint16_t get_u16_le(const uint8_t *src) {
    return (uint16_t)src[0] | ((uint16_t)src[1] << 8);
}

static uint32_t get_u32_le(const uint8_t *src) {
    return (uint32_t)src[0] |
           ((uint32_t)src[1] << 8) |
           ((uint32_t)src[2] << 16) |
           ((uint32_t)src[3] << 24);
}

static uint32_t compute_chunk_crc(const uint8_t hdr[OEVC_CHUNK_HEADER_SIZE], const uint8_t *payload, uint32_t len) {
    /* CRC over header bytes [4..15] (tier, kind, flags, chunk_id, payload_len) + payload */
    uint32_t crc = oevc_crc32(0u, hdr + 4, 12);
    if (len > 0 && payload != NULL) {
        crc = oevc_crc32(crc, payload, len);
    }
    return crc;
}

static void encode_file_header(uint8_t out[OEVC_FILE_HEADER_SIZE], const oevc_header_t *h) {
    memcpy(out, OEVC_MAGIC_BYTES, OEVC_MAGIC_LEN);
    put_u16_le(out + 8, h->version);
    put_u16_le(out + 10, h->flags);
    put_u32_le(out + 12, h->qrate);
}

static oevc_status_t decode_file_header(const uint8_t in[OEVC_FILE_HEADER_SIZE], oevc_header_t *h) {
    if (memcmp(in, OEVC_MAGIC_BYTES, OEVC_MAGIC_LEN) != 0) {
        return OEVC_ERR_BAD_MAGIC;
    }
    h->version = get_u16_le(in + 8);
    if (h->version != OEVC_FORMAT_VERSION) {
        return OEVC_ERR_UNSUPPORTED_VER;
    }
    h->flags = get_u16_le(in + 10);
    h->qrate = get_u32_le(in + 12);
    return OEVC_OK;
}

static void encode_chunk_header(uint8_t hdr[OEVC_CHUNK_HEADER_SIZE],
                                uint8_t tier, uint8_t kind, uint16_t flags,
                                uint32_t chunk_id, uint32_t payload_len,
                                const uint8_t *payload, uint32_t *out_crc) {
    memcpy(hdr, OEVC_CHUNK_MAGIC, OEVC_CHUNK_MAGIC_LEN);
    hdr[4] = tier;
    hdr[5] = kind;
    put_u16_le(hdr + 6, flags);
    put_u32_le(hdr + 8, chunk_id);
    put_u32_le(hdr + 12, payload_len);
    uint32_t crc = compute_chunk_crc(hdr, payload, payload_len);
    put_u32_le(hdr + 16, crc);
    put_u32_le(hdr + 20, 0u);
    if (out_crc) {
        *out_crc = crc;
    }
}

oevc_status_t oevc_init(oevc_container_t *cont, uint16_t flags, uint32_t qrate) {
    if (!cont) return OEVC_ERR_INVALID_ARG;
    memset(cont, 0, sizeof(*cont));
    cont->header.version = OEVC_FORMAT_VERSION;
    cont->header.flags = flags;
    cont->header.qrate = qrate;
    cont->next_chunk_id = 1;
    return OEVC_OK;
}

void oevc_free(oevc_container_t *cont) {
    if (!cont) return;
    for (size_t i = 0; i < cont->chunk_count; ++i) {
        free(cont->chunks[i].payload);
    }
    free(cont->chunks);
    memset(cont, 0, sizeof(*cont));
}

static oevc_status_t append_chunk_struct(oevc_container_t *cont,
                                         uint8_t tier, uint8_t kind, uint16_t flags,
                                         uint32_t chunk_id, const void *payload,
                                         uint32_t payload_len, uint32_t crc,
                                         uint64_t file_offset) {
    if (cont->chunk_count == cont->chunk_cap) {
        size_t new_cap = cont->chunk_cap == 0 ? 16 : cont->chunk_cap * 2;
        oevc_chunk_t *grown = (oevc_chunk_t *)realloc(cont->chunks, new_cap * sizeof(oevc_chunk_t));
        if (!grown) return OEVC_ERR_NOMEM;
        cont->chunks = grown;
        cont->chunk_cap = new_cap;
    }

    uint8_t *copy = NULL;
    if (payload_len > 0) {
        copy = (uint8_t *)malloc(payload_len);
        if (!copy) return OEVC_ERR_NOMEM;
        memcpy(copy, payload, payload_len);
    }

    oevc_chunk_t *c = &cont->chunks[cont->chunk_count++];
    c->tier = tier;
    c->kind = kind;
    c->flags = flags;
    c->chunk_id = chunk_id;
    c->payload_len = payload_len;
    c->crc32 = crc;
    c->file_offset = file_offset;
    c->payload = copy;

    if (chunk_id >= cont->next_chunk_id) {
        cont->next_chunk_id = chunk_id + 1;
    }
    return OEVC_OK;
}

oevc_status_t oevc_add_chunk(oevc_container_t *cont,
                             uint8_t tier,
                             uint8_t kind,
                             uint16_t flags,
                             const void *payload,
                             uint32_t payload_len,
                             uint32_t *out_chunk_id) {
    if (!cont) return OEVC_ERR_INVALID_ARG;
    if (payload_len > 0 && !payload) return OEVC_ERR_INVALID_ARG;

    uint32_t id = cont->next_chunk_id++;
    uint8_t hdr[OEVC_CHUNK_HEADER_SIZE];
    uint32_t crc = 0;
    encode_chunk_header(hdr, tier, kind, flags, id, payload_len, (const uint8_t *)payload, &crc);

    oevc_status_t st = append_chunk_struct(cont, tier, kind, flags, id, payload, payload_len, crc, 0);
    if (st == OEVC_OK && out_chunk_id) {
        *out_chunk_id = id;
    }
    return st;
}

oevc_status_t oevc_mark_deleted(oevc_container_t *cont, uint32_t chunk_id) {
    if (!cont) return OEVC_ERR_INVALID_ARG;
    for (size_t i = 0; i < cont->chunk_count; ++i) {
        if (cont->chunks[i].chunk_id == chunk_id) {
            cont->chunks[i].flags |= OEV_CHUNK_DELETED;
            return OEVC_OK;
        }
    }
    return OEVC_ERR_NOT_FOUND;
}

oevc_status_t oevc_save_file(const oevc_container_t *cont, const char *path) {
    if (!cont || !path) return OEVC_ERR_INVALID_ARG;
    FILE *fp = fopen(path, "wb");
    if (!fp) return OEVC_ERR_IO;

    uint8_t fhdr[OEVC_FILE_HEADER_SIZE];
    encode_file_header(fhdr, &cont->header);
    if (fwrite(fhdr, 1, sizeof(fhdr), fp) != sizeof(fhdr)) {
        fclose(fp);
        return OEVC_ERR_IO;
    }

    for (size_t i = 0; i < cont->chunk_count; ++i) {
        const oevc_chunk_t *c = &cont->chunks[i];
        uint8_t chdr[OEVC_CHUNK_HEADER_SIZE];
        encode_chunk_header(chdr, c->tier, c->kind, c->flags, c->chunk_id,
                            c->payload_len, c->payload, NULL);
        if (fwrite(chdr, 1, sizeof(chdr), fp) != sizeof(chdr)) {
            fclose(fp);
            return OEVC_ERR_IO;
        }
        if (c->payload_len > 0 && fwrite(c->payload, 1, c->payload_len, fp) != c->payload_len) {
            fclose(fp);
            return OEVC_ERR_IO;
        }
    }

    if (fclose(fp) != 0) return OEVC_ERR_IO;
    return OEVC_OK;
}

oevc_status_t oevc_append_chunk_file(const char *path,
                                     uint8_t tier,
                                     uint8_t kind,
                                     uint16_t flags,
                                     uint32_t chunk_id,
                                     const void *payload,
                                     uint32_t payload_len) {
    if (!path) return OEVC_ERR_INVALID_ARG;
    if (payload_len > 0 && !payload) return OEVC_ERR_INVALID_ARG;

    FILE *fp = fopen(path, "r+b");
    if (!fp) return OEVC_ERR_IO;

    uint8_t fhdr[OEVC_FILE_HEADER_SIZE];
    oevc_header_t h;
    if (fread(fhdr, 1, sizeof(fhdr), fp) != sizeof(fhdr)) {
        fclose(fp);
        return OEVC_ERR_IO;
    }
    oevc_status_t st = decode_file_header(fhdr, &h);
    if (st != OEVC_OK) {
        fclose(fp);
        return st;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return OEVC_ERR_IO;
    }

    uint8_t chdr[OEVC_CHUNK_HEADER_SIZE];
    encode_chunk_header(chdr, tier, kind, flags, chunk_id, payload_len, (const uint8_t *)payload, NULL);
    if (fwrite(chdr, 1, sizeof(chdr), fp) != sizeof(chdr)) {
        fclose(fp);
        return OEVC_ERR_IO;
    }
    if (payload_len > 0 && fwrite(payload, 1, payload_len, fp) != payload_len) {
        fclose(fp);
        return OEVC_ERR_IO;
    }

    if (fclose(fp) != 0) return OEVC_ERR_IO;
    return OEVC_OK;
}

static oevc_status_t read_entire_file(const char *path, uint8_t **out_buf, size_t *out_len) {
    FILE *fp = fopen(path, "rb");
    if (!fp) return OEVC_ERR_IO;
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return OEVC_ERR_IO;
    }
    long sz = ftell(fp);
    if (sz < 0) {
        fclose(fp);
        return OEVC_ERR_IO;
    }
    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return OEVC_ERR_IO;
    }
    uint8_t *buf = (uint8_t *)malloc((size_t)sz + 1);
    if (!buf) {
        fclose(fp);
        return OEVC_ERR_NOMEM;
    }
    size_t nread = sz > 0 ? fread(buf, 1, (size_t)sz, fp) : 0;
    fclose(fp);
    if (nread != (size_t)sz) {
        free(buf);
        return OEVC_ERR_IO;
    }
    *out_buf = buf;
    *out_len = nread;
    return OEVC_OK;
}

oevc_status_t oevc_load_file(oevc_container_t *cont, const char *path) {
    if (!cont || !path) return OEVC_ERR_INVALID_ARG;
    uint8_t *buf = NULL;
    size_t len = 0;
    oevc_status_t st = read_entire_file(path, &buf, &len);
    if (st != OEVC_OK) return st;

    if (len < OEVC_FILE_HEADER_SIZE) {
        free(buf);
        return OEVC_ERR_CORRUPT;
    }

    oevc_header_t h;
    st = decode_file_header(buf, &h);
    if (st != OEVC_OK) {
        free(buf);
        return st;
    }

    st = oevc_init(cont, h.flags, h.qrate);
    if (st != OEVC_OK) {
        free(buf);
        return st;
    }

    size_t pos = OEVC_FILE_HEADER_SIZE;
    while (pos < len) {
        if (len - pos < OEVC_CHUNK_HEADER_SIZE) {
            oevc_free(cont);
            free(buf);
            return OEVC_ERR_CORRUPT;
        }
        const uint8_t *chdr = buf + pos;
        if (memcmp(chdr, OEVC_CHUNK_MAGIC, OEVC_CHUNK_MAGIC_LEN) != 0) {
            oevc_free(cont);
            free(buf);
            return OEVC_ERR_CORRUPT;
        }
        uint8_t tier = chdr[4];
        uint8_t kind = chdr[5];
        uint16_t flags = get_u16_le(chdr + 6);
        uint32_t chunk_id = get_u32_le(chdr + 8);
        uint32_t plen = get_u32_le(chdr + 12);
        uint32_t expected_crc = get_u32_le(chdr + 16);

        if (plen > len - pos - OEVC_CHUNK_HEADER_SIZE) {
            oevc_free(cont);
            free(buf);
            return OEVC_ERR_CORRUPT;
        }

        const uint8_t *payload = buf + pos + OEVC_CHUNK_HEADER_SIZE;
        uint32_t actual_crc = compute_chunk_crc(chdr, payload, plen);
        if (actual_crc != expected_crc) {
            oevc_free(cont);
            free(buf);
            return OEVC_ERR_CORRUPT;
        }

        st = append_chunk_struct(cont, tier, kind, flags, chunk_id, payload, plen, actual_crc, (uint64_t)pos);
        if (st != OEVC_OK) {
            oevc_free(cont);
            free(buf);
            return st;
        }
        pos += OEVC_CHUNK_HEADER_SIZE + plen;
    }

    free(buf);
    return OEVC_OK;
}

static oevc_status_t scan_and_salvage(const uint8_t *buf, size_t len,
                                      oevc_container_t *out_salvaged,
                                      oevc_rtest_report_t *report) {
    memset(report, 0, sizeof(*report));
    oevc_header_t h = { OEVC_FORMAT_VERSION, OEV_CONT_FAST_SAVE, 1u };

    if (len >= OEVC_FILE_HEADER_SIZE && decode_file_header(buf, &h) == OEVC_OK) {
        report->header_valid = 1;
    } else {
        report->header_valid = 0;
        report->needs_repair = 1;
    }

    if (out_salvaged) {
        oevc_status_t st = oevc_init(out_salvaged, h.flags, h.qrate);
        if (st != OEVC_OK) return st;
    }

    size_t pos = report->header_valid ? OEVC_FILE_HEADER_SIZE : 0;
    while (pos < len) {
        if (len - pos < OEVC_CHUNK_HEADER_SIZE) {
            report->corrupt_chunks++;
            report->needs_repair = 1;
            break;
        }

        if (memcmp(buf + pos, OEVC_CHUNK_MAGIC, OEVC_CHUNK_MAGIC_LEN) != 0) {
            /* Resync forward to next CHMK marker */
            report->needs_repair = 1;
            pos++;
            continue;
        }

        report->total_chunks_scanned++;
        const uint8_t *chdr = buf + pos;
        uint8_t tier = chdr[4];
        uint8_t kind = chdr[5];
        uint16_t flags = get_u16_le(chdr + 6);
        uint32_t chunk_id = get_u32_le(chdr + 8);
        uint32_t plen = get_u32_le(chdr + 12);
        uint32_t expected_crc = get_u32_le(chdr + 16);

        if (plen > len - pos - OEVC_CHUNK_HEADER_SIZE) {
            report->corrupt_chunks++;
            report->needs_repair = 1;
            pos += OEVC_CHUNK_MAGIC_LEN;
            continue;
        }

        const uint8_t *payload = buf + pos + OEVC_CHUNK_HEADER_SIZE;
        uint32_t actual_crc = compute_chunk_crc(chdr, payload, plen);
        if (actual_crc != expected_crc) {
            report->corrupt_chunks++;
            report->needs_repair = 1;
            pos += OEVC_CHUNK_MAGIC_LEN;
            continue;
        }

        report->valid_chunks++;
        if (out_salvaged) {
            oevc_status_t st = append_chunk_struct(out_salvaged, tier, kind, flags, chunk_id,
                                                   payload, plen, actual_crc, (uint64_t)pos);
            if (st != OEVC_OK) return st;
        }
        pos += OEVC_CHUNK_HEADER_SIZE + plen;
    }

    return OEVC_OK;
}

oevc_status_t oevc_rtest_file(const char *path, oevc_rtest_report_t *report) {
    if (!path || !report) return OEVC_ERR_INVALID_ARG;
    uint8_t *buf = NULL;
    size_t len = 0;
    oevc_status_t st = read_entire_file(path, &buf, &len);
    if (st != OEVC_OK) return st;

    st = scan_and_salvage(buf, len, NULL, report);
    free(buf);
    return st;
}

oevc_status_t oevc_repair_file(const char *src_path, const char *dst_path, oevc_rtest_report_t *report) {
    if (!src_path || !dst_path || !report) return OEVC_ERR_INVALID_ARG;
    uint8_t *buf = NULL;
    size_t len = 0;
    oevc_status_t st = read_entire_file(src_path, &buf, &len);
    if (st != OEVC_OK) return st;

    oevc_container_t salvaged;
    st = scan_and_salvage(buf, len, &salvaged, report);
    free(buf);
    if (st != OEVC_OK) {
        oevc_free(&salvaged);
        return st;
    }

    st = oevc_save_file(&salvaged, dst_path);
    oevc_free(&salvaged);
    return st;
}

oevc_status_t oevc_consolidate_file(const char *src_path, const char *dst_path) {
    if (!src_path || !dst_path) return OEVC_ERR_INVALID_ARG;
    oevc_container_t src;
    oevc_status_t st = oevc_load_file(&src, src_path);
    if (st != OEVC_OK) return st;

    oevc_container_t out;
    oevc_init(&out, src.header.flags, src.header.qrate);
    for (size_t i = 0; i < src.chunk_count; ++i) {
        const oevc_chunk_t *c = &src.chunks[i];
        if (c->flags & OEV_CHUNK_DELETED) continue;
        st = append_chunk_struct(&out, c->tier, c->kind, c->flags, c->chunk_id,
                                 c->payload, c->payload_len, c->crc32, 0);
        if (st != OEVC_OK) {
            oevc_free(&out);
            oevc_free(&src);
            return st;
        }
    }

    st = oevc_save_file(&out, dst_path);
    oevc_free(&out);
    oevc_free(&src);
    return st;
}

size_t oevc_count_chunks(const oevc_container_t *cont, int tier_filter, int kind_filter, int include_deleted) {
    if (!cont) return 0;
    size_t count = 0;
    for (size_t i = 0; i < cont->chunk_count; ++i) {
        const oevc_chunk_t *c = &cont->chunks[i];
        if (!include_deleted && (c->flags & OEV_CHUNK_DELETED)) continue;
        if (tier_filter >= 0 && (int)c->tier != tier_filter) continue;
        if (kind_filter >= 0 && (int)c->kind != kind_filter) continue;
        count++;
    }
    return count;
}

const oevc_chunk_t *oevc_find_chunk(const oevc_container_t *cont, uint32_t chunk_id) {
    if (!cont) return NULL;
    for (size_t i = 0; i < cont->chunk_count; ++i) {
        if (cont->chunks[i].chunk_id == chunk_id) return &cont->chunks[i];
    }
    return NULL;
}

const char *oevc_tier_name(uint8_t tier) {
    switch (tier) {
        case OEV_TIER_OPEN:        return "open";
        case OEV_TIER_VENDOR_VOID: return "vendor_void";
        case OEV_TIER_ARCHIVE:     return "archive";
        default:                   return "unknown";
    }
}

const char *oevc_kind_name(uint8_t kind) {
    switch (kind) {
        case OEV_KIND_META:        return "meta";
        case OEV_KIND_SYM_TRACK:   return "sym_track";
        case OEV_KIND_SYM_CLIP:    return "sym_clip";
        case OEV_KIND_SYM_ROUTE:   return "sym_route";
        case OEV_KIND_SYM_PLUGIN:  return "sym_plugin";
        case OEV_KIND_RAW_BUF:     return "raw_buf";
        case OEV_KIND_VENDOR_BLOB: return "vendor_blob";
        default:                   return "unknown";
    }
}

const char *oevc_status_str(oevc_status_t st) {
    switch (st) {
        case OEVC_OK:                 return "ok";
        case OEVC_ERR_INVALID_ARG:    return "invalid_arg";
        case OEVC_ERR_IO:             return "io_error";
        case OEVC_ERR_NOMEM:          return "out_of_memory";
        case OEVC_ERR_BAD_MAGIC:      return "bad_magic";
        case OEVC_ERR_UNSUPPORTED_VER:return "unsupported_version";
        case OEVC_ERR_CORRUPT:        return "corrupt_container";
        case OEVC_ERR_NOT_FOUND:      return "not_found";
        default:                      return "unknown_error";
    }
}
