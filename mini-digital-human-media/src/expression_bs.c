#include "expression_bs.h"
#include <string.h>
#include <math.h>
#include <stdio.h>

static const char* ebs_blendshape_names[EBS_BLENDSHAPE_COUNT] = {
    "browDownLeft", "browDownRight", "browInnerUp",
    "browOuterUpLeft", "browOuterUpRight", "cheekPuff",
    "cheekSquintLeft", "cheekSquintRight",
    "eyeBlinkLeft", "eyeBlinkRight",
    "eyeLookDownLeft", "eyeLookDownRight",
    "eyeLookInLeft", "eyeLookInRight",
    "eyeLookOutLeft", "eyeLookOutRight",
    "eyeLookUpLeft", "eyeLookUpRight",
    "eyeSquintLeft", "eyeSquintRight",
    "eyeWideLeft", "eyeWideRight",
    "jawOpen", "jawForward", "jawLeft", "jawRight",
    "mouthClose", "mouthDimpleLeft", "mouthDimpleRight",
    "mouthFrownLeft", "mouthFrownRight",
    "mouthFunnel", "mouthLeft", "mouthRight",
    "mouthLowerDownLeft", "mouthLowerDownRight",
    "mouthPressLeft", "mouthPressRight",
    "mouthPucker", "mouthRollLower", "mouthRollUpper",
    "mouthShrugLower", "mouthShrugUpper",
    "mouthSmileLeft", "mouthSmileRight",
    "mouthStretchLeft", "mouthStretchRight",
    "mouthUpperUpLeft", "mouthUpperUpRight",
    "noseSneerLeft", "noseSneerRight",
    "tongueOut"
};

static const char* ebs_expr_names[EBS_EXPR_COUNT] = {
    "neutral", "happy", "sad", "angry", "surprised", "fear", "disgust"
};

static const char* ebs_viseme_names[EBS_VISEME_COUNT] = {
    "sil", "PP", "FF", "TH", "DD", "kk", "CH",
    "SS", "nn", "RR", "aa", "E", "I", "O", "U", "MB"
};

static const int ebs_preset_table[EBS_EXPR_COUNT][16] = {
    { -1 },
    { EBS_MOUTH_SMILE_LEFT, 0, EBS_MOUTH_SMILE_RIGHT, 1, EBS_CHEEK_SQUINT_LEFT, 3,
      EBS_CHEEK_SQUINT_RIGHT, 4, EBS_BROW_OUTER_UP_LEFT, 6, -1 },
    { EBS_MOUTH_FROWN_LEFT, 0, EBS_MOUTH_FROWN_RIGHT, 1, EBS_BROW_INNER_UP, 4,
      EBS_EYE_LOOK_DOWN_LEFT, 6, -1 },
    { EBS_BROW_DOWN_LEFT, 0, EBS_BROW_DOWN_RIGHT, 1,
      EBS_MOUTH_PRESS_LEFT, 3, EBS_MOUTH_PRESS_RIGHT, 4, -1 },
    { EBS_EYE_WIDE_LEFT, 0, EBS_EYE_WIDE_RIGHT, 1, EBS_BROW_OUTER_UP_LEFT, 3,
      EBS_BROW_OUTER_UP_RIGHT, 4, EBS_JAW_OPEN, 6, EBS_MOUTH_FUNNEL, 8, -1 },
    { EBS_EYE_WIDE_LEFT, 0, EBS_EYE_WIDE_RIGHT, 1, EBS_BROW_INNER_UP, 3,
      EBS_MOUTH_STRETCH_LEFT, 5, -1 },
    { EBS_NOSE_SNEER_LEFT, 0, EBS_NOSE_SNEER_RIGHT, 1,
      EBS_MOUTH_UPPER_UP_LEFT, 3, EBS_MOUTH_UPPER_UP_RIGHT, 4, -1 }
};

void ebs_init_face_rig(ebs_face_rig_t* rig)
{
    memset(rig, 0, sizeof(ebs_face_rig_t));
}

void ebs_set_blendshape_weight(ebs_blendshapes_t* bs, int index, float weight)
{
    if (index < 0 || index >= EBS_BLENDSHAPE_COUNT) return;
    if (weight < 0.0f) weight = 0.0f;
    if (weight > 1.0f) weight = 1.0f;
    bs->weights[index] = weight;
}

float ebs_get_blendshape_weight(const ebs_blendshapes_t* bs, int index)
{
    if (index < 0 || index >= EBS_BLENDSHAPE_COUNT) return 0.0f;
    return bs->weights[index];
}

void ebs_blend_blendshapes(const ebs_blendshapes_t* a, const ebs_blendshapes_t* b,
                           float t, ebs_blendshapes_t* out)
{
    for (int i = 0; i < EBS_BLENDSHAPE_COUNT; i++) {
        out->weights[i] = a->weights[i] + (b->weights[i] - a->weights[i]) * t;
    }
}

void ebs_combine_blendshapes(const ebs_blendshapes_t* layers[], const float* weights,
                             int layer_count, ebs_blendshapes_t* out)
{
    memset(out, 0, sizeof(ebs_blendshapes_t));
    for (int i = 0; i < EBS_BLENDSHAPE_COUNT; i++) {
        float val = 0.0f;
        for (int j = 0; j < layer_count; j++) {
            val += layers[j]->weights[i] * weights[j];
        }
        if (val > 1.0f) val = 1.0f;
        out->weights[i] = val;
    }
}

void ebs_reset_blendshapes(ebs_blendshapes_t* bs)
{
    memset(bs, 0, sizeof(ebs_blendshapes_t));
}

void ebs_copy_blendshapes(const ebs_blendshapes_t* src, ebs_blendshapes_t* dst)
{
    memcpy(dst, src, sizeof(ebs_blendshapes_t));
}

void ebs_classify_expression(const ebs_blendshapes_t* bs,
                             ebs_expression_t* expression, float* confidence)
{
    float scores[EBS_EXPR_COUNT] = { 0 };
    scores[EBS_EXPR_HAPPY] = bs->weights[EBS_MOUTH_SMILE_LEFT] +
                             bs->weights[EBS_MOUTH_SMILE_RIGHT];
    scores[EBS_EXPR_SAD] = bs->weights[EBS_MOUTH_FROWN_LEFT] +
                           bs->weights[EBS_MOUTH_FROWN_RIGHT] +
                           bs->weights[EBS_BROW_INNER_UP];
    scores[EBS_EXPR_ANGRY] = bs->weights[EBS_BROW_DOWN_LEFT] +
                             bs->weights[EBS_BROW_DOWN_RIGHT] +
                             bs->weights[EBS_MOUTH_PRESS_LEFT];
    scores[EBS_EXPR_SURPRISED] = bs->weights[EBS_EYE_WIDE_LEFT] +
                                 bs->weights[EBS_EYE_WIDE_RIGHT] +
                                 bs->weights[EBS_JAW_OPEN] +
                                 bs->weights[EBS_BROW_OUTER_UP_LEFT];
    scores[EBS_EXPR_FEAR] = bs->weights[EBS_EYE_WIDE_LEFT] +
                            bs->weights[EBS_EYE_WIDE_RIGHT] +
                            bs->weights[EBS_MOUTH_STRETCH_LEFT];
    scores[EBS_EXPR_DISGUST] = bs->weights[EBS_NOSE_SNEER_LEFT] +
                               bs->weights[EBS_MOUTH_UPPER_UP_LEFT];

    float max_score = 0.0f;
    int max_idx = EBS_EXPR_NEUTRAL;

    for (int i = 0; i < EBS_EXPR_COUNT; i++) {
        if (scores[i] > max_score) {
            max_score = scores[i];
            max_idx = i;
        }
    }

    if (max_score < 0.2f) {
        *expression = EBS_EXPR_NEUTRAL;
        *confidence = 1.0f - max_score;
    } else {
        *expression = (ebs_expression_t)max_idx;
        *confidence = max_score / 3.0f;
        if (*confidence > 1.0f) *confidence = 1.0f;
    }
}

const char* ebs_expression_name(ebs_expression_t expr)
{
    if (expr < 0 || expr >= EBS_EXPR_COUNT) return "unknown";
    return ebs_expr_names[expr];
}

const char* ebs_blendshape_name(int index)
{
    if (index < 0 || index >= EBS_BLENDSHAPE_COUNT) return "unknown";
    return ebs_blendshape_names[index];
}

const char* ebs_viseme_name(ebs_viseme_id_t viseme)
{
    if (viseme < 0 || viseme >= EBS_VISEME_COUNT) return "unknown";
    return ebs_viseme_names[viseme];
}

void ebs_preset_expression(ebs_blendshapes_t* bs, ebs_expression_t expr, float strength)
{
    if (expr <= EBS_EXPR_NEUTRAL || expr >= EBS_EXPR_COUNT) {
        ebs_reset_blendshapes(bs);
        return;
    }

    const int* preset = ebs_preset_table[expr];
    for (int i = 0; preset[i] != -1 && i < 16; i += 2) {
        int bs_id = preset[i];
        float val = preset[i + 1] / 10.0f * strength;
        bs->weights[bs_id] = val;
    }
}

void ebs_expression_mix(const ebs_blendshapes_t* bs, float out_mix[EBS_EXPR_COUNT])
{
    ebs_expression_t expr;
    float conf;
    ebs_classify_expression(bs, &expr, &conf);

    memset(out_mix, 0, sizeof(float) * EBS_EXPR_COUNT);
    out_mix[expr] = conf;
}

void ebs_rig_init(ebs_rig_t* rig)
{
    memset(rig, 0, sizeof(ebs_rig_t));
}

void ebs_rig_add_morph_target(ebs_rig_t* rig, const ebs_blendshapes_t* target)
{
    if (rig->target_count >= EBS_MAX_MORPH_TARGETS) return;
    ebs_copy_blendshapes(target, &rig->targets[rig->target_count].targets[0]);
    rig->target_count++;
}

void ebs_rig_reset(ebs_rig_t* rig)
{
    memset(rig, 0, sizeof(ebs_rig_t));
}

void ebs_rig_apply_weights(ebs_rig_t* rig, const float* weights, int count)
{
    ebs_blendshapes_t result;
    ebs_reset_blendshapes(&result);

    int n = count < rig->target_count ? count : rig->target_count;
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < EBS_BLENDSHAPE_COUNT; j++) {
            result.weights[j] += rig->targets[i].targets[0].weights[j] * weights[i];
        }
    }

    ebs_clamp_blendshapes(&result);
    ebs_copy_blendshapes(&result, &rig->current);
}

void ebs_viseme_mapping_init(ebs_viseme_mapping_t* mapping)
{
    memset(mapping, 0, sizeof(ebs_viseme_mapping_t));
}

void ebs_viseme_to_blendshapes(ebs_viseme_id_t viseme, float weight,
                                ebs_blendshapes_t* out)
{
    ebs_reset_blendshapes(out);

    switch (viseme) {
    case EBS_VISEME_SIL:
        break;
    case EBS_VISEME_PP:
        out->weights[EBS_MOUTH_PRESS_LEFT] = 0.8f * weight;
        out->weights[EBS_MOUTH_PRESS_RIGHT] = 0.8f * weight;
        break;
    case EBS_VISEME_FF:
        out->weights[EBS_MOUTH_FUNNEL] = 0.7f * weight;
        out->weights[EBS_MOUTH_LOWER_DOWN_LEFT] = 0.5f * weight;
        break;
    case EBS_VISEME_TH:
        out->weights[EBS_TONGUE_OUT] = 0.6f * weight;
        out->weights[EBS_JAW_OPEN] = 0.2f * weight;
        break;
    case EBS_VISEME_DD:
        out->weights[EBS_JAW_OPEN] = 0.3f * weight;
        out->weights[EBS_MOUTH_CLOSE] = 0.3f * weight;
        break;
    case EBS_VISEME_KK:
        out->weights[EBS_JAW_OPEN] = 0.4f * weight;
        out->weights[EBS_MOUTH_STRETCH_LEFT] = 0.3f * weight;
        out->weights[EBS_MOUTH_STRETCH_RIGHT] = 0.3f * weight;
        break;
    case EBS_VISEME_CH:
        out->weights[EBS_JAW_OPEN] = 0.3f * weight;
        out->weights[EBS_MOUTH_PUCKER] = 0.6f * weight;
        break;
    case EBS_VISEME_SS:
        out->weights[EBS_MOUTH_STRETCH_LEFT] = 0.5f * weight;
        out->weights[EBS_MOUTH_STRETCH_RIGHT] = 0.5f * weight;
        out->weights[EBS_JAW_OPEN] = 0.2f * weight;
        break;
    case EBS_VISEME_NN:
        out->weights[EBS_JAW_OPEN] = 0.2f * weight;
        out->weights[EBS_MOUTH_PRESS_LEFT] = 0.4f * weight;
        break;
    case EBS_VISEME_RR:
        out->weights[EBS_MOUTH_PUCKER] = 0.7f * weight;
        out->weights[EBS_MOUTH_LOWER_DOWN_LEFT] = 0.3f * weight;
        break;
    case EBS_VISEME_AA:
        out->weights[EBS_JAW_OPEN] = 0.8f * weight;
        out->weights[EBS_MOUTH_FUNNEL] = 0.3f * weight;
        break;
    case EBS_VISEME_E:
        out->weights[EBS_MOUTH_STRETCH_LEFT] = 0.6f * weight;
        out->weights[EBS_MOUTH_STRETCH_RIGHT] = 0.6f * weight;
        out->weights[EBS_JAW_OPEN] = 0.3f * weight;
        break;
    case EBS_VISEME_I:
        out->weights[EBS_MOUTH_STRETCH_LEFT] = 0.4f * weight;
        out->weights[EBS_MOUTH_STRETCH_RIGHT] = 0.4f * weight;
        out->weights[EBS_MOUTH_SMILE_LEFT] = 0.3f * weight;
        break;
    case EBS_VISEME_O:
        out->weights[EBS_JAW_OPEN] = 0.5f * weight;
        out->weights[EBS_MOUTH_FUNNEL] = 0.8f * weight;
        break;
    case EBS_VISEME_U:
        out->weights[EBS_MOUTH_PUCKER] = 0.9f * weight;
        out->weights[EBS_MOUTH_UPPER_UP_RIGHT] = 0.4f * weight;
        break;
    case EBS_VISEME_MB:
        out->weights[EBS_MOUTH_PRESS_LEFT] = 0.9f * weight;
        out->weights[EBS_MOUTH_PRESS_RIGHT] = 0.9f * weight;
        break;
    default:
        break;
    }
}

void ebs_viseme_get_targets(ebs_viseme_id_t viseme,
                             int* blendshape_ids, float* values, int* count)
{
    ebs_blendshapes_t bs;
    ebs_viseme_to_blendshapes(viseme, 1.0f, &bs);

    *count = 0;
    for (int i = 0; i < EBS_BLENDSHAPE_COUNT; i++) {
        if (bs.weights[i] > 0.01f) {
            blendshape_ids[*count] = i;
            values[*count] = bs.weights[i];
            (*count)++;
        }
    }
}

int ebs_phoneme_to_viseme(const char* phoneme)
{
    if (!phoneme) return EBS_VISEME_SIL;
    switch (phoneme[0]) {
    case 'a': return EBS_VISEME_AA;
    case 'e': return EBS_VISEME_E;
    case 'i': return EBS_VISEME_I;
    case 'o': return EBS_VISEME_O;
    case 'u': return EBS_VISEME_U;
    case 'p': case 'b': return phoneme[1] ? EBS_VISEME_PP : EBS_VISEME_PP;
    case 'm': return EBS_VISEME_MB;
    case 'f': case 'v': return EBS_VISEME_FF;
    case 't': case 'd': return EBS_VISEME_DD;
    case 'k': case 'g': return EBS_VISEME_KK;
    case 's': case 'z': return EBS_VISEME_SS;
    case 'n': return EBS_VISEME_NN;
    case 'r': return EBS_VISEME_RR;
    case 'c': return EBS_VISEME_CH;
    case 't': return EBS_VISEME_TH;
    case 'w': return EBS_VISEME_U;
    default: return EBS_VISEME_SIL;
    }
}

void ebs_viseme_blend(ebs_viseme_id_t viseme_a, ebs_viseme_id_t viseme_b,
                      float t, ebs_blendshapes_t* out)
{
    ebs_blendshapes_t bs_a, bs_b;
    ebs_viseme_to_blendshapes(viseme_a, 1.0f, &bs_a);
    ebs_viseme_to_blendshapes(viseme_b, 1.0f, &bs_b);
    ebs_blend_blendshapes(&bs_a, &bs_b, t, out);
}

void ebs_clamp_blendshapes(ebs_blendshapes_t* bs)
{
    for (int i = 0; i < EBS_BLENDSHAPE_COUNT; i++) {
        if (bs->weights[i] < 0.0f) bs->weights[i] = 0.0f;
        if (bs->weights[i] > 1.0f) bs->weights[i] = 1.0f;
    }
}

void ebs_scale_blendshapes(ebs_blendshapes_t* bs, float factor)
{
    for (int i = 0; i < EBS_BLENDSHAPE_COUNT; i++) {
        bs->weights[i] *= factor;
    }
    ebs_clamp_blendshapes(bs);
}

float ebs_blendshape_distance(const ebs_blendshapes_t* a, const ebs_blendshapes_t* b)
{
    float dist = 0.0f;
    for (int i = 0; i < EBS_BLENDSHAPE_COUNT; i++) {
        float d = a->weights[i] - b->weights[i];
        dist += d * d;
    }
    return sqrtf(dist);
}
