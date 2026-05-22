#include "resample_mix.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

int resample_linear(const float *input, int in_len, float *output, int out_len)
{
    if (!input || !output || in_len < 2 || out_len < 2) return -1;

    float step = (float)(in_len - 1) / (float)(out_len - 1);

    for (int i = 0; i < out_len; i++) {
        float pos = step * i;
        int idx   = (int)pos;
        float frac = pos - (float)idx;

        if (idx >= in_len - 1) {
            output[i] = input[in_len - 1];
        } else {
            output[i] = input[idx] * (1.0f - frac) + input[idx + 1] * frac;
        }
    }
    return 0;
}

static float sinc_normalized(float x)
{
    if (fabsf(x) < 1e-9f) return 1.0f;
    return sinf((float)M_PI * x) / ((float)M_PI * x);
}

int resample_sinc(const float *input, int in_len, float *output, int out_len, int kernel_size)
{
    if (!input || !output || in_len < 2 || out_len < 2 || kernel_size < 3) return -1;

    float ratio = (float)in_len / (float)out_len;
    int half_kernel = kernel_size / 2;

    for (int i = 0; i < out_len; i++) {
        float center = (float)i * ratio;
        float sum = 0.0f;
        float weight_sum = 0.0f;

        for (int k = -half_kernel; k <= half_kernel; k++) {
            float src_idx = center + (float)k;
            if (src_idx >= 0.0f && src_idx < (float)in_len) {
                float w = sinc_normalized(src_idx - center);
                float hann = 0.5f * (1.0f + cosf((float)M_PI * (float)k / half_kernel));
                w *= hann;
                int idx = (int)src_idx;
                float frac = src_idx - (float)idx;
                float val;
                if (idx >= in_len - 1) {
                    val = input[in_len - 1];
                } else {
                    val = input[idx] * (1.0f - frac) + input[idx + 1] * frac;
                }
                sum += val * w;
                weight_sum += w;
            }
        }
        output[i] = (weight_sum > 1e-9f) ? sum / weight_sum : 0.0f;
    }
    return 0;
}

int resample_lanczos(const float *input, int in_len, float *output, int out_len, int a)
{
    if (!input || !output || in_len < 2 || out_len < 2) return -1;
    if (a < 2) a = 3;

    float ratio = (float)in_len / (float)out_len;

    for (int i = 0; i < out_len; i++) {
        float center = (float)i * ratio;
        float sum = 0.0f;
        float weight_sum = 0.0f;

        int left  = (int)center - a + 1;
        int right = (int)center + a;
        if (left < 0) left = 0;
        if (right >= in_len) right = in_len - 1;

        for (int j = left; j <= right; j++) {
            float x = (float)j - center;
            float w;
            if (fabsf(x) < 1e-9f) {
                w = 1.0f;
            } else if (fabsf(x) >= (float)a) {
                w = 0.0f;
            } else {
                w = (float)a * sinf((float)M_PI * x) * sinf((float)M_PI * x / (float)a)
                    / ((float)M_PI * (float)M_PI * x * x);
            }
            sum += input[j] * w;
            weight_sum += w;
        }
        output[i] = (weight_sum > 1e-9f) ? sum / weight_sum : 0.0f;
    }
    return 0;
}

int src_convert(const float *input, int in_len, int in_rate,
                 float *output, int *out_len, int out_rate, InterpMethod method)
{
    if (!input || !output || !out_len || in_len <= 0 || in_rate <= 0 || out_rate <= 0) return -1;

    float ratio = (float)out_rate / (float)in_rate;
    int needed_len = (int)((float)in_len * ratio) + 1;
    if (needed_len > *out_len) return -2;

    switch (method) {
    case INTERP_LINEAR:
        return resample_linear(input, in_len, output, needed_len);
    case INTERP_SINC:
        return resample_sinc(input, in_len, output, needed_len, 32);
    case INTERP_LANCZOS:
        return resample_lanczos(input, in_len, output, needed_len, 3);
    default:
        return -1;
    }
}

int channel_stereo_to_mono(const float *stereo, int num_samples, float *mono)
{
    if (!stereo || !mono || num_samples <= 0) return -1;
    for (int i = 0; i < num_samples; i++) {
        mono[i] = (stereo[2 * i] + stereo[2 * i + 1]) * 0.5f;
    }
    return 0;
}

int channel_mono_to_stereo(const float *mono, int num_samples, float *stereo)
{
    if (!mono || !stereo || num_samples <= 0) return -1;
    for (int i = 0; i < num_samples; i++) {
        stereo[2 * i]     = mono[i];
        stereo[2 * i + 1] = mono[i];
    }
    return 0;
}

int multitrack_init(MultiTrack *mt, int num_tracks, int track_length)
{
    if (!mt || num_tracks <= 0 || track_length <= 0) return -1;

    mt->num_tracks   = num_tracks;
    mt->track_length = track_length;
    mt->tracks = (float *)calloc((size_t)(num_tracks * track_length), sizeof(float));
    mt->gains  = (float *)malloc((size_t)num_tracks * sizeof(float));
    if (!mt->tracks || !mt->gains) { multitrack_free(mt); return -1; }

    for (int i = 0; i < num_tracks; i++) {
        mt->gains[i] = 1.0f;
    }
    return 0;
}

void multitrack_free(MultiTrack *mt)
{
    if (mt) {
        free(mt->tracks);
        free(mt->gains);
        mt->tracks = NULL;
        mt->gains  = NULL;
        mt->num_tracks   = 0;
        mt->track_length = 0;
    }
}

int multitrack_set_gain(MultiTrack *mt, int track_index, float gain_db)
{
    if (!mt || track_index < 0 || track_index >= mt->num_tracks) return -1;
    mt->gains[track_index] = powf(10.0f, gain_db / 20.0f);
    return 0;
}

int multitrack_set_track(MultiTrack *mt, int track_index, const float *data)
{
    if (!mt || !data || track_index < 0 || track_index >= mt->num_tracks) return -1;
    int offset = track_index * mt->track_length;
    memcpy(mt->tracks + offset, data, (size_t)mt->track_length * sizeof(float));
    return 0;
}

int multitrack_mixdown(const MultiTrack *mt, float *output, int out_len)
{
    if (!mt || !mt->tracks || !output) return -1;
    if (out_len > mt->track_length) out_len = mt->track_length;

    memset(output, 0, (size_t)out_len * sizeof(float));

    for (int t = 0; t < mt->num_tracks; t++) {
        int offset = t * mt->track_length;
        float gain = mt->gains[t];
        for (int i = 0; i < out_len; i++) {
            output[i] += mt->tracks[offset + i] * gain;
        }
    }
    return 0;
}

int audiobuf_pool_init(AudioBufPool *pool, int capacity)
{
    if (!pool || capacity <= 0) return -1;

    pool->pool = (AudioBuffer *)calloc((size_t)capacity, sizeof(AudioBuffer));
    if (!pool->pool) return -1;

    pool->pool_capacity = capacity;
    pool->pool_size     = 0;
    return 0;
}

void audiobuf_pool_free(AudioBufPool *pool)
{
    if (pool) {
        for (int i = 0; i < pool->pool_capacity; i++) {
            audiobuf_free(&pool->pool[i]);
        }
        free(pool->pool);
        pool->pool = NULL;
        pool->pool_capacity = 0;
        pool->pool_size     = 0;
    }
}

int audiobuf_pool_alloc(AudioBufPool *pool, int num_samples, int num_channels,
                         int sample_rate, PcmFormat format, int *index)
{
    if (!pool || num_samples <= 0 || !index) return -1;

    for (int i = 0; i < pool->pool_capacity; i++) {
        if (!pool->pool[i].data) {
            if (audiobuf_alloc(&pool->pool[i], num_samples, num_channels, sample_rate, format) != 0)
                return -1;
            *index = i;
            if (i >= pool->pool_size) pool->pool_size = i + 1;
            return 0;
        }
    }
    return -2;
}

int audiobuf_pool_free_idx(AudioBufPool *pool, int index)
{
    if (!pool || index < 0 || index >= pool->pool_capacity) return -1;
    audiobuf_free(&pool->pool[index]);
    return 0;
}

int audiobuf_pool_copy_to(AudioBufPool *pool, int src_idx, int dst_idx)
{
    if (!pool || src_idx < 0 || src_idx >= pool->pool_capacity ||
        dst_idx < 0 || dst_idx >= pool->pool_capacity) return -1;
    return audiobuf_copy(&pool->pool[dst_idx], &pool->pool[src_idx]);
}
