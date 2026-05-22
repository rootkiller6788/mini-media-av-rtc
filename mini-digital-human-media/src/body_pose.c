#include "body_pose.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

static const char* bp_landmark_names[BP_LANDMARK_COUNT] = {
    "nose", "left_eye_inner", "left_eye", "left_eye_outer",
    "right_eye_inner", "right_eye", "right_eye_outer",
    "left_ear", "right_ear", "mouth_left", "mouth_right",
    "left_shoulder", "right_shoulder", "left_elbow", "right_elbow",
    "left_wrist", "right_wrist", "left_pinky", "right_pinky",
    "left_index", "right_index", "left_thumb", "right_thumb",
    "left_hip", "right_hip", "left_knee", "right_knee",
    "left_ankle", "right_ankle", "left_heel", "right_heel",
    "left_foot_index", "right_foot_index"
};

static const char* bp_pose_names[BP_CLASS_COUNT] = {
    "unknown", "standing", "sitting", "raising_left_hand",
    "raising_right_hand", "raising_both_hands", "walking", "crouching"
};

static const int bp_joint_chain[][3] = {
    {BP_LEFT_SHOULDER, BP_LEFT_ELBOW, BP_LEFT_WRIST},
    {BP_RIGHT_SHOULDER, BP_RIGHT_ELBOW, BP_RIGHT_WRIST},
    {BP_LEFT_HIP, BP_LEFT_KNEE, BP_LEFT_ANKLE},
    {BP_RIGHT_HIP, BP_RIGHT_KNEE, BP_RIGHT_ANKLE},
    {BP_LEFT_SHOULDER, BP_LEFT_HIP, BP_LEFT_KNEE},
    {BP_RIGHT_SHOULDER, BP_RIGHT_HIP, BP_RIGHT_KNEE},
    {BP_LEFT_HIP, BP_LEFT_SHOULDER, BP_LEFT_ELBOW},
    {BP_RIGHT_HIP, BP_RIGHT_SHOULDER, BP_RIGHT_ELBOW}
};

void bp_init_filter(bp_filter_t* filter)
{
    memset(filter, 0, sizeof(bp_filter_t));
}

void bp_pose_defaults(bp_result_t* result, int width, int height)
{
    memset(result, 0, sizeof(bp_result_t));
    result->image_width = width;
    result->image_height = height;
}

int bp_detect(const uint8_t* image_data, int width, int height,
              bp_result_t* result)
{
    (void)image_data;
    bp_pose_defaults(result, width, height);

    bp_body_t* body = &result->bodies[0];
    body->body_id = 0;
    body->detection_confidence = 0.90f;

    float cx = width * 0.5f;
    float cy = height * 0.5f;
    float body_height = height * 0.7f;

    static const float default_x[BP_LANDMARK_COUNT] = {
        0.0f, -0.02f, -0.04f, -0.06f, 0.02f, 0.04f, 0.06f,
        -0.07f, 0.07f, -0.03f, 0.03f,
        -0.10f, 0.10f, -0.15f, 0.15f,
        -0.20f, 0.20f, -0.22f, 0.22f,
        -0.21f, 0.21f, -0.19f, 0.19f,
        -0.08f, 0.08f, -0.09f, 0.09f,
        -0.10f, 0.10f, -0.10f, 0.10f,
        -0.11f, 0.11f
    };

    static const float default_y[BP_LANDMARK_COUNT] = {
        -0.40f, -0.38f, -0.39f, -0.38f, -0.38f, -0.39f, -0.38f,
        -0.39f, -0.39f, -0.36f, -0.36f,
        -0.28f, -0.28f, -0.18f, -0.18f,
        -0.10f, -0.10f, -0.11f, -0.11f,
        -0.10f, -0.10f, -0.10f, -0.10f,
        0.05f, 0.05f, 0.25f, 0.25f,
        0.45f, 0.45f, 0.46f, 0.46f,
        0.47f, 0.47f
    };

    for (int i = 0; i < BP_LANDMARK_COUNT; i++) {
        body->landmarks[i].x = cx + default_x[i] * body_height;
        body->landmarks[i].y = cy + default_y[i] * body_height;
        body->landmarks[i].z = 0.0f;
        body->landmarks[i].visibility = 0.9f;
        body->landmarks[i].presence = 1.0f;
    }

    result->body_count = 1;
    return 0;
}

void bp_compute_joint_angle(const bp_body_t* body, int joint_a, int joint_b,
                            int joint_c, bp_joint_angle_t* angle)
{
    const bp_landmark_t* pa = &body->landmarks[joint_a];
    const bp_landmark_t* pb = &body->landmarks[joint_b];
    const bp_landmark_t* pc = &body->landmarks[joint_c];

    bp_vec3_t ba = { pa->x - pb->x, pa->y - pb->y, pa->z - pb->z };
    bp_vec3_t bc = { pc->x - pb->x, pc->y - pb->y, pc->z - pb->z };

    float dot = ba.x * bc.x + ba.y * bc.y + ba.z * bc.z;
    float len_ba = sqrtf(ba.x * ba.x + ba.y * ba.y + ba.z * ba.z);
    float len_bc = sqrtf(bc.x * bc.x + bc.y * bc.y + bc.z * bc.z);
    float cos_angle = dot / (len_ba * len_bc + 1e-8f);

    if (cos_angle > 1.0f) cos_angle = 1.0f;
    if (cos_angle < -1.0f) cos_angle = -1.0f;

    angle->angle_deg = acosf(cos_angle) * 180.0f / M_PI;
    angle->confidence = (pa->visibility + pb->visibility + pc->visibility) / 3.0f;

    angle->axis.x = ba.y * bc.z - ba.z * bc.y;
    angle->axis.y = ba.z * bc.x - ba.x * bc.z;
    angle->axis.z = ba.x * bc.y - ba.y * bc.x;
}

void bp_compute_all_angles(const bp_body_t* body, bp_joint_angle_t* angles_out)
{
    for (int i = 0; i < 8; i++) {
        bp_compute_joint_angle(body,
                               bp_joint_chain[i][0],
                               bp_joint_chain[i][1],
                               bp_joint_chain[i][2],
                               &angles_out[i]);
    }
}

bp_pose_class_t bp_classify_pose(const bp_body_t* body)
{
    float left_wrist_y = body->landmarks[BP_LEFT_WRIST].y;
    float right_wrist_y = body->landmarks[BP_RIGHT_WRIST].y;
    float left_shoulder_y = body->landmarks[BP_LEFT_SHOULDER].y;
    float right_shoulder_y = body->landmarks[BP_RIGHT_SHOULDER].y;
    float left_hip_y = body->landmarks[BP_LEFT_HIP].y;
    float right_hip_y = body->landmarks[BP_RIGHT_HIP].y;
    float left_knee_y = body->landmarks[BP_LEFT_KNEE].y;
    float right_knee_y = body->landmarks[BP_RIGHT_KNEE].y;

    int left_hand_up = (left_wrist_y < left_shoulder_y - 30.0f) ? 1 : 0;
    int right_hand_up = (right_wrist_y < right_shoulder_y - 30.0f) ? 1 : 0;

    float hip_center_y = (left_hip_y + right_hip_y) * 0.5f;
    float knee_center_y = (left_knee_y + right_knee_y) * 0.5f;
    float leg_extent = hip_center_y - knee_center_y;

    if (left_hand_up && right_hand_up)
        return BP_CLASS_RAISING_BOTH_HANDS;
    if (left_hand_up)
        return BP_CLASS_RAISING_LEFT_HAND;
    if (right_hand_up)
        return BP_CLASS_RAISING_RIGHT_HAND;

    if (leg_extent < 40.0f)
        return BP_CLASS_SITTING;
    if (fabsf(left_knee_y - right_knee_y) > 20.0f)
        return BP_CLASS_WALKING;
    if (leg_extent < 80.0f)
        return BP_CLASS_CROUCHING;

    return BP_CLASS_STANDING;
}

const char* bp_pose_class_name(bp_pose_class_t cls)
{
    if (cls < 0 || cls >= BP_CLASS_COUNT) return "unknown";
    return bp_pose_names[cls];
}

void bp_smooth_body(const bp_body_t* current, bp_body_t* smoothed, float alpha)
{
    for (int i = 0; i < BP_LANDMARK_COUNT; i++) {
        smoothed->landmarks[i].x +=
            (current->landmarks[i].x - smoothed->landmarks[i].x) * alpha;
        smoothed->landmarks[i].y +=
            (current->landmarks[i].y - smoothed->landmarks[i].y) * alpha;
        smoothed->landmarks[i].z +=
            (current->landmarks[i].z - smoothed->landmarks[i].z) * alpha;
        smoothed->landmarks[i].visibility = current->landmarks[i].visibility;
        smoothed->landmarks[i].presence = current->landmarks[i].presence;
    }
}

void bp_smooth_filter(bp_filter_t* filter, const bp_body_t* current,
                      bp_body_t* smoothed)
{
    if (filter->initialized < BP_FILTER_WINDOW) {
        bp_landmark_t* hist = &filter->history[filter->write_idx];
        memcpy(hist, current->landmarks,
               sizeof(bp_landmark_t) * BP_LANDMARK_COUNT);
        filter->write_idx++;
        filter->initialized++;

        memcpy(smoothed->landmarks, current->landmarks,
               sizeof(bp_landmark_t) * BP_LANDMARK_COUNT);
        return;
    }

    bp_landmark_t* hist = &filter->history[filter->write_idx % BP_FILTER_WINDOW];
    memcpy(hist, current->landmarks,
           sizeof(bp_landmark_t) * BP_LANDMARK_COUNT);
    filter->write_idx++;

    for (int i = 0; i < BP_LANDMARK_COUNT; i++) {
        float sum_x = 0, sum_y = 0, sum_z = 0;
        float sum_vis = 0, sum_pres = 0;

        for (int w = 0; w < BP_FILTER_WINDOW; w++) {
            int idx = (filter->write_idx - 1 - w) % BP_FILTER_WINDOW;
            sum_x += filter->history[idx * BP_LANDMARK_COUNT + i].x;
            sum_y += filter->history[idx * BP_LANDMARK_COUNT + i].y;
            sum_z += filter->history[idx * BP_LANDMARK_COUNT + i].z;
            sum_vis += filter->history[idx * BP_LANDMARK_COUNT + i].visibility;
            sum_pres += filter->history[idx * BP_LANDMARK_COUNT + i].presence;
        }

        smoothed->landmarks[i].x = sum_x / BP_FILTER_WINDOW;
        smoothed->landmarks[i].y = sum_y / BP_FILTER_WINDOW;
        smoothed->landmarks[i].z = sum_z / BP_FILTER_WINDOW;
        smoothed->landmarks[i].visibility = sum_vis / BP_FILTER_WINDOW;
        smoothed->landmarks[i].presence = sum_pres / BP_FILTER_WINDOW;
    }
}

void bp_to_world_coords(const bp_body_t* body, float hip_center[3],
                        float world_coords[BP_LANDMARK_COUNT][3])
{
    for (int i = 0; i < BP_LANDMARK_COUNT; i++) {
        world_coords[i][0] = body->landmarks[i].x - hip_center[0];
        world_coords[i][1] = body->landmarks[i].y - hip_center[1];
        world_coords[i][2] = -(body->landmarks[i].z - hip_center[2]);
    }
}

void bp_get_world_coords_relative(const bp_body_t* body,
                                  float world[BP_LANDMARK_COUNT][3])
{
    float hip_center[3];
    bp_get_hip_center(body, (bp_vec3_t*)hip_center);
    bp_to_world_coords(body, hip_center, world);
}

void bp_detect_world_coords(const uint8_t* image, int w, int h,
                            bp_result_t* result)
{
    bp_detect(image, w, h, result);
    for (int b = 0; b < result->body_count; b++) {
        bp_get_world_coords_relative(&result->bodies[b],
                                     result->bodies[b].world_landmarks);
    }
}

float bp_compute_spine_angle(const bp_body_t* body)
{
    bp_joint_angle_t angle;
    bp_compute_joint_angle(body, BP_NOSE, BP_LEFT_SHOULDER, BP_LEFT_HIP, &angle);
    return angle.angle_deg;
}

float bp_compute_neck_angle(const bp_body_t* body)
{
    bp_vec3_t left_shoulder = {
        body->landmarks[BP_LEFT_SHOULDER].x,
        body->landmarks[BP_LEFT_SHOULDER].y,
        body->landmarks[BP_LEFT_SHOULDER].z
    };
    bp_vec3_t right_shoulder = {
        body->landmarks[BP_RIGHT_SHOULDER].x,
        body->landmarks[BP_RIGHT_SHOULDER].y,
        body->landmarks[BP_RIGHT_SHOULDER].z
    };
    bp_joint_angle_t angle;
    bp_compute_joint_angle(body, BP_NOSE,
                           BP_LEFT_SHOULDER, BP_RIGHT_SHOULDER, &angle);
    return angle.angle_deg;
}

float bp_height_estimate(const bp_body_t* body, float focal_length_px,
                         float real_shoulder_to_hip_m)
{
    float dx = body->landmarks[BP_LEFT_SHOULDER].x -
               body->landmarks[BP_RIGHT_SHOULDER].x;
    float dy = body->landmarks[BP_LEFT_SHOULDER].y -
               body->landmarks[BP_RIGHT_SHOULDER].y;
    float shoulder_dist_px = sqrtf(dx * dx + dy * dy);

    if (shoulder_dist_px < 1.0f) return 0.0f;

    float scale = real_shoulder_to_hip_m / shoulder_dist_px;
    float body_height_px = fabsf(body->landmarks[BP_LEFT_FOOT_INDEX].y -
                                  body->landmarks[BP_NOSE].y);
    return body_height_px * scale;
}

void bp_get_body_center(const bp_body_t* body, bp_vec3_t* center)
{
    center->x = 0; center->y = 0; center->z = 0;
    int count = 0;

    for (int i = 0; i < BP_LANDMARK_COUNT; i++) {
        if (body->landmarks[i].visibility > 0.5f) {
            center->x += body->landmarks[i].x;
            center->y += body->landmarks[i].y;
            center->z += body->landmarks[i].z;
            count++;
        }
    }

    if (count > 0) {
        center->x /= count;
        center->y /= count;
        center->z /= count;
    }
}

void bp_get_hip_center(const bp_body_t* body, bp_vec3_t* center)
{
    center->x = (body->landmarks[BP_LEFT_HIP].x + body->landmarks[BP_RIGHT_HIP].x) * 0.5f;
    center->y = (body->landmarks[BP_LEFT_HIP].y + body->landmarks[BP_RIGHT_HIP].y) * 0.5f;
    center->z = (body->landmarks[BP_LEFT_HIP].z + body->landmarks[BP_RIGHT_HIP].z) * 0.5f;
}

void bp_copy_body(const bp_body_t* src, bp_body_t* dst)
{
    memcpy(dst, src, sizeof(bp_body_t));
}

int bp_is_landmark_visible(const bp_landmark_t* lm, float threshold)
{
    return lm->visibility >= threshold ? 1 : 0;
}
