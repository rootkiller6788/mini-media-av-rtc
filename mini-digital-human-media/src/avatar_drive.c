#include "avatar_drive.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

static const char* ad_joint_names[AD_JOINT_COUNT] = {
    "root", "spine", "spine1", "spine2", "neck", "head",
    "left_shoulder", "left_arm", "left_forearm", "left_hand",
    "right_shoulder", "right_arm", "right_forearm", "right_hand",
    "left_hip", "left_upleg", "left_leg", "left_foot",
    "right_hip", "right_upleg", "right_leg", "right_foot"
};

static const int ad_default_parents[AD_JOINT_COUNT] = {
    -1, 0, 1, 2, 3, 4,
    3, 6, 7, 8,
    3, 10, 11, 12,
    0, 14, 15, 16,
    0, 18, 19, 20
};

static const ad_vec3_t ad_default_rest[AD_JOINT_COUNT] = {
    {0, 0, 0}, {0, 0.05f, 0}, {0, 0.10f, 0}, {0, 0.15f, 0},
    {0, 0.20f, 0}, {0, 0.23f, 0},
    {-0.06f, 0.14f, 0}, {-0.12f, 0.14f, 0}, {-0.18f, 0.13f, 0}, {-0.24f, 0.13f, 0},
    {0.06f, 0.14f, 0}, {0.12f, 0.14f, 0}, {0.18f, 0.13f, 0}, {0.24f, 0.13f, 0},
    {-0.04f, -0.02f, 0}, {-0.05f, -0.10f, 0}, {-0.05f, -0.19f, 0}, {-0.05f, -0.28f, 0},
    {0.04f, -0.02f, 0}, {0.05f, -0.10f, 0}, {0.05f, -0.19f, 0}, {0.05f, -0.28f, 0}
};

void ad_init_driver(ad_driver_t* driver)
{
    memset(driver, 0, sizeof(ad_driver_t));
    ad_build_default_skeleton(&driver->skeleton);
    ad_retarget_init(&driver->retarget);
    ls_init_sync(&driver->lip_sync);
    ad_idle_init(&driver->idle);
    driver->animation_blend = 1.0f;
    driver->global_time = 0.0f;
}

void ad_init_skeleton(ad_skeleton_t* skeleton)
{
    memset(skeleton, 0, sizeof(ad_skeleton_t));
    skeleton->root_index = 0;
}

int ad_add_bone(ad_skeleton_t* skeleton, const char* name, int parent_index)
{
    if (skeleton->bone_count >= AD_MAX_BONES) return -1;
    ad_bone_t* bone = &skeleton->bones[skeleton->bone_count];
    strncpy(bone->name, name, 31);
    bone->name[31] = '\0';
    bone->parent_index = parent_index;
    bone->bind_pose.translation.x = 0;
    bone->bind_pose.translation.y = 0;
    bone->bind_pose.translation.z = 0;
    bone->bind_pose.scale.x = 1;
    bone->bind_pose.scale.y = 1;
    bone->bind_pose.scale.z = 1;
    ad_quat_identity(&bone->bind_pose.rotation);
    ad_quat_identity(&bone->current_pose.rotation);
    return skeleton->bone_count++;
}

void ad_set_bone_bind_pose(ad_skeleton_t* skeleton, int bone_index,
                           const ad_transform_t* pose)
{
    if (bone_index < 0 || bone_index >= skeleton->bone_count) return;
    skeleton->bones[bone_index].bind_pose = *pose;
}

void ad_build_default_skeleton(ad_skeleton_t* skeleton)
{
    ad_init_skeleton(skeleton);
    for (int i = 0; i < AD_JOINT_COUNT; i++) {
        int idx = ad_add_bone(skeleton, ad_joint_names[i], ad_default_parents[i]);
        if (idx >= 0) {
            skeleton->bones[idx].bind_pose.translation = ad_default_rest[i];
            skeleton->bones[idx].current_pose.translation = ad_default_rest[i];
            ad_quat_identity(&skeleton->bones[idx].current_pose.rotation);
            skeleton->bones[idx].current_pose.scale.x = 1;
            skeleton->bones[idx].current_pose.scale.y = 1;
            skeleton->bones[idx].current_pose.scale.z = 1;
        }
    }
}

void ad_vec3_add(ad_vec3_t* out, const ad_vec3_t* a, const ad_vec3_t* b)
{
    out->x = a->x + b->x;
    out->y = a->y + b->y;
    out->z = a->z + b->z;
}

void ad_vec3_sub(ad_vec3_t* out, const ad_vec3_t* a, const ad_vec3_t* b)
{
    out->x = a->x - b->x;
    out->y = a->y - b->y;
    out->z = a->z - b->z;
}

void ad_vec3_scale(ad_vec3_t* out, const ad_vec3_t* a, float s)
{
    out->x = a->x * s;
    out->y = a->y * s;
    out->z = a->z * s;
}

float ad_vec3_length(const ad_vec3_t* v)
{
    return sqrtf(v->x * v->x + v->y * v->y + v->z * v->z);
}

float ad_vec3_dot(const ad_vec3_t* a, const ad_vec3_t* b)
{
    return a->x * b->x + a->y * b->y + a->z * b->z;
}

void ad_vec3_cross(ad_vec3_t* out, const ad_vec3_t* a, const ad_vec3_t* b)
{
    out->x = a->y * b->z - a->z * b->y;
    out->y = a->z * b->x - a->x * b->z;
    out->z = a->x * b->y - a->y * b->x;
}

void ad_vec3_normalize(ad_vec3_t* out, const ad_vec3_t* v)
{
    float len = ad_vec3_length(v);
    if (len > 1e-8f) {
        out->x = v->x / len;
        out->y = v->y / len;
        out->z = v->z / len;
    } else {
        out->x = 0; out->y = 1; out->z = 0;
    }
}

void ad_quat_identity(ad_quat_t* q)
{
    q->x = 0; q->y = 0; q->z = 0; q->w = 1;
}

void ad_quat_from_axis_angle(ad_quat_t* q, const ad_vec3_t* axis, float angle)
{
    float half = angle * 0.5f;
    float s = sinf(half);
    q->x = axis->x * s;
    q->y = axis->y * s;
    q->z = axis->z * s;
    q->w = cosf(half);
}

void ad_quat_multiply(ad_quat_t* out, const ad_quat_t* a, const ad_quat_t* b)
{
    out->x = a->w * b->x + a->x * b->w + a->y * b->z - a->z * b->y;
    out->y = a->w * b->y - a->x * b->z + a->y * b->w + a->z * b->x;
    out->z = a->w * b->z + a->x * b->y - a->y * b->x + a->z * b->w;
    out->w = a->w * b->w - a->x * b->x - a->y * b->y - a->z * b->z;
}

void ad_quat_slerp(ad_quat_t* out, const ad_quat_t* a, const ad_quat_t* b, float t)
{
    float cos_omega = a->x * b->x + a->y * b->y + a->z * b->z + a->w * b->w;
    int flip = 0;

    if (cos_omega < 0.0f) {
        cos_omega = -cos_omega;
        flip = 1;
    }

    float k0, k1;
    if (cos_omega > 0.9999f) {
        k0 = 1.0f - t;
        k1 = t;
    } else {
        float sin_omega = sqrtf(1.0f - cos_omega * cos_omega);
        float omega = atan2f(sin_omega, cos_omega);
        k0 = sinf((1.0f - t) * omega) / sin_omega;
        k1 = sinf(t * omega) / sin_omega;
    }

    if (flip) k1 = -k1;

    out->x = k0 * a->x + k1 * b->x;
    out->y = k0 * a->y + k1 * b->y;
    out->z = k0 * a->z + k1 * b->z;
    out->w = k0 * a->w + k1 * b->w;
}

void ad_ik_solve(ad_ik_solver_t* solver, const ad_skeleton_t* skeleton)
{
    (void)skeleton;
    if (!solver->is_active) return;

    ad_vec3_t target;
    target.x = solver->target.x;
    target.y = solver->target.y;
    target.z = solver->target.z;

    ad_ik_fabrik(&solver->chain, &target,
                 solver->chain.solver_tolerance,
                 solver->chain.solver_iterations);
}

void ad_ik_ccd(ad_ik_chain_t* chain, const ad_vec3_t* target, int max_iter)
{
    if (chain->joint_count < 2) return;

    for (int iter = 0; iter < max_iter; iter++) {
        for (int i = chain->joint_count - 2; i >= 0; i--) {
            ad_vec3_t joint_pos = chain->joint_positions[i];
            ad_vec3_t end_effector = chain->joint_positions[chain->joint_count - 1];
            ad_vec3_t target_pos = *target;

            ad_vec3_t to_target, to_end;
            ad_vec3_sub(&to_target, &target_pos, &joint_pos);
            ad_vec3_sub(&to_end, &end_effector, &joint_pos);

            ad_vec3_normalize(&to_target, &to_target);
            ad_vec3_normalize(&to_end, &to_end);

            float dot = ad_vec3_dot(&to_end, &to_target);
            if (dot > 0.9999f) continue;

            ad_vec3_t rot_axis;
            ad_vec3_cross(&rot_axis, &to_end, &to_target);
            ad_vec3_normalize(&rot_axis, &rot_axis);

            float angle = acosf(dot);
            ad_quat_t rot;
            ad_quat_from_axis_angle(&rot, &rot_axis, angle);

            for (int j = i + 1; j < chain->joint_count; j++) {
                ad_vec3_t rel;
                ad_vec3_sub(&rel, &chain->joint_positions[j], &joint_pos);

                ad_vec3_t rotated;
                rotated.x = rel.x * (1 - 2*(rot.y*rot.y + rot.z*rot.z)) +
                            rel.y * 2*(rot.x*rot.y - rot.w*rot.z) +
                            rel.z * 2*(rot.x*rot.z + rot.w*rot.y);
                rotated.y = rel.x * 2*(rot.x*rot.y + rot.w*rot.z) +
                            rel.y * (1 - 2*(rot.x*rot.x + rot.z*rot.z)) +
                            rel.z * 2*(rot.y*rot.z - rot.w*rot.x);
                rotated.z = rel.x * 2*(rot.x*rot.z - rot.w*rot.y) +
                            rel.y * 2*(rot.y*rot.z + rot.w*rot.x) +
                            rel.z * (1 - 2*(rot.x*rot.x + rot.y*rot.y));

                chain->joint_positions[j].x = joint_pos.x + rotated.x;
                chain->joint_positions[j].y = joint_pos.y + rotated.y;
                chain->joint_positions[j].z = joint_pos.z + rotated.z;
            }
        }

        ad_vec3_t end_effector = chain->joint_positions[chain->joint_count - 1];
        float dx = end_effector.x - target->x;
        float dy = end_effector.y - target->y;
        float dz = end_effector.z - target->z;
        float dist = sqrtf(dx*dx + dy*dy + dz*dz);
        if (dist < 0.001f) break;
    }
}

void ad_ik_fabrik(ad_ik_chain_t* chain, const ad_vec3_t* target, float tolerance,
                  int max_iter)
{
    if (chain->joint_count < 2) return;

    for (int iter = 0; iter < max_iter; iter++) {
        chain->joint_positions[chain->joint_count - 1] = *target;

        for (int i = chain->joint_count - 2; i >= 0; i--) {
            ad_vec3_t dir;
            ad_vec3_sub(&dir, &chain->joint_positions[i],
                        &chain->joint_positions[i + 1]);
            float len = ad_vec3_length(&dir);
            if (len < 1e-6f) continue;

            ad_vec3_normalize(&dir, &dir);
            float bone_len = chain->joint_lengths[i];
            chain->joint_positions[i].x = chain->joint_positions[i + 1].x + dir.x * bone_len;
            chain->joint_positions[i].y = chain->joint_positions[i + 1].y + dir.y * bone_len;
            chain->joint_positions[i].z = chain->joint_positions[i + 1].z + dir.z * bone_len;
        }

        float dx = chain->joint_positions[chain->joint_count - 1].x - target->x;
        float dy = chain->joint_positions[chain->joint_count - 1].y - target->y;
        float dz = chain->joint_positions[chain->joint_count - 1].z - target->z;
        if (sqrtf(dx*dx + dy*dy + dz*dz) < tolerance) break;
    }
}

void ad_fk_solve(const ad_skeleton_t* skeleton, ad_vec3_t* positions,
                 ad_quat_t* rotations, int bone_count)
{
    for (int i = 0; i < bone_count && i < skeleton->bone_count; i++) {
        const ad_bone_t* bone = &skeleton->bones[i];
        positions[i] = bone->current_pose.translation;
        rotations[i] = bone->current_pose.rotation;

        if (bone->parent_index >= 0) {
            const ad_quat_t* parent_rot = &rotations[bone->parent_index];
            const ad_vec3_t* parent_pos = &positions[bone->parent_index];

            ad_vec3_t rotated;
            rotated.x = positions[i].x;
            rotated.y = positions[i].y;
            rotated.z = positions[i].z;

            positions[i].x = parent_pos->x + rotated.x;
            positions[i].y = parent_pos->y + rotated.y;
            positions[i].z = parent_pos->z + rotated.z;

            ad_quat_multiply(&rotations[i], parent_rot, &rotations[i]);
        }
    }
}

void ad_fk_transform(const ad_skeleton_t* skeleton, int bone_index,
                     ad_transform_t* world_transform)
{
    if (bone_index < 0 || bone_index >= skeleton->bone_count) {
        memset(world_transform, 0, sizeof(ad_transform_t));
        ad_quat_identity(&world_transform->rotation);
        world_transform->scale.x = 1;
        world_transform->scale.y = 1;
        world_transform->scale.z = 1;
        return;
    }

    const ad_bone_t* bone = &skeleton->bones[bone_index];
    *world_transform = bone->current_pose;

    if (bone->parent_index >= 0) {
        ad_transform_t parent_world;
        ad_fk_transform(skeleton, bone->parent_index, &parent_world);

        ad_quat_multiply(&world_transform->rotation,
                         &parent_world.rotation,
                         &world_transform->rotation);

        ad_vec3_t rotated;
        ad_quat_t* pr = &parent_world.rotation;
        rotated.x = world_transform->translation.x;
        rotated.y = world_transform->translation.y;
        rotated.z = world_transform->translation.z;

        float x = rotated.x, y = rotated.y, z = rotated.z;
        float w = pr->w, px = pr->x, py = pr->y, pz = pr->z;
        rotated.x = w*w*x + 2*py*w*z - 2*pz*w*y + px*px*x + 2*py*px*y + 2*pz*px*z - z*(py*py + pz*pz + px*px) + y*(py*py + pz*pz + px*px);
        rotated.y = 2*px*py*x + py*py*y + 2*pz*py*z + 2*px*w*z - 2*pz*w*x - z*(px*px + pz*pz + py*py) + x*(px*px + pz*pz + py*py);
        rotated.z = 2*px*pz*x + 2*py*pz*y + pz*pz*z - 2*py*w*x + 2*px*w*y - y*(px*px + py*py + pz*pz) + x*(px*px + py*py + pz*pz);

        world_transform->translation.x = parent_world.translation.x + x;
        world_transform->translation.y = parent_world.translation.y + y;
        world_transform->translation.z = parent_world.translation.z + z;
    }
}

void ad_retarget_init(ad_retarget_config_t* config)
{
    memset(config, 0, sizeof(ad_retarget_config_t));
    config->smoothing = 0.3f;
    config->scale_factor = 1.0f;

    static const int default_maps[][2] = {
        {BP_LEFT_SHOULDER, AD_JOINT_LEFT_SHOULDER},
        {BP_RIGHT_SHOULDER, AD_JOINT_RIGHT_SHOULDER},
        {BP_LEFT_ELBOW, AD_JOINT_LEFT_ARM},
        {BP_RIGHT_ELBOW, AD_JOINT_RIGHT_ARM},
        {BP_LEFT_WRIST, AD_JOINT_LEFT_FOREARM},
        {BP_RIGHT_WRIST, AD_JOINT_RIGHT_FOREARM},
        {BP_LEFT_HIP, AD_JOINT_LEFT_HIP},
        {BP_RIGHT_HIP, AD_JOINT_RIGHT_HIP},
        {BP_LEFT_KNEE, AD_JOINT_LEFT_UPLEG},
        {BP_RIGHT_KNEE, AD_JOINT_RIGHT_UPLEG},
        {BP_LEFT_ANKLE, AD_JOINT_LEFT_LEG},
        {BP_RIGHT_ANKLE, AD_JOINT_RIGHT_LEG},
        {BP_NOSE, AD_JOINT_HEAD},
    };
    int map_count = (int)(sizeof(default_maps) / sizeof(default_maps[0]));

    for (int i = 0; i < map_count; i++) {
        ad_retarget_add_map(config, default_maps[i][0],
                            default_maps[i][1], 1.0f);
    }
}

void ad_retarget_add_map(ad_retarget_config_t* config, int landmark, int bone,
                         float influence)
{
    if (config->map_count >= AD_MAX_RETARGET_MAP) return;
    ad_retarget_map_t* map = &config->maps[config->map_count];
    map->landmark_index = landmark;
    map->bone_index = bone;
    map->influence = influence;
    map->offset.x = 0;
    map->offset.y = 0;
    map->offset.z = 0;
    map->axis_mask = 0x07;
    config->map_count++;
}

void ad_retarget_apply(const ad_retarget_config_t* config,
                       const bp_body_t* body, ad_skeleton_t* skeleton)
{
    float scale = config->scale_factor;
    float smooth = config->smoothing;

    for (int m = 0; m < config->map_count; m++) {
        const ad_retarget_map_t* map = &config->maps[m];
        if (map->landmark_index < 0 || map->landmark_index >= BP_LANDMARK_COUNT) continue;
        if (map->bone_index < 0 || map->bone_index >= skeleton->bone_count) continue;

        const bp_landmark_t* lm = &body->landmarks[map->landmark_index];
        ad_bone_t* bone = &skeleton->bones[map->bone_index];

        float target_x = lm->x * scale + map->offset.x;
        float target_y = lm->y * scale + map->offset.y;
        float target_z = lm->z * scale + map->offset.z;

        if (map->axis_mask & 0x01) {
            bone->local_position.x += (target_x - bone->local_position.x) * smooth * map->influence;
        }
        if (map->axis_mask & 0x02) {
            bone->local_position.y += (target_y - bone->local_position.y) * smooth * map->influence;
        }
        if (map->axis_mask & 0x04) {
            bone->local_position.z += (target_z - bone->local_position.z) * smooth * map->influence;
        }
    }
}

void ad_retarget_face(const fl_face_mesh_t* face,
                      const ebs_blendshapes_t* expression,
                      ad_skeleton_t* skeleton)
{
    (void)face;
    (void)expression;
    (void)skeleton;

    int head_idx = ad_bone_index_by_name(skeleton, "head");
    if (head_idx >= 0) {
    }
}

void ad_drive_face_body(ad_driver_t* driver, const fl_face_mesh_t* face,
                        const bp_body_t* body, const ebs_blendshapes_t* expression,
                        ad_skeleton_t* out_skeleton)
{
    ad_skeleton_reset_pose(out_skeleton);

    if (body) {
        ad_retarget_apply(&driver->retarget, body, out_skeleton);
        driver->avatar.body = *body;
        driver->avatar.has_body = 1;
    }

    if (face) {
        driver->avatar.face = *face;
        driver->avatar.has_face = 1;
    }

    if (expression) {
        driver->avatar.blendshapes = *expression;
        driver->avatar.has_expression = 1;
    }

    ad_skeleton_apply_transforms(out_skeleton);
}

void ad_drive_audio(ad_driver_t* driver, const float* audio, int audio_len,
                    float sample_rate, ad_skeleton_t* out_skeleton)
{
    ebs_blendshapes_t bs;
    ebs_reset_blendshapes(&bs);

    ls_audio_drive(audio, audio_len, sample_rate, NULL, &driver->lip_sync, &bs);
    ad_retarget_face(NULL, &bs, out_skeleton);
}

void ad_idle_init(ad_idle_state_t* idle)
{
    memset(idle, 0, sizeof(ad_idle_state_t));
    idle->idle_threshold = 1.5f;
    idle->idle_timeout = 0.3f;
    idle->is_idle = 0;
}

void ad_idle_add_animation(ad_idle_state_t* idle, const ad_idle_animation_t* anim)
{
    if (idle->anim_count >= AD_IDLE_ANIM_COUNT) return;
    idle->animations[idle->anim_count] = *anim;
    idle->anim_count++;
}

void ad_idle_update(ad_idle_state_t* idle, float dt, int has_input)
{
    if (has_input) {
        idle->last_input_time = idle->current_time;
        idle->is_idle = 0;
    } else {
        float time_since_input = idle->current_time - idle->last_input_time;
        if (time_since_input > idle->idle_threshold) {
            idle->is_idle = 1;
        }
    }

    idle->current_time += dt;
}

void ad_idle_get_pose(const ad_idle_state_t* idle, ad_skeleton_t* skeleton)
{
    (void)idle;
    (void)skeleton;
    if (!idle->is_idle || idle->anim_count == 0) return;

    const ad_idle_animation_t* anim = &idle->animations[idle->current_anim];
    if (anim->key_count < 2) return;

    float loop_time = fmodf(idle->current_time, anim->duration);
    if (loop_time > anim->duration) loop_time = anim->duration;

    ad_vec3_t pos = anim->keys[0].position;
    ad_quat_t rot = anim->keys[0].rotation;

    int head_idx = ad_bone_index_by_name(skeleton, "head");
    if (head_idx >= 0) {
        skeleton->bones[head_idx].local_rotation = rot;
    }

    int spine_idx = ad_bone_index_by_name(skeleton, "spine");
    if (spine_idx >= 0) {
        float sway = sinf(loop_time * 1.3f) * 0.02f;
        skeleton->bones[spine_idx].local_position.x += sway;
    }
}

void ad_animate_skeleton(ad_skeleton_t* skeleton, float dt)
{
    (void)dt;
    (void)skeleton;
}

void ad_blend_skeletons(const ad_skeleton_t* a, const ad_skeleton_t* b,
                        float t, ad_skeleton_t* out)
{
    int count = a->bone_count < b->bone_count ? a->bone_count : b->bone_count;
    out->bone_count = count;

    for (int i = 0; i < count; i++) {
        out->bones[i].local_position.x =
            a->bones[i].local_position.x + (b->bones[i].local_position.x - a->bones[i].local_position.x) * t;
        out->bones[i].local_position.y =
            a->bones[i].local_position.y + (b->bones[i].local_position.y - a->bones[i].local_position.y) * t;
        out->bones[i].local_position.z =
            a->bones[i].local_position.z + (b->bones[i].local_position.z - a->bones[i].local_position.z) * t;

        ad_quat_slerp(&out->bones[i].local_rotation,
                      &a->bones[i].local_rotation,
                      &b->bones[i].local_rotation, t);
    }
}

void ad_copy_skeleton(const ad_skeleton_t* src, ad_skeleton_t* dst)
{
    memcpy(dst, src, sizeof(ad_skeleton_t));
}

void ad_landmark_to_bone_rotation(const ad_vec3_t* landmark,
                                  const ad_vec3_t* parent,
                                  ad_quat_t* rotation)
{
    ad_vec3_t dir;
    ad_vec3_sub(&dir, landmark, parent);
    ad_vec3_normalize(&dir, &dir);

    ad_vec3_t up = { 0, 1, 0 };
    ad_vec3_t axis;
    ad_vec3_cross(&axis, &up, &dir);
    float dot = ad_vec3_dot(&up, &dir);
    float angle = acosf(dot > 1.0f ? 1.0f : (dot < -1.0f ? -1.0f : dot));

    if (ad_vec3_length(&axis) < 1e-6f) {
        ad_quat_identity(rotation);
    } else {
        ad_vec3_normalize(&axis, &axis);
        ad_quat_from_axis_angle(rotation, &axis, angle);
    }
}

void ad_smooth_skeleton(const ad_skeleton_t* current, ad_skeleton_t* smoothed,
                        float alpha)
{
    int count = current->bone_count < smoothed->bone_count ?
                current->bone_count : smoothed->bone_count;

    for (int i = 0; i < count; i++) {
        ad_vec3_t* cp = &current->bones[i].local_position;
        ad_vec3_t* sp = &smoothed->bones[i].local_position;
        sp->x += (cp->x - sp->x) * alpha;
        sp->y += (cp->y - sp->y) * alpha;
        sp->z += (cp->z - sp->z) * alpha;

        ad_quat_slerp(&smoothed->bones[i].local_rotation,
                      &smoothed->bones[i].local_rotation,
                      &current->bones[i].local_rotation, alpha);
    }
}

void ad_drive_multimodal(ad_driver_t* driver,
                         const fl_face_mesh_t* face,
                         const bp_body_t* body,
                         const ebs_blendshapes_t* expression,
                         const float* audio, int audio_len,
                         float sample_rate, float dt,
                         ad_skeleton_t* out_skeleton)
{
    ad_skeleton_t body_skel, face_skel, idle_skel;
    ad_init_skeleton(&body_skel);
    ad_init_skeleton(&face_skel);
    ad_init_skeleton(&idle_skel);

    ad_build_default_skeleton(&body_skel);
    ad_build_default_skeleton(&face_skel);
    ad_build_default_skeleton(&idle_skel);

    int has_input = (face || body || expression || (audio && audio_len > 0)) ? 1 : 0;

    ad_drive_face_body(driver, face, body, expression, &body_skel);

    if (audio && audio_len > 0) {
        ad_drive_audio(driver, audio, audio_len, sample_rate, &face_skel);
    }

    ad_idle_update(&driver->idle, dt, has_input);
    ad_idle_get_pose(&driver->idle, &idle_skel);

    ad_skeleton_t combined_body_face;
    ad_blend_skeletons(&body_skel, &face_skel, 0.5f, &combined_body_face);

    if (driver->idle.is_idle) {
        ad_blend_skeletons(&combined_body_face, &idle_skel,
                           driver->animation_blend, out_skeleton);
    } else {
        ad_copy_skeleton(&combined_body_face, out_skeleton);
    }

    driver->global_time += dt;
}

int ad_bone_index_by_name(const ad_skeleton_t* skeleton, const char* name)
{
    for (int i = 0; i < skeleton->bone_count; i++) {
        if (strcmp(skeleton->bones[i].name, name) == 0) return i;
    }
    return -1;
}

void ad_skeleton_reset_pose(ad_skeleton_t* skeleton)
{
    for (int i = 0; i < skeleton->bone_count; i++) {
        skeleton->bones[i].current_pose = skeleton->bones[i].bind_pose;
        skeleton->bones[i].local_position = skeleton->bones[i].bind_pose.translation;
        ad_quat_identity(&skeleton->bones[i].local_rotation);
    }
}

void ad_skeleton_apply_transforms(ad_skeleton_t* skeleton)
{
    for (int i = 0; i < skeleton->bone_count; i++) {
        skeleton->bones[i].current_pose.translation = skeleton->bones[i].local_position;
        skeleton->bones[i].current_pose.rotation = skeleton->bones[i].local_rotation;
    }
}
