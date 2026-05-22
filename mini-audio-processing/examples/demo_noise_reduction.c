#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "pcm_wav.h"
#include "fft_core.h"
#include "audio_filter.h"

typedef struct {
    float  *spectrum;
    float  *noise_estimate;
    float  *denoised;
    int     num_bins;
    float   alpha;
    float   beta;
    int     fft_size;
    int     hop_size;
} NoiseReducer;

static int noise_reducer_init(NoiseReducer *nr, int fft_size, int hop_size)
{
    if (!nr || fft_size < 64 || hop_size <= 0) return -1;

    nr->fft_size = fft_size;
    nr->hop_size = hop_size;
    nr->num_bins = fft_size / 2 + 1;
    nr->alpha    = 0.98f;
    nr->beta     = 2.0f;

    nr->spectrum       = (float *)calloc((size_t)nr->num_bins, sizeof(float));
    nr->noise_estimate = (float *)calloc((size_t)nr->num_bins, sizeof(float));
    nr->denoised       = (float *)calloc((size_t)nr->num_bins, sizeof(float));
    if (!nr->spectrum || !nr->noise_estimate || !nr->denoised) {
        free(nr->spectrum); free(nr->noise_estimate); free(nr->denoised);
        return -1;
    }
    return 0;
}

static void noise_reducer_free(NoiseReducer *nr)
{
    if (nr) {
        free(nr->spectrum);
        free(nr->noise_estimate);
        free(nr->denoised);
        memset(nr, 0, sizeof(*nr));
    }
}

static int noise_reduce_frame(NoiseReducer *nr, float *frame, int frame_len, float *out_frame)
{
    int n_bins = nr->num_bins;
    Complex *cplx = (Complex *)calloc((size_t)nr->fft_size, sizeof(Complex));
    float   *win  = (float *)malloc((size_t)frame_len * sizeof(float));
    if (!cplx || !win) { free(cplx); free(win); return -1; }

    window_generate(win, frame_len, WINDOW_HANN);
    for (int i = 0; i < frame_len; i++) {
        cplx[i].re = frame[i] * win[i];
    }
    fft(cplx, nr->fft_size);

    for (int i = 0; i < n_bins; i++) {
        float mag = sqrtf(cplx[i].re * cplx[i].re + cplx[i].im * cplx[i].im);
        nr->noise_estimate[i] = nr->alpha * nr->noise_estimate[i] + (1.0f - nr->alpha) * mag;

        float gain = 1.0f;
        if (nr->noise_estimate[i] > 0.0f) {
            float snr = mag / (nr->noise_estimate[i] + 1e-9f);
            gain = snr / (snr + nr->beta);
            if (gain < 0.01f) gain = 0.01f;
        }
        cplx[i].re *= gain;
        cplx[i].im *= gain;
    }

    ifft(cplx, nr->fft_size);
    for (int i = 0; i < frame_len; i++) {
        out_frame[i] = cplx[i].re * win[i];
    }

    free(cplx);
    free(win);
    return 0;
}

static int overlap_add(const float *frames, int num_frames, int frame_size, int hop_size, float *output, int out_len)
{
    float *acc = (float *)calloc((size_t)out_len, sizeof(float));
    if (!acc) return -1;

    for (int f = 0; f < num_frames; f++) {
        int start = f * hop_size;
        for (int i = 0; i < frame_size && (start + i) < out_len; i++) {
            acc[start + i] += frames[f * frame_size + i];
        }
    }
    memcpy(output, acc, (size_t)out_len * sizeof(float));
    free(acc);
    return 0;
}

int main(void)
{
    printf("=== mini-audio-processing: Spectral Noise Reduction Demo ===\n\n");

    const int sr         = 44100;
    const int total_len  = sr * 3;
    const int fft_size   = 1024;
    const int hop_size   = fft_size / 2;

    printf("Platform: C99, Sample Rate: %d Hz, Duration: 3 seconds\n", sr);
    printf("FFT Size: %d, Hop Size: %d\n\n", fft_size, hop_size);

    float *clean  = (float *)calloc((size_t)total_len, sizeof(float));
    float *noisy  = (float *)calloc((size_t)total_len, sizeof(float));
    float *output = (float *)calloc((size_t)total_len, sizeof(float));
    if (!clean || !noisy || !output) {
        printf("ERROR: memory allocation failed\n");
        return 1;
    }

    printf("[1] Generating clean speech-like signal\n");
    float frequencies[] = { 200.0f, 400.0f, 600.0f, 800.0f, 1200.0f, 1600.0f };
    int num_freqs = 6;
    for (int i = 0; i < total_len; i++) {
        float t = (float)i / sr;
        float val = 0.0f;
        float envelope = expf(-((float)(i % sr / 2)) / (float)(sr / 8));
        for (int f = 0; f < num_freqs; f++) {
            val += 0.1f * sinf(2.0f * 3.141592654f * frequencies[f] * t);
        }
        clean[i] = val * envelope;
        noisy[i] = clean[i] + 0.15f * ((float)rand() / RAND_MAX * 2.0f - 1.0f);
    }
    printf("    OK: clean signal with %d harmonics generated\n", num_freqs);

    printf("[2] Writing noisy signal to WAV\n");
    {
        AudioBuffer buf;
        audiobuf_alloc(&buf, total_len, 1, sr, PCM_FMT_FLOAT32);
        memcpy(buf.data, noisy, (size_t)total_len * sizeof(float));
        wav_write("demo_noisy.wav", &buf);
        audiobuf_free(&buf);
        printf("    OK: demo_noisy.wav written\n");
    }

    printf("[3] Initializing noise reducer (spectral subtraction method)\n");
    NoiseReducer nr;
    if (noise_reducer_init(&nr, fft_size, hop_size) != 0) {
        printf("ERROR: noise reducer init failed\n");
        return 1;
    }
    printf("    OK: initialized (alpha=%.2f, beta=%.1f)\n", nr.alpha, nr.beta);

    printf("[4] Processing frames with overlap-add\n");
    {
        int num_frames = (total_len - fft_size) / hop_size + 1;
        float *frames    = (float *)calloc((size_t)(num_frames * fft_size), sizeof(float));
        float *out_frames = (float *)calloc((size_t)(num_frames * fft_size), sizeof(float));
        if (!frames || !out_frames) {
            printf("ERROR: frame buffer allocation failed\n");
            return 1;
        }

        for (int f = 0; f < num_frames; f++) {
            int start = f * hop_size;
            for (int i = 0; i < fft_size && (start + i) < total_len; i++) {
                frames[f * fft_size + i] = noisy[start + i];
            }
        }

        for (int f = 0; f < num_frames; f++) {
            noise_reduce_frame(&nr, &frames[f * fft_size], fft_size, &out_frames[f * fft_size]);
        }

        overlap_add(out_frames, num_frames, fft_size, hop_size, output, total_len);

        printf("    OK: %d frames processed\n", num_frames);

        free(frames);
        free(out_frames);
    }

    printf("[5] Applying post-processing: gentle low-pass FIR filter\n");
    {
        FirFilter fir;
        fir_design(&fir, 31, 8000.0f, (float)sr, FIR_LOWPASS);
        fir_process(&fir, output, output, total_len);
        fir_free(&fir);
        printf("    OK: post-filter applied (8000 Hz low-pass)\n");
    }

    printf("[6] Writing denoised signal to WAV\n");
    {
        AudioBuffer buf;
        audiobuf_alloc(&buf, total_len, 1, sr, PCM_FMT_FLOAT32);
        memcpy(buf.data, output, (size_t)total_len * sizeof(float));
        wav_write("demo_denoised.wav", &buf);
        audiobuf_free(&buf);
        printf("    OK: demo_denoised.wav written\n");
    }

    printf("[7] Signal quality metrics\n");
    {
        double clean_power = 0.0, noisy_power = 0.0, output_power = 0.0;
        double noise_only_power = 0.0, residual_power = 0.0;
        for (int i = 0; i < total_len; i++) {
            clean_power  += (double)clean[i] * clean[i];
            noisy_power  += (double)noisy[i] * noisy[i];
            output_power += (double)output[i] * output[i];
            float n_only = noisy[i] - clean[i];
            float r_res  = output[i] - clean[i];
            noise_only_power += n_only * n_only;
            residual_power   += r_res * r_res;
        }
        clean_power  /= total_len;
        noisy_power  /= total_len;
        output_power /= total_len;
        noise_only_power /= total_len;
        residual_power   /= total_len;

        printf("    Clean signal RMS:     %.4f\n", sqrt(clean_power));
        printf("    Noisy signal RMS:     %.4f\n", sqrt(noisy_power));
        printf("    Denoised signal RMS:  %.4f\n", sqrt(output_power));
        printf("    Input noise power:    %.4f\n", sqrt(noise_only_power));
        printf("    Residual noise power: %.4f\n", sqrt(residual_power));
        printf("    SNR improvement:      %.2f dB\n",
               10.0 * log10((clean_power + 1e-9) / (residual_power + 1e-9))
             - 10.0 * log10((clean_power + 1e-9) / (noise_only_power + 1e-9)));
    }

    noise_reducer_free(&nr);
    free(clean);
    free(noisy);
    free(output);

    printf("\nOutput files: demo_noisy.wav, demo_denoised.wav\n");
    printf("=== Demo complete ===\n");
    return 0;
}
