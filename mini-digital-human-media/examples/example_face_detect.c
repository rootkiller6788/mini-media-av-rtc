/* example_face_detect.c — Face landmark detection example */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "face_landmark.h"

#define WIDTH  640
#define HEIGHT 480

static void print_landmark(const fl_face_mesh_t* face, int idx)
{
    printf("  [%3d] (%.1f, %.1f, %.2f) vis=%.2f pres=%d",
           idx,
           face->landmarks[idx].x,
           face->landmarks[idx].y,
           face->landmarks[idx].z,
           face->visibility[idx],
           face->present[idx]);

    const char* name = fl_landmark_name(idx);
    if (name && name[0] != '\0') {
        printf("  <%s>", name);
    }
    printf("\n");
}

static void simulate_image(float* image, int w, int h, int frame)
{
    int count = w * h;
    float base = 0.5f;
    float wave = sinf(frame * 0.1f) * 0.1f;
    for (int i = 0; i < count; i++) {
        image[i] = base + wave + ((float)(rand() % 1000) / 10000.0f);
    }
}

int main(void)
{
    printf("=== Mini Digital Human: Face Landmark Detection Example ===\n\n");

    fl_tracker_t tracker;
    fl_config_t config;

    fl_init(&tracker, 0.4f);
    fl_config_default(&config);
    config.max_num_faces = 2;
    config.min_detection_confidence = 0.6f;
    config.refine_landmarks = 1;

    printf("Tracker initialized (alpha=%.2f)\n", tracker.alpha);
    printf("Config: max_faces=%d, detection_conf=%.2f\n\n",
           config.max_num_faces, config.min_detection_confidence);

    float* image = (float*)malloc(WIDTH * HEIGHT * sizeof(float));
    if (!image) { fprintf(stderr, "malloc failed\n"); return 1; }

    fl_face_mesh_t prev_face;
    memset(&prev_face, 0, sizeof(prev_face));

    for (int frame = 0; frame < 10; frame++) {
        simulate_image(image, WIDTH, HEIGHT, frame);

        fl_result_t result;
        int ret = fl_detect(&config, &tracker, image, WIDTH, HEIGHT, &result);

        printf("Frame %3d: faces=%d, det_conf=%.2f, trk_conf=%.2f (ret=%d)\n",
               frame, result.face_count,
               result.detection_confidence,
               result.tracking_confidence, ret);

        if (result.face_count > 0) {
            fl_face_mesh_t* face = &result.faces[0];

            fl_bbox_t bbox;
            fl_get_bounding_box(face, &bbox);
            printf("  BBox: [%.0f, %.0f, %.0f, %.0f] (%.0f x %.0f)\n",
                   bbox.x_min, bbox.y_min, bbox.x_max, bbox.y_max,
                   bbox.width, bbox.height);

            float face_size = fl_compute_face_size(face);
            printf("  Face size: %.1f px\n", face_size);

            printf("  Key points:\n");
            print_landmark(face, FL_KEY_NOSE_TIP);
            print_landmark(face, FL_KEY_LEFT_EYE_CENTER);
            print_landmark(face, FL_KEY_RIGHT_EYE_CENTER);
            print_landmark(face, FL_KEY_LIP_TOP);
            print_landmark(face, FL_KEY_LIP_BOTTOM);

            float ear = fl_eye_aspect_ratio(face);
            float mar = fl_mouth_aspect_ratio(face);
            printf("  Eye Aspect Ratio: %.3f (blink threshold: 0.2)\n", ear);
            printf("  Mouth Aspect Ratio: %.3f\n", mar);

            if (ear < 0.22f) {
                printf("  ** Eye blink detected! **\n");
            }
            if (mar > 0.35f) {
                printf("  ** Mouth open detected! **\n");
            }

            if (frame > 0) {
                fl_face_mesh_t interp;
                fl_interpolate_landmarks(&prev_face, face, 0.5f, &interp);
                printf("  Interpolated face at t=0.5: nose_tip=(%.1f, %.1f)\n",
                       interp.landmarks[FL_KEY_NOSE_TIP].x,
                       interp.landmarks[FL_KEY_NOSE_TIP].y);
            }

            fl_face_mesh_t normalized;
            fl_normalize_landmarks(face, &normalized, WIDTH, HEIGHT);
            printf("  Normalized nose_tip: (%.3f, %.3f, %.3f)\n",
                   normalized.landmarks[FL_KEY_NOSE_TIP].x,
                   normalized.landmarks[FL_KEY_NOSE_TIP].y,
                   normalized.landmarks[FL_KEY_NOSE_TIP].z);

            fl_vec3_t outer_lip[40], inner_lip[20];
            int outer_cnt, inner_cnt;
            fl_get_lip_contour(face, outer_lip, inner_lip, &outer_cnt, &inner_cnt);
            printf("  Lip contour: outer=%d points, inner=%d points\n",
                   outer_cnt, inner_cnt);

            printf("  Landmarks by type:\n");
            const char* type_names[] = {"lips", "left_eye", "right_eye",
                                        "left_eyebrow", "right_eyebrow",
                                        "nose", "face_oval", "iris"};
            for (int t = 0; t < FL_TYPE_COUNT; t++) {
                fl_landmark_indices_t indices;
                fl_get_landmarks_by_type(face, (fl_landmark_type_t)t, &indices);
                int count = 0;
                for (int k = 0; k < 40; k++) {
                    if (indices.indices[k] > 0) count = k + 1;
                    else break;
                }
                printf("    %s: %d landmarks\n", type_names[t], count);
            }

            fl_copy_face_mesh(face, &prev_face);
        }
        printf("\n");
    }

    printf("Flush tracker...\n");
    fl_reset_tracker(&tracker);

    free(image);
    printf("Done.\n");
    return 0;
}
