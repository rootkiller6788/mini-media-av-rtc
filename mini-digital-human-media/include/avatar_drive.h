#ifndef AVATAR_DRIVE_H
#define AVATAR_DRIVE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include "face_landmark.h"
#include "body_pose.h"
#include "expression_bs.h"
#include "lip_sync.h"

#define AD_MAX_BONES 128
#define AD_MAX_JOINTS 64
#define AD_MAX_IK_ITERATIONS 50
#define AD_IK_TOLERANCE 0.001f
#define AD_IDLE_ANIM_COUNT 8
#define AD_MAX_RETARGET_MAP 128

typedef struct {
    float x, y, z;
} ad_vec3_t;

typedef struct {
    float x, y, z, w;
} ad_quat_t;

typedef struct {
    ad_vec3_t translation;
    ad_quat_t rotation;
    ad_vec3_t scale;
} ad_transform_t;

typedef struct {
    char name[32];
    int parent_index;
    ad_transform_t bind_pose;
    ad_transform_t current_pose;
    ad_vec3_t local_position;
    ad_quat_t local_rotation;
    int driven_by_landmark;
} ad_bone_t;

typedef struct {
    ad_bone_t bones[AD_MAX_BONES];
    int bone_count;
    int root_index;
} ad_skeleton_t;

typedef enum {
    AD_JOINT_ROOT = 0,
    AD_JOINT_SPINE,
    AD_JOINT_SPINE1,
    AD_JOINT_SPINE2,
    AD_JOINT_NECK,
    AD_JOINT_HEAD,
    AD_JOINT_LEFT_SHOULDER,
    AD_JOINT_LEFT_ARM,
    AD_JOINT_LEFT_FOREARM,
    AD_JOINT_LEFT_HAND,
    AD_JOINT_RIGHT_SHOULDER,
    AD_JOINT_RIGHT_ARM,
    AD_JOINT_RIGHT_FOREARM,
    AD_JOINT_RIGHT_HAND,
    AD_JOINT_LEFT_HIP,
    AD_JOINT_LEFT_UPLEG,
    AD_JOINT_LEFT_LEG,
    AD_JOINT_LEFT_FOOT,
    AD_JOINT_RIGHT_HIP,
    AD_JOINT_RIGHT_UPLEG,
    AD_JOINT_RIGHT_LEG,
    AD_JOINT_RIGHT_FOOT,
    AD_JOINT_COUNT
} ad_joint_id_t;

typedef struct {
    int joint_count;
    int joint_parents[AD_MAX_JOINTS];
    ad_vec3_t joint_positions[AD_MAX_JOINTS];
    ad_quat_t joint_rotations[AD_MAX_JOINTS];
    float joint_lengths[AD_MAX_JOINTS];
    int solver_iterations;
    float solver_tolerance;
} ad_ik_chain_t;

typedef struct {
    ad_ik_chain_t chain;
    ad_vec3_t target;
    ad_vec3_t pole_vector;
    int target_joint;
    int is_active;
} ad_ik_solver_t;

typedef struct {
    int landmark_index;
    int bone_index;
    float influence;
    ad_vec3_t offset;
    int axis_mask;
} ad_retarget_map_t;

typedef struct {
    ad_retarget_map_t maps[AD_MAX_RETARGET_MAP];
    int map_count;
    float smoothing;
    float scale_factor;
} ad_retarget_config_t;

typedef struct {
    ad_vec3_t root_position;
    ad_quat_t root_rotation;
    ebs_blendshapes_t blendshapes;
    bp_body_t body;
    fl_face_mesh_t face;
    ad_skeleton_t skeleton;
    ad_retarget_config_t retarget;
    int has_face;
    int has_body;
    int has_expression;
} ad_avatar_state_t;

typedef struct {
    ad_vec3_t position;
    ad_quat_t rotation;
    ad_vec3_t velocity;
    ad_vec3_t angular_velocity;
    float timestamp;
    int frame_id;
} ad_idle_anim_key_t;

typedef struct {
    ad_idle_anim_key_t keys[120];
    int key_count;
    float duration;
    float loop_time;
    char name[32];
} ad_idle_animation_t;

typedef struct {
    ad_idle_animation_t animations[AD_IDLE_ANIM_COUNT];
    int anim_count;
    int current_anim;
    float current_time;
    float idle_threshold;
    float idle_timeout;
    float last_input_time;
    int is_idle;
} ad_idle_state_t;

typedef struct {
    ad_skeleton_t skeleton;
    ad_ik_solver_t ik_solver;
    ad_retarget_config_t retarget;
    ad_avatar_state_t avatar;
    ad_idle_state_t idle;
    ls_sync_state_t lip_sync;
    float animation_blend;
    float global_time;
} ad_driver_t;

void ad_init_driver(ad_driver_t* driver);
void ad_init_skeleton(ad_skeleton_t* skeleton);
int ad_add_bone(ad_skeleton_t* skeleton, const char* name, int parent_index);
void ad_set_bone_bind_pose(ad_skeleton_t* skeleton, int bone_index,
                           const ad_transform_t* pose);
void ad_build_default_skeleton(ad_skeleton_t* skeleton);

void ad_vec3_add(ad_vec3_t* out, const ad_vec3_t* a, const ad_vec3_t* b);
void ad_vec3_sub(ad_vec3_t* out, const ad_vec3_t* a, const ad_vec3_t* b);
void ad_vec3_scale(ad_vec3_t* out, const ad_vec3_t* a, float s);
float ad_vec3_length(const ad_vec3_t* v);
float ad_vec3_dot(const ad_vec3_t* a, const ad_vec3_t* b);
void ad_vec3_cross(ad_vec3_t* out, const ad_vec3_t* a, const ad_vec3_t* b);
void ad_vec3_normalize(ad_vec3_t* out, const ad_vec3_t* v);

void ad_quat_identity(ad_quat_t* q);
void ad_quat_from_axis_angle(ad_quat_t* q, const ad_vec3_t* axis, float angle);
void ad_quat_multiply(ad_quat_t* out, const ad_quat_t* a, const ad_quat_t* b);
void ad_quat_slerp(ad_quat_t* out, const ad_quat_t* a, const ad_quat_t* b, float t);

void ad_ik_solve(ad_ik_solver_t* solver, const ad_skeleton_t* skeleton);
void ad_ik_ccd(ad_ik_chain_t* chain, const ad_vec3_t* target, int max_iter);
void ad_ik_fabrik(ad_ik_chain_t* chain, const ad_vec3_t* target, float tolerance,
                  int max_iter);

void ad_fk_solve(const ad_skeleton_t* skeleton, ad_vec3_t* positions,
                 ad_quat_t* rotations, int bone_count);
void ad_fk_transform(const ad_skeleton_t* skeleton, int bone_index,
                     ad_transform_t* world_transform);

void ad_retarget_init(ad_retarget_config_t* config);
void ad_retarget_add_map(ad_retarget_config_t* config, int landmark, int bone,
                         float influence);
void ad_retarget_apply(const ad_retarget_config_t* config,
                       const bp_body_t* body, ad_skeleton_t* skeleton);
void ad_retarget_face(const fl_face_mesh_t* face, const ebs_blendshapes_t* expression,
                      ad_skeleton_t* skeleton);

void ad_drive_face_body(ad_driver_t* driver, const fl_face_mesh_t* face,
                        const bp_body_t* body, const ebs_blendshapes_t* expression,
                        ad_skeleton_t* out_skeleton);
void ad_drive_audio(ad_driver_t* driver, const float* audio, int audio_len,
                    float sample_rate, ad_skeleton_t* out_skeleton);

void ad_idle_init(ad_idle_state_t* idle);
void ad_idle_add_animation(ad_idle_state_t* idle, const ad_idle_animation_t* anim);
void ad_idle_update(ad_idle_state_t* idle, float dt, int has_input);
void ad_idle_get_pose(const ad_idle_state_t* idle, ad_skeleton_t* skeleton);

void ad_animate_skeleton(ad_skeleton_t* skeleton, float dt);
void ad_blend_skeletons(const ad_skeleton_t* a, const ad_skeleton_t* b,
                        float t, ad_skeleton_t* out);
void ad_copy_skeleton(const ad_skeleton_t* src, ad_skeleton_t* dst);

void ad_landmark_to_bone_rotation(const ad_vec3_t* landmark,
                                  const ad_vec3_t* parent,
                                  ad_quat_t* rotation);
void ad_smooth_skeleton(const ad_skeleton_t* current, ad_skeleton_t* smoothed,
                        float alpha);

void ad_drive_multimodal(ad_driver_t* driver,
                         const fl_face_mesh_t* face,
                         const bp_body_t* body,
                         const ebs_blendshapes_t* expression,
                         const float* audio, int audio_len,
                         float sample_rate, float dt,
                         ad_skeleton_t* out_skeleton);

int ad_bone_index_by_name(const ad_skeleton_t* skeleton, const char* name);
void ad_skeleton_reset_pose(ad_skeleton_t* skeleton);
void ad_skeleton_apply_transforms(ad_skeleton_t* skeleton);

#ifdef __cplusplus
}
#endif

#endif
