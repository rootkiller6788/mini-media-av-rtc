#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "pcm_wav.h"

int main(void)
{
    printf("=== mini-audio-processing: WAV Playback Demo ===\n\n");

    AudioBuffer buf;
    AudioBuffer copy;

    const int samples = 48000;
    const int sr = 44100;
    const int ch = 1;

    printf("[1] Allocating AudioBuffer: %d samples, %d Hz, %d channel(s)\n", samples, sr, ch);
    if (audiobuf_alloc(&buf, samples, ch, sr, PCM_FMT_FLOAT32) != 0) {
        printf("ERROR: allocation failed\n");
        return 1;
    }
    printf("    OK: %zu bytes allocated\n", (size_t)samples * ch * sizeof(float));

    printf("[2] Generating 440 Hz sine wave (A4 note)\n");
    for (int i = 0; i < samples; i++) {
        buf.data[i] = 0.5f * sinf(2.0f * 3.141592654f * 440.0f * (float)i / sr);
    }
    printf("    OK: generated %d samples\n", samples);

    printf("[3] Writing to WAV file: output_sine.wav\n");
    if (wav_write("output_sine.wav", &buf) != 0) {
        printf("ERROR: write failed\n");
        audiobuf_free(&buf);
        return 1;
    }
    printf("    OK: written successfully\n");

    printf("[4] Creating silence buffer\n");
    audiobuf_silence(&buf);
    printf("    OK: buffer zeroed\n");

    printf("[5] Copying buffer\n");
    if (audiobuf_copy(&copy, &buf) != 0) {
        printf("ERROR: copy failed\n");
    } else {
        printf("    OK: copy successful (%d samples)\n", copy.num_samples);
        audiobuf_free(&copy);
    }

    printf("[6] Mixing two buffers\n");
    AudioBuffer a, b, mixed;
    audiobuf_alloc(&a, 1000, 1, sr, PCM_FMT_FLOAT32);
    audiobuf_alloc(&b, 1000, 1, sr, PCM_FMT_FLOAT32);
    for (int i = 0; i < 1000; i++) {
        a.data[i] = sinf(2.0f * 3.141592654f * 220.0f * (float)i / sr);
        b.data[i] = sinf(2.0f * 3.141592654f * 880.0f * (float)i / sr);
    }
    if (audiobuf_mix(&mixed, &a, &b) == 0) {
        printf("    OK: mixed %d samples\n", mixed.num_samples);
        audiobuf_free(&mixed);
    }

    audiobuf_free(&a);
    audiobuf_free(&b);
    audiobuf_free(&buf);

    printf("\n=== Test complete ===\n");
    return 0;
}
