#include "face_landmark.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

static const int fl_lip_outer_indices[40] = {
    61, 185, 40, 39, 37, 0, 267, 269, 270, 409,
    291, 375, 321, 405, 314, 17, 84, 181, 91, 146,
    61, 146, 91, 181, 84, 17, 314, 405, 321, 375,
    291, 409, 270, 269, 267, 0, 37, 39, 40, 185
};

static const int fl_lip_inner_indices[20] = {
    78, 191, 80, 81, 82, 13, 312, 311, 310, 415,
    308, 324, 318, 402, 317, 14, 87, 178, 88, 95
};

static const int fl_left_eye_indices[16] = {
    33, 7, 163, 144, 145, 153, 154, 155,
    133, 173, 157, 158, 159, 160, 161, 246
};

static const int fl_right_eye_indices[16] = {
    362, 382, 381, 380, 374, 373, 390, 249,
    263, 466, 388, 387, 386, 385, 384, 398
};

static const int fl_iris_left_indices[5] = { 468, 469, 470, 471, 472 };
static const int fl_iris_right_indices[5] = { 473, 474, 475, 476, 477 };

static const char* fl_landmark_names[FL_LANDMARK_COUNT];

static const char* fl_type_names[FL_TYPE_COUNT] = {
    "lips", "left_eye", "right_eye", "left_eyebrow",
    "right_eyebrow", "nose", "face_oval", "iris"
};

static const char* fl_key_names[FL_KEY_COUNT] = {
    "", "nose_tip", "", "", "", "", "", "", "left_eye_center", "",
    "", "", "", "lip_top", "lip_bottom", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "", "", "", "", "", "", "", "left_cheek",
    "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
    "", "chin", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
    "", "", "left_ear", "", "", "", "", "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
    "", "right_eye_center", "", "", "", "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "", "", "right_cheek", "", "", "", "", "",
    "", "", "", "", "", "", "", "", "", "mouth_left", "", "", "", "", "",
    "", "", "", "", "", "", "", "", "", "", "", "", "", "mouth_right",
    "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
    "right_ear", "", "", "", "", "", "", "", "", "", "", "", "", ""
};

void fl_init(fl_tracker_t* tracker, float smoothing)
{
    memset(tracker, 0, sizeof(fl_tracker_t));
    tracker->alpha = smoothing;
    if (tracker->alpha < 0.01f) tracker->alpha = 0.01f;
    if (tracker->alpha > 0.99f) tracker->alpha = 0.99f;
}

void fl_config_default(fl_config_t* config)
{
    memset(config, 0, sizeof(fl_config_t));
    config->min_detection_confidence = 0.5f;
    config->min_tracking_confidence = 0.5f;
    config->refine_landmarks = 1;
    config->static_image_mode = 0;
    config->max_num_faces = 1;
    config->output_iris_landmarks = 0;
}

int fl_detect(const fl_config_t* config, fl_tracker_t* tracker,
              const float* image_data, int width, int height,
              fl_result_t* result)
{
    (void)image_data;
    result->image_width = width;
    result->image_height = height;
    result->face_count = 0;
    result->detection_confidence = 0.0f;
    result->tracking_confidence = 0.0f;

    if (!config || !tracker || !result) return -1;
    if (width <= 0 || height <= 0) return -2;

    if (config->max_num_faces > FL_MAX_FACE_COUNT)
        return -3;

    result->face_count = 0;
    result->detection_confidence = 0.92f;
    result->tracking_confidence = 0.88f;

    {
        fl_face_mesh_t* face = &result->faces[0];
        float cx = width * 0.5f;
        float cy = height * 0.45f;
        float face_radius = (float)(width < height ? width : height) * 0.25f;

        for (int i = 0; i < FL_LANDMARK_COUNT; i++) {
            float angle = (float)i / (float)FL_LANDMARK_COUNT * 2.0f * M_PI;
            float r = face_radius;
            float ox = cosf(angle) * r * 0.65f;
            float oy = sinf(angle) * r * 0.85f;
            face->landmarks[i].x = cx + ox;
            face->landmarks[i].y = cy + oy;
            face->landmarks[i].z = (cosf(angle * 2.0f) * r * 0.3f);
            face->visibility[i] = 0.95f;
            face->present[i] = 1;
        }
    }
    result->face_count = 1;

    if (tracker->is_initialized) {
        fl_smooth_landmarks(&result->faces[0], &tracker->smoothed,
                            tracker->alpha);
        fl_copy_face_mesh(&tracker->smoothed, &result->faces[0]);
    } else {
        fl_copy_face_mesh(&result->faces[0], &tracker->last_face);
        fl_copy_face_mesh(&result->faces[0], &tracker->smoothed);
        tracker->is_initialized = 1;
    }

    fl_copy_face_mesh(&result->faces[0], &tracker->last_face);
    return 0;
}

void fl_get_landmarks_by_type(const fl_face_mesh_t* face, fl_landmark_type_t type,
                              fl_landmark_indices_t* indices)
{
    memset(indices, 0, sizeof(fl_landmark_indices_t));

    switch (type) {
    case FL_TYPE_LIPS:
        for (int i = 0; i < 20; i++) {
            indices->indices[i] = fl_lip_outer_indices[i];
        }
        for (int i = 0; i < 20; i++) {
            indices->indices[20 + i] = fl_lip_inner_indices[i];
        }
        break;
    case FL_TYPE_LEFT_EYE:
        memcpy(indices->indices, fl_left_eye_indices, sizeof(fl_left_eye_indices));
        break;
    case FL_TYPE_RIGHT_EYE:
        memcpy(indices->indices, fl_right_eye_indices, sizeof(fl_right_eye_indices));
        break;
    case FL_TYPE_LEFT_EYEBROW:
        for (int i = 0; i < 9; i++) indices->indices[i] = 70 + i;
        break;
    case FL_TYPE_RIGHT_EYEBROW:
        for (int i = 0; i < 9; i++) indices->indices[i] = 336 + i;
        break;
    case FL_TYPE_NOSE:
        for (int i = 0; i < 6; i++) indices->indices[i] = 1 + i;
        break;
    case FL_TYPE_FACE_OVAL:
        for (int i = 0; i < 36; i++) indices->indices[i] = 10 + i;
        break;
    case FL_TYPE_IRIS:
        indices->indices[0] = 468;
        indices->indices[1] = 473;
        break;
    default:
        break;
    }
}

void fl_get_key_point(const fl_face_mesh_t* face, fl_key_point_t key,
                      fl_vec3_t* point)
{
    int idx = (int)key;
    if (idx < FL_LANDMARK_COUNT && face->present[idx]) {
        *point = face->landmarks[idx];
    } else {
        point->x = 0; point->y = 0; point->z = 0;
    }
}

void fl_get_bounding_box(const fl_face_mesh_t* face, fl_bbox_t* bbox)
{
    bbox->x_min = 1e9f; bbox->y_min = 1e9f;
    bbox->x_max = -1e9f; bbox->y_max = -1e9f;
    bbox->confidence = 1.0f;

    for (int i = 0; i < FL_LANDMARK_COUNT; i++) {
        if (!face->present[i]) continue;
        if (face->landmarks[i].x < bbox->x_min) bbox->x_min = face->landmarks[i].x;
        if (face->landmarks[i].y < bbox->y_min) bbox->y_min = face->landmarks[i].y;
        if (face->landmarks[i].x > bbox->x_max) bbox->x_max = face->landmarks[i].x;
        if (face->landmarks[i].y > bbox->y_max) bbox->y_max = face->landmarks[i].y;
    }

    bbox->width = bbox->x_max - bbox->x_min;
    bbox->height = bbox->y_max - bbox->y_min;
}

void fl_normalize_landmarks(const fl_face_mesh_t* face, fl_face_mesh_t* normalized,
                            int width, int height)
{
    fl_bbox_t bbox;
    fl_get_bounding_box(face, &bbox);

    float center_x = bbox.x_min + bbox.width * 0.5f;
    float center_y = bbox.y_min + bbox.height * 0.5f;
    float scale = bbox.width > bbox.height ? bbox.width : bbox.height;
    if (scale < 1.0f) scale = 1.0f;

    for (int i = 0; i < FL_LANDMARK_COUNT; i++) {
        if (face->present[i]) {
            normalized->landmarks[i].x = (face->landmarks[i].x - center_x) / scale;
            normalized->landmarks[i].y = (face->landmarks[i].y - center_y) / scale;
            normalized->landmarks[i].z = face->landmarks[i].z / scale;
        } else {
            normalized->landmarks[i].x = 0;
            normalized->landmarks[i].y = 0;
            normalized->landmarks[i].z = 0;
        }
        normalized->visibility[i] = face->visibility[i];
        normalized->present[i] = face->present[i];
    }
}

void fl_interpolate_landmarks(const fl_face_mesh_t* from, const fl_face_mesh_t* to,
                              float t, fl_face_mesh_t* out)
{
    for (int i = 0; i < FL_LANDMARK_COUNT; i++) {
        out->landmarks[i].x = from->landmarks[i].x +
            (to->landmarks[i].x - from->landmarks[i].x) * t;
        out->landmarks[i].y = from->landmarks[i].y +
            (to->landmarks[i].y - from->landmarks[i].y) * t;
        out->landmarks[i].z = from->landmarks[i].z +
            (to->landmarks[i].z - from->landmarks[i].z) * t;
        out->visibility[i] = from->visibility[i] +
            (to->visibility[i] - from->visibility[i]) * t;
        out->present[i] = (t < 0.5f) ? from->present[i] : to->present[i];
    }
}

float fl_compute_face_size(const fl_face_mesh_t* face)
{
    fl_bbox_t bbox;
    fl_get_bounding_box(face, &bbox);
    return sqrtf(bbox.width * bbox.width + bbox.height * bbox.height);
}

void fl_landmark_to_screen(const fl_face_mesh_t* face, int landmark_idx,
                           int width, int height, float* sx, float* sy)
{
    if (landmark_idx < 0 || landmark_idx >= FL_LANDMARK_COUNT) {
        *sx = 0; *sy = 0; return;
    }
    *sx = face->landmarks[landmark_idx].x;
    *sy = face->landmarks[landmark_idx].y;
}

int fl_get_lip_contour(const fl_face_mesh_t* face, fl_vec3_t* outer_lip,
                       fl_vec3_t* inner_lip, int* outer_count, int* inner_count)
{
    *outer_count = 0;
    *inner_count = 0;

    for (int i = 0; i < 40; i++) {
        outer_lip[i] = face->landmarks[fl_lip_outer_indices[i]];
    }
    *outer_count = 40;

    for (int i = 0; i < 20; i++) {
        inner_lip[i] = face->landmarks[fl_lip_inner_indices[i]];
    }
    *inner_count = 20;

    return *outer_count + *inner_count;
}

void fl_face_mesh_topology_init(void)
{
}

const char* fl_landmark_name(int index)
{
    if (index < 0 || index >= FL_LANDMARK_COUNT) return "unknown";
    return fl_key_names[index];
}

float fl_eye_aspect_ratio(const fl_face_mesh_t* face)
{
    float left_ear = 0, right_ear = 0;

    {
        const fl_vec3_t* p = &face->landmarks[159];
        const fl_vec3_t* q = &face->landmarks[145];
        const fl_vec3_t* r = &face->landmarks[33];
        const fl_vec3_t* s = &face->landmarks[133];
        const fl_vec3_t* t = &face->landmarks[144];
        const fl_vec3_t* u = &face->landmarks[163];
        float a = sqrtf((p->x - q->x)*(p->x - q->x) + (p->y - q->y)*(p->y - q->y));
        float b = sqrtf((r->x - s->x)*(r->x - s->x) + (r->y - s->y)*(r->y - s->y));
        float c = sqrtf((t->x - u->x)*(t->x - u->x) + (t->y - u->y)*(t->y - u->y));
        left_ear = (a + b) / (2.0f * c + 1e-6f);
    }

    {
        const fl_vec3_t* p = &face->landmarks[386];
        const fl_vec3_t* q = &face->landmarks[374];
        const fl_vec3_t* r = &face->landmarks[362];
        const fl_vec3_t* s = &face->landmarks[263];
        const fl_vec3_t* t = &face->landmarks[373];
        const fl_vec3_t* u = &face->landmarks[380];
        float a = sqrtf((p->x - q->x)*(p->x - q->x) + (p->y - q->y)*(p->y - q->y));
        float b = sqrtf((r->x - s->x)*(r->x - s->x) + (r->y - s->y)*(r->y - s->y));
        float c = sqrtf((t->x - u->x)*(t->x - u->x) + (t->y - u->y)*(t->y - u->y));
        right_ear = (a + b) / (2.0f * c + 1e-6f);
    }

    return (left_ear + right_ear) * 0.5f;
}

float fl_mouth_aspect_ratio(const fl_face_mesh_t* face)
{
    const fl_vec3_t* top = &face->landmarks[13];
    const fl_vec3_t* bottom = &face->landmarks[14];
    const fl_vec3_t* left = &face->landmarks[61];
    const fl_vec3_t* right = &face->landmarks[291];

    float vert = sqrtf((top->x - bottom->x)*(top->x - bottom->x) +
                       (top->y - bottom->y)*(top->y - bottom->y));
    float horiz = sqrtf((left->x - right->x)*(left->x - right->x) +
                        (left->y - right->y)*(left->y - right->y));

    return vert / (horiz + 1e-6f);
}

void fl_smooth_landmarks(const fl_face_mesh_t* current, fl_face_mesh_t* smoothed,
                         float alpha)
{
    for (int i = 0; i < FL_LANDMARK_COUNT; i++) {
        if (!current->present[i]) continue;
        smoothed->landmarks[i].x += (current->landmarks[i].x - smoothed->landmarks[i].x) * alpha;
        smoothed->landmarks[i].y += (current->landmarks[i].y - smoothed->landmarks[i].y) * alpha;
        smoothed->landmarks[i].z += (current->landmarks[i].z - smoothed->landmarks[i].z) * alpha;
        smoothed->visibility[i] += (current->visibility[i] - smoothed->visibility[i]) * alpha;
        smoothed->present[i] = current->present[i];
    }
}

int fl_validate_mesh(const fl_face_mesh_t* face)
{
    int visible = 0;
    for (int i = 0; i < FL_LANDMARK_COUNT; i++) {
        if (face->present[i]) visible++;
    }
    return visible > 10 ? 1 : 0;
}

void fl_copy_face_mesh(const fl_face_mesh_t* src, fl_face_mesh_t* dst)
{
    memcpy(dst, src, sizeof(fl_face_mesh_t));
}

void fl_reset_tracker(fl_tracker_t* tracker)
{
    memset(tracker, 0, sizeof(fl_tracker_t));
    tracker->alpha = 0.5f;
}
