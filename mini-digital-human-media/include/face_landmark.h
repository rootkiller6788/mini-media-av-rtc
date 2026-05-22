#ifndef FACE_LANDMARK_H
#define FACE_LANDMARK_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

#define FL_LANDMARK_COUNT 468
#define FL_MAX_FACE_COUNT 4
#define FL_LIPS_COUNT 40
#define FL_EYES_COUNT 32
#define FL_EYEBROWS_COUNT 18
#define FL_NOSE_COUNT 6
#define FL_FACE_OVAL_COUNT 36

typedef enum {
    FL_TYPE_LIPS = 0,
    FL_TYPE_LEFT_EYE,
    FL_TYPE_RIGHT_EYE,
    FL_TYPE_LEFT_EYEBROW,
    FL_TYPE_RIGHT_EYEBROW,
    FL_TYPE_NOSE,
    FL_TYPE_FACE_OVAL,
    FL_TYPE_IRIS,
    FL_TYPE_COUNT
} fl_landmark_type_t;

typedef enum {
    FL_KEY_NOSE_TIP = 1,
    FL_KEY_LEFT_EYE_CENTER = 8,
    FL_KEY_RIGHT_EYE_CENTER = 263,
    FL_KEY_LIP_TOP = 13,
    FL_KEY_LIP_BOTTOM = 14,
    FL_KEY_LEFT_EAR = 234,
    FL_KEY_RIGHT_EAR = 454,
    FL_KEY_FOREHEAD = 10,
    FL_KEY_CHIN = 152,
    FL_KEY_LEFT_CHEEK = 50,
    FL_KEY_RIGHT_CHEEK = 280,
    FL_KEY_MOUTH_LEFT = 61,
    FL_KEY_MOUTH_RIGHT = 291,
    FL_KEY_COUNT = 13
} fl_key_point_t;

typedef struct {
    float x, y, z;
} fl_vec3_t;

typedef struct {
    float x_min, y_min, x_max, y_max;
    float width, height;
    float confidence;
} fl_bbox_t;

typedef struct {
    fl_vec3_t landmarks[FL_LANDMARK_COUNT];
    float visibility[FL_LANDMARK_COUNT];
    int present[FL_LANDMARK_COUNT];
} fl_face_mesh_t;

typedef struct {
    fl_face_mesh_t faces[FL_MAX_FACE_COUNT];
    int face_count;
    int image_width;
    int image_height;
    float detection_confidence;
    float tracking_confidence;
} fl_result_t;

typedef struct {
    float smoothing_factor;
    float min_detection_confidence;
    float min_tracking_confidence;
    int refine_landmarks;
    int static_image_mode;
    int max_num_faces;
    int output_iris_landmarks;
} fl_config_t;

typedef struct {
    fl_face_mesh_t last_face;
    fl_face_mesh_t smoothed;
    int is_initialized;
    float alpha;
} fl_tracker_t;

typedef struct {
    int indices[40];
} fl_landmark_indices_t;

void fl_init(fl_tracker_t* tracker, float smoothing);
void fl_config_default(fl_config_t* config);
int fl_detect(const fl_config_t* config, fl_tracker_t* tracker,
              const float* image_data, int width, int height,
              fl_result_t* result);
void fl_get_landmarks_by_type(const fl_face_mesh_t* face, fl_landmark_type_t type,
                              fl_landmark_indices_t* indices);
void fl_get_key_point(const fl_face_mesh_t* face, fl_key_point_t key,
                      fl_vec3_t* point);
void fl_get_bounding_box(const fl_face_mesh_t* face, fl_bbox_t* bbox);
void fl_normalize_landmarks(const fl_face_mesh_t* face, fl_face_mesh_t* normalized,
                            int width, int height);
void fl_interpolate_landmarks(const fl_face_mesh_t* from, const fl_face_mesh_t* to,
                              float t, fl_face_mesh_t* out);
float fl_compute_face_size(const fl_face_mesh_t* face);
void fl_landmark_to_screen(const fl_face_mesh_t* face, int landmark_idx,
                           int width, int height, float* sx, float* sy);
int fl_get_lip_contour(const fl_face_mesh_t* face, fl_vec3_t* outer_lip,
                       fl_vec3_t* inner_lip, int* outer_count, int* inner_count);
void fl_face_mesh_topology_init(void);
const char* fl_landmark_name(int index);
float fl_eye_aspect_ratio(const fl_face_mesh_t* face);
float fl_mouth_aspect_ratio(const fl_face_mesh_t* face);
void fl_smooth_landmarks(const fl_face_mesh_t* current, fl_face_mesh_t* smoothed,
                         float alpha);
int fl_validate_mesh(const fl_face_mesh_t* face);
void fl_copy_face_mesh(const fl_face_mesh_t* src, fl_face_mesh_t* dst);
void fl_reset_tracker(fl_tracker_t* tracker);

#ifdef __cplusplus
}
#endif

#endif
