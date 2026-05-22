#include "dash_segmenter.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

int dash_segmenter_init(dash_segmenter_t *seg, const char *output_dir, const char *prefix)
{
    if (!seg || !output_dir || !prefix) return -1;
    memset(seg, 0, sizeof(*seg));
    strncpy(seg->output_dir, output_dir, DASH_MAX_URL_LEN - 1);
    strncpy(seg->segment_prefix, prefix, 31);
    strncpy(seg->init_prefix, "init", 31);
    seg->timescale = DASH_DEFAULT_TIMESCALE;
    seg->current_segment_number = 1;
    seg->current_time_ms = 0;
    seg->sequence_number = 0;
    seg->mpd.min_buffer_time_ms = DASH_MIN_BUFFER_TIME;
    return 0;
}

void dash_segmenter_deinit(dash_segmenter_t *seg)
{
    if (!seg) return;
    if (seg->segment_file) {
        fclose(seg->segment_file);
        seg->segment_file = NULL;
    }
    if (seg->init_file) {
        fclose(seg->init_file);
        seg->init_file = NULL;
    }
}

int dash_segmenter_mpd_init(dash_segmenter_t *seg, const char *profiles, uint8_t is_dynamic)
{
    if (!seg || !profiles) return -1;
    strncpy(seg->mpd.profiles, profiles, 127);
    seg->mpd.is_dynamic = is_dynamic;
    seg->mpd.period_count = 0;
    seg->mpd.has_utc_timing = 0;
    seg->mpd.availability_start_time_ms = (uint64_t)time(NULL) * 1000;
    seg->mpd.time_shift_buffer_depth_ms = 60000;
    return 0;
}

int dash_segmenter_add_period(dash_segmenter_t *seg, const char *period_id,
                              uint64_t start_ms, uint64_t duration_ms)
{
    dash_period_t *p;
    if (!seg || !period_id || seg->mpd.period_count >= DASH_MAX_PERIODS) return -1;

    p = &seg->mpd.periods[seg->mpd.period_count];
    memset(p, 0, sizeof(*p));
    strncpy(p->id, period_id, 63);
    p->start_ms = start_ms;
    p->duration_ms = duration_ms;
    seg->mpd.period_count++;
    return 0;
}

int dash_segmenter_add_adaptation_set(dash_segmenter_t *seg, uint32_t period_index,
                                      const char *as_id, dash_content_type_t type,
                                      const char *lang)
{
    dash_period_t *period;
    dash_adaptation_set_t *as;
    if (!seg || !as_id || period_index >= seg->mpd.period_count) return -1;

    period = &seg->mpd.periods[period_index];
    if (period->adaptation_count >= DASH_MAX_ADAPTATIONS) return -1;

    as = &period->adaptation_sets[period->adaptation_count];
    memset(as, 0, sizeof(*as));
    strncpy(as->id, as_id, 63);
    as->content_type = type;
    if (lang) strncpy(as->lang, lang, 7);
    as->segment_alignment = 1;
    as->bitstream_switching = 1;
    period->adaptation_count++;
    return 0;
}

int dash_segmenter_add_representation(dash_segmenter_t *seg, uint32_t period_index,
                                      uint32_t as_index, const char *rep_id,
                                      uint32_t bandwidth, uint32_t width, uint32_t height,
                                      const char *codecs, const char *mime)
{
    dash_period_t *period;
    dash_adaptation_set_t *as;
    dash_representation_t *rep;
    if (!seg || !rep_id || period_index >= seg->mpd.period_count) return -1;

    period = &seg->mpd.periods[period_index];
    if (as_index >= period->adaptation_count) return -1;

    as = &period->adaptation_sets[as_index];
    if (as->rep_count >= DASH_MAX_REPRESENTATIONS) return -1;

    rep = &as->representations[as->rep_count];
    memset(rep, 0, sizeof(*rep));
    strncpy(rep->id, rep_id, 63);
    rep->bandwidth = bandwidth;
    rep->width = width;
    rep->height = height;
    if (codecs) strncpy(rep->codecs, codecs, 63);
    if (mime) strncpy(rep->mime_type, mime, 63);
    rep->timescale = seg->timescale;
    rep->start_number = 1;
    as->rep_count++;
    return 0;
}

int dash_segmenter_set_template(dash_segmenter_t *seg, uint32_t period_index,
                                uint32_t as_index, uint32_t rep_index,
                                dash_template_type_t type, uint32_t start_number,
                                uint32_t duration)
{
    dash_period_t *period;
    dash_adaptation_set_t *as;
    dash_representation_t *rep;
    if (!seg || period_index >= seg->mpd.period_count) return -1;

    period = &seg->mpd.periods[period_index];
    if (as_index >= period->adaptation_count) return -1;

    as = &period->adaptation_sets[as_index];
    if (rep_index >= as->rep_count) return -1;

    rep = &as->representations[rep_index];
    rep->template_type = type;
    rep->start_number = start_number;
    rep->segment_duration = duration;
    return 0;
}

int dash_segmenter_add_timeline_entry(dash_segmenter_t *seg, uint32_t period_index,
                                      uint32_t as_index, uint32_t rep_index,
                                      uint64_t time, uint64_t duration, uint32_t number)
{
    dash_period_t *period;
    dash_adaptation_set_t *as;
    dash_representation_t *rep;
    dash_segment_timeline_entry_t *entry;
    if (!seg || period_index >= seg->mpd.period_count) return -1;

    period = &seg->mpd.periods[period_index];
    if (as_index >= period->adaptation_count) return -1;

    as = &period->adaptation_sets[as_index];
    if (rep_index >= as->rep_count) return -1;

    rep = &as->representations[rep_index];
    if (rep->timeline.count >= DASH_MAX_SEGMENTS) return -1;

    entry = &rep->timeline.entries[rep->timeline.count];
    entry->time = time;
    entry->duration = duration;
    entry->number = number;
    snprintf(entry->url, sizeof(entry->url), "%s_%u.m4s", seg->segment_prefix, number);
    rep->timeline.count++;
    return 0;
}

int dash_segmenter_set_init_segment(dash_segmenter_t *seg, uint32_t period_index,
                                    uint32_t as_index, uint32_t rep_index, const char *url)
{
    dash_period_t *period;
    dash_adaptation_set_t *as;
    dash_representation_t *rep;
    if (!seg || !url || period_index >= seg->mpd.period_count) return -1;

    period = &seg->mpd.periods[period_index];
    if (as_index >= period->adaptation_count) return -1;

    as = &period->adaptation_sets[as_index];
    if (rep_index >= as->rep_count) return -1;

    rep = &as->representations[rep_index];
    strncpy(rep->init_segment_url, url, DASH_MAX_URL_LEN - 1);
    return 0;
}

int dash_segmenter_set_media_segment(dash_segmenter_t *seg, uint32_t period_index,
                                     uint32_t as_index, uint32_t rep_index,
                                     const char *url)
{
    dash_period_t *period;
    dash_adaptation_set_t *as;
    dash_representation_t *rep;
    if (!seg || !url || period_index >= seg->mpd.period_count) return -1;

    period = &seg->mpd.periods[period_index];
    if (as_index >= period->adaptation_count) return -1;

    as = &period->adaptation_sets[as_index];
    if (rep_index >= as->rep_count) return -1;

    rep = &as->representations[rep_index];
    strncpy(rep->media_segment_url, url, DASH_MAX_URL_LEN - 1);
    return 0;
}

int dash_segmenter_set_utc_timing(dash_segmenter_t *seg,
                                  dash_utc_timing_scheme_t scheme,
                                  const char *source_url)
{
    if (!seg || !source_url) return -1;
    seg->mpd.has_utc_timing = 1;
    strncpy(seg->mpd.utc_timing.source_url, source_url, DASH_MAX_URL_LEN - 1);

    switch (scheme) {
    case DASH_TIMING_NTP:
        strncpy(seg->mpd.utc_timing.scheme_id, "urn:mpeg:dash:utc:ntp:2014", 63);
        break;
    case DASH_TIMING_HTTP:
        strncpy(seg->mpd.utc_timing.scheme_id, "urn:mpeg:dash:utc:http-iso:2014", 63);
        break;
    case DASH_TIMING_GPS:
        strncpy(seg->mpd.utc_timing.scheme_id, "urn:mpeg:dash:utc:gps:2014", 63);
        break;
    default:
        return -1;
    }
    return 0;
}

int dash_segmenter_write_init_segment(dash_segmenter_t *seg, const uint8_t *ftyp,
                                      size_t ftyp_len, const uint8_t *moov, size_t moov_len)
{
    char fullpath[DASH_MAX_URL_LEN];
    if (!seg || !ftyp || !moov) return -1;

    snprintf(fullpath, sizeof(fullpath), "%s/%s.mp4",
             seg->output_dir, seg->init_prefix);
    seg->init_file = fopen(fullpath, "wb");
    if (!seg->init_file) return -1;

    fwrite(ftyp, 1, ftyp_len, seg->init_file);
    fwrite(moov, 1, moov_len, seg->init_file);
    fclose(seg->init_file);
    seg->init_file = NULL;
    return 0;
}

int dash_segmenter_start_fmp4(dash_segmenter_t *seg, const char *filename,
                              uint64_t base_decode_time)
{
    char fullpath[DASH_MAX_URL_LEN];
    if (!seg || !filename) return -1;

    if (seg->segment_file) {
        fclose(seg->segment_file);
    }

    snprintf(fullpath, sizeof(fullpath), "%s/%s", seg->output_dir, filename);
    seg->segment_file = fopen(fullpath, "wb");
    if (!seg->segment_file) return -1;

    return 0;
}

int dash_segmenter_finish_fmp4(dash_segmenter_t *seg)
{
    if (!seg || !seg->segment_file) return -1;
    fclose(seg->segment_file);
    seg->segment_file = NULL;
    seg->sequence_number++;
    return 0;
}

int dash_segmenter_advance_segment(dash_segmenter_t *seg)
{
    if (!seg) return -1;
    seg->current_segment_number++;
    seg->current_time_ms += seg->mpd.max_segment_duration_ms;
    return 0;
}

int dash_segmenter_generate_mpd(dash_segmenter_t *seg, char *buffer, size_t buf_size)
{
    uint32_t pi, ai, ri;
    size_t pos = 0;
    int written;

    if (!seg || !buffer || buf_size == 0) return -1;

    written = snprintf(buffer + pos, buf_size - pos,
                       "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
                       "<MPD xmlns=\"urn:mpeg:dash:schema:mpd:2011\"\n"
                       "     profiles=\"%s\"\n"
                       "     minBufferTime=\"PT%u.%03uS\"\n"
                       "     type=\"%s\"\n",
                       seg->mpd.profiles,
                       seg->mpd.min_buffer_time_ms / 1000,
                       seg->mpd.min_buffer_time_ms % 1000,
                       seg->mpd.is_dynamic ? "dynamic" : "static");
    if (written < 0 || (size_t)written >= buf_size - pos) return -1;
    pos += (size_t)written;

    if (seg->mpd.is_dynamic) {
        written = snprintf(buffer + pos, buf_size - pos,
                           "     availabilityStartTime=\"");
        if (written < 0 || (size_t)written >= buf_size - pos) return -1;
        pos += (size_t)written;

        {
            time_t t = (time_t)(seg->mpd.availability_start_time_ms / 1000);
            struct tm *tm_info = gmtime(&t);
            char time_str[32];
            strftime(time_str, sizeof(time_str), "%Y-%m-%dT%H:%M:%SZ", tm_info);
            written = snprintf(buffer + pos, buf_size - pos, "%s\"\n", time_str);
            if (written < 0 || (size_t)written >= buf_size - pos) return -1;
            pos += (size_t)written;
        }
    }

    if (seg->mpd.has_utc_timing) {
        written = snprintf(buffer + pos, buf_size - pos,
                           "     <UTCTiming schemeIdUri=\"%s\" value=\"%s\"/>\n",
                           seg->mpd.utc_timing.scheme_id,
                           seg->mpd.utc_timing.source_url);
        if (written < 0 || (size_t)written >= buf_size - pos) return -1;
        pos += (size_t)written;
    }

    written = snprintf(buffer + pos, buf_size - pos, "     >\n");
    if (written < 0 || (size_t)written >= buf_size - pos) return -1;
    pos += (size_t)written;

    for (pi = 0; pi < seg->mpd.period_count; pi++) {
        dash_period_t *period = &seg->mpd.periods[pi];

        written = snprintf(buffer + pos, buf_size - pos,
                           "  <Period id=\"%s\"", period->id);
        if (written < 0 || (size_t)written >= buf_size - pos) return -1;
        pos += (size_t)written;

        if (period->start_ms > 0) {
            written = snprintf(buffer + pos, buf_size - pos,
                               " start=\"PT%llu.%03lluS\"",
                               (unsigned long long)period->start_ms / 1000,
                               (unsigned long long)period->start_ms % 1000);
            if (written < 0 || (size_t)written >= buf_size - pos) return -1;
            pos += (size_t)written;
        }

        if (period->duration_ms > 0) {
            written = snprintf(buffer + pos, buf_size - pos,
                               " duration=\"PT%llu.%03lluS\"",
                               (unsigned long long)period->duration_ms / 1000,
                               (unsigned long long)period->duration_ms % 1000);
            if (written < 0 || (size_t)written >= buf_size - pos) return -1;
            pos += (size_t)written;
        }

        written = snprintf(buffer + pos, buf_size - pos, ">\n");
        if (written < 0 || (size_t)written >= buf_size - pos) return -1;
        pos += (size_t)written;

        for (ai = 0; ai < period->adaptation_count; ai++) {
            dash_adaptation_set_t *as = &period->adaptation_sets[ai];
            const char *ct = dash_content_type_string(as->content_type);

            written = snprintf(buffer + pos, buf_size - pos,
                               "    <AdaptationSet id=\"%s\" contentType=\"%s\"",
                               as->id, ct);
            if (written < 0 || (size_t)written >= buf_size - pos) return -1;
            pos += (size_t)written;

            if (as->lang[0]) {
                written = snprintf(buffer + pos, buf_size - pos,
                                   " lang=\"%s\"", as->lang);
                if (written < 0 || (size_t)written >= buf_size - pos) return -1;
                pos += (size_t)written;
            }

            if (as->segment_alignment) {
                written = snprintf(buffer + pos, buf_size - pos,
                                   " segmentAlignment=\"true\"");
                if (written < 0 || (size_t)written >= buf_size - pos) return -1;
                pos += (size_t)written;
            }

            if (as->bitstream_switching) {
                written = snprintf(buffer + pos, buf_size - pos,
                                   " bitstreamSwitching=\"true\"");
                if (written < 0 || (size_t)written >= buf_size - pos) return -1;
                pos += (size_t)written;
            }

            written = snprintf(buffer + pos, buf_size - pos, ">\n");
            if (written < 0 || (size_t)written >= buf_size - pos) return -1;
            pos += (size_t)written;

            for (ri = 0; ri < as->rep_count; ri++) {
                dash_representation_t *rep = &as->representations[ri];

                written = snprintf(buffer + pos, buf_size - pos,
                                   "      <Representation id=\"%s\" bandwidth=\"%u\"",
                                   rep->id, rep->bandwidth);
                if (written < 0 || (size_t)written >= buf_size - pos) return -1;
                pos += (size_t)written;

                if (rep->width > 0 && rep->height > 0) {
                    written = snprintf(buffer + pos, buf_size - pos,
                                       " width=\"%u\" height=\"%u\"",
                                       rep->width, rep->height);
                    if (written < 0 || (size_t)written >= buf_size - pos) return -1;
                    pos += (size_t)written;
                }

                if (rep->codecs[0]) {
                    written = snprintf(buffer + pos, buf_size - pos,
                                       " codecs=\"%s\"", rep->codecs);
                    if (written < 0 || (size_t)written >= buf_size - pos) return -1;
                    pos += (size_t)written;
                }

                if (rep->mime_type[0]) {
                    written = snprintf(buffer + pos, buf_size - pos,
                                       " mimeType=\"%s\"", rep->mime_type);
                    if (written < 0 || (size_t)written >= buf_size - pos) return -1;
                    pos += (size_t)written;
                }

                written = snprintf(buffer + pos, buf_size - pos, ">\n");
                if (written < 0 || (size_t)written >= buf_size - pos) return -1;
                pos += (size_t)written;

                if (rep->init_segment_url[0]) {
                    if (rep->template_type == DASH_TEMPLATE_NUMBER) {
                        written = snprintf(buffer + pos, buf_size - pos,
                                           "        <SegmentTemplate timescale=\"%u\"\n"
                                           "                        initialization=\"%s\"\n"
                                           "                        media=\"%s\"\n"
                                           "                        startNumber=\"%u\">\n"
                                           "          <SegmentTimeline>\n"
                                           "            <S t=\"0\" d=\"%u\" r=\"-1\"/>\n"
                                           "          </SegmentTimeline>\n"
                                           "        </SegmentTemplate>\n",
                                           rep->timescale,
                                           rep->init_segment_url,
                                           rep->media_segment_url,
                                           rep->start_number,
                                           rep->segment_duration);
                    } else {
                        written = snprintf(buffer + pos, buf_size - pos,
                                           "        <SegmentList>\n"
                                           "          <Initialization sourceURL=\"%s\"/>\n",
                                           rep->init_segment_url);
                    }
                    if (written < 0 || (size_t)written >= buf_size - pos) return -1;
                    pos += (size_t)written;

                    if (rep->template_type == DASH_TEMPLATE_TIME && rep->timeline.count > 0) {
                        uint32_t ti;
                        for (ti = 0; ti < rep->timeline.count; ti++) {
                            dash_segment_timeline_entry_t *e = &rep->timeline.entries[ti];
                            written = snprintf(buffer + pos, buf_size - pos,
                                               "          <SegmentURL media=\"%s\"/>\n",
                                               e->url);
                            if (written < 0 || (size_t)written >= buf_size - pos) return -1;
                            pos += (size_t)written;
                        }
                        written = snprintf(buffer + pos, buf_size - pos,
                                           "        </SegmentList>\n");
                        if (written < 0 || (size_t)written >= buf_size - pos) return -1;
                        pos += (size_t)written;
                    }
                } else if (rep->media_segment_url[0]) {
                    written = snprintf(buffer + pos, buf_size - pos,
                                       "        <BaseURL>%s</BaseURL>\n",
                                       rep->media_segment_url);
                    if (written < 0 || (size_t)written >= buf_size - pos) return -1;
                    pos += (size_t)written;
                }

                written = snprintf(buffer + pos, buf_size - pos,
                                   "      </Representation>\n");
                if (written < 0 || (size_t)written >= buf_size - pos) return -1;
                pos += (size_t)written;
            }

            written = snprintf(buffer + pos, buf_size - pos, "    </AdaptationSet>\n");
            if (written < 0 || (size_t)written >= buf_size - pos) return -1;
            pos += (size_t)written;
        }

        written = snprintf(buffer + pos, buf_size - pos, "  </Period>\n");
        if (written < 0 || (size_t)written >= buf_size - pos) return -1;
        pos += (size_t)written;
    }

    written = snprintf(buffer + pos, buf_size - pos, "</MPD>\n");
    if (written < 0 || (size_t)written >= buf_size - pos) return -1;
    pos += (size_t)written;

    return (int)pos;
}

int dash_segmenter_write_mpd_file(dash_segmenter_t *seg, const char *filename)
{
    static char buffer[DASH_MPD_MAX_SIZE];
    char fullpath[DASH_MAX_URL_LEN];
    FILE *f;
    int len;

    if (!seg || !filename) return -1;
    len = dash_segmenter_generate_mpd(seg, buffer, sizeof(buffer));
    if (len <= 0) return -1;

    snprintf(fullpath, sizeof(fullpath), "%s/%s", seg->output_dir, filename);
    f = fopen(fullpath, "w");
    if (!f) return -1;

    fwrite(buffer, 1, (size_t)len, f);
    fclose(f);
    return 0;
}

const char *dash_content_type_string(dash_content_type_t type)
{
    switch (type) {
    case DASH_CONTENT_VIDEO:    return "video";
    case DASH_CONTENT_AUDIO:    return "audio";
    case DASH_CONTENT_SUBTITLE: return "subtitle";
    case DASH_CONTENT_TEXT:     return "text";
    default:                    return "unknown";
    }
}

const char *dash_template_type_string(dash_template_type_t type)
{
    switch (type) {
    case DASH_TEMPLATE_NUMBER: return "Number";
    case DASH_TEMPLATE_TIME:   return "Time";
    default:                   return "Unknown";
    }
}
