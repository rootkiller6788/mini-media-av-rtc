#ifndef DASH_SEGMENTER_H
#define DASH_SEGMENTER_H

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

#define DASH_MAX_PERIODS          32
#define DASH_MAX_ADAPTATIONS       8
#define DASH_MAX_REPRESENTATIONS  16
#define DASH_MAX_SEGMENTS       4096
#define DASH_MAX_URL_LEN        2048
#define DASH_MPD_MAX_SIZE      131072u
#define DASH_DEFAULT_TIMESCALE   1000u
#define DASH_MIN_BUFFER_TIME    1500u

typedef enum {
    DASH_TEMPLATE_NUMBER,
    DASH_TEMPLATE_TIME
} dash_template_type_t;

typedef enum {
    DASH_CONTENT_VIDEO,
    DASH_CONTENT_AUDIO,
    DASH_CONTENT_SUBTITLE,
    DASH_CONTENT_TEXT
} dash_content_type_t;

typedef enum {
    DASH_TIMING_NTP,
    DASH_TIMING_HTTP,
    DASH_TIMING_GPS
} dash_utc_timing_scheme_t;

typedef struct {
    char     source_url[DASH_MAX_URL_LEN];
    char     scheme_id[64];
} dash_utc_timing_t;

typedef struct {
    uint64_t  time;             /* presentation time in timescale */
    uint64_t  duration;         /* segment duration in timescale */
    uint32_t  number;           /* segment number */
    char      url[DASH_MAX_URL_LEN];
} dash_segment_timeline_entry_t;

typedef struct {
    dash_segment_timeline_entry_t entries[DASH_MAX_SEGMENTS];
    uint32_t                      count;
} dash_segment_timeline_t;

typedef struct {
    char                      id[64];
    uint32_t                  bandwidth;
    uint32_t                  width;
    uint32_t                  height;
    char                      codecs[64];
    char                      mime_type[64];
    uint32_t                  timescale;
    char                      init_segment_url[DASH_MAX_URL_LEN];
    char                      media_segment_url[DASH_MAX_URL_LEN];
    dash_template_type_t      template_type;
    uint32_t                  start_number;
    dash_segment_timeline_t   timeline;
    uint32_t                  segment_duration;
} dash_representation_t;

typedef struct {
    char                         id[64];
    dash_content_type_t          content_type;
    uint32_t                     group;
    dash_representation_t        representations[DASH_MAX_REPRESENTATIONS];
    uint32_t                     rep_count;
    uint8_t                      segment_alignment;
    uint8_t                      bitstream_switching;
    char                         lang[8];
} dash_adaptation_set_t;

typedef struct {
    char                      id[64];
    uint64_t                  start_ms;
    uint64_t                  duration_ms;
    dash_adaptation_set_t     adaptation_sets[DASH_MAX_ADAPTATIONS];
    uint32_t                  adaptation_count;
} dash_period_t;

typedef struct {
    dash_period_t      periods[DASH_MAX_PERIODS];
    uint32_t            period_count;
    char                id[64];
    uint32_t            min_buffer_time_ms;
    uint32_t            max_segment_duration_ms;
    char                profiles[128];
    dash_utc_timing_t   utc_timing;
    uint8_t             has_utc_timing;
    uint8_t             is_dynamic;
    uint64_t            availability_start_time_ms;
    uint64_t            time_shift_buffer_depth_ms;
} dash_mpd_t;

typedef struct {
    uint8_t  version;
    uint32_t flags;
    uint32_t track_id;
    uint64_t base_data_offset;
    uint32_t sample_description_index;
    uint32_t default_sample_duration;
    uint32_t default_sample_size;
    uint32_t default_sample_flags;
} dash_tfhd_t;

typedef struct {
    uint32_t sequence_number;
} dash_mfhd_t;

typedef struct {
    uint64_t base_media_decode_time;
} dash_tfdt_t;

typedef struct {
    uint32_t  sample_count;
    uint32_t *sample_durations;
    uint32_t *sample_sizes;
    uint32_t *sample_flags;
} dash_trun_t;

typedef struct {
    dash_mfhd_t  mfhd;
    dash_tfhd_t  tfhd;
    dash_tfdt_t  tfdt;
    dash_trun_t  trun;
    uint32_t     sample_count;
} dash_moof_t;

typedef struct {
    dash_mpd_t    mpd;
    char          output_dir[DASH_MAX_URL_LEN];
    char          init_prefix[32];
    char          segment_prefix[32];
    uint32_t      current_segment_number;
    uint64_t      current_time_ms;
    FILE         *segment_file;
    FILE         *init_file;
    uint32_t      timescale;
    uint32_t      sequence_number;
} dash_segmenter_t;

int     dash_segmenter_init(dash_segmenter_t *seg, const char *output_dir, const char *prefix);
void    dash_segmenter_deinit(dash_segmenter_t *seg);

int     dash_segmenter_mpd_init(dash_segmenter_t *seg, const char *profiles, uint8_t is_dynamic);
int     dash_segmenter_add_period(dash_segmenter_t *seg, const char *period_id,
                                  uint64_t start_ms, uint64_t duration_ms);
int     dash_segmenter_add_adaptation_set(dash_segmenter_t *seg, uint32_t period_index,
                                          const char *as_id, dash_content_type_t type,
                                          const char *lang);
int     dash_segmenter_add_representation(dash_segmenter_t *seg, uint32_t period_index,
                                          uint32_t as_index, const char *rep_id,
                                          uint32_t bandwidth, uint32_t width, uint32_t height,
                                          const char *codecs, const char *mime);

int     dash_segmenter_set_template(dash_segmenter_t *seg, uint32_t period_index,
                                    uint32_t as_index, uint32_t rep_index,
                                    dash_template_type_t type, uint32_t start_number,
                                    uint32_t duration);
int     dash_segmenter_add_timeline_entry(dash_segmenter_t *seg, uint32_t period_index,
                                          uint32_t as_index, uint32_t rep_index,
                                          uint64_t time, uint64_t duration, uint32_t number);

int     dash_segmenter_set_init_segment(dash_segmenter_t *seg, uint32_t period_index,
                                        uint32_t as_index, uint32_t rep_index,
                                        const char *url);
int     dash_segmenter_set_media_segment(dash_segmenter_t *seg, uint32_t period_index,
                                         uint32_t as_index, uint32_t rep_index,
                                         const char *url);
int     dash_segmenter_set_utc_timing(dash_segmenter_t *seg,
                                      dash_utc_timing_scheme_t scheme,
                                      const char *source_url);

int     dash_segmenter_write_init_segment(dash_segmenter_t *seg, const uint8_t *ftyp,
                                          size_t ftyp_len, const uint8_t *moov, size_t moov_len);
int     dash_segmenter_start_fmp4(dash_segmenter_t *seg, const char *filename,
                                  uint64_t base_decode_time);
int     dash_segmenter_finish_fmp4(dash_segmenter_t *seg);

int     dash_segmenter_generate_mpd(dash_segmenter_t *seg, char *buffer, size_t buf_size);
int     dash_segmenter_write_mpd_file(dash_segmenter_t *seg, const char *filename);

int     dash_segmenter_advance_segment(dash_segmenter_t *seg);

const char *dash_content_type_string(dash_content_type_t type);
const char *dash_template_type_string(dash_template_type_t type);

#endif
