#ifndef AUDIO_FILTER_H
#define AUDIO_FILTER_H

#include <stddef.h>

typedef enum {
    FIR_LOWPASS  = 0,
    FIR_HIGHPASS = 1,
    FIR_BANDPASS = 2
} FirFilterType;

typedef struct {
    int     num_taps;
    float  *coeffs;
    float  *delay_line;
    int     delay_index;
} FirFilter;

typedef enum {
    BIQUAD_LOWPASS  = 0,
    BIQUAD_HIGHPASS = 1,
    BIQUAD_BANDPASS = 2,
    BIQUAD_NOTCH    = 3,
    BIQUAD_PEAKING  = 4,
    BIQUAD_LOWSHELF = 5,
    BIQUAD_HIGHSHELF = 6
} BiquadType;

typedef struct {
    float b0, b1, b2;
    float a1, a2;
    float x1, x2;
    float y1, y2;
} BiquadFilter;

typedef struct {
    float  *impulse_response;
    int     ir_len;
    float  *delay_line;
    int     delay_index;
} ConvReverb;

typedef struct {
    float  *delay_buffer;
    int     buffer_len;
    int     write_index;
    int     delay_samples;
    float   feedback;
    float   wet_mix;
    float   dry_mix;
} EchoDelay;

int  fir_design(FirFilter *fir, int num_taps, float cutoff, float sample_rate, FirFilterType type);
int  fir_design_bp(FirFilter *fir, int num_taps, float low_cut, float high_cut, float sample_rate);
void fir_free(FirFilter *fir);
int  fir_process(const FirFilter *fir, const float *input, float *output, int num_samples);

int  biquad_design(BiquadFilter *bq, BiquadType type, float freq, float Q, float gain_db, float sample_rate);
int  biquad_process(BiquadFilter *bq, const float *input, float *output, int num_samples);
void biquad_reset(BiquadFilter *bq);

int  conv_reverb_init(ConvReverb *rev, const float *ir, int ir_len);
void conv_reverb_free(ConvReverb *rev);
int  conv_reverb_process(ConvReverb *rev, const float *input, float *output, int num_samples);

int  echo_delay_init(EchoDelay *echo, int delay_ms, float sample_rate, float feedback, float wet_mix);
void echo_delay_free(EchoDelay *echo);
int  echo_delay_process(EchoDelay *echo, const float *input, float *output, int num_samples);

int  apply_gain(const float *input, float *output, int num_samples, float gain_db);
int  apply_limiter(float *samples, int num_samples, float threshold_db, float ceiling_db);

#endif
