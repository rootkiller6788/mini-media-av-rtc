#ifndef LIP_SYNC_H
#define LIP_SYNC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include "expression_bs.h"

#define LS_MAX_PHONEMES 64
#define LS_VISEME_COUNT 16
#define LS_MAX_SEQUENCE 1024
#define LS_FORMANT_COUNT 3
#define LS_FRAME_SIZE 512
#define LS_SAMPLE_RATE 16000.0f
#define LS_MIN_PHONEME_DURATION 0.03f
#define LS_MAX_PHONEME_DURATION 0.5f

typedef enum {
    LS_PHONEME_SIL = 0,
    LS_PHONEME_AA, LS_PHONEME_AE, LS_PHONEME_AH,
    LS_PHONEME_AO, LS_PHONEME_AW, LS_PHONEME_AY,
    LS_PHONEME_B, LS_PHONEME_CH, LS_PHONEME_D,
    LS_PHONEME_DH, LS_PHONEME_EH, LS_PHONEME_ER,
    LS_PHONEME_EY, LS_PHONEME_F, LS_PHONEME_G,
    LS_PHONEME_HH, LS_PHONEME_IH, LS_PHONEME_IY,
    LS_PHONEME_JH, LS_PHONEME_K, LS_PHONEME_L,
    LS_PHONEME_M, LS_PHONEME_N, LS_PHONEME_NG,
    LS_PHONEME_OW, LS_PHONEME_OY, LS_PHONEME_P,
    LS_PHONEME_R, LS_PHONEME_S, LS_PHONEME_SH,
    LS_PHONEME_T, LS_PHONEME_TH, LS_PHONEME_UH,
    LS_PHONEME_UW, LS_PHONEME_V, LS_PHONEME_W,
    LS_PHONEME_Y, LS_PHONEME_Z, LS_PHONEME_ZH,
    LS_PHONEME_COUNT
} ls_phoneme_t;

typedef struct {
    float f1, f2, f3;
    float bandwidth[3];
    float energy;
    float pitch;
    int is_voiced;
} ls_formants_t;

typedef struct {
    ls_phoneme_t phoneme;
    float start_time;
    float end_time;
    float duration;
    float confidence;
    int stress;
} ls_phoneme_event_t;

typedef struct {
    ebs_viseme_id_t viseme;
    float weight;
    float start_time;
    float end_time;
    float transition_in;
    float transition_out;
} ls_viseme_frame_t;

typedef struct {
    ls_viseme_frame_t frames[LS_MAX_SEQUENCE];
    int frame_count;
    float total_duration;
    float current_time;
    int sample_rate;
} ls_viseme_sequence_t;

typedef struct {
    ls_viseme_frame_t prev_frame;
    float coarticulation_factor;
    int is_initialized;
    float hold_time;
} ls_articulation_state_t;

typedef struct {
    ebs_blendshapes_t current;
    ebs_blendshapes_t target;
    ls_articulation_state_t articulation;
    float transition_speed;
    float jaw_open_amount;
    float lip_round_amount;
    int smoothing_enabled;
} ls_sync_state_t;

void ls_init_sync(ls_sync_state_t* state);
void ls_reset_sequence(ls_viseme_sequence_t* seq);
const char* ls_phoneme_name(ls_phoneme_t phoneme);
const char* ls_viseme_label(ebs_viseme_id_t viseme);

int ls_phoneme_to_viseme(ls_phoneme_t phoneme);
int ls_char_to_phoneme(char c, ls_phoneme_t* phoneme);
void ls_phoneme_to_formants(ls_phoneme_t phoneme, ls_formants_t* formants);

void ls_text_to_phonemes(const char* text, ls_phoneme_t* phonemes, int* count);
void ls_text_to_visemes(const char* text, ebs_viseme_id_t* visemes, int* count);
void ls_phonemes_to_visemes(const ls_phoneme_t* phonemes, int count,
                            ebs_viseme_id_t* visemes);

void ls_build_viseme_sequence(const ebs_viseme_id_t* visemes, int count,
                              const float* durations,
                              ls_viseme_sequence_t* sequence);
void ls_add_viseme_frame(ls_viseme_sequence_t* seq, ebs_viseme_id_t viseme,
                         float weight, float start, float end);
void ls_get_viseme_at_time(const ls_viseme_sequence_t* seq, float time,
                           ebs_viseme_id_t* viseme, float* weight);
void ls_interpolate_visemes(const ls_viseme_sequence_t* seq, float time,
                            ebs_blendshapes_t* out);

void ls_audio_features_to_viseme(const float* audio_frame, int frame_size,
                                 ebs_viseme_id_t* viseme, float* weight);
void ls_extract_formants(const float* audio, int length, float sample_rate,
                         ls_formants_t* formants);

void ls_coarticulation_apply(const ebs_viseme_id_t* visemes, int count,
                             ls_viseme_sequence_t* sequence);
void ls_smooth_transition(ls_articulation_state_t* state,
                          ebs_viseme_id_t target_viseme, float weight,
                          ebs_blendshapes_t* out);
void ls_sync_update(ls_sync_state_t* state, ebs_viseme_id_t viseme,
                    float weight, float dt, ebs_blendshapes_t* out);

void ls_audio_drive(const float* audio_data, int audio_length,
                    float sample_rate, ls_viseme_sequence_t* sequence,
                    ls_sync_state_t* sync, ebs_blendshapes_t* blendshapes);

void ls_generate_viseme_duration(ebs_viseme_id_t viseme, float base_duration,
                                 float* duration);
void ls_viseme_blendshape_map(ebs_viseme_id_t viseme, float weight,
                              ebs_blendshapes_t* bs);
void ls_build_default_viseme_table(void);

float ls_compute_audio_energy(const float* audio, int length);
int ls_detect_voice_activity(const float* audio, int length, float threshold);
void ls_estimate_pitch(const float* audio, int length, float sample_rate,
                       float* pitch, float* confidence);

#ifdef __cplusplus
}
#endif

#endif
