#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "audio_filter.h"
#include "pcm_wav.h"

int main(void)
{
    printf("=== mini-audio-processing: Audio Filter Suite Demo ===\n\n");

    const int sr = 44100;
    const int len = 16000;
    float *signal = (float *)malloc((size_t)len * sizeof(float));
    float *output = (float *)malloc((size_t)len * sizeof(float));
    if (!signal || !output) return 1;

    printf("[1] Generating white noise + 440 Hz tone\n");
    for (int i = 0; i < len; i++) {
        signal[i] = 0.3f * sinf(2.0f * 3.141592654f * 440.0f * (float)i / sr)
                  + 0.05f * ((float)rand() / RAND_MAX * 2.0f - 1.0f);
    }
    printf("    OK: %d samples\n", len);

    printf("[2] FIR Lowpass Filter (cutoff=1000 Hz, 51 taps)\n");
    FirFilter fir;
    if (fir_design(&fir, 51, 1000.0f, (float)sr, FIR_LOWPASS) == 0) {
        fir_process(&fir, signal, output, len);
        printf("    OK: filtered (taps=%d)\n", fir.num_taps);
        printf("    Sample output: in[0]=%.4f -> out[0]=%.4f\n", signal[0], output[0]);
        fir_free(&fir);
    }

    printf("[3] FIR Highpass Filter (cutoff=2000 Hz, 51 taps)\n");
    if (fir_design(&fir, 51, 2000.0f, (float)sr, FIR_HIGHPASS) == 0) {
        fir_process(&fir, signal, output, len);
        printf("    OK: filtered (taps=%d)\n", fir.num_taps);
        fir_free(&fir);
    }

    printf("[4] FIR Bandpass Filter (500-1500 Hz, 101 taps)\n");
    if (fir_design_bp(&fir, 101, 500.0f, 1500.0f, (float)sr) == 0) {
        fir_process(&fir, signal, output, len);
        printf("    OK: filtered (taps=%d)\n", fir.num_taps);
        fir_free(&fir);
    }

    printf("[5] Biquad Lowpass (freq=2000 Hz, Q=0.707)\n");
    BiquadFilter bq;
    if (biquad_design(&bq, BIQUAD_LOWPASS, 2000.0f, 0.707f, 0.0f, (float)sr) == 0) {
        biquad_process(&bq, signal, output, len);
        printf("    OK: b0=%.4f b1=%.4f b2=%.4f a1=%.4f a2=%.4f\n",
               bq.b0, bq.b1, bq.b2, bq.a1, bq.a2);
    }

    printf("[6] Biquad Peaking (freq=1000 Hz, Q=1.0, gain=+6 dB)\n");
    if (biquad_design(&bq, BIQUAD_PEAKING, 1000.0f, 1.0f, 6.0f, (float)sr) == 0) {
        biquad_process(&bq, signal, output, len);
        printf("    OK: peaking EQ applied\n");
    }

    printf("[7] Biquad Notch (freq=440 Hz, Q=10.0)\n");
    if (biquad_design(&bq, BIQUAD_NOTCH, 440.0f, 10.0f, 0.0f, (float)sr) == 0) {
        biquad_process(&bq, signal, output, len);
        float before_rms = 0.0f, after_rms = 0.0f;
        for (int i = 0; i < len; i++) {
            before_rms += signal[i] * signal[i];
            after_rms += output[i] * output[i];
        }
        before_rms = sqrtf(before_rms / len);
        after_rms  = sqrtf(after_rms / len);
        printf("    OK: RMS before=%.4f, after=%.4f\n", before_rms, after_rms);
    }

    printf("[8] Echo/Delay (300ms, feedback=0.4)\n");
    EchoDelay echo;
    if (echo_delay_init(&echo, 300, (float)sr, 0.4f, 0.5f) == 0) {
        echo_delay_process(&echo, signal, output, len);
        printf("    OK: echo applied (delay=%d samples, wet=%.2f)\n",
               echo.delay_samples, echo.wet_mix);
        echo_delay_free(&echo);
    }

    printf("[9] Convolution Reverb (synthetic IR, 256 taps)\n");
    float ir[256];
    for (int i = 0; i < 256; i++) {
        ir[i] = expf(-(float)i / 50.0f) * ((float)rand() / RAND_MAX * 2.0f - 1.0f);
    }
    ConvReverb rev;
    if (conv_reverb_init(&rev, ir, 256) == 0) {
        conv_reverb_process(&rev, signal, output, len);
        printf("    OK: reverb applied (IR len=%d)\n", rev.ir_len);
        conv_reverb_free(&rev);
    }

    printf("[10] Gain adjustment (+3 dB)\n");
    apply_gain(signal, output, len, 3.0f);
    printf("    OK: +3 dB gain applied (in_max=%.4f -> out_max=%.4f)\n",
           signal[len/2], output[len/2]);

    printf("[11] Limiter (threshold=-3 dB, ceiling=-1 dB)\n");
    apply_limiter(output, len, -3.0f, -1.0f);
    printf("    OK: limiter applied\n");

    printf("[12] Writing processed audio to output_filtered.wav\n");
    AudioBuffer buf;
    audiobuf_alloc(&buf, len, 1, sr, PCM_FMT_FLOAT32);
    memcpy(buf.data, output, (size_t)len * sizeof(float));
    wav_write("output_filtered.wav", &buf);
    audiobuf_free(&buf);
    printf("    OK: written\n");

    free(signal);
    free(output);

    printf("\n=== Test complete ===\n");
    return 0;
}
