#include "oevc/oevc.h"
#include "oevc/sym_il.h"

#include <stdio.h>
#include <string.h>

/*
 * Demonstrates a minimal 4-track DAW saving a rescuable project:
 * - Track 1: MIDI Synth (with notes + open plugin params + vendor_void preset blob)
 * - Track 2: Audio Drums (WAV asset reference)
 * - Track 3: Audio Vocal (WAV asset reference)
 * - Track 4: Master Bus (with routes from tracks 1..3)
 */
int main(int argc, char **argv) {
    const char *out_path = (argc >= 2) ? argv[1] : "var/basic_daw.oevc";

    oevc_container_t cont;
    if (oevc_init(&cont, OEV_CONT_COMPLETE, 0u) != OEVC_OK) {
        fprintf(stderr, "failed to init container\n");
        return 1;
    }

    oevc_sym_meta_t meta = {
        .bpm_milli = 128000,
        .ppq = 960,
        .project_name = "Four Track Rescue Demo",
        .author = "Matt",
        .software = "BasicDAW 0.1"
    };
    oevc_sym_add_meta(&cont, &meta, NULL);

    oevc_sym_track_t tracks[4] = {
        { .track_id = 1, .track_type = OEV_TRACK_MIDI,  .gain_db_milli = -3000, .pan_milli = 0,    .name = "Lead Synth" },
        { .track_id = 2, .track_type = OEV_TRACK_AUDIO, .gain_db_milli = -1500, .pan_milli = -100, .name = "Drum Loop" },
        { .track_id = 3, .track_type = OEV_TRACK_AUDIO, .gain_db_milli = 0,     .pan_milli = 100,  .name = "Vocal Take" },
        { .track_id = 4, .track_type = OEV_TRACK_BUS,   .gain_db_milli = 0,     .pan_milli = 0,    .name = "Master" }
    };
    for (int i = 0; i < 4; ++i) {
        oevc_sym_add_track(&cont, &tracks[i], NULL);
    }

    /* MIDI clip on Track 1 */
    oevc_sym_note_t notes[3] = {
        { .offset_tick = 0,    .duration_tick = 480, .pitch = 60, .velocity = 100, .channel = 0 },
        { .offset_tick = 480,  .duration_tick = 480, .pitch = 64, .velocity = 95,  .channel = 0 },
        { .offset_tick = 960,  .duration_tick = 960, .pitch = 67, .velocity = 110, .channel = 0 }
    };
    oevc_sym_clip_t midi_clip = {
        .clip_id = 10, .track_id = 1, .clip_type = OEV_CLIP_MIDI,
        .start_tick = 0, .length_ticks = 1920,
        .asset_path = "", .notes = notes, .note_count = 3
    };
    oevc_sym_add_clip(&cont, &midi_clip, NULL);

    /* Audio clips on Tracks 2 & 3 */
    oevc_sym_clip_t drum_clip = {
        .clip_id = 11, .track_id = 2, .clip_type = OEV_CLIP_AUDIO,
        .start_tick = 0, .length_ticks = 3840,
        .asset_path = "assets/drums_128bpm.wav", .notes = NULL, .note_count = 0
    };
    oevc_sym_clip_t vocal_clip = {
        .clip_id = 12, .track_id = 3, .clip_type = OEV_CLIP_AUDIO,
        .start_tick = 1920, .length_ticks = 3840,
        .asset_path = "assets/vocal_chorus.wav", .notes = NULL, .note_count = 0
    };
    oevc_sym_add_clip(&cont, &drum_clip, NULL);
    oevc_sym_add_clip(&cont, &vocal_clip, NULL);

    /* Routes to Master Bus (Track 4) */
    for (uint32_t src = 1; src <= 3; ++src) {
        oevc_sym_route_t rt = { .src_track_id = src, .dst_track_id = 4, .send_db_milli = 0 };
        oevc_sym_add_route(&cont, &rt, NULL);
    }

    /* Protected vendor_void preset blob + open-tier plugin descriptor */
    const uint8_t proprietary_preset[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0x01, 0x02, 0x03, 0x04 };
    uint32_t vendor_blob_id = 0;
    oevc_add_chunk(&cont, OEV_TIER_VENDOR_VOID, OEV_KIND_VENDOR_BLOB, 0,
                   proprietary_preset, (uint32_t)sizeof(proprietary_preset), &vendor_blob_id);

    oevc_sym_param_t params[2] = {
        { .param_id = 1, .value_milli = 650, .name = "cutoff" },
        { .param_id = 2, .value_milli = 250, .name = "resonance" }
    };
    oevc_sym_plugin_t synth = {
        .plugin_id = 100, .track_id = 1, .vendor_chunk_id = vendor_blob_id,
        .name = "VendorSynth Pro", .format = "VST3",
        .params = params, .param_count = 2
    };
    oevc_sym_add_plugin(&cont, &synth, NULL);

    if (oevc_save_file(&cont, out_path) != OEVC_OK) {
        fprintf(stderr, "failed to save %s\n", out_path);
        oevc_free(&cont);
        return 1;
    }

    printf("saved basic_daw project to %s (%zu chunks)\n", out_path, cont.chunk_count);
    oevc_free(&cont);
    return 0;
}
