#include "fft_core.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

int next_pow2(int n)
{
    int p = 1;
    while (p < n) p <<= 1;
    return p;
}

int bit_reverse(int x, int bits)
{
    int rev = 0;
    for (int i = 0; i < bits; i++) {
        rev = (rev << 1) | (x & 1);
        x >>= 1;
    }
    return rev;
}

int fft(Complex *x, int n)
{
    if (!x || n < 2 || (n & (n - 1)) != 0) return -1;

    int bits = 0;
    int temp = n;
    while (temp >>= 1) bits++;

    for (int i = 0; i < n; i++) {
        int j = bit_reverse(i, bits);
        if (j > i) {
            Complex t = x[i];
            x[i] = x[j];
            x[j] = t;
        }
    }

    for (int len = 2; len <= n; len <<= 1) {
        float angle = -2.0f * (float)M_PI / len;
        Complex w = { cosf(angle), sinf(angle) };
        for (int i = 0; i < n; i += len) {
            Complex wlen = { 1.0f, 0.0f };
            int half = len >> 1;
            for (int j = 0; j < half; j++) {
                Complex u = x[i + j];
                Complex v = {
                    wlen.re * x[i + j + half].re - wlen.im * x[i + j + half].im,
                    wlen.re * x[i + j + half].im + wlen.im * x[i + j + half].re
                };
                x[i + j].re = u.re + v.re;
                x[i + j].im = u.im + v.im;
                x[i + j + half].re = u.re - v.re;
                x[i + j + half].im = u.im - v.im;
                Complex next = {
                    wlen.re * w.re - wlen.im * w.im,
                    wlen.re * w.im + wlen.im * w.re
                };
                wlen = next;
            }
        }
    }
    return 0;
}

int ifft(Complex *x, int n)
{
    if (!x || n < 2) return -1;

    for (int i = 0; i < n; i++) {
        x[i].im = -x[i].im;
    }
    fft(x, n);
    for (int i = 0; i < n; i++) {
        x[i].re /= n;
        x[i].im = -x[i].im / n;
    }
    return 0;
}

int fft_real(const float *real_input, Complex *output, int n)
{
    if (!real_input || !output || n < 2) return -1;
    for (int i = 0; i < n; i++) {
        output[i].re = real_input[i];
        output[i].im = 0.0f;
    }
    return fft(output, n);
}

int fft_magnitude(const Complex *fft_result, float *magnitude, int n)
{
    if (!fft_result || !magnitude || n <= 0) return -1;
    int half = n / 2 + 1;
    for (int i = 0; i < half; i++) {
        magnitude[i] = sqrtf(fft_result[i].re * fft_result[i].re +
                             fft_result[i].im * fft_result[i].im);
    }
    return half;
}

int fft_phase(const Complex *fft_result, float *phase, int n)
{
    if (!fft_result || !phase || n <= 0) return -1;
    int half = n / 2 + 1;
    for (int i = 0; i < half; i++) {
        phase[i] = atan2f(fft_result[i].im, fft_result[i].re);
    }
    return half;
}

int spectrum_compute(Spectrum *spec, const Complex *fft_result, int n)
{
    if (!spec || !fft_result || n <= 0) return -1;
    int half = n / 2 + 1;
    spec->num_bins = half;
    spec->magnitude = (float *)malloc((size_t)half * sizeof(float));
    spec->phase     = (float *)malloc((size_t)half * sizeof(float));
    if (!spec->magnitude || !spec->phase) { spectrum_free(spec); return -1; }
    fft_magnitude(fft_result, spec->magnitude, n);
    fft_phase(fft_result, spec->phase, n);
    return 0;
}

void spectrum_free(Spectrum *spec)
{
    if (spec) {
        free(spec->magnitude);
        free(spec->phase);
        spec->magnitude = NULL;
        spec->phase = NULL;
        spec->num_bins = 0;
    }
}

int window_generate(float *window, int size, WindowType type)
{
    if (!window || size <= 0) return -1;

    switch (type) {
    case WINDOW_RECTANGLE:
        for (int i = 0; i < size; i++) window[i] = 1.0f;
        break;
    case WINDOW_HANN:
        for (int i = 0; i < size; i++)
            window[i] = 0.5f * (1.0f - cosf(2.0f * (float)M_PI * i / (size - 1)));
        break;
    case WINDOW_HAMMING:
        for (int i = 0; i < size; i++)
            window[i] = 0.54f - 0.46f * cosf(2.0f * (float)M_PI * i / (size - 1));
        break;
    case WINDOW_BLACKMAN:
        for (int i = 0; i < size; i++)
            window[i] = 0.42f - 0.5f * cosf(2.0f * (float)M_PI * i / (size - 1))
                        + 0.08f * cosf(4.0f * (float)M_PI * i / (size - 1));
        break;
    default:
        return -1;
    }
    return 0;
}

int window_apply(float *signal, int size, WindowType type)
{
    if (!signal || size <= 0) return -1;
    float *win = (float *)malloc((size_t)size * sizeof(float));
    if (!win) return -1;
    window_generate(win, size, type);
    for (int i = 0; i < size; i++) {
        signal[i] *= win[i];
    }
    free(win);
    return 0;
}

int stft_init(Spectrogram *sg, int num_frames, int frame_size, int hop_size,
              int sample_rate, WindowType window_type)
{
    if (!sg || num_frames <= 0 || frame_size <= 0 || hop_size <= 0) return -1;

    sg->frames = (float **)malloc((size_t)num_frames * sizeof(float *));
    if (!sg->frames) return -1;

    for (int i = 0; i < num_frames; i++) {
        sg->frames[i] = (float *)calloc((size_t)frame_size, sizeof(float));
        if (!sg->frames[i]) { stft_free(sg); return -1; }
    }

    sg->num_frames  = num_frames;
    sg->frame_size  = frame_size;
    sg->hop_size    = hop_size;
    sg->sample_rate = sample_rate;
    sg->window_type = window_type;
    return 0;
}

int stft_compute(const float *signal, int signal_len, Spectrogram *sg)
{
    if (!signal || signal_len <= 0 || !sg) return -1;

    int fft_n = next_pow2(sg->frame_size);
    Complex *cplx = (Complex *)malloc((size_t)fft_n * sizeof(Complex));
    float   *frame = (float *)malloc((size_t)sg->frame_size * sizeof(float));
    float   *window = (float *)malloc((size_t)sg->frame_size * sizeof(float));
    if (!cplx || !frame || !window) { free(cplx); free(frame); free(window); return -1; }

    window_generate(window, sg->frame_size, sg->window_type);

    for (int f = 0; f < sg->num_frames; f++) {
        int start = f * sg->hop_size;
        if (start + sg->frame_size > signal_len) break;

        for (int i = 0; i < sg->frame_size; i++) {
            frame[i] = signal[start + i] * window[i];
        }

        for (int i = 0; i < fft_n; i++) {
            cplx[i].re = (i < sg->frame_size) ? frame[i] : 0.0f;
            cplx[i].im = 0.0f;
        }
        fft(cplx, fft_n);

        for (int i = 0; i < sg->frame_size; i++) {
            sg->frames[f][i] = sqrtf(cplx[i].re * cplx[i].re + cplx[i].im * cplx[i].im);
        }
    }

    free(cplx);
    free(frame);
    free(window);
    return 0;
}

void stft_free(Spectrogram *sg)
{
    if (sg && sg->frames) {
        for (int i = 0; i < sg->num_frames; i++) {
            free(sg->frames[i]);
        }
        free(sg->frames);
        sg->frames = NULL;
    }
    sg->num_frames = 0;
    sg->frame_size = 0;
}
