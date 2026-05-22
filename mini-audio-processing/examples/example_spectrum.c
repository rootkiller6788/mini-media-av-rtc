#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "fft_core.h"
#include "pcm_wav.h"

static void print_spectrum_bars(const float *mag, int num_bins, int max_bars, float max_mag)
{
    int step = num_bins / max_bars;
    if (step < 1) step = 1;

    for (int i = 0; i < max_bars; i++) {
        int idx = i * step;
        if (idx >= num_bins) break;

        float norm = mag[idx] / (max_mag + 1e-9f);
        int bar_len = (int)(norm * 50.0f);
        if (bar_len > 50) bar_len = 50;

        printf("%4d Hz |", i * 48000 / max_bars / 2);
        for (int j = 0; j < bar_len; j++) printf("#");
        printf("\n");
    }
}

int main(void)
{
    printf("=== mini-audio-processing: Spectrum Analyzer Demo ===\n\n");

    const int sr = 48000;
    const int signal_len = 1024;
    const int fft_n = 1024;

    float *signal = (float *)malloc((size_t)signal_len * sizeof(float));
    if (!signal) return 1;

    printf("[1] Generating test signal: 440 Hz + 880 Hz + 1760 Hz\n");
    for (int i = 0; i < signal_len; i++) {
        float t = (float)i / sr;
        signal[i] = 0.4f * sinf(2.0f * 3.141592654f * 440.0f * t)
                  + 0.3f * sinf(2.0f * 3.141592654f * 880.0f * t)
                  + 0.2f * sinf(2.0f * 3.141592654f * 1760.0f * t);
    }
    printf("    OK: %d samples generated\n", signal_len);

    printf("[2] Applying Hann window\n");
    window_apply(signal, signal_len, WINDOW_HANN);
    printf("    OK: window applied\n");

    printf("[3] Computing FFT (radix-2 Cooley-Tukey)\n");
    Complex *cplx = (Complex *)malloc((size_t)fft_n * sizeof(Complex));
    if (!cplx) { free(signal); return 1; }

    fft_real(signal, cplx, fft_n);
    printf("    OK: FFT computed\n");

    printf("[4] Computing spectrum (magnitude + phase)\n");
    Spectrum spec;
    spectrum_compute(&spec, cplx, fft_n);
    printf("    OK: %d frequency bins\n", spec.num_bins);

    float max_mag = 0.0f;
    for (int i = 0; i < spec.num_bins; i++) {
        if (spec.magnitude[i] > max_mag) max_mag = spec.magnitude[i];
    }

    printf("[5] Computing STFT spectrogram (size 256, hop 128)\n");
    Spectrogram sg;
    int frame_size = 256;
    int hop_size = 128;
    int num_frames = (signal_len - frame_size) / hop_size + 1;

    if (stft_init(&sg, num_frames, frame_size, hop_size, sr, WINDOW_HANN) == 0) {
        stft_compute(signal, signal_len, &sg);
        printf("    OK: %d frames computed\n", sg.num_frames);
        printf("    Frame 0 energy: %.4f\n", sg.frames[0][0]);
        stft_free(&sg);
    }

    printf("\n--- Frequency Spectrum (0 - 5000 Hz) ---\n");
    print_spectrum_bars(spec.magnitude, spec.num_bins, 40, max_mag);

    printf("\n[6] Computing inverse FFT to verify reconstruction\n");
    float *reconstructed = (float *)malloc((size_t)fft_n * sizeof(float));
    Complex *ifft_data = (Complex *)malloc((size_t)fft_n * sizeof(Complex));
    memcpy(ifft_data, cplx, (size_t)fft_n * sizeof(Complex));
    ifft(ifft_data, fft_n);
    for (int i = 0; i < fft_n; i++) reconstructed[i] = ifft_data[i].re;

    float err_sum = 0.0f;
    for (int i = 0; i < fft_n; i++) {
        err_sum += fabsf(reconstructed[i] - signal[i]);
    }
    printf("    OK: mean abs error = %.6f\n", err_sum / fft_n);

    spectrum_free(&spec);
    free(reconstructed);
    free(ifft_data);
    free(cplx);
    free(signal);

    printf("\n=== Test complete ===\n");
    return 0;
}
