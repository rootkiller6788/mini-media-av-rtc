/* example_avatar_drive.c — Virtual avatar driving example (face+body+audio) */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "avatar_drive.h"

#define WIDTH  640
#define HEIGHT 480

static void print_skeleton(const ad_skeleton_t* skel, const char* label)
{
    printf("  %s skeleton (%d bones):\n", label, skel->bone_count);
    for (int i = 0; i < skel->bone_count; i++) {
        const ad_bone_t* b = &skel->bones[i];
        if (i >= 22) { printf("    ... +%d more\n", skel->bone_count - 22); break; }
        printf("    [%2d] %-18s parent=%2d  pos=(%+.3f,%+.3f,%+.3f)\n",
               i, b->name, b->parent_index,
               b->current_pose.translation.x,
               b->current_pose.translation.y,
               b->current_pose.translation.z);
    }
}

static void generate_test_audio(float* audio, int len, float rate)
{
    float phase = 0.0f;
    for (int i = 0; i < len; i++) {
        phase += 2.0f * 3.14159265f * 220.0f / rate;
        audio[i] = sinf(phase) * 0.3f;
    }
}

int main(void)
{
    printf("=== Mini Digital Human: Avatar Drive Example ===\n\n");

    /* Part 1: Build default skeleton */
    printf("--- Part 1: Skeleton Setup ---\n");
    ad_skeleton_t skeleton;
    ad_build_default_skeleton(&skeleton);
    printf("  Default skeleton: %d bones\n", skeleton.bone_count);

    const char* bone_names[] = {
        "head", "left_hand", "right_hand", "left_foot", "right_foot", "spine", "root"
    };
    for (int i = 0; i < 7; i++) {
        int idx = ad_bone_index_by_name(&skeleton, bone_names[i]);
        printf("  %s = index %d\n", bone_names[i], idx);
    }
    print_skeleton(&skeleton, "Default");
    printf("\n");

    /* Part 2: Vector & Quaternion math */
    printf("--- Part 2: Math Utilities ---\n");
    ad_vec3_t v1 = {1, 0, 0}, v2 = {0, 1, 0}, cross;
    ad_vec3_cross(&cross, &v1, &v2);
    printf("  cross((1,0,0), (0,1,0)) = (%.1f, %.1f, %.1f)\n", cross.x, cross.y, cross.z);

    float dot = ad_vec3_dot(&v1, &v2);
    printf("  dot((1,0,0), (0,1,0)) = %.1f\n", dot);

    ad_vec3_t v_scale;
    ad_vec3_scale(&v_scale, &v1, 3.0f);
    printf("  scale((1,0,0), 3) = (%.1f, %.1f, %.1f)\n", v_scale.x, v_scale.y, v_scale.z);

    ad_quat_t q1, q2, q_lerp;
    ad_quat_identity(&q1);
    ad_vec3_t axis = {0, 0, 1};
    ad_quat_from_axis_angle(&q2, &axis, 3.14159265f * 0.5f);
    ad_quat_slerp(&q_lerp, &q1, &q2, 0.5f);
    printf("  slerp(id, rot90deg(z), 0.5) = (%.3f, %.3f, %.3f, %.3f)\n\n",
           q_lerp.x, q_lerp.y, q_lerp.z, q_lerp.w);

    /* Part 3: IK Solver */
    printf("--- Part 3: Inverse Kinematics ---\n");
    ad_ik_solver_t ik;
    memset(&ik, 0, sizeof(ik));
    ik.chain.joint_count = 3;
    ik.chain.joint_parents[0] = -1;
    ik.chain.joint_parents[1] = 0;
    ik.chain.joint_parents[2] = 1;
    ik.chain.joint_positions[0].x = 0;
    ik.chain.joint_positions[0].y = 0;
    ik.chain.joint_positions[0].z = 0;
    ik.chain.joint_positions[1].x = 0;
    ik.chain.joint_positions[1].y = 1;
    ik.chain.joint_positions[1].z = 0;
    ik.chain.joint_positions[2].x = 0;
    ik.chain.joint_positions[2].y = 2;
    ik.chain.joint_positions[2].z = 0;
    ik.chain.joint_lengths[0] = 1.0f;
    ik.chain.joint_lengths[1] = 1.0f;
    ik.chain.solver_iterations = 20;
    ik.chain.solver_tolerance = 0.001f;
    ik.is_active = 1;

    ad_vec3_t ik_target = {0.5f, 0.3f, 0.2f};
    ik.target = ik_target;

    printf("  IK chain: %d joints, target=(%.2f, %.2f, %.2f)\n",
           ik.chain.joint_count, ik_target.x, ik_target.y, ik_target.z);
    printf("  Before IK: end_effector=(%.3f, %.3f, %.3f)\n",
           ik.chain.joint_positions[2].x,
           ik.chain.joint_positions[2].y,
           ik.chain.joint_positions[2].z);

    ad_ik_fabrik(&ik.chain, &ik_target, 0.001f, 50);

    printf("  After IK:  end_effector=(%.3f, %.3f, %.3f)\n",
           ik.chain.joint_positions[2].x,
           ik.chain.joint_positions[2].y,
           ik.chain.joint_positions[2].z);

    float dx = ik.chain.joint_positions[2].x - ik_target.x;
    float dy = ik.chain.joint_positions[2].y - ik_target.y;
    float dz = ik.chain.joint_positions[2].z - ik_target.z;
    printf("  Error: (%.4f, %.4f, %.4f) dist=%.4f\n\n", dx, dy, dz,
           sqrtf(dx*dx + dy*dy + dz*dz));

    /* Part 4: Retargeting */
    printf("--- Part 4: Motion Retargeting ---\n");
    ad_retarget_config_t retarget;
    ad_retarget_init(&retarget);
    printf("  Retarget config: %d maps, smoothing=%.2f\n",
           retarget.map_count, retarget.smoothing);

    for (int i = 0; i < retarget.map_count && i < 6; i++) {
        printf("    [%d] landmark[%d] -> bone[%d] (infl=%.1f)\n",
               i, retarget.maps[i].landmark_index,
               retarget.maps[i].bone_index,
               retarget.maps[i].influence);
    }
    printf("\n");

    /* Part 5: Body pose to skeleton retargeting */
    printf("--- Part 5: Body Pose -> Skeleton ---\n");

    bp_result_t pose_result;
    bp_body_t body;
    memset(&body, 0, sizeof(body));

    bp_detect(NULL, WIDTH, HEIGHT, &pose_result);
    if (pose_result.body_count > 0) {
        body = pose_result.bodies[0];
        printf("  Detected body, %d/33 landmarks visible\n", BP_LANDMARK_COUNT);

        ad_skeleton_t retargeted_skel;
        ad_build_default_skeleton(&retargeted_skel);

        ad_retarget_apply(&retarget, &body, &retargeted_skel);
        printf("  After retargeting:\n");

        int head_idx = ad_bone_index_by_name(&retargeted_skel, "head");
        int lhand_idx = ad_bone_index_by_name(&retargeted_skel, "left_hand");
        int rhand_idx = ad_bone_index_by_name(&retargeted_skel, "right_hand");
        int lfoot_idx = ad_bone_index_by_name(&retargeted_skel, "left_foot");

        if (head_idx >= 0) printf("    head:     (%.2f, %.2f, %.2f)\n",
            retargeted_skel.bones[head_idx].current_pose.translation.x,
            retargeted_skel.bones[head_idx].current_pose.translation.y,
            retargeted_skel.bones[head_idx].current_pose.translation.z);
        if (lhand_idx >= 0) printf("    left_hand: (%.2f, %.2f, %.2f)\n",
            retargeted_skel.bones[lhand_idx].current_pose.translation.x,
            retargeted_skel.bones[lhand_idx].current_pose.translation.y,
            retargeted_skel.bones[lhand_idx].current_pose.translation.z);
        if (rhand_idx >= 0) printf("    right_hand:(%.2f, %.2f, %.2f)\n",
            retargeted_skel.bones[rhand_idx].current_pose.translation.x,
            retargeted_skel.bones[rhand_idx].current_pose.translation.y,
            retargeted_skel.bones[rhand_idx].current_pose.translation.z);
        if (lfoot_idx >= 0) printf("    left_foot: (%.2f, %.2f, %.2f)\n",
            retargeted_skel.bones[lfoot_idx].current_pose.translation.x,
            retargeted_skel.bones[lfoot_idx].current_pose.translation.y,
            retargeted_skel.bones[lfoot_idx].current_pose.translation.z);
    }

    bp_pose_class_t pose_class = bp_classify_pose(&body);
    printf("  Pose classification: %s\n\n", bp_pose_class_name(pose_class));

    /* Part 6: Full avatar driver */
    printf("--- Part 6: Full Avatar Driver (Multimodal) ---\n");

    ad_driver_t driver;
    ad_init_driver(&driver);
    printf("  Driver initialized\n");

    fl_tracker_t face_tracker;
    fl_config_t face_config;
    fl_init(&face_tracker, 0.4f);
    fl_config_default(&face_config);

    float* image = (float*)malloc(WIDTH * HEIGHT * sizeof(float));
    for (int i = 0; i < WIDTH * HEIGHT; i++) image[i] = 0.5f;

    fl_result_t face_result;
    fl_detect(&face_config, &face_tracker, image, WIDTH, HEIGHT, &face_result);

    bp_result_t body_result;
    bp_detect(NULL, WIDTH, HEIGHT, &body_result);

    ebs_blendshapes_t expression;
    ebs_reset_blendshapes(&expression);
    ebs_preset_expression(&expression, EBS_EXPR_HAPPY, 0.7f);

    int audio_len = 16000;
    float* audio = (float*)malloc(audio_len * sizeof(float));
    generate_test_audio(audio, audio_len, 16000.0f);

    ad_skeleton_t out_skeleton;
    ad_build_default_skeleton(&out_skeleton);

    printf("  Inputs: face=%d, body=%d, expression=%s, audio=%d samples\n",
           face_result.face_count, body_result.body_count,
           ebs_expression_name(EBS_EXPR_HAPPY), audio_len);

    for (int frame = 0; frame < 3; frame++) {
        fl_detect(&face_config, &face_tracker, image, WIDTH, HEIGHT, &face_result);
        bp_detect(NULL, WIDTH, HEIGHT, &body_result);

        ebs_blendshapes_t expr;
        ebs_reset_blendshapes(&expr);
        ebs_preset_expression(&expr, EBS_EXPR_HAPPY, 0.7f + frame * 0.1f);

        ad_drive_multimodal(&driver,
                            face_result.face_count > 0 ? &face_result.faces[0] : NULL,
                            body_result.body_count > 0 ? &body_result.bodies[0] : NULL,
                            &expr, audio, 8000, 16000.0f, 0.033f, &out_skeleton);

        int head_idx = ad_bone_index_by_name(&out_skeleton, "head");
        int lhand_idx = ad_bone_index_by_name(&out_skeleton, "left_hand");

        printf("  Frame %d: ", frame);
        if (head_idx >= 0) printf("head=(%.3f,%.3f) ",
            out_skeleton.bones[head_idx].current_pose.translation.x,
            out_skeleton.bones[head_idx].current_pose.translation.y);
        if (lhand_idx >= 0) printf("lhand=(%.3f,%.3f)",
            out_skeleton.bones[lhand_idx].current_pose.translation.x,
            out_skeleton.bones[lhand_idx].current_pose.translation.y);
        printf("  idle=%d\n", driver.idle.is_idle);
    }

    /* Part 7: Skeleton blending */
    printf("\n--- Part 7: Skeleton Blending ---\n");

    ad_skeleton_t skelA, skelB, skelBlend;
    ad_build_default_skeleton(&skelA);
    ad_build_default_skeleton(&skelB);
    ad_build_default_skeleton(&skelBlend);

    int headA = ad_bone_index_by_name(&skelA, "head");
    if (headA >= 0) {
        skelA.bones[headA].local_position.x = 0.1f;
        skelB.bones[headA].local_position.x = -0.1f;
    }

    ad_blend_skeletons(&skelA, &skelB, 0.5f, &skelBlend);
    if (headA >= 0) {
        printf("  Blend A(head.x=%.2f) + B(head.x=%.2f) at t=0.5 -> head.x=%.2f\n",
               skelA.bones[headA].local_position.x,
               skelB.bones[headA].local_position.x,
               skelBlend.bones[headA].local_position.x);
    }

    /* Part 8: Idle animation */
    printf("\n--- Part 8: Idle Animation ---\n");

    ad_idle_state_t idle;
    ad_idle_init(&idle);
    printf("  Idle initialized: threshold=%.1fs, timeout=%.1fs\n",
           idle.idle_threshold, idle.idle_timeout);

    for (int f = 0; f < 8; f++) {
        int has_input = (f < 4);
        ad_idle_update(&idle, 0.5f, has_input);
        printf("  t=%.1f  input=%d  idle=%d  last_input=%.1f\n",
               idle.current_time, has_input, idle.is_idle, idle.last_input_time);
    }

    /* Cleanup */
    free(image);
    free(audio);
    fl_reset_tracker(&face_tracker);

    printf("\nDone.\n");
    return 0;
}
