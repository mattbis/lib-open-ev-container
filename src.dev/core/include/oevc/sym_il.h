#ifndef OEVC_SYM_IL_H
#define OEVC_SYM_IL_H

#include "oevc/oevc.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OEVC_STR_MAX 128

typedef enum {
    OEV_TRACK_MIDI  = 0,
    OEV_TRACK_AUDIO = 1,
    OEV_TRACK_BUS   = 2
} oevc_track_type_t;

typedef enum {
    OEV_CLIP_MIDI  = 0,
    OEV_CLIP_AUDIO = 1
} oevc_clip_type_t;

typedef struct {
    uint32_t bpm_milli; /* e.g., 120000 = 120.000 BPM */
    uint32_t ppq;       /* ticks per quarter note, e.g. 960 */
    char     project_name[OEVC_STR_MAX];
    char     author[OEVC_STR_MAX];
    char     software[OEVC_STR_MAX];
} oevc_sym_meta_t;

typedef struct {
    uint32_t track_id;
    uint8_t  track_type; /* oevc_track_type_t */
    int32_t  gain_db_milli;
    int32_t  pan_milli; /* -1000..1000 */
    char     name[OEVC_STR_MAX];
} oevc_sym_track_t;

typedef struct {
    uint32_t offset_tick;
    uint32_t duration_tick;
    uint8_t  pitch;
    uint8_t  velocity;
    uint8_t  channel;
} oevc_sym_note_t;

typedef struct {
    uint32_t         clip_id;
    uint32_t         track_id;
    uint8_t          clip_type; /* oevc_clip_type_t */
    uint64_t         start_tick;
    uint64_t         length_ticks;
    char             asset_path[OEVC_STR_MAX];
    oevc_sym_note_t *notes;
    uint32_t         note_count;
} oevc_sym_clip_t;

typedef struct {
    uint32_t src_track_id;
    uint32_t dst_track_id;
    int32_t  send_db_milli;
} oevc_sym_route_t;

typedef struct {
    uint32_t param_id;
    int32_t  value_milli; /* 0..1000 normalized or domain value */
    char     name[OEVC_STR_MAX];
} oevc_sym_param_t;

typedef struct {
    uint32_t          plugin_id;
    uint32_t          track_id;
    uint32_t          vendor_chunk_id; /* 0 if none */
    char              name[OEVC_STR_MAX];
    char              format[32];      /* e.g. "VST3", "CLAP", "Native" */
    oevc_sym_param_t *params;
    uint32_t          param_count;
} oevc_sym_plugin_t;

/* Add typed sym_IL chunks into an open-tier container */
oevc_status_t oevc_sym_add_meta(oevc_container_t *cont, const oevc_sym_meta_t *meta, uint32_t *out_id);
oevc_status_t oevc_sym_add_track(oevc_container_t *cont, const oevc_sym_track_t *track, uint32_t *out_id);
oevc_status_t oevc_sym_add_clip(oevc_container_t *cont, const oevc_sym_clip_t *clip, uint32_t *out_id);
oevc_status_t oevc_sym_add_route(oevc_container_t *cont, const oevc_sym_route_t *route, uint32_t *out_id);
oevc_status_t oevc_sym_add_plugin(oevc_container_t *cont, const oevc_sym_plugin_t *plugin, uint32_t *out_id);

/* Decode helpers */
oevc_status_t oevc_sym_decode_meta(const oevc_chunk_t *chunk, oevc_sym_meta_t *out);
oevc_status_t oevc_sym_decode_track(const oevc_chunk_t *chunk, oevc_sym_track_t *out);
oevc_status_t oevc_sym_decode_clip(const oevc_chunk_t *chunk, oevc_sym_clip_t *out);
void          oevc_sym_free_clip(oevc_sym_clip_t *clip);
oevc_status_t oevc_sym_decode_route(const oevc_chunk_t *chunk, oevc_sym_route_t *out);
oevc_status_t oevc_sym_decode_plugin(const oevc_chunk_t *chunk, oevc_sym_plugin_t *out);
void          oevc_sym_free_plugin(oevc_sym_plugin_t *plugin);

/* Export entire container (open sym_IL + vendor_void summaries) to JSON */
oevc_status_t oevc_export_json_file(const oevc_container_t *cont, const char *json_path);
oevc_status_t oevc_export_json_stream(const oevc_container_t *cont, FILE *out);

#ifdef __cplusplus
}
#endif

#endif /* OEVC_SYM_IL_H */
