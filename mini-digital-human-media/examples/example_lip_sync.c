/* example_lip_sync.c — Lip sync & audio-driven animation example */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "lip_sync.h"
#include "expression_bs.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

static void generate_test_audio(float* audio, int length, float sample_rate,
                                const char* phoneme_sequence)
{
    float phase = 0.0f;
    float freq = 440.0f;

    for (int i = 0; i < length; i++) {
        float t = (float)i / sample_rate;
        char c = phoneme_sequence[(int)(t * 5.0f) % (int)strlen(phoneme_sequence)];

        switch (c) {
        case 'a': freq = 200.0f; break;
        case 'e': freq = 350.0f; break;
        case 'i': freq = 500.0f; break;
        case 'o': freq = 300.0f; break;
        case 'u': freq = 250.0f; break;
        case 's': freq = 6000.0f; break;
        case 'f': freq = 4000.0f; break;
        case 'p': freq = 100.0f; break;
        case 'm': freq = 280.0f; break;
        case 't': freq = 3000.0f; break;
        default:  freq = 440.0f; break;
        }

        float envelope = 0.7f;
        phase += 2.0f * M_PI * freq / sample_rate;
        audio[i] = sinf(phase) * envelope;

        if (audio[i] > 1.0f) audio[i] = 1.0f;
        if (audio[i] < -1.0f) audio[i] = -1.0f;
    }
}

static void print_blendshape_state(const ebs_blendshapes_t* bs)
{
    printf("  BS weights: jawOpen=%.2f mouthSmile=%.2f mouthPucker=%.2f "
           "mouthFunnel=%.2f eyeBlink=%.2f\n",
           ebs_get_blendshape_weight(bs, EBS_JAW_OPEN),
           ebs_get_blendshape_weight(bs, EBS_MOUTH_SMILE_LEFT),
           ebs_get_blendshape_weight(bs, EBS_MOUTH_PUCKER),
           ebs_get_blendshape_weight(bs, EBS_MOUTH_FUNNEL),
           ebs_get_blendshape_weight(bs, EBS_EYE_BLINK_LEFT));

    ebs_expression_t expr;
    float conf;
    ebs_classify_expression(bs, &expr, &conf);
    printf("  Expression: %s (conf=%.2f)\n", ebs_expression_name(expr), conf);
}

int main(void)
{
    printf("=== Mini Digital Human: Lip Sync Example ===\n\n");

    /* Initialize lip sync state */
    ls_sync_state_t sync_state;
    ls_init_sync(&sync_state);
    printf("Lip sync state initialized (transition_speed=%.1f, smoothing=%d)\n\n",
           sync_state.transition_speed, sync_state.smoothing_enabled);

    /* Part 1: Phoneme-to-viseme mapping */
    printf("--- Part 1: Phoneme to Viseme Mapping ---\n");
    const char* test_phonemes[] = {"AA", "IY", "UW", "M", "S", "F", "P", "K", "SIL"};
    for (int i = 0; i < 9; i++) {
        ls_phoneme_t ph;
        ls_char_to_phoneme(test_phonemes[i][0], &ph);
        ebs_viseme_id_t vs = (ebs_viseme_id_t)ls_phoneme_to_viseme(ph);
        printf("  '%s' -> phoneme=%s -> viseme=%s\n",
               test_phonemes[i],
               ls_phoneme_name(ph),
               ls_viseme_label(vs));
    }
    printf("\n");

    /* Part 2: Text to viseme sequence */
    printf("--- Part 2: Text to Viseme Sequence ---\n");
    const char* text = "Hello world";
    ebs_viseme_id_t visemes[64];
    int viseme_count = 0;
    ls_text_to_visemes(text, visemes, &viseme_count);

    printf("  Text: \"%s\"\n", text);
    printf("  Viseme sequence (%d phonemes):\n", viseme_count);
    for (int i = 0; i < viseme_count; i++) {
        printf("    [%d] viseme=%s\n", i, ls_viseme_label(visemes[i]));
    }

    float durations[64];
    for (int i = 0; i < viseme_count; i++) durations[i] = 0.1f;

    ls_viseme_sequence_t sequence;
    ls_build_viseme_sequence(visemes, viseme_count, durations, &sequence);
    printf("  Sequence built: %d frames, total_duration=%.2fs\n\n",
           sequence.frame_count, sequence.total_duration);

    /* Part 3: Audio-driven viseme detection */
    printf("--- Part 3: Audio-Driven Viseme Detection ---\n");

    const float sample_rate = 16000.0f;
    const int audio_duration = 3;
    int audio_len = (int)(sample_rate * audio_duration);
    float* audio = (float*)malloc(audio_len * sizeof(float));

    const char* pattern = "aeiou";
    generate_test_audio(audio, audio_len, sample_rate, pattern);

    printf("  Generated %d audio samples (%.1fs, %d Hz)\n", audio_len,
           audio_duration, (int)sample_rate);

    float energy = ls_compute_audio_energy(audio, audio_len);
    printf("  Audio energy: %.4f\n", energy);

    int vad = ls_detect_voice_activity(audio, audio_len, 0.1f);
    printf("  Voice activity detected: %s\n", vad ? "YES" : "NO");

    float pitch, pitch_conf;
    ls_estimate_pitch(audio, audio_len, sample_rate, &pitch, &pitch_conf);
    printf("  Estimated pitch: %.1f Hz (conf=%.2f)\n\n", pitch, pitch_conf);

    /* Part 4: Viseme sequence playback */
    printf("--- Part 4: Viseme Sequence Playback ---\n");

    ls_viseme_sequence_t anim_seq;
    ls_reset_sequence(&anim_seq);

    ebs_viseme_id_t anim_visemes[] = {
        EBS_VISEME_SIL, EBS_VISEME_AA, EBS_VISEME_E,
        EBS_VISEME_I, EBS_VISEME_O, EBS_VISEME_U,
        EBS_VISEME_PP, EBS_VISEME_MB, EBS_VISEME_SIL
    };
    float anim_durations[] = {0.1f, 0.2f, 0.2f, 0.15f, 0.2f, 0.15f, 0.1f, 0.1f, 0.2f};
    int anim_count = 9;

    ls_build_viseme_sequence(anim_visemes, anim_count, anim_durations, &anim_seq);

    printf("  Playing animation (%d frames, %.2fs):\n",
           anim_seq.frame_count, anim_seq.total_duration);

    for (float t = 0.0f; t < anim_seq.total_duration; t += 0.05f) {
        ebs_viseme_id_t viseme;
        float weight;
        ls_get_viseme_at_time(&anim_seq, t, &viseme, &weight);

        ebs_blendshapes_t bs;
        ls_interpolate_visemes(&anim_seq, t, &bs);

        ebs_expression_t expr;
        float conf;
        ebs_classify_expression(&bs, &expr, &conf);

        printf("    t=%.2fs  viseme=%s (w=%.2f)  expr=%s (c=%.2f)\n",
               t, ls_viseme_label(viseme), weight,
               ebs_expression_name(expr), conf);
    }
    printf("\n");

    /* Part 5: Real-time sync simulation */
    printf("--- Part 5: Real-time Sync Simulation ---\n");

    ls_sync_state_t realtime_sync;
    ls_init_sync(&realtime_sync);

    ebs_viseme_id_t drive_sequence[] = {
        EBS_VISEME_SIL, EBS_VISEME_SIL, EBS_VISEME_AA, EBS_VISEME_AA,
        EBS_VISEME_E, EBS_VISEME_E, EBS_VISEME_O, EBS_VISEME_O,
        EBS_VISEME_PP, EBS_VISEME_PP, EBS_VISEME_SIL, EBS_VISEME_SIL
    };

    for (int i = 0; i < 12; i++) {
        ebs_blendshapes_t output_bs;
        float dt = 0.016f;
        ls_sync_update(&realtime_sync, drive_sequence[i], 1.0f, dt, &output_bs);

        printf("  frame %2d: drive=%s -> jawOpen=%.2f mouthPucker=%.2f\n",
               i, ls_viseme_label(drive_sequence[i]),
               ebs_get_blendshape_weight(&output_bs, EBS_JAW_OPEN),
               ebs_get_blendshape_weight(&output_bs, EBS_MOUTH_PUCKER));
    }
    printf("\n");

    /* Part 6: Gesamt expression blending */
    printf("--- Part 6: Expression Blending ---\n");

    const char* expr_names[] = {"neutral", "happy", "sad", "angry", "surprised"};
    for (int e = 0; e < 5; e++) {
        ebs_blendshapes_t preset;
        ebs_reset_blendshapes(&preset);
        ebs_preset_expression(&preset, (ebs_expression_t)e, 1.0f);

        ebs_expression_t cls;
        float conf;
        ebs_classify_expression(&preset, &cls, &conf);

        printf("  Preset '%s': classified as '%s' (conf=%.2f)\n",
               expr_names[e], ebs_expression_name(cls), conf);
    }
    printf("\n");

    /* Part 7: Morph target rig */
    printf("--- Part 7: Morph Target Rig ---\n");
    ebs_rig_t rig;
    ebs_rig_init(&rig);

    ebs_blendshapes_t target1, target2;
    ebs_reset_blendshapes(&target1);
    ebs_reset_blendshapes(&target2);
    ebs_preset_expression(&target1, EBS_EXPR_HAPPY, 1.0f);
    ebs_preset_expression(&target2, EBS_EXPR_SURPRISED, 0.5f);

    ebs_rig_add_morph_target(&rig, &target1);
    ebs_rig_add_morph_target(&rig, &target2);
    printf("  Added %d morph targets\n", rig.target_count);

    float rig_weights[] = {0.7f, 0.3f};
    ebs_rig_apply_weights(&rig, rig_weights, 2);
    printf("  Applied rig weights [0.7, 0.3]\n");
    print_blendshape_state(&rig.current);

    free(audio);
    printf("\nDone.\n");
    return 0;
}
