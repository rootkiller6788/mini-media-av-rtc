#include "hls_segmenter.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

int hls_segmenter_init(hls_segmenter_t *seg, const char *output_dir, const char *prefix)
{
    if (!seg || !output_dir || !prefix) return -1;
    memset(seg, 0, sizeof(*seg));
    strncpy(seg->output_dir, output_dir, HLS_MAX_URL_LEN - 1);
    strncpy(seg->segment_prefix, prefix, 63);
    seg->playlist.version = HLS_DEFAULT_VERSION;
    seg->playlist.target_duration = HLS_DEFAULT_TARGET_DUR;
    seg->playlist.window_size = 3;
    seg->playlist.type = HLS_PLAYLIST_VOD;
    seg->current_segment_seq = 0;
    return 0;
}

void hls_segmenter_deinit(hls_segmenter_t *seg)
{
    if (!seg) return;
    if (seg->segment_file) {
        fclose(seg->segment_file);
        seg->segment_file = NULL;
    }
}

int hls_segmenter_set_type(hls_segmenter_t *seg, hls_playlist_type_t type)
{
    if (!seg) return -1;
    seg->playlist.type = type;
    return 0;
}

int hls_segmenter_set_target_duration(hls_segmenter_t *seg, uint32_t duration)
{
    if (!seg || duration == 0) return -1;
    seg->playlist.target_duration = duration;
    return 0;
}

int hls_segmenter_set_window_size(hls_segmenter_t *seg, uint32_t size)
{
    if (!seg || size == 0) return -1;
    seg->playlist.window_size = size;
    return 0;
}

int hls_segmenter_set_version(hls_segmenter_t *seg, uint32_t version)
{
    if (!seg || version == 0) return -1;
    seg->playlist.version = version;
    return 0;
}

int hls_segmenter_start_segment(hls_segmenter_t *seg, const char *filename)
{
    char fullpath[HLS_MAX_URL_LEN];
    if (!seg || !filename) return -1;

    if (seg->segment_file) {
        fclose(seg->segment_file);
    }

    snprintf(fullpath, sizeof(fullpath), "%s/%s", seg->output_dir, filename);
    seg->segment_file = fopen(fullpath, "wb");
    if (!seg->segment_file) return -1;

    if (seg->playlist.segment_count >= HLS_MAX_SEGMENTS) {
        if (seg->playlist.type == HLS_PLAYLIST_LIVE && seg->playlist.window_size > 0) {
            uint32_t shift = seg->playlist.segment_count - seg->playlist.window_size + 1;
            memmove(seg->playlist.segments, seg->playlist.segments + shift,
                    (seg->playlist.segment_count - shift) * sizeof(hls_segment_t));
            seg->playlist.segment_count -= shift;
            seg->playlist.media_sequence += shift;
        } else {
            fclose(seg->segment_file);
            seg->segment_file = NULL;
            return -1;
        }
    }

    return 0;
}

int hls_segmenter_write_packet(hls_segmenter_t *seg, const uint8_t *data, size_t len)
{
    if (!seg || !seg->segment_file || !data || len == 0) return -1;
    size_t written = fwrite(data, 1, len, seg->segment_file);
    return (written == len) ? 0 : -1;
}

int hls_segmenter_finish_segment(hls_segmenter_t *seg, double duration)
{
    hls_segment_t *s;
    if (!seg || !seg->segment_file) return -1;

    fclose(seg->segment_file);
    seg->segment_file = NULL;

    s = &seg->playlist.segments[seg->playlist.segment_count];
    s->duration = duration;
    s->sequence = seg->current_segment_seq++;
    s->discontinuity = 0;
    s->has_byte_range = 0;
    s->has_key = 0;
    s->program_date_time_ms = -1;

    snprintf(s->url, sizeof(s->url), "%s_%u.ts",
             seg->segment_prefix, s->sequence);

    seg->playlist.segment_count++;
    seg->playlist.total_duration += duration;

    if (duration > (double)seg->playlist.target_duration) {
        seg->playlist.target_duration = (uint32_t)(duration + 1.0);
    }

    return 0;
}

int hls_segmenter_mark_discontinuity(hls_segmenter_t *seg)
{
    uint32_t idx;
    if (!seg) return -1;
    if (seg->playlist.segment_count == 0) return -1;
    idx = seg->playlist.segment_count - 1;
    seg->playlist.segments[idx].discontinuity = 1;
    seg->playlist.has_discontinuity = 1;
    return 0;
}

int hls_segmenter_set_program_date_time(hls_segmenter_t *seg, int64_t time_ms)
{
    uint32_t idx;
    if (!seg) return -1;
    if (seg->playlist.segment_count == 0) return -1;
    idx = seg->playlist.segment_count - 1;
    seg->playlist.segments[idx].program_date_time_ms = time_ms;
    return 0;
}

int hls_segmenter_add_key(hls_segmenter_t *seg, const hls_key_t *key)
{
    uint32_t idx;
    if (!seg || !key) return -1;
    if (seg->playlist.segment_count == 0) return -1;
    idx = seg->playlist.segment_count - 1;
    memcpy(&seg->playlist.segments[idx].key, key, sizeof(hls_key_t));
    seg->playlist.segments[idx].has_key = 1;
    return 0;
}

int hls_segmenter_rotate_key(hls_segmenter_t *seg, const hls_key_t *new_key)
{
    return hls_segmenter_add_key(seg, new_key);
}

int hls_segmenter_add_byte_range(hls_segmenter_t *seg, uint64_t offset, uint64_t length)
{
    uint32_t idx;
    if (!seg) return -1;
    if (seg->playlist.segment_count == 0) return -1;
    idx = seg->playlist.segment_count - 1;
    seg->playlist.segments[idx].byte_offset = offset;
    seg->playlist.segments[idx].byte_length = length;
    seg->playlist.segments[idx].has_byte_range = 1;
    return 0;
}

int hls_segmenter_end_playlist(hls_segmenter_t *seg)
{
    if (!seg) return -1;
    seg->playlist.ended = 1;
    return 0;
}

static void format_time(char *buffer, size_t buf_size, double seconds)
{
    int h = (int)(seconds / 3600.0);
    int m = (int)((seconds - h * 3600.0) / 60.0);
    double s = seconds - h * 3600.0 - m * 60.0;
    snprintf(buffer, buf_size, "%d:%02d:%06.3f", h, m, s);
}

static void format_iso8601(char *buffer, size_t buf_size, int64_t time_ms)
{
    time_t t = (time_t)(time_ms / 1000);
    struct tm *tm_info = gmtime(&t);
    int ms = (int)(time_ms % 1000);
    strftime(buffer, buf_size, "%Y-%m-%dT%H:%M:%S", tm_info);
    char ms_str[8];
    snprintf(ms_str, sizeof(ms_str), ".%03dZ", ms);
    strncat(buffer, ms_str, buf_size - strlen(buffer) - 1);
}

int hls_segmenter_generate_playlist(hls_segmenter_t *seg, char *buffer, size_t buf_size)
{
    hls_playlist_t *pl;
    uint32_t i;
    size_t pos = 0;
    int written;

    if (!seg || !buffer || buf_size == 0) return -1;
    pl = &seg->playlist;

    written = snprintf(buffer + pos, buf_size - pos,
                       "#EXTM3U\n#EXT-X-VERSION:%u\n#EXT-X-TARGETDURATION:%u\n",
                       pl->version, pl->target_duration);
    if (written < 0 || (size_t)written >= buf_size - pos) return -1;
    pos += (size_t)written;

    if (pl->media_sequence > 0) {
        written = snprintf(buffer + pos, buf_size - pos,
                           "#EXT-X-MEDIA-SEQUENCE:%u\n", pl->media_sequence);
        if (written < 0 || (size_t)written >= buf_size - pos) return -1;
        pos += (size_t)written;
    }

    if (pl->type == HLS_PLAYLIST_VOD) {
        char type_str[] = "VOD";
        written = snprintf(buffer + pos, buf_size - pos,
                           "#EXT-X-PLAYLIST-TYPE:%s\n", type_str);
        if (written < 0 || (size_t)written >= buf_size - pos) return -1;
        pos += (size_t)written;
    } else if (pl->type == HLS_PLAYLIST_EVENT) {
        char type_str[] = "EVENT";
        written = snprintf(buffer + pos, buf_size - pos,
                           "#EXT-X-PLAYLIST-TYPE:%s\n", type_str);
        if (written < 0 || (size_t)written >= buf_size - pos) return -1;
        pos += (size_t)written;
    }

    if (!pl->allow_cache) {
        written = snprintf(buffer + pos, buf_size - pos,
                           "#EXT-X-ALLOW-CACHE:NO\n");
        if (written < 0 || (size_t)written >= buf_size - pos) return -1;
        pos += (size_t)written;
    }

    for (i = 0; i < pl->segment_count; i++) {
        hls_segment_t *s = &pl->segments[i];

        if (s->discontinuity) {
            written = snprintf(buffer + pos, buf_size - pos,
                               "#EXT-X-DISCONTINUITY\n");
            if (written < 0 || (size_t)written >= buf_size - pos) return -1;
            pos += (size_t)written;
        }

        if (s->program_date_time_ms >= 0) {
            char dt_str[40];
            format_iso8601(dt_str, sizeof(dt_str), s->program_date_time_ms);
            written = snprintf(buffer + pos, buf_size - pos,
                               "#EXT-X-PROGRAM-DATE-TIME:%s\n", dt_str);
            if (written < 0 || (size_t)written >= buf_size - pos) return -1;
            pos += (size_t)written;
        }

        if (s->has_key) {
            const char *method_str = hls_key_method_string(s->key.method);
            written = snprintf(buffer + pos, buf_size - pos,
                               "#EXT-X-KEY:METHOD=%s,URI=\"%s\"",
                               method_str, s->key.uri);
            if (s->key.has_iv) {
                char iv_str[40];
                snprintf(iv_str, sizeof(iv_str),
                         ",IV=0x%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X",
                         s->key.iv[0], s->key.iv[1], s->key.iv[2], s->key.iv[3],
                         s->key.iv[4], s->key.iv[5], s->key.iv[6], s->key.iv[7],
                         s->key.iv[8], s->key.iv[9], s->key.iv[10], s->key.iv[11],
                         s->key.iv[12], s->key.iv[13], s->key.iv[14], s->key.iv[15]);
                written += snprintf(buffer + pos + written, buf_size - pos - written,
                                    "%s", iv_str);
            }
            written += snprintf(buffer + pos + written, buf_size - pos - written, "\n");
            if (written < 0 || (size_t)written >= buf_size - pos) return -1;
            pos += (size_t)written;
        }

        if (s->has_byte_range) {
            written = snprintf(buffer + pos, buf_size - pos,
                               "#EXT-X-BYTERANGE:%llu@%llu\n",
                               (unsigned long long)s->byte_length,
                               (unsigned long long)s->byte_offset);
            if (written < 0 || (size_t)written >= buf_size - pos) return -1;
            pos += (size_t)written;
        }

        char formatted_time[32];
        format_time(formatted_time, sizeof(formatted_time), s->duration);
        written = snprintf(buffer + pos, buf_size - pos,
                           "#EXTINF:%.3f,\n%s\n",
                           s->duration, s->url);
        if (written < 0 || (size_t)written >= buf_size - pos) return -1;
        pos += (size_t)written;
    }

    if (pl->ended) {
        written = snprintf(buffer + pos, buf_size - pos, "#EXT-X-ENDLIST\n");
        if (written < 0 || (size_t)written >= buf_size - pos) return -1;
        pos += (size_t)written;
    }

    return (int)pos;
}

int hls_segmenter_write_playlist_file(hls_segmenter_t *seg, const char *filename)
{
    static char buffer[HLS_PLAYLIST_MAX_SIZE];
    char fullpath[HLS_MAX_URL_LEN];
    FILE *f;
    int len;

    if (!seg || !filename) return -1;
    len = hls_segmenter_generate_playlist(seg, buffer, sizeof(buffer));
    if (len <= 0) return -1;

    snprintf(fullpath, sizeof(fullpath), "%s/%s", seg->output_dir, filename);
    f = fopen(fullpath, "w");
    if (!f) return -1;

    fwrite(buffer, 1, (size_t)len, f);
    fclose(f);
    return 0;
}

int hls_variant_init(hls_variant_playlist_t *vp)
{
    if (!vp) return -1;
    memset(vp, 0, sizeof(*vp));
    return 0;
}

int hls_variant_add_stream(hls_variant_playlist_t *vp, const char *uri,
                           uint32_t bandwidth, uint32_t width, uint32_t height,
                           const char *codecs)
{
    hls_variant_stream_t *s;
    if (!vp || !uri || vp->variant_count >= HLS_MAX_VARIANTS) return -1;

    s = &vp->variants[vp->variant_count];
    strncpy(s->uri, uri, HLS_MAX_URL_LEN - 1);
    s->bandwidth = bandwidth;
    s->width = width;
    s->height = height;
    if (codecs) strncpy(s->codecs, codecs, 63);
    snprintf(s->resolution, sizeof(s->resolution), "%ux%u", width, height);
    s->avg_bandwidth = bandwidth;
    vp->variant_count++;
    return 0;
}

int hls_variant_generate_m3u8(hls_variant_playlist_t *vp, char *buffer, size_t buf_size)
{
    uint32_t i;
    size_t pos = 0;
    int written;

    if (!vp || !buffer || buf_size == 0) return -1;

    written = snprintf(buffer + pos, buf_size - pos,
                       "#EXTM3U\n#EXT-X-VERSION:3\n");
    if (written < 0 || (size_t)written >= buf_size - pos) return -1;
    pos += (size_t)written;

    for (i = 0; i < vp->variant_count; i++) {
        hls_variant_stream_t *s = &vp->variants[i];
        written = snprintf(buffer + pos, buf_size - pos,
                           "#EXT-X-STREAM-INF:BANDWIDTH=%u,RESOLUTION=%s",
                           s->bandwidth, s->resolution);
        if (written < 0 || (size_t)written >= buf_size - pos) return -1;
        pos += (size_t)written;

        if (s->codecs[0]) {
            written = snprintf(buffer + pos, buf_size - pos,
                               ",CODECS=\"%s\"", s->codecs);
            if (written < 0 || (size_t)written >= buf_size - pos) return -1;
            pos += (size_t)written;
        }

        if (s->avg_bandwidth != s->bandwidth) {
            written = snprintf(buffer + pos, buf_size - pos,
                               ",AVERAGE-BANDWIDTH=%u", s->avg_bandwidth);
            if (written < 0 || (size_t)written >= buf_size - pos) return -1;
            pos += (size_t)written;
        }

        if (s->frame_rate > 0.0) {
            written = snprintf(buffer + pos, buf_size - pos,
                               ",FRAME-RATE=%.3f", s->frame_rate);
            if (written < 0 || (size_t)written >= buf_size - pos) return -1;
            pos += (size_t)written;
        }

        written = snprintf(buffer + pos, buf_size - pos, "\n%s\n", s->uri);
        if (written < 0 || (size_t)written >= buf_size - pos) return -1;
        pos += (size_t)written;
    }

    return (int)pos;
}

const char *hls_key_method_string(hls_key_method_t method)
{
    switch (method) {
    case HLS_KEY_AES128:     return "AES-128";
    case HLS_KEY_SAMPLE_AES: return "SAMPLE-AES";
    default:                 return "NONE";
    }
}

const char *hls_playlist_type_string(hls_playlist_type_t type)
{
    switch (type) {
    case HLS_PLAYLIST_VOD:   return "VOD";
    case HLS_PLAYLIST_LIVE:  return "LIVE";
    case HLS_PLAYLIST_EVENT: return "EVENT";
    default:                 return "UNKNOWN";
    }
}
