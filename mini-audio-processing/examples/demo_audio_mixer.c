#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "pcm_wav.h"
#include "resample_mix.h"
#include "audio_filter.h"

typedef struct {
    char    name[32];
    float   gain_db;
    float   pan;
    int     muted;
    int     solo;
    int     sample_rate;
    int     num_samples;
    float  *data;
} MixTrack;

typedef struct {
    MixTrack *tracks;
    int       num_tracks;
    int       capacity;
    int       master_sr;
    float     master_gain;
    BiquadFilter master_eq[3];
} AudioMixer;

static int mixer_init(AudioMixer *mixer, int capacity, int sample_rate)
{
    if (!mixer || capacity <= 0) return -1;

    mixer->tracks = (MixTrack *)calloc((size_t)capacity, sizeof(MixTrack));
    if (!mixer->tracks) return -1;

    mixer->num_tracks = 0;
    mixer->capacity   = capacity;
    mixer->master_sr  = sample_rate;
    mixer->master_gain = 0.0f;

    biquad_design(&mixer->master_eq[0], BIQUAD_PEAKING, 100.0f, 0.7f, 0.0f, (float)sample_rate);
    biquad_design(&mixer->master_eq[1], BIQUAD_PEAKING, 1000.0f, 1.0f, 0.0f, (float)sample_rate);
    biquad_design(&mixer->master_eq[2], BIQUAD_PEAKING, 8000.0f, 0.7f, 0.0f, (float)sample_rate);

    return 0;
}

static void mixer_free(AudioMixer *mixer)
{
    if (mixer && mixer->tracks) {
        for (int i = 0; i < mixer->num_tracks; i++) {
            free(mixer->tracks[i].data);
        }
        free(mixer->tracks);
        mixer->tracks     = NULL;
        mixer->num_tracks = 0;
    }
}

static int mixer_add_track(AudioMixer *mixer, const char *name, const float *data,
                            int num_samples, int sample_rate)
{
    if (!mixer || mixer->num_tracks >= mixer->capacity) return -1;

    MixTrack *t = &mixer->tracks[mixer->num_tracks];

    strncpy(t->name, name, 31);
    t->name[31] = '\0';
    t->gain_db    = 0.0f;
    t->pan        = 0.5f;
    t->muted      = 0;
    t->solo       = 0;
    t->sample_rate = sample_rate;
    t->num_samples = num_samples;

    t->data = (float *)malloc((size_t)num_samples * sizeof(float));
    if (!t->data) return -1;

    if (sample_rate != mixer->master_sr) {
        int new_len = (int)((float)num_samples * mixer->master_sr / sample_rate) + 1;
        float *resampled = (float *)malloc((size_t)new_len * sizeof(float));
        if (!resampled) { free(t->data); return -1; }
        resample_lanczos(data, num_samples, resampled, new_len, 3);
        t->num_samples = new_len;
        memcpy(t->data, resampled, (size_t)new_len * sizeof(float));
        free(resampled);
    } else {
        memcpy(t->data, data, (size_t)num_samples * sizeof(float));
    }

    mixer->num_tracks++;
    return mixer->num_tracks - 1;
}

static int mixer_set_track_gain(AudioMixer *mixer, int idx, float gain_db)
{
    if (!mixer || idx < 0 || idx >= mixer->num_tracks) return -1;
    mixer->tracks[idx].gain_db = gain_db;
    return 0;
}

static int mixer_set_track_pan(AudioMixer *mixer, int idx, float pan)
{
    if (!mixer || idx < 0 || idx >= mixer->num_tracks) return -1;
    if (pan < 0.0f) pan = 0.0f;
    if (pan > 1.0f) pan = 1.0f;
    mixer->tracks[idx].pan = pan;
    return 0;
}

static int mixer_set_track_mute(AudioMixer *mixer, int idx, int muted)
{
    if (!mixer || idx < 0 || idx >= mixer->num_tracks) return -1;
    mixer->tracks[idx].muted = muted;
    return 0;
}

static int mixer_set_solo(AudioMixer *mixer, int idx, int solo)
{
    if (!mixer || idx < 0 || idx >= mixer->num_tracks) return -1;
    mixer->tracks[idx].solo = solo;
    return 0;
}

static int mixer_set_master_eq(AudioMixer *mixer, int band, float gain_db)
{
    if (!mixer || band < 0 || band >= 3) return -1;
    BiquadFilter *bq = &mixer->master_eq[band];
    float freq = (band == 0) ? 100.0f : (band == 1) ? 1000.0f : 8000.0f;
    float Q    = (band == 1) ? 1.0f : 0.7f;
    return biquad_design(bq, BIQUAD_PEAKING, freq, Q, gain_db, (float)mixer->master_sr);
}

static int mixer_render(const AudioMixer *mixer, float *output, int out_len, int num_channels)
{
    if (!mixer || !output) return -1;

    int has_solo = 0;
    for (int t = 0; t < mixer->num_tracks; t++) {
        if (mixer->tracks[t].solo) { has_solo = 1; break; }
    }

    memset(output, 0, (size_t)(out_len * num_channels) * sizeof(float));

    for (int t = 0; t < mixer->num_tracks; t++) {
        const MixTrack *tr = &mixer->tracks[t];

        int active = 1;
        if (tr->muted) active = 0;
        if (has_solo && !tr->solo) active = 0;
        if (!active) continue;

        float gain_lin = powf(10.0f, tr->gain_db / 20.0f) * powf(10.0f, mixer->master_gain / 20.0f);
        float left_gain  = gain_lin * (1.0f - tr->pan);
        float right_gain = gain_lin * tr->pan;

        for (int i = 0; i < out_len && i < tr->num_samples; i++) {
            float sample = tr->data[i];
            output[i * num_channels] += sample * left_gain;
            if (num_channels >= 2) {
                output[i * num_channels + 1] += sample * right_gain;
            }
        }
    }

    if (num_channels >= 2) {
        for (int b = 0; b < 3; b++) {
            float *temp = (float *)malloc((size_t)out_len * 2 * sizeof(float));
            if (!temp) continue;
            biquad_process(&mixer->master_eq[b], output, temp, out_len * 2);
            memcpy(output, temp, (size_t)(out_len * 2) * sizeof(float));
            free(temp);
        }
    }

    for (int i = 0; i < out_len * num_channels; i++) {
        if (output[i] > 1.0f) output[i] = 1.0f;
        if (output[i] < -1.0f) output[i] = -1.0f;
    }

    return 0;
}

static void generate_tone(float *buf, int len, float freq, float sample_rate, float amp)
{
    for (int i = 0; i < len; i++) {
        buf[i] = amp * sinf(2.0f * 3.141592654f * freq * (float)i / sample_rate);
    }
}

static void generate_chord(float *buf, int len, const float *freqs, int num_notes,
                            float sample_rate, float amp)
{
    memset(buf, 0, (size_t)len * sizeof(float));
    for (int n = 0; n < num_notes; n++) {
        for (int i = 0; i < len; i++) {
            buf[i] += amp / num_notes * sinf(2.0f * 3.141592654f * freqs[n] * (float)i / sample_rate);
        }
    }
}

int main(void)
{
    printf("=== mini-audio-processing: Multi-track Audio Mixer Demo ===\n\n");

    const int sr = 48000;
    const int duration_sec = 5;
    const int track_len = sr * duration_sec;

    printf("Platform: C99, Sample Rate: %d Hz, Duration: %d seconds\n", sr, duration_sec);
    printf("Tracks: 8, Channels: Stereo\n\n");

    AudioMixer mixer;
    if (mixer_init(&mixer, 8, sr) != 0) {
        printf("ERROR: mixer init failed\n");
        return 1;
    }
    printf("[1] Mixer initialized (capacity=8, master_sr=%d)\n", sr);

    printf("[2] Creating 8 instrument tracks\n");

    float *buf = (float *)malloc((size_t)track_len * sizeof(float));
    if (!buf) { mixer_free(&mixer); return 1; }

    generate_tone(buf, track_len, 261.63f, (float)sr, 0.3f);
    mixer_add_track(&mixer, "Bass (C4)", buf, track_len, sr);
    mixer_set_track_gain(&mixer, 0, -6.0f);
    mixer_set_track_pan(&mixer, 0, 0.5f);
    printf("    Track 0: %s (gain=%.1fdB, pan=%.1f)\n",
           mixer.tracks[0].name, mixer.tracks[0].gain_db, mixer.tracks[0].pan);

    generate_tone(buf, track_len, 329.63f, (float)sr, 0.25f);
    mixer_add_track(&mixer, "Melody (E4)", buf, track_len, sr);
    mixer_set_track_gain(&mixer, 1, -3.0f);
    mixer_set_track_pan(&mixer, 1, 0.3f);
    printf("    Track 1: %s (gain=%.1fdB, pan=%.1f)\n",
           mixer.tracks[1].name, mixer.tracks[1].gain_db, mixer.tracks[1].pan);

    generate_tone(buf, track_len, 392.00f, (float)sr, 0.2f);
    mixer_add_track(&mixer, "Melody (G4)", buf, track_len, sr);
    mixer_set_track_gain(&mixer, 2, -3.0f);
    mixer_set_track_pan(&mixer, 2, 0.7f);
    printf("    Track 2: %s (gain=%.1fdB, pan=%.1f)\n",
           mixer.tracks[2].name, mixer.tracks[2].gain_db, mixer.tracks[2].pan);

    float c_major[] = { 261.63f, 329.63f, 392.00f };
    generate_chord(buf, track_len, c_major, 3, (float)sr, 0.15f);
    mixer_add_track(&mixer, "Chord (C Major)", buf, track_len, sr);
    mixer_set_track_gain(&mixer, 3, -9.0f);
    mixer_set_track_pan(&mixer, 3, 0.5f);
    printf("    Track 3: %s (gain=%.1fdB, pan=%.1f)\n",
           mixer.tracks[3].name, mixer.tracks[3].gain_db, mixer.tracks[3].pan);

    for (int t = 0; t < 4; t++) {
        memset(buf, 0, (size_t)track_len * sizeof(float));
        for (int i = 0; i < track_len; i++) {
            buf[i] = 0.02f * ((float)rand() / RAND_MAX * 2.0f - 1.0f);
        }
        char name[32];
        snprintf(name, sizeof(name), "Texture %d", t);
        mixer_add_track(&mixer, name, buf, track_len, sr);
        mixer_set_track_gain(&mixer, 4 + t, -20.0f);
        printf("    Track %d: %s (gain=%.1fdB)\n",
               4 + t, mixer.tracks[4 + t].name, mixer.tracks[4 + t].gain_db);
    }

    printf("[3] Configuring master EQ\n");
    mixer_set_master_eq(&mixer, 0, -2.0f);
    mixer_set_master_eq(&mixer, 1, 3.0f);
    mixer_set_master_eq(&mixer, 2, 1.5f);
    printf("    Low shelf (100Hz): -2dB\n");
    printf("    Mid peaking (1kHz): +3dB\n");
    printf("    High shelf (8kHz): +1.5dB\n");

    printf("[4] Applying mute/solo test\n");
    mixer_set_track_mute(&mixer, 4, 1);
    mixer_set_track_mute(&mixer, 5, 1);
    printf("    Muted: Texture 0, Texture 1\n");

    printf("[5] Rendering final mix (stereo, %d samples)\n", track_len);
    float *stereo_out = (float *)malloc((size_t)(track_len * 2) * sizeof(float));
    if (!stereo_out) { mixer_free(&mixer); free(buf); return 1; }

    mixer_render(&mixer, stereo_out, track_len, 2);
    printf("    OK: mixdown complete\n");

    printf("[6] Measuring output levels\n");
    float max_val = 0.0f;
    double rms_l = 0.0, rms_r = 0.0;
    for (int i = 0; i < track_len; i++) {
        float left  = fabsf(stereo_out[i * 2]);
        float right = fabsf(stereo_out[i * 2 + 1]);
        if (left > max_val)  max_val = left;
        if (right > max_val) max_val = right;
        rms_l += (double)stereo_out[i * 2] * stereo_out[i * 2];
        rms_r += (double)stereo_out[i * 2 + 1] * stereo_out[i * 2 + 1];
    }
    rms_l = sqrt(rms_l / track_len);
    rms_r = sqrt(rms_r / track_len);
    printf("    Peak level:   %.4f (%.1f dBFS)\n", max_val, 20.0 * log10(max_val + 1e-9));
    printf("    RMS Left:     %.4f (%.1f dB)\n", rms_l, 20.0 * log10(rms_l + 1e-9));
    printf("    RMS Right:    %.4f (%.1f dB)\n", rms_r, 20.0 * log10(rms_r + 1e-9));

    printf("[7] Writing final mix to WAV file\n");
    {
        AudioBuffer out_buf;
        audiobuf_alloc(&out_buf, track_len, 2, sr, PCM_FMT_FLOAT32);
        memcpy(out_buf.data, stereo_out, (size_t)(track_len * 2) * sizeof(float));
        wav_write("demo_mixdown.wav", &out_buf);
        audiobuf_free(&out_buf);
        printf("    OK: demo_mixdown.wav written (%d samples, stereo)\n", track_len);
    }

    printf("[8] AudioBuffer pool test\n");
    AudioBufPool pool;
    audiobuf_pool_init(&pool, 4);
    printf("    Pool capacity: %d\n", pool.pool_capacity);

    int idx_a, idx_b, idx_c;
    audiobuf_pool_alloc(&pool, 1024, 1, sr, PCM_FMT_FLOAT32, &idx_a);
    audiobuf_pool_alloc(&pool, 2048, 2, 44100, PCM_FMT_FLOAT32, &idx_b);
    printf("    Allocated: idx_a=%d (1024 samp), idx_b=%d (2048 samp)\n", idx_a, idx_b);

    audiobuf_pool_copy_to(&pool, idx_a, 2);
    printf("    Copied idx_a -> idx_c\n");

    audiobuf_pool_free_idx(&pool, idx_a);
    printf("    Freed idx_a\n");

    audiobuf_pool_alloc(&pool, 512, 1, sr, PCM_FMT_FLOAT32, &idx_c);
    printf("    Reused slot: idx_c=%d\n", idx_c);

    audiobuf_pool_free(&pool);

    mixer_free(&mixer);
    free(buf);
    free(stereo_out);

    printf("\nOutput file: demo_mixdown.wav\n");
    printf("=== Demo complete ===\n");
    return 0;
}
