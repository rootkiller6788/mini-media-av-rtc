#ifndef EXPRESSION_BS_H
#define EXPRESSION_BS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

#define EBS_BLENDSHAPE_COUNT 52
#define EBS_VISEME_COUNT 16
#define EBS_MAX_MORPH_TARGETS 128

typedef enum {
    EBS_BROW_DOWN_LEFT = 0,
    EBS_BROW_DOWN_RIGHT,
    EBS_BROW_INNER_UP,
    EBS_BROW_OUTER_UP_LEFT,
    EBS_BROW_OUTER_UP_RIGHT,
    EBS_CHEEK_PUFF,
    EBS_CHEEK_SQUINT_LEFT,
    EBS_CHEEK_SQUINT_RIGHT,
    EBS_EYE_BLINK_LEFT,
    EBS_EYE_BLINK_RIGHT,
    EBS_EYE_LOOK_DOWN_LEFT,
    EBS_EYE_LOOK_DOWN_RIGHT,
    EBS_EYE_LOOK_IN_LEFT,
    EBS_EYE_LOOK_IN_RIGHT,
    EBS_EYE_LOOK_OUT_LEFT,
    EBS_EYE_LOOK_OUT_RIGHT,
    EBS_EYE_LOOK_UP_LEFT,
    EBS_EYE_LOOK_UP_RIGHT,
    EBS_EYE_SQUINT_LEFT,
    EBS_EYE_SQUINT_RIGHT,
    EBS_EYE_WIDE_LEFT,
    EBS_EYE_WIDE_RIGHT,
    EBS_JAW_OPEN,
    EBS_JAW_FORWARD,
    EBS_JAW_LEFT,
    EBS_JAW_RIGHT,
    EBS_MOUTH_CLOSE,
    EBS_MOUTH_DIMPLE_LEFT,
    EBS_MOUTH_DIMPLE_RIGHT,
    EBS_MOUTH_FROWN_LEFT,
    EBS_MOUTH_FROWN_RIGHT,
    EBS_MOUTH_FUNNEL,
    EBS_MOUTH_LEFT,
    EBS_MOUTH_LOWER_DOWN_LEFT,
    EBS_MOUTH_LOWER_DOWN_RIGHT,
    EBS_MOUTH_PRESS_LEFT,
    EBS_MOUTH_PRESS_RIGHT,
    EBS_MOUTH_PUCKER,
    EBS_MOUTH_RIGHT,
    EBS_MOUTH_ROLL_LOWER,
    EBS_MOUTH_ROLL_UPPER,
    EBS_MOUTH_SHRUG_LOWER,
    EBS_MOUTH_SHRUG_UPPER,
    EBS_MOUTH_SMILE_LEFT,
    EBS_MOUTH_SMILE_RIGHT,
    EBS_MOUTH_STRETCH_LEFT,
    EBS_MOUTH_STRETCH_RIGHT,
    EBS_MOUTH_UPPER_UP_LEFT,
    EBS_MOUTH_UPPER_UP_RIGHT,
    EBS_NOSE_SNEER_LEFT,
    EBS_NOSE_SNEER_RIGHT,
    EBS_TONGUE_OUT
} ebs_blendshape_id_t;

typedef enum {
    EBS_VISEME_SIL = 0,
    EBS_VISEME_PP,
    EBS_VISEME_FF,
    EBS_VISEME_TH,
    EBS_VISEME_DD,
    EBS_VISEME_KK,
    EBS_VISEME_CH,
    EBS_VISEME_SS,
    EBS_VISEME_NN,
    EBS_VISEME_RR,
    EBS_VISEME_AA,
    EBS_VISEME_E,
    EBS_VISEME_I,
    EBS_VISEME_O,
    EBS_VISEME_U,
    EBS_VISEME_MB
} ebs_viseme_id_t;

typedef enum {
    EBS_EXPR_NEUTRAL = 0,
    EBS_EXPR_HAPPY,
    EBS_EXPR_SAD,
    EBS_EXPR_ANGRY,
    EBS_EXPR_SURPRISED,
    EBS_EXPR_FEAR,
    EBS_EXPR_DISGUST,
    EBS_EXPR_COUNT
} ebs_expression_t;

typedef struct {
    float weights[EBS_BLENDSHAPE_COUNT];
} ebs_blendshapes_t;

typedef struct {
    ebs_blendshapes_t blendshapes;
    float expression_mix[EBS_EXPR_COUNT];
    ebs_expression_t dominant_expression;
    float dominant_confidence;
    float timestamp;
} ebs_face_rig_t;

typedef struct {
    float weights[EBS_BLENDSHAPE_COUNT];
    int num_blendshapes;
} ebs_morph_target_t;

typedef struct {
    ebs_morph_target_t targets[EBS_MAX_MORPH_TARGETS];
    int target_count;
    ebs_blendshapes_t current;
    ebs_blendshapes_t base;
    ebs_blendshapes_t delta;
} ebs_rig_t;

typedef struct {
    float viseme_weights[EBS_BLENDSHAPE_COUNT];
} ebs_viseme_mapping_t;

void ebs_init_face_rig(ebs_face_rig_t* rig);
void ebs_set_blendshape_weight(ebs_blendshapes_t* bs, int index, float weight);
float ebs_get_blendshape_weight(const ebs_blendshapes_t* bs, int index);
void ebs_blend_blendshapes(const ebs_blendshapes_t* a, const ebs_blendshapes_t* b,
                           float t, ebs_blendshapes_t* out);
void ebs_combine_blendshapes(const ebs_blendshapes_t* layers[], const float* weights,
                             int layer_count, ebs_blendshapes_t* out);
void ebs_reset_blendshapes(ebs_blendshapes_t* bs);
void ebs_copy_blendshapes(const ebs_blendshapes_t* src, ebs_blendshapes_t* dst);

void ebs_classify_expression(const ebs_blendshapes_t* bs,
                             ebs_expression_t* expression, float* confidence);
const char* ebs_expression_name(ebs_expression_t expr);
const char* ebs_blendshape_name(int index);
const char* ebs_viseme_name(ebs_viseme_id_t viseme);

void ebs_preset_expression(ebs_blendshapes_t* bs, ebs_expression_t expr, float strength);
void ebs_expression_mix(const ebs_blendshapes_t* bs, float out_mix[EBS_EXPR_COUNT]);

void ebs_rig_init(ebs_rig_t* rig);
void ebs_rig_add_morph_target(ebs_rig_t* rig, const ebs_blendshapes_t* target);
void ebs_rig_reset(ebs_rig_t* rig);
void ebs_rig_apply_weights(ebs_rig_t* rig, const float* weights, int count);

void ebs_viseme_mapping_init(ebs_viseme_mapping_t* mapping);
void ebs_viseme_to_blendshapes(ebs_viseme_id_t viseme, float weight,
                               ebs_blendshapes_t* out);
void ebs_viseme_get_targets(ebs_viseme_id_t viseme,
                            int* blendshape_ids, float* values, int* count);
int ebs_phoneme_to_viseme(const char* phoneme);
void ebs_viseme_blend(ebs_viseme_id_t viseme_a, ebs_viseme_id_t viseme_b,
                      float t, ebs_blendshapes_t* out);
void ebs_clamp_blendshapes(ebs_blendshapes_t* bs);
void ebs_scale_blendshapes(ebs_blendshapes_t* bs, float factor);
float ebs_blendshape_distance(const ebs_blendshapes_t* a, const ebs_blendshapes_t* b);

#ifdef __cplusplus
}
#endif

#endif
