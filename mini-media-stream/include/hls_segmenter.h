#ifndef HLS_SEGMENTER_H
#define HLS_SEGMENTER_H

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

#define HLS_MAX_SEGMENTS        1024
#define HLS_MAX_VARIANTS          16
#define HLS_MAX_KEY_URI_LEN      512
#define HLS_MAX_TAG_LEN          128
#define HLS_MAX_URL_LEN         2048
#define HLS_PLAYLIST_MAX_SIZE  65536u
#define HLS_DEFAULT_VERSION        3
#define HLS_DEFAULT_TARGET_DUR     6

typedef enum {
    HLS_PLAYLIST_VOD,
    HLS_PLAYLIST_LIVE,
    HLS_PLAYLIST_EVENT
} hls_playlist_type_t;

typedef enum {
    HLS_KEY_NONE,
    HLS_KEY_AES128,
    HLS_KEY_SAMPLE_AES
} hls_key_method_t;

typedef enum {
    HLS_KEY_FORMAT_IDENTITY,
    HLS_KEY_FORMAT_BASE64,
    HLS_KEY_FORMAT_HEX
} hls_key_format_t;

typedef struct {
    char             uri[HLS_MAX_KEY_URI_LEN];
    uint8_t          iv[16];
    hls_key_method_t method;
    hls_key_format_t format;
    uint8_t          has_iv;
} hls_key_t;

typedef struct {
    char        url[HLS_MAX_URL_LEN];
    double      duration;
    uint64_t    byte_offset;
    uint64_t    byte_length;
    uint64_t    sequence;
    uint8_t     has_byte_range;
    uint8_t     discontinuity;
    int64_t     program_date_time_ms;
    hls_key_t   key;
    uint8_t     has_key;
} hls_segment_t;

typedef struct {
    hls_segment_t segments[HLS_MAX_SEGMENTS];
    uint32_t      segment_count;
    uint32_t      media_sequence;
    uint32_t      target_duration;
    uint32_t      version;
    hls_playlist_type_t type;
    double        total_duration;
    uint32_t      window_size;
    uint8_t       ended;
    uint8_t       has_discontinuity;
    uint8_t       allow_cache;
} hls_playlist_t;

typedef struct {
    char            uri[HLS_MAX_URL_LEN];
    char            codecs[64];
    uint32_t        bandwidth;
    uint32_t        avg_bandwidth;
    uint32_t        width;
    uint32_t        height;
    double          frame_rate;
    char            resolution[32];
} hls_variant_stream_t;

typedef struct {
    hls_variant_stream_t variants[HLS_MAX_VARIANTS];
    uint32_t             variant_count;
    char                 uri[HLS_MAX_URL_LEN];
} hls_variant_playlist_t;

typedef struct {
    hls_playlist_t playlist;
    hls_variant_playlist_t variant;
    FILE           *segment_file;
    uint32_t        current_segment_seq;
    char            output_dir[HLS_MAX_URL_LEN];
    char            segment_prefix[64];
} hls_segmenter_t;

int         hls_segmenter_init(hls_segmenter_t *seg, const char *output_dir, const char *prefix);
void        hls_segmenter_deinit(hls_segmenter_t *seg);

int         hls_segmenter_set_type(hls_segmenter_t *seg, hls_playlist_type_t type);
int         hls_segmenter_set_target_duration(hls_segmenter_t *seg, uint32_t duration);
int         hls_segmenter_set_window_size(hls_segmenter_t *seg, uint32_t size);
int         hls_segmenter_set_version(hls_segmenter_t *seg, uint32_t version);

int         hls_segmenter_start_segment(hls_segmenter_t *seg, const char *filename);
int         hls_segmenter_write_packet(hls_segmenter_t *seg, const uint8_t *data, size_t len);
int         hls_segmenter_finish_segment(hls_segmenter_t *seg, double duration);
int         hls_segmenter_mark_discontinuity(hls_segmenter_t *seg);
int         hls_segmenter_set_program_date_time(hls_segmenter_t *seg, int64_t time_ms);

int         hls_segmenter_add_key(hls_segmenter_t *seg, const hls_key_t *key);
int         hls_segmenter_rotate_key(hls_segmenter_t *seg, const hls_key_t *new_key);

int         hls_segmenter_generate_playlist(hls_segmenter_t *seg, char *buffer, size_t buf_size);
int         hls_segmenter_write_playlist_file(hls_segmenter_t *seg, const char *filename);

int         hls_segmenter_add_byte_range(hls_segmenter_t *seg, uint64_t offset, uint64_t length);
int         hls_segmenter_end_playlist(hls_segmenter_t *seg);

int         hls_variant_init(hls_variant_playlist_t *vp);
int         hls_variant_add_stream(hls_variant_playlist_t *vp, const char *uri,
                                    uint32_t bandwidth, uint32_t width, uint32_t height,
                                    const char *codecs);
int         hls_variant_generate_m3u8(hls_variant_playlist_t *vp, char *buffer, size_t buf_size);

const char *hls_key_method_string(hls_key_method_t method);
const char *hls_playlist_type_string(hls_playlist_type_t type);

#endif
