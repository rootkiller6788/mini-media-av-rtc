/* demo_avatar_full.c — Full avatar driving demo (250+ lines)
 *
 * Demonstrates the complete digital human pipeline:
 *   - Face landmark detection -> facial animation
 *   - Body pose estimation -> skeleton retargeting
 *   - Expression blendshapes -> emotion display
 *   - Audio-driven lip sync -> viseme blending
 *   - IK solving -> limb positioning
 *   - FK computation -> world-space transforms
 *   - Multi-modal fusion -> combined face+body+audio
 *   - Idle animation -> natural resting behavior
 *   - Skeleton blending -> smooth transitions
 *
 * Usage: demo_avatar_full [duration_s=10]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "avatar_drive.h"

#define IMG_WIDTH   1280
#define IMG_HEIGHT  720
#define AUDIO_RATE  16000
#define DEF_DURATION 10

typedef struct {
    int frames_processed;
    int face_detections;
    int body_detections;
    int audio_frames_processed;
    float total_ik_error;
    int ik_solves;
    float avg_blend_time;
    int idle_triggers;
    int expressions_changed;
    struct {
        float jaw_open_sum;
        float mouth_smile_sum;
        float blink_sum;
        int samples;
    } blendshape_history;
} demo_full_stats_t;

static void demo_stats_init(demo_full_stats_t* s)
{
    memset(s, 0, sizeof(demo_full_stats_t));
}

static void generate_simulated_input(float* image, int w, int h, int frame,
                                     float* audio, int audio_len, float dt)
{
    for (int i = 0; i < w * h; i++) {
        image[i] = 0.5f + sinf(frame * 0.05f + i * 0.001f) * 0.1f;
    }

    for (int i = 0; i < audio_len; i++) {
        float t = (float)i / AUDIO_RATE + dt;
        audio[i] = sinf(t * 220.0f * 2.0f * 3.14159265f) * 0.3f *
                   (1.0f - fabsf(sinf(t * 1.5f)));
        if (audio[i] > 1.0f) audio[i] = 1.0f;
        if (audio[i] < -1.0f) audio[i] = -1.0f;
    }
}

static void print_skeleton_bone(const ad_skeleton_t* skel, int idx)
{
    if (idx < 0 || idx >= skel->bone_count) return;
    const ad_bone_t* b = &skel->bones[idx];
    printf("  %-20s pos=(%+.3f,%+.3f,%+.3f)  parent=%d\n",
           b->name,
           b->current_pose.translation.x,
           b->current_pose.translation.y,
           b->current_pose.translation.z,
           b->parent_index);
}

static void print_pose_analysis(const bp_body_t* body)
{
    printf("  Pose Analysis:\n");
    bp_pose_class_t cls = bp_classify_pose(body);
    printf("    Classification: %s\n", bp_pose_class_name(cls));

    bp_joint_angle_t angles[8];
    bp_compute_all_angles(body, angles);
    printf("    Joint angles (deg):\n");
    const char* angle_names[] = {
        "L_shoulder-elbow-wrist", "R_shoulder-elbow-wrist",
        "L_hip-knee-ankle",       "R_hip-knee-ankle",
        "L_shoulder-hip-knee",    "R_shoulder-hip-knee",
        "L_hip-shoulder-elbow",   "R_hip-shoulder-elbow"
    };
    for (int i = 0; i < 8; i++) {
        printf("      %-28s: %6.1f deg (conf=%.2f)\n",
               angle_names[i], angles[i].angle_deg, angles[i].confidence);
    }

    bp_vec3_t hip_center;
    bp_get_hip_center(body, &hip_center);
    printf("    Hip center: (%.1f, %.1f, %.1f)\n", hip_center.x, hip_center.y, hip_center.z);
}

static void print_expression_report(const ebs_blendshapes_t* bs)
{
    printf("  Expression Blendshapes:\n");

    int key_ids[] = { EBS_JAW_OPEN, EBS_MOUTH_SMILE_LEFT, EBS_MOUTH_SMILE_RIGHT,
                      EBS_EYE_BLINK_LEFT, EBS_EYE_BLINK_RIGHT,
                      EBS_BROW_DOWN_LEFT, EBS_MOUTH_PUCKER, EBS_MOUTH_FUNNEL,
                      EBS_NOSE_SNEER_LEFT, EBS_TONGUE_OUT };
    for (int k = 0; k < 10; k++) {
        float w = ebs_get_blendshape_weight(bs, key_ids[k]);
        if (w > 0.01f) {
            printf("    %-24s: %.3f\n", ebs_blendshape_name(key_ids[k]), w);
        }
    }

    ebs_expression_t expr;
    float conf;
    ebs_classify_expression(bs, &expr, &conf);
    printf("    Dominant: %s (%.2f)\n", ebs_expression_name(expr), conf);
}

static void run_ik_demo(const bp_body_t* body, ad_skeleton_t* skel)
{
    printf("  IK Demonstration:\n");

    ad_ik_solver_t ik;
    memset(&ik, 0, sizeof(ik));
    ik.chain.joint_count = 3;
    ik.chain.joint_parents[0] = -1;
    ik.chain.joint_parents[1] = 0;
    ik.chain.joint_parents[2] = 1;
    ik.chain.joint_lengths[0] = 1.0f;
    ik.chain.joint_lengths[1] = 1.0f;
    ik.chain.solver_iterations = 30;
    ik.chain.solver_tolerance = 0.001f;

    ik.chain.joint_positions[0].x = 0.0f;
    ik.chain.joint_positions[0].y = 0.0f;
    ik.chain.joint_positions[0].z = 0.0f;
    ik.chain.joint_positions[1].x = 0.0f;
    ik.chain.joint_positions[1].y = 1.0f;
    ik.chain.joint_positions[1].z = 0.0f;
    ik.chain.joint_positions[2].x = 0.0f;
    ik.chain.joint_positions[2].y = 2.0f;
    ik.chain.joint_positions[2].z = 0.0f;

    ad_vec3_t targets[3] = {
        {0.5f, 0.5f, 0.0f},
        {-0.5f, 0.8f, 0.3f},
        {0.0f, 0.0f, 1.5f}
    };

    for (int t = 0; t < 3; t++) {
        ad_vec3_t save[3];
        memcpy(save, ik.chain.joint_positions, sizeof(save));

        ad_ik_fabrik(&ik.chain, &targets[t], 0.001f, 50);

        float dx = ik.chain.joint_positions[2].x - targets[t].x;
        float dy = ik.chain.joint_positions[2].y - targets[t].y;
        float dz = ik.chain.joint_positions[2].z - targets[t].z;
        float dist = sqrtf(dx*dx + dy*dy + dz*dz);

        printf("    Target %d: (%.2f,%.2f,%.2f) -> end=(%.3f,%.3f,%.3f) err=%.4f\n",
               t, targets[t].x, targets[t].y, targets[t].z,
               ik.chain.joint_positions[2].x,
               ik.chain.joint_positions[2].y,
               ik.chain.joint_positions[2].z, dist);

        memcpy(ik.chain.joint_positions, save, sizeof(save));
    }

    (void)body;
    (void)skel;
}

int main(int argc, char** argv)
{
    int duration = DEF_DURATION;
    if (argc > 1) {
        duration = atoi(argv[1]);
        if (duration <= 0) duration = DEF_DURATION;
    }

    printf("================================================================\n");
    printf("  Mini Digital Human — Full Avatar Drive Demo\n");
    printf("  C99 Implementation | Face + Body + Audio + Expression\n");
    printf("  Duration: %ds | %dx%d | %d Hz Audio\n",
           duration, IMG_WIDTH, IMG_HEIGHT, AUDIO_RATE);
    printf("================================================================\n\n");

    demo_full_stats_t stats;
    demo_stats_init(&stats);

    /* Initialize driver */
    ad_driver_t driver;
    ad_init_driver(&driver);

    printf("=== Initialization ===\n");
    printf("  Driver: OK (skeleton %d bones, retarget %d maps)\n",
           driver.skeleton.bone_count, driver.retarget.map_count);
    printf("  Lip sync: transition_speed=%.1f\n", driver.lip_sync.transition_speed);
    printf("  Idle: threshold=%.1fs, timeout=%.1fs\n",
           driver.idle.idle_threshold, driver.idle.idle_timeout);

    /* Initialize face tracker */
    fl_tracker_t face_tracker;
    fl_config_t face_config;
    fl_init(&face_tracker, 0.4f);
    fl_config_default(&face_config);
    printf("  Face tracker: alpha=%.2f, max_faces=%d\n",
           face_tracker.alpha, face_config.max_num_faces);

    /* Allocate buffers */
    float* image = (float*)malloc(IMG_WIDTH * IMG_HEIGHT * sizeof(float));
    int audio_chunk = (int)(AUDIO_RATE * 0.1f);
    float* audio = (float*)malloc(audio_chunk * sizeof(float));

    if (!image || !audio) {
        fprintf(stderr, "ERROR: Memory allocation failed\n");
        return 1;
    }

    ad_skeleton_t output_skeleton;
    ad_build_default_skeleton(&output_skeleton);

    float sim_time = 0.0f;
    int total_frames = duration * 10;
    int report_interval = total_frames / 8;
    if (report_interval < 1) report_interval = 1;

    printf("\n=== Simulation Loop (%d frames) ===\n\n", total_frames);

    for (int frame = 0; frame < total_frames; frame++) {
        float dt = 0.1f;
        sim_time += dt;

        generate_simulated_input(image, IMG_WIDTH, IMG_HEIGHT, frame,
                                 audio, audio_chunk, sim_time);

        /* Face detection */
        fl_result_t face_result;
        int face_ok = (fl_detect(&face_config, &face_tracker,
                                  image, IMG_WIDTH, IMG_HEIGHT, &face_result) == 0);
        if (face_ok && face_result.face_count > 0) stats.face_detections++;

        /* Body detection */
        bp_result_t body_result;
        bp_detect(NULL, IMG_WIDTH, IMG_HEIGHT, &body_result);
        if (body_result.body_count > 0) stats.body_detections++;

        /* Expression */
        ebs_blendshapes_t expression;
        ebs_reset_blendshapes(&expression);
        int expr_idx = (frame / 5) % EBS_EXPR_COUNT;
        if (expr_idx == 0) expr_idx = 4;
        ebs_preset_expression(&expression, (ebs_expression_t)expr_idx,
                             0.6f + sinf(frame * 0.3f) * 0.3f);

        /* Multimodal drive */
        ad_drive_multimodal(&driver,
                            face_ok ? &face_result.faces[0] : NULL,
                            body_result.body_count > 0 ? &body_result.bodies[0] : NULL,
                            &expression,
                            audio, audio_chunk, AUDIO_RATE,
                            dt, &output_skeleton);

        stats.frames_processed++;
        stats.audio_frames_processed += audio_chunk;

        /* Track blendshape stats */
        stats.blendshape_history.jaw_open_sum +=
            ebs_get_blendshape_weight(&driver.lip_sync.current, EBS_JAW_OPEN);
        stats.blendshape_history.mouth_smile_sum +=
            ebs_get_blendshape_weight(&driver.lip_sync.current, EBS_MOUTH_SMILE_LEFT);
        stats.blendshape_history.blink_sum +=
            ebs_get_blendshape_weight(&expression, EBS_EYE_BLINK_LEFT);
        stats.blendshape_history.samples++;

        if (driver.idle.is_idle && frame > 0 &&
            !(driver.idle.is_idle && driver.idle.current_time - dt <= driver.idle.idle_threshold)) {
            stats.idle_triggers++;
        }

        /* Periodic report */
        if (frame % report_interval == 0) {
            int sec = (int)(sim_time);
            printf("--- t=%ds (frame %d) ---\n", sec, frame);

            if (face_ok && face_result.face_count > 0) {
                float ear = fl_eye_aspect_ratio(&face_result.faces[0]);
                float mar = fl_mouth_aspect_ratio(&face_result.faces[0]);
                printf("  Face: EAR=%.3f MAR=%.3f size=%.1f\n",
                       ear, mar, fl_compute_face_size(&face_result.faces[0]));
            }

            if (body_result.body_count > 0) {
                bp_pose_class_t cls = bp_classify_pose(&body_result.bodies[0]);
                printf("  Body: pose=%s\n", bp_pose_class_name(cls));
            }

            printf("  Expression: %s\n", ebs_expression_name((ebs_expression_t)expr_idx));
            printf("  Idle: %s\n", driver.idle.is_idle ? "YES" : "no");

            int head = ad_bone_index_by_name(&output_skeleton, "head");
            int lh = ad_bone_index_by_name(&output_skeleton, "left_hand");
            int rh = ad_bone_index_by_name(&output_skeleton, "right_hand");

            printf("  Skeleton key bones:\n");
            print_skeleton_bone(&output_skeleton, head);
            print_skeleton_bone(&output_skeleton, lh);
            print_skeleton_bone(&output_skeleton, rh);
            printf("\n");
        }
    }

    /* Final analysis */
    int final_sec = (int)(sim_time);
    printf("=== Analysis at t=%ds ===\n\n", final_sec);

    if (stats.frames_processed > 0) {
        printf("  Processing:\n");
        printf("    Frames: %d (%.0f FPS simulated)\n",
               stats.frames_processed,
               stats.frames_processed / (float)duration);
        printf("    Face detections: %d / %d (%.1f%%)\n",
               stats.face_detections, stats.frames_processed,
               stats.face_detections * 100.0f / stats.frames_processed);
        printf("    Body detections: %d / %d (%.1f%%)\n",
               stats.body_detections, stats.frames_processed,
               stats.body_detections * 100.0f / stats.frames_processed);
        printf("    Audio samples: %d\n", stats.audio_frames_processed);
        printf("    Idle triggers: %d\n", stats.idle_triggers);

        if (stats.blendshape_history.samples > 0) {
            float n = (float)stats.blendshape_history.samples;
            printf("\n  Avg blendshape weights:\n");
            printf("    jawOpen:      %.3f\n",
                   stats.blendshape_history.jaw_open_sum / n);
            printf("    mouthSmile:   %.3f\n",
                   stats.blendshape_history.mouth_smile_sum / n);
            printf("    eyeBlink:     %.3f\n",
                   stats.blendshape_history.blink_sum / n);
        }
    }

    /* Pose analysis on last frame */
    bp_result_t final_body;
    bp_detect(NULL, IMG_WIDTH, IMG_HEIGHT, &final_body);
    if (final_body.body_count > 0) {
        print_pose_analysis(&final_body.bodies[0]);
    }

    /* Expression detail on current state */
    printf("\n=== Current Expression Detail ===\n");
    ebs_blendshapes_t current_expr;
    ebs_reset_blendshapes(&current_expr);
    ebs_preset_expression(&current_expr, EBS_EXPR_HAPPY, 0.8f);
    print_expression_report(&current_expr);

    /* Expression blending demo */
    printf("\n=== Expression Blending Demo ===\n");
    ebs_blendshapes_t happy_bs, sad_bs, blend_result;
    ebs_reset_blendshapes(&happy_bs);
    ebs_reset_blendshapes(&sad_bs);
    ebs_preset_expression(&happy_bs, EBS_EXPR_HAPPY, 1.0f);
    ebs_preset_expression(&sad_bs, EBS_EXPR_SAD, 1.0f);

    const ebs_blendshapes_t* layers[2] = { &happy_bs, &sad_bs };
    for (float t = 0.0f; t <= 1.0f; t += 0.25f) {
        float weights[2] = { 1.0f - t, t };
        ebs_combine_blendshapes(layers, weights, 2, &blend_result);

        ebs_expression_t cls;
        float conf;
        ebs_classify_expression(&blend_result, &cls, &conf);

        printf("  happy=%.2f + sad=%.2f -> expr=%s (conf=%.2f) "
               "jawOpen=%.2f mouthSmile=%.2f\n",
               weights[0], weights[1],
               ebs_expression_name(cls), conf,
               ebs_get_blendshape_weight(&blend_result, EBS_JAW_OPEN),
               ebs_get_blendshape_weight(&blend_result, EBS_MOUTH_SMILE_LEFT));
    }

    /* IK demo */
    printf("\n=== IK Solver Demo ===\n");
    ad_skeleton_t ik_skel;
    ad_build_default_skeleton(&ik_skel);
    run_ik_demo(final_body.body_count > 0 ? &final_body.bodies[0] : NULL, &ik_skel);

    /* Skeleton smoothing demo */
    printf("\n=== Skeleton Smoothing Demo ===\n");
    ad_skeleton_t skel_a, skel_b;
    ad_build_default_skeleton(&skel_a);
    ad_build_default_skeleton(&skel_b);

    int head_idx = ad_bone_index_by_name(&skel_a, "head");
    if (head_idx >= 0) {
        skel_a.bones[head_idx].local_position.x = 0.2f;
        skel_b.bones[head_idx].local_position.x = -0.2f;
    }

    printf("  Before smoothing: head.x = %.3f\n",
           skel_a.bones[head_idx >= 0 ? head_idx : 0].local_position.x);

    ad_smooth_skeleton(&skel_b, &skel_a, 0.3f);
    printf("  After one smooth step (alpha=0.3): head.x = %.3f\n",
           skel_a.bones[head_idx >= 0 ? head_idx : 0].local_position.x);
    ad_smooth_skeleton(&skel_b, &skel_a, 0.3f);
    printf("  After two smooth steps: head.x = %.3f\n",
           skel_a.bones[head_idx >= 0 ? head_idx : 0].local_position.x);

    /* Cleanup */
    fl_reset_tracker(&face_tracker);
    free(image);
    free(audio);

    printf("\n================================================================\n");
    printf("  Full Avatar Drive Demo Complete.\n");
    printf("================================================================\n");
    return 0;
}
