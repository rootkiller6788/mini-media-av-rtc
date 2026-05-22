/* demo_face_landmark.c — Interactive face landmark demo (250+ lines)
 *
 * Demonstrates full face landmark pipeline:
 *   - Detection with confidence filtering
 *   - 468 3D landmark extraction
 *   - Eye Aspect Ratio (EAR) for blink detection
 *   - Mouth Aspect Ratio (MAR) for mouth-open detection
 *   - Landmark classification by type (lips, eyes, eyebrows, nose, face_oval)
 *   - Face bounding box computation
 *   - Landmark normalization (relative to face size)
 *   - Interpolation between frames
 *   - Key point extraction
 *   - Smoothing filter over time
 *
 * Usage: demo_face_landmark [frames=60]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "face_landmark.h"

#define DEF_WIDTH    1280
#define DEF_HEIGHT   720
#define DEF_FRAMES   60

typedef struct {
    float ear_history[90];
    float mar_history[90];
    int history_count;
    int blink_count;
    float total_eye_closed_time;
    int mouth_open_count;
    float avg_face_size;
    float landmark_jitter;
} demo_stats_t;

static void stats_init(demo_stats_t* s)
{
    memset(s, 0, sizeof(demo_stats_t));
}

static void simulate_face(float* image, int w, int h, int frame,
                          float* blink_phase, float* mouth_open)
{
    int count = w * h;
    float t = frame * 0.1f;

    *blink_phase = sinf(t * 0.7f + 0.5f);
    *mouth_open = fabsf(sinf(t * 0.3f)) * 0.6f;

    float center_x = w * (0.5f + sinf(t * 0.2f) * 0.03f);
    float center_y = h * (0.45f + cosf(t * 0.15f) * 0.02f);

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            float dx = x - center_x;
            float dy = y - center_y;
            float dist = sqrtf(dx*dx + dy*dy);
            float face_radius = 130.0f;
            float base = dist < face_radius ? 0.7f : 0.3f;
            image[y * w + x] = base + ((float)(rand() % 1000) / 15000.0f);
        }
    }
}

static void print_bar(const char* label, float value, int width)
{
    printf("  %-18s [", label);
    int filled = (int)(value * width);
    for (int i = 0; i < width; i++) {
        printf("%c", i < filled ? '#' : ' ');
    }
    printf("] %.3f\n", value);
}

static void print_face_status(const fl_face_mesh_t* face, int frame,
                              const demo_stats_t* stats)
{
    float ear = fl_eye_aspect_ratio(face);
    float mar = fl_mouth_aspect_ratio(face);
    float face_size = fl_compute_face_size(face);

    int blinking = (ear < 0.22f) ? 1 : 0;
    int mouth_open = (mar > 0.35f) ? 1 : 0;

    printf("\n=== Frame %d ===\n", frame);
    printf("  Face size: %.1f px (avg: %.1f)\n", face_size, stats->avg_face_size);

    print_bar("Eye AR (blink)", ear, 40);
    printf("  %s\n", blinking ? "  *** BLINK ***" : "");

    print_bar("Mouth AR (open)", mar, 40);
    printf("  %s\n", mouth_open ? "  *** MOUTH OPEN ***" : "");

    fl_bbox_t bbox;
    fl_get_bounding_box(face, &bbox);
    printf("  BBox: [%.0f, %.0f] - [%.0f, %.0f]  %.0fx%.0f\n",
           bbox.x_min, bbox.y_min, bbox.x_max, bbox.y_max,
           bbox.width, bbox.height);

    fl_vec3_t nose_tip, left_eye, right_eye, lip_top, lip_bottom;
    fl_get_key_point(face, FL_KEY_NOSE_TIP, &nose_tip);
    fl_get_key_point(face, FL_KEY_LEFT_EYE_CENTER, &left_eye);
    fl_get_key_point(face, FL_KEY_RIGHT_EYE_CENTER, &right_eye);
    fl_get_key_point(face, FL_KEY_LIP_TOP, &lip_top);
    fl_get_key_point(face, FL_KEY_LIP_BOTTOM, &lip_bottom);

    printf("  Key points:\n");
    printf("    nose_tip:       (%7.1f, %7.1f, %6.2f)\n", nose_tip.x, nose_tip.y, nose_tip.z);
    printf("    left_eye_ctr:   (%7.1f, %7.1f, %6.2f)\n", left_eye.x, left_eye.y, left_eye.z);
    printf("    right_eye_ctr:  (%7.1f, %7.1f, %6.2f)\n", right_eye.x, right_eye.y, right_eye.z);
    printf("    lip_top:        (%7.1f, %7.1f, %6.2f)\n", lip_top.x, lip_top.y, lip_top.z);
    printf("    lip_bottom:     (%7.1f, %7.1f, %6.2f)\n", lip_bottom.x, lip_bottom.y, lip_bottom.z);

    float inter_eye_dist = sqrtf(
        (left_eye.x - right_eye.x) * (left_eye.x - right_eye.x) +
        (left_eye.y - right_eye.y) * (left_eye.y - right_eye.y));
    printf("    inter-eye dist: %.1f px\n", inter_eye_dist);
}

static void print_stats_brief(const demo_stats_t* stats, int frame)
{
    printf("\n-- Session Stats (frame %d) --\n", frame);
    printf("  Blinks detected: %d\n", stats->blink_count);
    printf("  Mouth opens:     %d\n", stats->mouth_open_count);
    printf("  Avg face size:   %.1f px\n", stats->avg_face_size);
    printf("  Jitter:          %.4f px\n", stats->landmark_jitter);
}

static void analyze_landmark_distribution(const fl_face_mesh_t* face)
{
    printf("\n  Landmark distribution by type:\n");
    struct { const char* name; fl_landmark_type_t type; int expected; } types[] = {
        {"Lips",       FL_TYPE_LIPS,        40},
        {"Left Eye",   FL_TYPE_LEFT_EYE,    16},
        {"Right Eye",  FL_TYPE_RIGHT_EYE,   16},
        {"Left Brow",  FL_TYPE_LEFT_EYEBROW, 9},
        {"Right Brow", FL_TYPE_RIGHT_EYEBROW,9},
        {"Nose",       FL_TYPE_NOSE,         6},
        {"Face Oval",  FL_TYPE_FACE_OVAL,   36},
        {"Iris",       FL_TYPE_IRIS,         2},
    };

    for (int t = 0; t < 8; t++) {
        fl_landmark_indices_t indices;
        fl_get_landmarks_by_type(face, types[t].type, &indices);
        int count = 0;
        for (int k = 0; k < 40; k++) {
            if (indices.indices[k] >= 0 && indices.indices[k] < FL_LANDMARK_COUNT)
                count = k + 1;
            else break;
        }
        printf("    %-12s: %d/%d landmarks\n", types[t].name, count, types[t].expected);
    }
}

int main(int argc, char** argv)
{
    int num_frames = DEF_FRAMES;
    if (argc > 1) {
        num_frames = atoi(argv[1]);
        if (num_frames <= 0) num_frames = DEF_FRAMES;
    }

    printf("============================================================\n");
    printf("  Mini Digital Human — Face Landmark Demo\n");
    printf("  C99 Implementation | 468 3D Landmarks (MediaPipe-style)\n");
    printf("  Frames: %d | Resolution: %dx%d\n", num_frames, DEF_WIDTH, DEF_HEIGHT);
    printf("============================================================\n\n");

    fl_tracker_t tracker;
    fl_config_t config;
    demo_stats_t stats;

    fl_init(&tracker, 0.35f);
    fl_config_default(&config);
    config.max_num_faces = 1;
    config.min_detection_confidence = 0.5f;
    config.min_tracking_confidence = 0.5f;
    config.refine_landmarks = 1;
    stats_init(&stats);

    printf("Tracker: alpha=%.2f | Config: max_faces=%d, det_conf=%.2f, trk_conf=%.2f\n\n",
           tracker.alpha, config.max_num_faces,
           config.min_detection_confidence, config.min_tracking_confidence);

    float* image = (float*)malloc(DEF_WIDTH * DEF_HEIGHT * sizeof(float));
    if (!image) { fprintf(stderr, "ERROR: malloc failed\n"); return 1; }

    fl_face_mesh_t prev_face;
    memset(&prev_face, 0, sizeof(prev_face));
    int prev_valid = 0;

    int report_interval = num_frames / 5;
    if (report_interval < 1) report_interval = 1;

    for (int frame = 0; frame < num_frames; frame++) {
        float blink_phase, mouth_open;
        simulate_face(image, DEF_WIDTH, DEF_HEIGHT, frame, &blink_phase, &mouth_open);

        fl_result_t result;
        int ret = fl_detect(&config, &tracker, image,
                            DEF_WIDTH, DEF_HEIGHT, &result);

        if (ret != 0 || result.face_count < 1) {
            printf("Frame %d: detection failed (ret=%d, faces=%d)\n",
                   frame, ret, result.face_count);
            continue;
        }

        fl_face_mesh_t* face = &result.faces[0];
        int valid = fl_validate_mesh(face);

        if (!valid) {
            printf("Frame %d: invalid mesh\n", frame);
            continue;
        }

        float ear = fl_eye_aspect_ratio(face);
        float mar = fl_mouth_aspect_ratio(face);
        float face_size = fl_compute_face_size(face);

        if (stats.history_count < 90) {
            stats.ear_history[stats.history_count] = ear;
            stats.mar_history[stats.history_count] = mar;
            stats.history_count++;
        }

        if (ear < 0.22f) {
            stats.blink_count++;
            stats.total_eye_closed_time += 1.0f;
        }
        if (mar > 0.35f) {
            stats.mouth_open_count++;
        }

        if (prev_valid) {
            float jitter = 0.0f;
            int count = 0;
            for (int i = 0; i < FL_LANDMARK_COUNT; i++) {
                if (face->present[i] && prev_face.present[i]) {
                    float dx = face->landmarks[i].x - prev_face.landmarks[i].x;
                    float dy = face->landmarks[i].y - prev_face.landmarks[i].y;
                    jitter += sqrtf(dx*dx + dy*dy);
                    count++;
                }
            }
            if (count > 0) stats.landmark_jitter = jitter / count;
        }

        stats.avg_face_size = (stats.avg_face_size * frame + face_size) / (frame + 1);

        if (frame % report_interval == 0) {
            print_face_status(face, frame, &stats);
            analyze_landmark_distribution(face);

            fl_face_mesh_t normalized;
            fl_normalize_landmarks(face, &normalized, DEF_WIDTH, DEF_HEIGHT);
            printf("\n  Normalized landmark sample (first 5):\n");
            for (int i = 0; i < 5 && i < FL_LANDMARK_COUNT; i++) {
                printf("    [%3d]: (%.4f, %.4f, %.4f)\n", i,
                       normalized.landmarks[i].x,
                       normalized.landmarks[i].y,
                       normalized.landmarks[i].z);
            }

            if (prev_valid) {
                fl_face_mesh_t interp;
                fl_interpolate_landmarks(&prev_face, face, 0.5f, &interp);
                printf("  Mid-interpolation nose_tip: (%.1f, %.1f)\n",
                       interp.landmarks[FL_KEY_NOSE_TIP].x,
                       interp.landmarks[FL_KEY_NOSE_TIP].y);
            }

            int lip_outer, lip_inner;
            fl_vec3_t outer[40], inner[20];
            fl_get_lip_contour(face, outer, inner, &lip_outer, &lip_inner);
            printf("  Lip contour: %d outer + %d inner points\n", lip_outer, lip_inner);
        }

        fl_copy_face_mesh(face, &prev_face);
        prev_valid = 1;
    }

    print_stats_brief(&stats, num_frames);

    printf("\n--- Blink analysis ---\n");
    printf("  Total blinks: %d\n", stats.blink_count);
    printf("  Estimated blink rate: %.1f /min\n",
           stats.blink_count * 60.0f / (float)num_frames);

    printf("\n--- Mouth analysis ---\n");
    printf("  Mouth open frames: %d / %d (%.1f%%)\n",
           stats.mouth_open_count, num_frames,
           stats.mouth_open_count * 100.0f / num_frames);

    printf("\n--- EAR History ---\n");
    for (int i = 0; i < stats.history_count && i < 90; i++) {
        printf("  [%2d] EAR=%.4f %s\n", i, stats.ear_history[i],
               stats.ear_history[i] < 0.22f ? "<-- BLINK" : "");
    }

    printf("\n--- MAR History ---\n");
    for (int i = 0; i < stats.history_count && i < 90; i++) {
        printf("  [%2d] MAR=%.4f %s\n", i, stats.mar_history[i],
               stats.mar_history[i] > 0.35f ? "<-- OPEN" : "");
    }

    fl_reset_tracker(&tracker);
    free(image);

    printf("\n============================================================\n");
    printf("  Face Landmark Demo Complete.\n");
    printf("============================================================\n");
    return 0;
}
