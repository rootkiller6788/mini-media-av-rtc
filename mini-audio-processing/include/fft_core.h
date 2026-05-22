#ifndef FFT_CORE_H
#define FFT_CORE_H

#include <stddef.h>
#include <math.h>

typedef struct {
    float re;
    float im;
} Complex;

typedef enum {
    WINDOW_RECTANGLE = 0,
    WINDOW_HANN      = 1,
    WINDOW_HAMMING   = 2,
    WINDOW_BLACKMAN  = 3
} WindowType;

typedef struct {
    float   *magnitude;
    float   *phase;
    int      num_bins;
} Spectrum;

typedef struct {
    float    **frames;
    int        num_frames;
    int        frame_size;
    int        hop_size;
    int        sample_rate;
    WindowType window_type;
} Spectrogram;

int      next_pow2(int n);
int      bit_reverse(int x, int bits);
int      fft(Complex *x, int n);
int      ifft(Complex *x, int n);
int      fft_real(const float *real_input, Complex *output, int n);
int      fft_magnitude(const Complex *fft_result, float *magnitude, int n);
int      fft_phase(const Complex *fft_result, float *phase, int n);
int      spectrum_compute(Spectrum *spec, const Complex *fft_result, int n);
void     spectrum_free(Spectrum *spec);

int      window_generate(float *window, int size, WindowType type);
int      window_apply(float *signal, int size, WindowType type);

int      stft_init(Spectrogram *sg, int num_frames, int frame_size, int hop_size,
                   int sample_rate, WindowType window_type);
int      stft_compute(const float *signal, int signal_len, Spectrogram *sg);
void     stft_free(Spectrogram *sg);

#endif
