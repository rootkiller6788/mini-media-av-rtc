#include "audio_filter.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static float sinc(float x)
{
    if (fabsf(x) < 1e-9f) return 1.0f;
    return sinf((float)M_PI * x) / ((float)M_PI * x);
}

static void fir_compute_coeffs(float *coeffs, int num_taps, float cutoff_norm,
                                FirFilterType type, float low_norm, float high_norm)
{
    int half = (num_taps - 1) / 2;

    for (int i = 0; i < num_taps; i++) {
        int idx = i - half;
        float h;

        if (type == FIR_BANDPASS) {
            h = 2.0f * high_norm * sinc(2.0f * high_norm * idx)
                - 2.0f * low_norm * sinc(2.0f * low_norm * idx);
        } else if (type == FIR_HIGHPASS) {
            if (idx == 0) h = 1.0f - 2.0f * cutoff_norm;
            else h = -2.0f * cutoff_norm * sinc(2.0f * cutoff_norm * idx);
        } else {
            h = 2.0f * cutoff_norm * sinc(2.0f * cutoff_norm * idx);
        }

        float win = 0.54f - 0.46f * cosf(2.0f * (float)M_PI * i / (num_taps - 1));
        coeffs[i] = h * win;
    }
}

int fir_design(FirFilter *fir, int num_taps, float cutoff, float sample_rate, FirFilterType type)
{
    if (!fir || num_taps < 3 || cutoff <= 0.0f || cutoff >= sample_rate / 2.0f) return -1;
    if (num_taps % 2 == 0) num_taps++;

    fir->num_taps = num_taps;
    fir->coeffs   = (float *)malloc((size_t)num_taps * sizeof(float));
    fir->delay_line = (float *)calloc((size_t)num_taps, sizeof(float));
    fir->delay_index = 0;
    if (!fir->coeffs || !fir->delay_line) { fir_free(fir); return -1; }

    float norm_cutoff = cutoff / sample_rate;
    fir_compute_coeffs(fir->coeffs, num_taps, norm_cutoff, type, 0.0f, 0.0f);
    return 0;
}

int fir_design_bp(FirFilter *fir, int num_taps, float low_cut, float high_cut, float sample_rate)
{
    if (!fir || num_taps < 3 || low_cut <= 0.0f || high_cut >= sample_rate / 2.0f) return -1;
    if (low_cut >= high_cut) return -1;
    if (num_taps % 2 == 0) num_taps++;

    fir->num_taps = num_taps;
    fir->coeffs   = (float *)malloc((size_t)num_taps * sizeof(float));
    fir->delay_line = (float *)calloc((size_t)num_taps, sizeof(float));
    fir->delay_index = 0;
    if (!fir->coeffs || !fir->delay_line) { fir_free(fir); return -1; }

    float low_norm  = low_cut / sample_rate;
    float high_norm = high_cut / sample_rate;
    fir_compute_coeffs(fir->coeffs, num_taps, 0.0f, FIR_BANDPASS, low_norm, high_norm);
    return 0;
}

void fir_free(FirFilter *fir)
{
    if (fir) {
        free(fir->coeffs);
        free(fir->delay_line);
        fir->coeffs     = NULL;
        fir->delay_line = NULL;
        fir->num_taps   = 0;
        fir->delay_index = 0;
    }
}

int fir_process(const FirFilter *fir, const float *input, float *output, int num_samples)
{
    if (!fir || !fir->coeffs || !fir->delay_line || !input || !output) return -1;

    int *idx = (int *)&fir->delay_index;
    for (int n = 0; n < num_samples; n++) {
        fir->delay_line[*idx] = input[n];
        float acc = 0.0f;
        for (int k = 0; k < fir->num_taps; k++) {
            int d_idx = (*idx - k + fir->num_taps) % fir->num_taps;
            acc += fir->coeffs[k] * fir->delay_line[d_idx];
        }
        output[n] = acc;
        *idx = (*idx + 1) % fir->num_taps;
    }
    return 0;
}

int biquad_design(BiquadFilter *bq, BiquadType type, float freq, float Q, float gain_db, float sample_rate)
{
    if (!bq || freq <= 0.0f || freq >= sample_rate / 2.0f) return -1;

    biquad_reset(bq);
    float w0 = 2.0f * (float)M_PI * freq / sample_rate;
    float cos_w0 = cosf(w0);
    float sin_w0 = sinf(w0);
    float alpha = sin_w0 / (2.0f * Q);
    float A = powf(10.0f, gain_db / 40.0f);

    switch (type) {
    case BIQUAD_LOWPASS: {
        float a0_inv = 1.0f / (1.0f + alpha);
        bq->b0 = (1.0f - cos_w0) / 2.0f * a0_inv;
        bq->b1 = (1.0f - cos_w0) * a0_inv;
        bq->b2 = (1.0f - cos_w0) / 2.0f * a0_inv;
        bq->a1 = -2.0f * cos_w0 * a0_inv;
        bq->a2 = (1.0f - alpha) * a0_inv;
        break;
    }
    case BIQUAD_HIGHPASS: {
        float a0_inv = 1.0f / (1.0f + alpha);
        bq->b0 = (1.0f + cos_w0) / 2.0f * a0_inv;
        bq->b1 = -(1.0f + cos_w0) * a0_inv;
        bq->b2 = (1.0f + cos_w0) / 2.0f * a0_inv;
        bq->a1 = -2.0f * cos_w0 * a0_inv;
        bq->a2 = (1.0f - alpha) * a0_inv;
        break;
    }
    case BIQUAD_BANDPASS: {
        float a0_inv = 1.0f / (1.0f + alpha);
        bq->b0 = sin_w0 / 2.0f * a0_inv;
        bq->b1 = 0.0f;
        bq->b2 = -sin_w0 / 2.0f * a0_inv;
        bq->a1 = -2.0f * cos_w0 * a0_inv;
        bq->a2 = (1.0f - alpha) * a0_inv;
        break;
    }
    case BIQUAD_NOTCH: {
        float a0_inv = 1.0f / (1.0f + alpha);
        bq->b0 = a0_inv;
        bq->b1 = -2.0f * cos_w0 * a0_inv;
        bq->b2 = a0_inv;
        bq->a1 = -2.0f * cos_w0 * a0_inv;
        bq->a2 = (1.0f - alpha) * a0_inv;
        break;
    }
    case BIQUAD_PEAKING: {
        float a0_inv = 1.0f / (1.0f + alpha / A);
        bq->b0 = (1.0f + alpha * A) * a0_inv;
        bq->b1 = -2.0f * cos_w0 * a0_inv;
        bq->b2 = (1.0f - alpha * A) * a0_inv;
        bq->a1 = -2.0f * cos_w0 * a0_inv;
        bq->a2 = (1.0f - alpha / A) * a0_inv;
        break;
    }
    case BIQUAD_LOWSHELF: {
        float a0_inv = 1.0f / ((A + 1.0f) + (A - 1.0f) * cos_w0 + 2.0f * sqrtf(A) * alpha);
        float sqA = 2.0f * sqrtf(A) * alpha;
        bq->b0 = A * ((A + 1.0f) - (A - 1.0f) * cos_w0 + sqA) * a0_inv;
        bq->b1 = 2.0f * A * ((A - 1.0f) - (A + 1.0f) * cos_w0) * a0_inv;
        bq->b2 = A * ((A + 1.0f) - (A - 1.0f) * cos_w0 - sqA) * a0_inv;
        bq->a1 = -2.0f * ((A - 1.0f) + (A + 1.0f) * cos_w0) * a0_inv;
        bq->a2 = ((A + 1.0f) + (A - 1.0f) * cos_w0 - sqA) * a0_inv;
        break;
    }
    case BIQUAD_HIGHSHELF: {
        float a0_inv = 1.0f / ((A + 1.0f) - (A - 1.0f) * cos_w0 + 2.0f * sqrtf(A) * alpha);
        float sqA = 2.0f * sqrtf(A) * alpha;
        bq->b0 = A * ((A + 1.0f) + (A - 1.0f) * cos_w0 + sqA) * a0_inv;
        bq->b1 = -2.0f * A * ((A - 1.0f) + (A + 1.0f) * cos_w0) * a0_inv;
        bq->b2 = A * ((A + 1.0f) + (A - 1.0f) * cos_w0 - sqA) * a0_inv;
        bq->a1 = 2.0f * ((A - 1.0f) - (A + 1.0f) * cos_w0) * a0_inv;
        bq->a2 = ((A + 1.0f) - (A - 1.0f) * cos_w0 - sqA) * a0_inv;
        break;
    }
    default:
        return -1;
    }
    return 0;
}

int biquad_process(BiquadFilter *bq, const float *input, float *output, int num_samples)
{
    if (!bq || !input || !output) return -1;
    for (int i = 0; i < num_samples; i++) {
        float x0 = input[i];
        float y0 = bq->b0 * x0 + bq->b1 * bq->x1 + bq->b2 * bq->x2
                   - bq->a1 * bq->y1 - bq->a2 * bq->y2;
        bq->x2 = bq->x1;
        bq->x1 = x0;
        bq->y2 = bq->y1;
        bq->y1 = y0;
        output[i] = y0;
    }
    return 0;
}

void biquad_reset(BiquadFilter *bq)
{
    if (bq) {
        bq->b0 = bq->b1 = bq->b2 = 0.0f;
        bq->a1 = bq->a2 = 0.0f;
        bq->x1 = bq->x2 = 0.0f;
        bq->y1 = bq->y2 = 0.0f;
    }
}

int conv_reverb_init(ConvReverb *rev, const float *ir, int ir_len)
{
    if (!rev || !ir || ir_len <= 0) return -1;
    rev->ir_len = ir_len;
    rev->impulse_response = (float *)malloc((size_t)ir_len * sizeof(float));
    rev->delay_line = (float *)calloc((size_t)ir_len, sizeof(float));
    rev->delay_index = 0;
    if (!rev->impulse_response || !rev->delay_line) { conv_reverb_free(rev); return -1; }
    memcpy(rev->impulse_response, ir, (size_t)ir_len * sizeof(float));
    return 0;
}

void conv_reverb_free(ConvReverb *rev)
{
    if (rev) {
        free(rev->impulse_response);
        free(rev->delay_line);
        rev->impulse_response = NULL;
        rev->delay_line = NULL;
        rev->ir_len = 0;
    }
}

int conv_reverb_process(ConvReverb *rev, const float *input, float *output, int num_samples)
{
    if (!rev || !rev->impulse_response || !input || !output) return -1;
    for (int n = 0; n < num_samples; n++) {
        rev->delay_line[rev->delay_index] = input[n];
        float acc = 0.0f;
        for (int k = 0; k < rev->ir_len; k++) {
            int d_idx = (rev->delay_index - k + rev->ir_len) % rev->ir_len;
            acc += rev->impulse_response[k] * rev->delay_line[d_idx];
        }
        output[n] = acc;
        rev->delay_index = (rev->delay_index + 1) % rev->ir_len;
    }
    return 0;
}

int echo_delay_init(EchoDelay *echo, int delay_ms, float sample_rate, float feedback, float wet_mix)
{
    if (!echo || delay_ms <= 0) return -1;

    echo->delay_samples = (int)(delay_ms * sample_rate / 1000.0f);
    echo->feedback      = feedback;
    echo->wet_mix       = wet_mix;
    echo->dry_mix       = 1.0f - wet_mix;
    echo->write_index   = 0;
    echo->buffer_len    = echo->delay_samples + 1;

    echo->delay_buffer = (float *)calloc((size_t)echo->buffer_len, sizeof(float));
    if (!echo->delay_buffer) return -1;
    return 0;
}

void echo_delay_free(EchoDelay *echo)
{
    if (echo) {
        free(echo->delay_buffer);
        echo->delay_buffer = NULL;
        echo->buffer_len = 0;
    }
}

int echo_delay_process(EchoDelay *echo, const float *input, float *output, int num_samples)
{
    if (!echo || !echo->delay_buffer || !input || !output) return -1;
    for (int n = 0; n < num_samples; n++) {
        int read_idx = (echo->write_index - echo->delay_samples + echo->buffer_len) % echo->buffer_len;
        float delayed = echo->delay_buffer[read_idx];

        output[n] = echo->dry_mix * input[n] + echo->wet_mix * delayed;

        echo->delay_buffer[echo->write_index] = input[n] + echo->feedback * delayed;
        echo->write_index = (echo->write_index + 1) % echo->buffer_len;
    }
    return 0;
}

int apply_gain(const float *input, float *output, int num_samples, float gain_db)
{
    if (!input || !output || num_samples <= 0) return -1;
    float gain_lin = powf(10.0f, gain_db / 20.0f);
    for (int i = 0; i < num_samples; i++) {
        output[i] = input[i] * gain_lin;
    }
    return 0;
}

int apply_limiter(float *samples, int num_samples, float threshold_db, float ceiling_db)
{
    if (!samples || num_samples <= 0) return -1;

    float threshold = powf(10.0f, threshold_db / 20.0f);
    float ceiling   = powf(10.0f, ceiling_db / 20.0f);
    float make_up   = ceiling / threshold;

    for (int i = 0; i < num_samples; i++) {
        float abs_val = fabsf(samples[i]);
        if (abs_val > threshold) {
            float gain_reduction = threshold / abs_val;
            samples[i] *= gain_reduction * make_up;
        } else {
            samples[i] *= make_up;
        }
    }
    return 0;
}
