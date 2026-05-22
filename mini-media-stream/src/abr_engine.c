#include "abr_engine.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef M_LN2
#define M_LN2 0.693147180559945309417232121458
#endif

int abr_engine_init(abr_engine_t *eng)
{
    if (!eng) return -1;
    memset(eng, 0, sizeof(*eng));
    eng->algorithm = ABR_ALGO_HYBRID;
    eng->current_rep_index = 0;
    eng->previous_rep_index = 0;
    eng->smoothing_alpha = 0.7;
    eng->history_head = 0;
    eng->history_count = 0;
    eng->enabled = 1;
    eng->pending_switch = 0;
    eng->oscillation_threshold = 3;
    eng->estimated_bandwidth_bps = 1000000.0;
    eng->smoothed_bandwidth_bps = 1000000.0;
    eng->buffer_level_sec = 30.0;
    eng->playback_rate = 1.0;

    eng->bola_state.buffer_level_sec = 30.0;
    eng->bola_state.target_buffer_sec = ABR_DEFAULT_BUFFER_TARGET;
    eng->bola_state.max_buffer_sec = ABR_DEFAULT_MAX_BUFFER;
    eng->bola_state.min_buffer_sec = ABR_DEFAULT_MIN_BUFFER;
    eng->bola_state.utility_offset = 5;
    eng->bola_state.segment_duration_sec = 2.0;

    eng->bola_config.beta = 0.9;
    eng->bola_config.safety_factor = 0.75;
    eng->bola_config.smoothing_window = 6;

    return 0;
}

void abr_engine_deinit(abr_engine_t *eng)
{
    if (!eng) return;
}

int abr_engine_add_representation(abr_engine_t *eng, uint32_t bitrate,
                                  uint32_t width, uint32_t height, const char *id)
{
    abr_representation_t *rep;
    if (!eng || eng->rep_count >= ABR_MAX_REPRESENTATIONS) return -1;

    rep = &eng->representations[eng->rep_count];
    rep->bitrate = bitrate;
    rep->width = width;
    rep->height = height;
    rep->index = eng->rep_count;
    if (id) strncpy(rep->id, id, 63);
    eng->rep_count++;
    return 0;
}

int abr_engine_set_algorithm(abr_engine_t *eng, abr_algorithm_t algo)
{
    if (!eng) return -1;
    eng->algorithm = algo;
    return 0;
}

int abr_engine_set_buffer_params(abr_engine_t *eng, uint32_t target_sec,
                                 uint32_t min_sec, uint32_t max_sec)
{
    if (!eng) return -1;
    eng->bola_state.target_buffer_sec = target_sec;
    eng->bola_state.min_buffer_sec = min_sec;
    eng->bola_state.max_buffer_sec = max_sec;
    return 0;
}

int abr_engine_set_bola_config(abr_engine_t *eng, const abr_bola_config_t *config)
{
    if (!eng || !config) return -1;
    memcpy(&eng->bola_config, config, sizeof(abr_bola_config_t));
    return 0;
}

int abr_engine_set_current_buffer(abr_engine_t *eng, double buffer_level_sec)
{
    if (!eng) return -1;
    eng->buffer_level_sec = buffer_level_sec;
    eng->bola_state.buffer_level_sec = buffer_level_sec;
    return 0;
}

int abr_engine_set_playback_rate(abr_engine_t *eng, double rate)
{
    if (!eng) return -1;
    eng->playback_rate = rate;
    return 0;
}

int abr_engine_record_download(abr_engine_t *eng, uint32_t bandwidth_bps,
                               uint64_t size_bytes, uint32_t segment_index)
{
    abr_download_record_t *rec;
    if (!eng) return -1;

    rec = &eng->history[eng->history_head];
    rec->bandwidth_bps = bandwidth_bps;
    rec->segment_size_bytes = size_bytes;
    rec->segment_index = segment_index;
    rec->valid = 1;

    eng->history_head = (eng->history_head + 1) % ABR_SEGMENT_HISTORY_SIZE;
    if (eng->history_count < ABR_SEGMENT_HISTORY_SIZE) {
        eng->history_count++;
    }

    {
        double samples[ABR_SEGMENT_HISTORY_SIZE];
        uint32_t count = 0;
        uint32_t idx = eng->history_head;
        uint32_t i;
        for (i = 0; i < eng->history_count; i++) {
            idx = (idx == 0) ? ABR_SEGMENT_HISTORY_SIZE - 1 : idx - 1;
            if (eng->history[idx].valid) {
                samples[count++] = (double)eng->history[idx].bandwidth_bps;
            }
        }
        if (count > 0) {
            eng->estimated_bandwidth_bps = (double)bandwidth_bps;
            eng->smoothed_bandwidth_bps = abr_smooth_bandwidth(samples, count,
                                                                eng->smoothing_alpha);
        }
    }

    return 0;
}

int abr_engine_record_segment(abr_engine_t *eng, uint32_t rep_index,
                              uint64_t size_bytes, uint64_t download_time_ms)
{
    uint32_t bandwidth;
    if (!eng || download_time_ms == 0) return -1;

    bandwidth = (uint32_t)((size_bytes * 8.0 * 1000.0) / (double)download_time_ms);

    eng->previous_rep_index = eng->current_rep_index;
    eng->current_rep_index = rep_index;

    return abr_engine_record_download(eng, bandwidth, size_bytes, rep_index);
}

uint32_t abr_engine_decide(abr_engine_t *eng)
{
    if (!eng || !eng->enabled) return eng ? eng->current_rep_index : 0;
    if (eng->rep_count == 0) return 0;

    switch (eng->algorithm) {
    case ABR_ALGO_BOLA:
        return (uint32_t)abr_engine_decide_bola(eng);
    case ABR_ALGO_THROUGHPUT:
        return (uint32_t)abr_engine_decide_throughput(eng);
    case ABR_ALGO_RULE_BASED:
        return (uint32_t)abr_engine_decide_rule_based(eng);
    case ABR_ALGO_HYBRID:
    default:
        return (uint32_t)abr_engine_decide_hybrid(eng);
    }
}

int abr_engine_decide_bola(abr_engine_t *eng)
{
    uint32_t best_index = 0;
    double best_score = -1.0;
    uint32_t i;

    if (!eng || eng->rep_count == 0) return -1;

    for (i = 0; i < eng->rep_count; i++) {
        double score = abr_bola_score(eng->bola_state.buffer_level_sec,
                                      eng->representations[i].bitrate,
                                      eng->bola_state.target_buffer_sec,
                                      (double)eng->bola_state.utility_offset,
                                      eng->bola_state.segment_duration_sec);
        if (score > best_score) {
            best_score = score;
            best_index = i;
        }
    }

    if (abr_engine_prevent_oscillation(eng, best_index) != 0) {
        return (int)eng->current_rep_index;
    }

    abr_engine_commit_switch(eng, best_index);
    return (int)best_index;
}

int abr_engine_decide_throughput(abr_engine_t *eng)
{
    uint32_t i;
    uint32_t best_index;

    if (!eng || eng->rep_count == 0) return -1;

    best_index = eng->rep_count - 1;
    for (i = eng->rep_count; i > 0; i--) {
        uint32_t idx = i - 1;
        if ((double)eng->representations[idx].bitrate <=
            eng->smoothed_bandwidth_bps * eng->bola_config.safety_factor) {
            best_index = idx;
            break;
        }
    }

    if (abr_engine_prevent_oscillation(eng, best_index) != 0) {
        return (int)eng->current_rep_index;
    }

    abr_engine_commit_switch(eng, best_index);
    return (int)best_index;
}

int abr_engine_decide_rule_based(abr_engine_t *eng)
{
    double buf;
    uint32_t new_index;
    int direction;

    if (!eng || eng->rep_count == 0) return -1;

    buf = eng->buffer_level_sec;
    new_index = eng->current_rep_index;

    if (buf < (double)eng->bola_state.min_buffer_sec) {
        direction = -1;
        if (new_index > 0) new_index--;
        if (buf < (double)eng->bola_state.min_buffer_sec * 0.5 &&
            new_index > 0) {
            new_index = 0;
        }
    } else if (buf > (double)eng->bola_state.max_buffer_sec) {
        direction = 1;
        if (new_index + 1 < eng->rep_count) new_index++;
    } else {
        direction = 0;
    }

    if (abr_engine_prevent_oscillation(eng, new_index) != 0) {
        return (int)eng->current_rep_index;
    }

    abr_engine_commit_switch(eng, new_index);
    return (int)new_index;
}

int abr_engine_decide_hybrid(abr_engine_t *eng)
{
    int bola_result;
    int tp_result;
    uint32_t hybrid_index;

    if (!eng || eng->rep_count == 0) return -1;

    bola_result = abr_engine_decide_bola(eng);
    tp_result = abr_engine_decide_throughput(eng);

    if (eng->buffer_level_sec < (double)eng->bola_state.min_buffer_sec) {
        hybrid_index = (uint32_t)bola_result;
        if ((uint32_t)tp_result < hybrid_index) {
            hybrid_index = (uint32_t)tp_result;
        }
    } else if (eng->buffer_level_sec > (double)eng->bola_state.max_buffer_sec) {
        hybrid_index = (uint32_t)bola_result;
    } else {
        hybrid_index = (uint32_t)bola_result;
        if (((uint32_t)tp_result) < hybrid_index) {
            hybrid_index = (uint32_t)tp_result;
        }
    }

    if (abr_engine_prevent_oscillation(eng, hybrid_index) != 0) {
        return (int)eng->current_rep_index;
    }

    abr_engine_commit_switch(eng, hybrid_index);
    return (int)hybrid_index;
}

int abr_engine_should_switch(abr_engine_t *eng, abr_switch_direction_t *direction)
{
    if (!eng || !direction) return -1;

    if (eng->buffer_level_sec < (double)eng->bola_state.min_buffer_sec) {
        *direction = ABR_SWITCH_DOWN;
        return 1;
    } else if (eng->buffer_level_sec > (double)eng->bola_state.max_buffer_sec) {
        *direction = ABR_SWITCH_UP;
        return 1;
    }

    *direction = ABR_SWITCH_KEEP;
    return 0;
}

int abr_engine_prevent_oscillation(abr_engine_t *eng, uint32_t new_rep_index)
{
    int32_t diff;
    (void)new_rep_index;

    if (!eng) return -1;
    diff = (int32_t)new_rep_index - (int32_t)eng->current_rep_index;

    if (diff == 0) return 0;

    eng->switch_history[eng->switch_history_len % 16] = (double)diff;
    eng->switch_history_len++;

    if (eng->switch_history_len >= (uint32_t)eng->oscillation_threshold * 2) {
        uint32_t changes = 0;
        uint32_t i;
        double prev = eng->switch_history[0];
        for (i = 1; i < eng->switch_history_len && i < 16; i++) {
            if (eng->switch_history[i] * prev < 0) changes++;
            prev = eng->switch_history[i];
        }
        if (changes >= eng->oscillation_threshold) {
            eng->switch_history_len = 0;
            return -1;
        }
    }

    return 0;
}

int abr_engine_commit_switch(abr_engine_t *eng, uint32_t new_rep_index)
{
    if (!eng || new_rep_index >= eng->rep_count) return -1;
    if (new_rep_index != eng->current_rep_index) {
        if (new_rep_index > eng->current_rep_index) {
            eng->switch_up_count++;
        } else {
            eng->switch_down_count++;
        }
    }
    eng->previous_rep_index = eng->current_rep_index;
    eng->current_rep_index = new_rep_index;
    return 0;
}

int abr_engine_add_cdn_server(abr_engine_t *eng, const char *base_url,
                              uint32_t bandwidth_bps, uint8_t priority)
{
    abr_cdn_server_t *cdn;
    if (!eng || !base_url || eng->cdn_count >= ABR_MAX_CDN_SERVERS) return -1;

    cdn = &eng->cdn_servers[eng->cdn_count];
    memset(cdn, 0, sizeof(*cdn));
    strncpy(cdn->base_url, base_url, 255);
    cdn->bandwidth_bps = bandwidth_bps;
    cdn->priority = priority;
    cdn->available = 1;
    cdn->response_time_ms = 100;
    eng->cdn_count++;
    return 0;
}

int abr_engine_select_cdn(abr_engine_t *eng)
{
    uint32_t best_idx = 0;
    double best_score = -1.0;
    uint32_t i;

    if (!eng || eng->cdn_count == 0) return -1;

    for (i = 0; i < eng->cdn_count; i++) {
        abr_cdn_server_t *cdn = &eng->cdn_servers[i];
        if (!cdn->available) continue;
        double score = (double)cdn->bandwidth_bps * (double)cdn->priority /
                       ((double)cdn->response_time_ms + 1.0);
        if (score > best_score) {
            best_score = score;
            best_idx = i;
        }
    }

    eng->active_cdn_index = best_idx;
    return (int)best_idx;
}

int abr_engine_switch_cdn(abr_engine_t *eng, uint32_t cdn_index)
{
    if (!eng || cdn_index >= eng->cdn_count) return -1;
    if (!eng->cdn_servers[cdn_index].available) return -1;
    eng->active_cdn_index = cdn_index;
    return 0;
}

int abr_engine_mark_cdn_slow(abr_engine_t *eng, uint32_t cdn_index)
{
    if (!eng || cdn_index >= eng->cdn_count) return -1;
    eng->cdn_servers[cdn_index].response_time_ms *= 2;
    if (eng->cdn_servers[cdn_index].response_time_ms > 10000) {
        eng->cdn_servers[cdn_index].available = 0;
    }
    return 0;
}

int abr_engine_get_current_bitrate(abr_engine_t *eng, uint32_t *bitrate)
{
    if (!eng || !bitrate || eng->current_rep_index >= eng->rep_count) return -1;
    *bitrate = eng->representations[eng->current_rep_index].bitrate;
    return 0;
}

uint32_t abr_engine_get_estimated_bandwidth(abr_engine_t *eng)
{
    if (!eng) return 0;
    return (uint32_t)eng->smoothed_bandwidth_bps;
}

int abr_engine_get_representation(abr_engine_t *eng, uint32_t index,
                                  abr_representation_t *rep)
{
    if (!eng || !rep || index >= eng->rep_count) return -1;
    memcpy(rep, &eng->representations[index], sizeof(abr_representation_t));
    return 0;
}

double abr_smooth_bandwidth(const double *samples, uint32_t count, double alpha)
{
    double smoothed;
    uint32_t i;
    if (!samples || count == 0) return 0.0;
    smoothed = samples[0];
    for (i = 1; i < count; i++) {
        smoothed = alpha * smoothed + (1.0 - alpha) * samples[i];
    }
    return smoothed;
}

double abr_bola_score(double buffer_level, uint32_t bitrate, uint32_t target_buffer,
                      double utility_offset, double segment_duration)
{
    double v;
    double p;
    (void)segment_duration;

    v = (target_buffer + utility_offset) /
        (buffer_level + utility_offset);

    p = (double)bitrate * v / (M_LN2 * (double)bitrate + 1.0);

    return v * (double)bitrate - p * log(p / (double)bitrate + 1.0);
}

const char *abr_algo_string(abr_algorithm_t algo)
{
    switch (algo) {
    case ABR_ALGO_BOLA:       return "BOLA";
    case ABR_ALGO_THROUGHPUT: return "Throughput";
    case ABR_ALGO_RULE_BASED: return "RuleBased";
    case ABR_ALGO_HYBRID:     return "Hybrid";
    default:                  return "Unknown";
    }
}
