#ifndef OEVC_H
#define OEVC_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OEVC_MAGIC_BYTES "OEVCONT\0"
#define OEVC_MAGIC_LEN 8
#define OEVC_CHUNK_MAGIC "CHMK"
#define OEVC_CHUNK_MAGIC_LEN 4
#define OEVC_FORMAT_VERSION 1

#define OEVC_FILE_HEADER_SIZE 16
#define OEVC_CHUNK_HEADER_SIZE 24

/* Container flags */
typedef enum {
    OEV_CONT_FAST_SAVE = 1 << 0,
    OEV_CONT_COMPLETE  = 1 << 1
} oevc_cont_flags_t;

/* Container tiers */
typedef enum {
    OEV_TIER_OPEN        = 0,
    OEV_TIER_VENDOR_VOID = 1,
    OEV_TIER_ARCHIVE     = 2
} oevc_tier_t;

/* Chunk kinds */
typedef enum {
    OEV_KIND_META        = 1,
    OEV_KIND_SYM_TRACK   = 2,
    OEV_KIND_SYM_CLIP    = 3,
    OEV_KIND_SYM_ROUTE   = 4,
    OEV_KIND_SYM_PLUGIN  = 5,
    OEV_KIND_RAW_BUF     = 6,
    OEV_KIND_VENDOR_BLOB = 7
} oevc_kind_t;

/* Chunk flags */
typedef enum {
    OEV_CHUNK_DELETED = 1 << 0
} oevc_chunk_flags_t;

/* Status / error codes */
typedef enum {
    OEVC_OK = 0,
    OEVC_ERR_INVALID_ARG = -1,
    OEVC_ERR_IO = -2,
    OEVC_ERR_NOMEM = -3,
    OEVC_ERR_BAD_MAGIC = -4,
    OEVC_ERR_UNSUPPORTED_VER = -5,
    OEVC_ERR_CORRUPT = -6,
    OEVC_ERR_NOT_FOUND = -7
} oevc_status_t;

typedef struct {
    uint16_t version;
    uint16_t flags;
    uint32_t qrate; /* 0 = complete/reproducible, >0 = partial/fast */
} oevc_header_t;

typedef struct {
    uint8_t  tier;
    uint8_t  kind;
    uint16_t flags;
    uint32_t chunk_id;
    uint32_t payload_len;
    uint32_t crc32;
    uint64_t file_offset; /* offset of chunk header in file */
    uint8_t *payload;     /* owned by chunk when loaded */
} oevc_chunk_t;

typedef struct {
    oevc_header_t header;
    oevc_chunk_t *chunks;
    size_t        chunk_count;
    size_t        chunk_cap;
    uint32_t      next_chunk_id;
} oevc_container_t;

typedef struct {
    size_t total_chunks_scanned;
    size_t valid_chunks;
    size_t corrupt_chunks;
    int    header_valid;
    int    needs_repair;
} oevc_rtest_report_t;

/* CRC-32 (IEEE 802.3) */
uint32_t oevc_crc32(uint32_t initial, const void *data, size_t len);

/* Container lifecycle */
oevc_status_t oevc_init(oevc_container_t *cont, uint16_t flags, uint32_t qrate);
void          oevc_free(oevc_container_t *cont);

/* Add chunk in memory (copies payload) */
oevc_status_t oevc_add_chunk(oevc_container_t *cont,
                             uint8_t tier,
                             uint8_t kind,
                             uint16_t flags,
                             const void *payload,
                             uint32_t payload_len,
                             uint32_t *out_chunk_id);

/* Mark a chunk deleted by chunk_id */
oevc_status_t oevc_mark_deleted(oevc_container_t *cont, uint32_t chunk_id);

/* File I/O */
oevc_status_t oevc_save_file(const oevc_container_t *cont, const char *path);
oevc_status_t oevc_load_file(oevc_container_t *cont, const char *path);

/* Append single chunk directly to an existing container file (fast-save) */
oevc_status_t oevc_append_chunk_file(const char *path,
                                     uint8_t tier,
                                     uint8_t kind,
                                     uint16_t flags,
                                     uint32_t chunk_id,
                                     const void *payload,
                                     uint32_t payload_len);

/* Query helpers */
size_t oevc_count_chunks(const oevc_container_t *cont, int tier_filter, int kind_filter, int include_deleted);
const oevc_chunk_t *oevc_find_chunk(const oevc_container_t *cont, uint32_t chunk_id);

/* Integrity test, repair, and consolidation */
oevc_status_t oevc_rtest_file(const char *path, oevc_rtest_report_t *report);
oevc_status_t oevc_repair_file(const char *src_path, const char *dst_path, oevc_rtest_report_t *report);
oevc_status_t oevc_consolidate_file(const char *src_path, const char *dst_path);

/* Human-readable names */
const char *oevc_tier_name(uint8_t tier);
const char *oevc_kind_name(uint8_t kind);
const char *oevc_status_str(oevc_status_t st);

#ifdef __cplusplus
}
#endif

#endif /* OEVC_H */
