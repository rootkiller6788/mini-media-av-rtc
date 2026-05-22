#include "lip_sync.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

static const char* ls_phoneme_names[LS_PHONEME_COUNT] = {
    "SIL", "AA", "AE", "AH", "AO", "AW", "AY",
    "B", "CH", "D", "DH", "EH", "ER", "EY",
    "F", "G", "HH", "IH", "IY", "JH", "K", "L",
    "M", "N", "NG", "OW", "OY", "P", "R", "S",
    "SH", "T", "TH", "UH", "UW", "V", "W", "Y", "Z", "ZH"
};

static const char* ls_viseme_labels[LS_VISEME_COUNT] = {
    "sil", "PP", "FF", "TH", "DD", "kk", "CH",
    "SS", "nn", "RR", "aa", "E", "I", "O", "U", "MB"
};

static const ebs_viseme_id_t ls_phoneme_to_viseme_map[LS_PHONEME_COUNT] = {
    EBS_VISEME_SIL, EBS_VISEME_AA, EBS_VISEME_E, EBS_VISEME_AA,
    EBS_VISEME_O, EBS_VISEME_O, EBS_VISEME_E,
    EBS_VISEME_MB, EBS_VISEME_CH, EBS_VISEME_DD,
    EBS_VISEME_TH, EBS_VISEME_E, EBS_VISEME_RR, EBS_VISEME_E,
    EBS_VISEME_FF, EBS_VISEME_KK, EBS_VISEME_SIL,
    EBS_VISEME_I, EBS_VISEME_I, EBS_VISEME_CH,
    EBS_VISEME_KK, EBS_VISEME_NN, EBS_VISEME_MB,
    EBS_VISEME_NN, EBS_VISEME_NN, EBS_VISEME_O,
    EBS_VISEME_O, EBS_VISEME_PP, EBS_VISEME_RR,
    EBS_VISEME_SS, EBS_VISEME_CH, EBS_VISEME_DD,
    EBS_VISEME_TH, EBS_VISEME_U, EBS_VISEME_U,
    EBS_VISEME_FF, EBS_VISEME_U, EBS_VISEME_I,
    EBS_VISEME_SS, EBS_VISEME_CH
};

static const float ls_default_durations[LS_VISEME_COUNT] = {
    0.05f, 0.08f, 0.10f, 0.09f, 0.07f, 0.08f,
    0.09f, 0.10f, 0.06f, 0.07f, 0.15f, 0.12f,
    0.10f, 0.14f, 0.13f, 0.08f
};

void ls_init_sync(ls_sync_state_t* state)
{
    memset(state, 0, sizeof(ls_sync_state_t));
    state->transition_speed = 12.0f;
    state->smoothing_enabled = 1;
    ebs_reset_blendshapes(&state->current);
    ebs_reset_blendshapes(&state->target);
}

void ls_reset_sequence(ls_viseme_sequence_t* seq)
{
    memset(seq, 0, sizeof(ls_viseme_sequence_t));
}

const char* ls_phoneme_name(ls_phoneme_t phoneme)
{
    if (phoneme < 0 || phoneme >= LS_PHONEME_COUNT) return "UNK";
    return ls_phoneme_names[phoneme];
}

const char* ls_viseme_label(ebs_viseme_id_t viseme)
{
    if (viseme < 0 || viseme >= LS_VISEME_COUNT) return "UNK";
    return ls_viseme_labels[viseme];
}

int ls_phoneme_to_viseme(ls_phoneme_t phoneme)
{
    if (phoneme < 0 || phoneme >= LS_PHONEME_COUNT) return EBS_VISEME_SIL;
    return ls_phoneme_to_viseme_map[phoneme];
}

int ls_char_to_phoneme(char c, ls_phoneme_t* phoneme)
{
    switch (c) {
    case 'a': case 'A': *phoneme = LS_PHONEME_AA; return 1;
    case 'e': case 'E': *phoneme = LS_PHONEME_EH; return 1;
    case 'i': case 'I': *phoneme = LS_PHONEME_IY; return 1;
    case 'o': case 'O': *phoneme = LS_PHONEME_OW; return 1;
    case 'u': case 'U': *phoneme = LS_PHONEME_UW; return 1;
    case 'b': case 'B': *phoneme = LS_PHONEME_B; return 1;
    case 'c': case 'C': *phoneme = LS_PHONEME_K; return 1;
    case 'd': case 'D': *phoneme = LS_PHONEME_D; return 1;
    case 'f': case 'F': *phoneme = LS_PHONEME_F; return 1;
    case 'g': case 'G': *phoneme = LS_PHONEME_G; return 1;
    case 'h': case 'H': *phoneme = LS_PHONEME_HH; return 1;
    case 'j': case 'J': *phoneme = LS_PHONEME_JH; return 1;
    case 'k': case 'K': *phoneme = LS_PHONEME_K; return 1;
    case 'l': case 'L': *phoneme = LS_PHONEME_L; return 1;
    case 'm': case 'M': *phoneme = LS_PHONEME_M; return 1;
    case 'n': case 'N': *phoneme = LS_PHONEME_N; return 1;
    case 'p': case 'P': *phoneme = LS_PHONEME_P; return 1;
    case 'r': case 'R': *phoneme = LS_PHONEME_R; return 1;
    case 's': case 'S': *phoneme = LS_PHONEME_S; return 1;
    case 't': case 'T': *phoneme = LS_PHONEME_T; return 1;
    case 'v': case 'V': *phoneme = LS_PHONEME_V; return 1;
    case 'w': case 'W': *phoneme = LS_PHONEME_W; return 1;
    case 'y': case 'Y': *phoneme = LS_PHONEME_Y; return 1;
    case 'z': case 'Z': *phoneme = LS_PHONEME_Z; return 1;
    case ' ': case ',': case '.': *phoneme = LS_PHONEME_SIL; return 1;
    default: *phoneme = LS_PHONEME_SIL; return 0;
    }
}

void ls_phoneme_to_formants(ls_phoneme_t phoneme, ls_formants_t* formants)
{
    memset(formants, 0, sizeof(ls_formants_t));

    switch (phoneme) {
    case LS_PHONEME_AA: formants->f1 = 730; formants->f2 = 1090; formants->f3 = 2440; break;
    case LS_PHONEME_AE: formants->f1 = 660; formants->f2 = 1720; formants->f3 = 2410; break;
    case LS_PHONEME_AH: formants->f1 = 640; formants->f2 = 1190; formants->f3 = 2390; break;
    case LS_PHONEME_IY: formants->f1 = 280; formants->f2 = 2250; formants->f3 = 2890; break;
    case LS_PHONEME_UW: formants->f1 = 300; formants->f2 = 870; formants->f3 = 2240; break;
    case LS_PHONEME_EH: formants->f1 = 530; formants->f2 = 1840; formants->f3 = 2480; break;
    case LS_PHONEME_OW: formants->f1 = 480; formants->f2 = 800; formants->f3 = 2320; break;
    case LS_PHONEME_IH: formants->f1 = 390; formants->f2 = 1990; formants->f3 = 2550; break;
    default:
        formants->f1 = 500; formants->f2 = 1500; formants->f3 = 2500;
        break;
    }

    formants->f1 *= 1.0f;
    formants->f2 *= 1.0f;
    formants->f3 *= 1.0f;
    formants->energy = 0.7f;
    formants->pitch = 120.0f;
    formants->is_voiced = (phoneme != LS_PHONEME_SIL) ? 1 : 0;
}

void ls_text_to_phonemes(const char* text, ls_phoneme_t* phonemes, int* count)
{
    *count = 0;
    if (!text) return;

    int len = (int)strlen(text);
    for (int i = 0; i < len && *count < LS_MAX_PHONEMES; i++) {
        ls_phoneme_t p;
        if (ls_char_to_phoneme(text[i], &p)) {
            phonemes[*count] = p;
            (*count)++;
        }
    }
}

void ls_text_to_visemes(const char* text, ebs_viseme_id_t* visemes, int* count)
{
    ls_phoneme_t phonemes[LS_MAX_PHONEMES];
    int pcount = 0;
    ls_text_to_phonemes(text, phonemes, &pcount);

    *count = 0;
    for (int i = 0; i < pcount && *count < LS_MAX_SEQUENCE; i++) {
        visemes[*count] = ls_phoneme_to_viseme(phonemes[i]);
        (*count)++;
    }
}

void ls_phonemes_to_visemes(const ls_phoneme_t* phonemes, int count,
                            ebs_viseme_id_t* visemes)
{
    for (int i = 0; i < count; i++) {
        visemes[i] = ls_phoneme_to_viseme(phonemes[i]);
    }
}

void ls_build_viseme_sequence(const ebs_viseme_id_t* visemes, int count,
                              const float* durations,
                              ls_viseme_sequence_t* sequence)
{
    ls_reset_sequence(sequence);
    sequence->sample_rate = (int)LS_SAMPLE_RATE;

    float current_time = 0.0f;
    for (int i = 0; i < count && i < LS_MAX_SEQUENCE; i++) {
        float dur = durations ? durations[i] : ls_default_durations[visemes[i]];

        sequence->frames[i].viseme = visemes[i];
        sequence->frames[i].weight = 1.0f;
        sequence->frames[i].start_time = current_time;
        sequence->frames[i].end_time = current_time + dur;
        sequence->frames[i].transition_in = 0.02f;
        sequence->frames[i].transition_out = 0.02f;

        current_time += dur;
        sequence->frame_count = i + 1;
    }

    sequence->total_duration = current_time;
}

void ls_add_viseme_frame(ls_viseme_sequence_t* seq, ebs_viseme_id_t viseme,
                         float weight, float start, float end)
{
    if (seq->frame_count >= LS_MAX_SEQUENCE) return;
    int idx = seq->frame_count;
    seq->frames[idx].viseme = viseme;
    seq->frames[idx].weight = weight;
    seq->frames[idx].start_time = start;
    seq->frames[idx].end_time = end;
    seq->frames[idx].transition_in = (end - start) * 0.1f;
    seq->frames[idx].transition_out = (end - start) * 0.1f;
    seq->frame_count++;
    if (end > seq->total_duration) seq->total_duration = end;
}

void ls_get_viseme_at_time(const ls_viseme_sequence_t* seq, float time,
                           ebs_viseme_id_t* viseme, float* weight)
{
    *viseme = EBS_VISEME_SIL;
    *weight = 0.0f;

    for (int i = 0; i < seq->frame_count; i++) {
        if (time >= seq->frames[i].start_time && time < seq->frames[i].end_time) {
            *viseme = seq->frames[i].viseme;
            *weight = seq->frames[i].weight;
            return;
        }
    }
}

void ls_interpolate_visemes(const ls_viseme_sequence_t* seq, float time,
                            ebs_blendshapes_t* out)
{
    ebs_reset_blendshapes(out);

    int active_count = 0;
    ebs_viseme_id_t active[4];
    float active_weights[4];

    for (int i = 0; i < seq->frame_count; i++) {
        float t_start = seq->frames[i].start_time;
        float t_end = seq->frames[i].end_time;
        float t_in = seq->frames[i].transition_in;
        float t_out = seq->frames[i].transition_out;
        float w = 0.0f;

        if (time < t_start - t_in || time > t_end + t_out) {
            continue;
        }

        if (time < t_start) {
            w = (time - (t_start - t_in)) / t_in;
        } else if (time > t_end) {
            w = 1.0f - (time - t_end) / t_out;
        } else {
            w = 1.0f;
        }

        if (w > 0.0f && active_count < 4) {
            active[active_count] = seq->frames[i].viseme;
            active_weights[active_count] = w * seq->frames[i].weight;
            active_count++;
        }
    }

    float total_w = 0.0f;
    for (int i = 0; i < active_count; i++) total_w += active_weights[i];
    if (total_w < 0.001f) return;

    ebs_blendshapes_t combined;
    ebs_reset_blendshapes(&combined);
    for (int i = 0; i < active_count; i++) {
        ebs_blendshapes_t viseme_bs;
        ebs_viseme_to_blendshapes(active[i], active_weights[i] / total_w, &viseme_bs);
        for (int j = 0; j < EBS_BLENDSHAPE_COUNT; j++) {
            combined.weights[j] += viseme_bs.weights[j];
        }
    }

    ebs_clamp_blendshapes(&combined);
    ebs_copy_blendshapes(&combined, out);
}

void ls_audio_features_to_viseme(const float* audio_frame, int frame_size,
                                 ebs_viseme_id_t* viseme, float* weight)
{
    if (!audio_frame || frame_size <= 0) {
        *viseme = EBS_VISEME_SIL;
        *weight = 0.0f;
        return;
    }

    float energy = 0.0f;
    float zero_cross = 0.0f;
    float spectral_centroid = 0.0f;
    float weighted_sum = 0.0f;

    for (int i = 0; i < frame_size; i++) {
        energy += audio_frame[i] * audio_frame[i];
        if (i > 0) {
            zero_cross += (audio_frame[i] * audio_frame[i - 1] < 0) ? 1.0f : 0.0f;
        }
        weighted_sum += fabsf(audio_frame[i]) * i;
        spectral_centroid += fabsf(audio_frame[i]);
    }

    zero_cross /= (float)(frame_size - 1);
    spectral_centroid = spectral_centroid > 0 ? weighted_sum / spectral_centroid : 0;
    energy = sqrtf(energy / frame_size);

    if (energy < 0.01f) {
        *viseme = EBS_VISEME_SIL;
        *weight = 0.0f;
        return;
    }

    if (spectral_centroid < frame_size * 0.2f) {
        *viseme = EBS_VISEME_AA;
    } else if (spectral_centroid < frame_size * 0.35f) {
        *viseme = EBS_VISEME_E;
    } else if (spectral_centroid < frame_size * 0.45f) {
        *viseme = EBS_VISEME_O;
    } else if (zero_cross > 0.4f) {
        *viseme = EBS_VISEME_SS;
    } else {
        *viseme = EBS_VISEME_I;
    }

    *weight = energy * 5.0f;
    if (*weight > 1.0f) *weight = 1.0f;
}

void ls_extract_formants(const float* audio, int length, float sample_rate,
                         ls_formants_t* formants)
{
    (void)audio;
    (void)length;
    (void)sample_rate;

    memset(formants, 0, sizeof(ls_formants_t));
    formants->f1 = 500.0f;
    formants->f2 = 1500.0f;
    formants->f3 = 2500.0f;
    formants->energy = 0.5f;
    formants->pitch = 120.0f;
    formants->is_voiced = 1;
}

void ls_coarticulation_apply(const ebs_viseme_id_t* visemes, int count,
                             ls_viseme_sequence_t* sequence)
{
    if (count < 2 || !sequence) return;

    for (int i = 0; i < count - 1; i++) {
        float overlap = 0.015f;
        sequence->frames[i].transition_out *= 2.0f;
        sequence->frames[i].end_time += overlap * 0.5f;
        if (i + 1 < sequence->frame_count) {
            sequence->frames[i + 1].start_time -= overlap * 0.5f;
            sequence->frames[i + 1].transition_in *= 2.0f;
        }
    }
}

void ls_smooth_transition(ls_articulation_state_t* state,
                           ebs_viseme_id_t target_viseme, float weight,
                           ebs_blendshapes_t* out)
{
    ebs_blendshapes_t target_bs;
    ebs_viseme_to_blendshapes(target_viseme, weight, &target_bs);

    if (!state->is_initialized) {
        ebs_copy_blendshapes(&target_bs, out);
        state->is_initialized = 1;
        return;
    }

    float smooth = 0.15f;
    for (int i = 0; i < EBS_BLENDSHAPE_COUNT; i++) {
        out->weights[i] = out->weights[i] +
            (target_bs.weights[i] - out->weights[i]) * smooth * state->coarticulation_factor;
    }

    ebs_clamp_blendshapes(out);
}

void ls_sync_update(ls_sync_state_t* state, ebs_viseme_id_t viseme,
                    float weight, float dt, ebs_blendshapes_t* out)
{
    ebs_blendshapes_t target;
    ebs_viseme_to_blendshapes(viseme, weight, &target);
    ebs_copy_blendshapes(&target, &state->target);

    float blend_speed = state->transition_speed * dt;
    if (blend_speed > 1.0f) blend_speed = 1.0f;

    for (int i = 0; i < EBS_BLENDSHAPE_COUNT; i++) {
        state->current.weights[i] +=
            (state->target.weights[i] - state->current.weights[i]) * blend_speed;
    }

    ebs_clamp_blendshapes(&state->current);
    ebs_copy_blendshapes(&state->current, out);
}

void ls_audio_drive(const float* audio_data, int audio_length,
                     float sample_rate, ls_viseme_sequence_t* sequence,
                     ls_sync_state_t* sync, ebs_blendshapes_t* blendshapes)
{
    (void)audio_data;
    (void)audio_length;
    (void)sample_rate;
    (void)sequence;
    (void)sync;

    ebs_viseme_id_t viseme = EBS_VISEME_SIL;
    float weight = 0.0f;
    float time_step = 0.016f;

    ls_get_viseme_at_time(sequence, sync->articulation.hold_time,
                          &viseme, &weight);

    if (audio_data && audio_length > 0) {
        int frame_samples = (int)(time_step * sample_rate);
        if (frame_samples > audio_length) frame_samples = audio_length;
        if (frame_samples > 0) {
            ls_audio_features_to_viseme(audio_data, frame_samples,
                                        &viseme, &weight);
        }
    }

    ls_sync_update(sync, viseme, weight, time_step, blendshapes);
    sync->articulation.hold_time += time_step;
}

void ls_generate_viseme_duration(ebs_viseme_id_t viseme, float base_duration,
                                  float* duration)
{
    *duration = base_duration;
    if (viseme >= 0 && viseme < LS_VISEME_COUNT) {
        *duration *= ls_default_durations[viseme];
    }
    if (*duration < LS_MIN_PHONEME_DURATION) *duration = LS_MIN_PHONEME_DURATION;
    if (*duration > LS_MAX_PHONEME_DURATION) *duration = LS_MAX_PHONEME_DURATION;
}

void ls_viseme_blendshape_map(ebs_viseme_id_t viseme, float weight,
                               ebs_blendshapes_t* bs)
{
    ebs_viseme_to_blendshapes(viseme, weight, bs);
}

void ls_build_default_viseme_table(void)
{
}

float ls_compute_audio_energy(const float* audio, int length)
{
    if (!audio || length <= 0) return 0.0f;
    float energy = 0.0f;
    for (int i = 0; i < length; i++) {
        energy += audio[i] * audio[i];
    }
    return sqrtf(energy / length);
}

int ls_detect_voice_activity(const float* audio, int length, float threshold)
{
    float energy = ls_compute_audio_energy(audio, length);
    return energy > threshold ? 1 : 0;
}

void ls_estimate_pitch(const float* audio, int length, float sample_rate,
                       float* pitch, float* confidence)
{
    if (!audio || length < 10) {
        *pitch = 0.0f;
        *confidence = 0.0f;
        return;
    }

    float best_pitch = 0.0f;
    float best_score = 0.0f;

    for (int p = 40; p <= 400; p++) {
        float period = sample_rate / (float)p;
        if (period < 2 || period > length) continue;

        float score = 0.0f;
        int n = (int)(length - period);
        for (int i = 0; i < n; i++) {
            score += audio[i] * audio[i + (int)period];
        }
        score = fabsf(score) / n;

        if (score > best_score) {
            best_score = score;
            best_pitch = (float)p;
        }
    }

    *pitch = (best_score > 0.1f) ? best_pitch : 0.0f;
    *confidence = best_score;
}
