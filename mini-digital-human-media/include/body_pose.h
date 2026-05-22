#ifndef BODY_POSE_H
#define BODY_POSE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

#define BP_LANDMARK_COUNT 33
#define BP_MAX_BODIES 4
#define BP_FILTER_WINDOW 5

typedef enum {
    BP_NOSE = 0,
    BP_LEFT_EYE_INNER, BP_LEFT_EYE, BP_LEFT_EYE_OUTER,
    BP_RIGHT_EYE_INNER, BP_RIGHT_EYE, BP_RIGHT_EYE_OUTER,
    BP_LEFT_EAR, BP_RIGHT_EAR,
    BP_MOUTH_LEFT, BP_MOUTH_RIGHT,
    BP_LEFT_SHOULDER = 11, BP_RIGHT_SHOULDER,
    BP_LEFT_ELBOW, BP_RIGHT_ELBOW,
    BP_LEFT_WRIST, BP_RIGHT_WRIST,
    BP_LEFT_PINKY, BP_RIGHT_PINKY,
    BP_LEFT_INDEX, BP_RIGHT_INDEX,
    BP_LEFT_THUMB, BP_RIGHT_THUMB,
    BP_LEFT_HIP = 23, BP_RIGHT_HIP,
    BP_LEFT_KNEE, BP_RIGHT_KNEE,
    BP_LEFT_ANKLE, BP_RIGHT_ANKLE,
    BP_LEFT_HEEL, BP_RIGHT_HEEL,
    BP_LEFT_FOOT_INDEX, BP_RIGHT_FOOT_INDEX
} bp_landmark_id_t;

typedef enum {
    BP_CLASS_UNKNOWN = 0,
    BP_CLASS_STANDING,
    BP_CLASS_SITTING,
    BP_CLASS_RAISING_LEFT_HAND,
    BP_CLASS_RAISING_RIGHT_HAND,
    BP_CLASS_RAISING_BOTH_HANDS,
    BP_CLASS_WALKING,
    BP_CLASS_CROUCHING,
    BP_CLASS_COUNT
} bp_pose_class_t;

typedef struct {
    float x, y, z;
    float visibility;
    float presence;
} bp_landmark_t;

typedef struct {
    bp_landmark_t landmarks[BP_LANDMARK_COUNT];
    float world_landmarks[BP_LANDMARK_COUNT][3];
    int body_id;
    float detection_confidence;
} bp_body_t;

typedef struct {
    bp_body_t bodies[BP_MAX_BODIES];
    int body_count;
    int image_width;
    int image_height;
} bp_result_t;

typedef struct {
    float x, y, z;
} bp_vec3_t;

typedef struct {
    float angle_deg;
    float confidence;
    bp_vec3_t axis;
} bp_joint_angle_t;

typedef struct {
    bp_landmark_t history[BP_FILTER_WINDOW];
    bp_body_t smoothed;
    int write_idx;
    int initialized;
} bp_filter_t;

void bp_init_filter(bp_filter_t* filter);
void bp_pose_defaults(bp_result_t* result, int width, int height);
int bp_detect(const uint8_t* image_data, int width, int height,
              bp_result_t* result);
void bp_compute_joint_angle(const bp_body_t* body, int joint_a, int joint_b,
                            int joint_c, bp_joint_angle_t* angle);
void bp_compute_all_angles(const bp_body_t* body, bp_joint_angle_t* angles_out);
bp_pose_class_t bp_classify_pose(const bp_body_t* body);
const char* bp_pose_class_name(bp_pose_class_t cls);
void bp_smooth_body(const bp_body_t* current, bp_body_t* smoothed, float alpha);
void bp_smooth_filter(bp_filter_t* filter, const bp_body_t* current,
                      bp_body_t* smoothed);
void bp_to_world_coords(const bp_body_t* body, float hip_center[3],
                        float world_coords[BP_LANDMARK_COUNT][3]);
void bp_get_world_coords_relative(const bp_body_t* body, float world[BP_LANDMARK_COUNT][3]);
void bp_detect_world_coords(const uint8_t* image, int w, int h,
                            bp_result_t* result);
float bp_compute_spine_angle(const bp_body_t* body);
float bp_compute_neck_angle(const bp_body_t* body);
float bp_height_estimate(const bp_body_t* body, float focal_length_px,
                         float real_shoulder_to_hip_m);
void bp_get_body_center(const bp_body_t* body, bp_vec3_t* center);
void bp_get_hip_center(const bp_body_t* body, bp_vec3_t* center);
void bp_copy_body(const bp_body_t* src, bp_body_t* dst);
int bp_is_landmark_visible(const bp_landmark_t* lm, float threshold);

#ifdef __cplusplus
}
#endif

#endif
