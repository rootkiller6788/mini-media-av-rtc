# API Reference — mini-digital-human-media

## Overview

The `mini-digital-human-media` library provides a C99 pipeline for virtual digital human animation — from face/body landmark detection through expression-driven blendshapes, lip-sync audio processing, to full avatar skeleton driving.

## Modules

### 1. face_landmark.h — Face Landmark Detection

468 3D face landmarks (MediaPipe Face Mesh style).

| Function | Description |
|---|---|
| `fl_init(tracker, smoothing)` | Initialize face tracker with EMA smoothing alpha |
| `fl_config_default(config)` | Fill config with sensible defaults |
| `fl_detect(config, tracker, image, w, h, result)` | Detect faces and extract 468 landmarks |
| `fl_get_landmarks_by_type(face, type, indices)` | Get landmark indices for a region (lips, eyes, etc.) |
| `fl_get_key_point(face, key, point)` | Get a specific key point (nose_tip, eye_center, etc.) |
| `fl_get_bounding_box(face, bbox)` | Compute axis-aligned bounding box of face |
| `fl_normalize_landmarks(face, normalized, w, h)` | Normalize landmarks relative to face center and size |
| `fl_interpolate_landmarks(from, to, t, out)` | Linear interpolation between two face meshes |
| `fl_compute_face_size(face)` | Approximate face diameter in pixels |
| `fl_landmark_to_screen(face, idx, w, h, sx, sy)` | Convert landmark to screen coordinates |
| `fl_get_lip_contour(face, outer, inner, ocnt, icnt)` | Extract outer + inner lip contours |
| `fl_eye_aspect_ratio(face)` | EAR (blink detection; threshold ~0.22) |
| `fl_mouth_aspect_ratio(face)` | MAR (mouth-open detection; threshold ~0.35) |
| `fl_smooth_landmarks(current, smoothed, alpha)` | Apply exponential moving average |
| `fl_validate_mesh(face)` | Check if mesh has enough visible points |
| `fl_copy_face_mesh(src, dst)` | Deep copy a face mesh |
| `fl_reset_tracker(tracker)` | Reset tracker state |

**Landmark types:** `FL_TYPE_LIPS` (40), `FL_TYPE_LEFT_EYE` (16), `FL_TYPE_RIGHT_EYE` (16), `FL_TYPE_LEFT_EYEBROW` (9), `FL_TYPE_RIGHT_EYEBROW` (9), `FL_TYPE_NOSE` (6), `FL_TYPE_FACE_OVAL` (36), `FL_TYPE_IRIS` (5).

**Key points:** `FL_KEY_NOSE_TIP` (1), `FL_KEY_LEFT_EYE_CENTER` (8), `FL_KEY_RIGHT_EYE_CENTER` (263), `FL_KEY_LIP_TOP` (13), `FL_KEY_LIP_BOTTOM` (14), etc.

### 2. body_pose.h — Body Pose Estimation

33 body landmarks (MediaPipe Pose style).

| Function | Description |
|---|---|
| `bp_init_filter(filter)` | Initialize moving-average filter |
| `bp_detect(image, w, h, result)` | Detect body pose (33 landmarks) |
| `bp_compute_joint_angle(body, a, b, c, angle)` | Compute angle at joint b between a-b-c |
| `bp_compute_all_angles(body, angles)` | Compute all 8 major joint angles |
| `bp_classify_pose(body)` | Classify as standing/sitting/raising_hand/etc. |
| `bp_smooth_body(current, smoothed, alpha)` | EMA smoothing of landmark positions |
| `bp_smooth_filter(filter, current, smoothed)` | Moving average filter over N frames |
| `bp_to_world_coords(body, hip_center, world)` | Convert to hip-relative 3D world coords |
| `bp_get_world_coords_relative(body, world)` | Shorthand for world coords relative to hip |
| `bp_compute_spine_angle(body)` | Spine lean angle between nose-shoulder-hip |
| `bp_compute_neck_angle(body)` | Neck angle from nose + shoulders |
| `bp_height_estimate(body, focal_px, real_m)` | Estimate body height from landmarks |
| `bp_get_hip_center(body, center)` | Midpoint of left/right hips |
| `bp_is_landmark_visible(lm, threshold)` | Visibility check |

**Pose classes:** `BP_CLASS_STANDING`, `BP_CLASS_SITTING`, `BP_CLASS_RAISING_LEFT_HAND`, `BP_CLASS_RAISING_RIGHT_HAND`, `BP_CLASS_RAISING_BOTH_HANDS`, `BP_CLASS_WALKING`, `BP_CLASS_CROUCHING`.

### 3. expression_bs.h — Expression Blendshapes

ARKit-compatible 52 blendshapes.

| Function | Description |
|---|---|
| `ebs_init_face_rig(rig)` | Initialize face rig |
| `ebs_set_blendshape_weight(bs, idx, weight)` | Set a single blendshape weight (0-1) |
| `ebs_get_blendshape_weight(bs, idx)` | Get a blendshape weight |
| `ebs_blend_blendshapes(a, b, t, out)` | Linear interpolation between two blendshape sets |
| `ebs_combine_blendshapes(layers, weights, n, out)` | Weighted sum of multiple blendshape layers |
| `ebs_classify_expression(bs, expr, conf)` | Classify expression (happy/sad/angry/surprised/fear/disgust) |
| `ebs_preset_expression(bs, expr, strength)` | Apply a preset expression at given strength |
| `ebs_expression_mix(bs, mix)` | Get expression mixture weights |
| `ebs_rig_init/add_morph_target/apply_weights` | Morph target rig system |
| `ebs_viseme_to_blendshapes(viseme, weight, bs)` | Map a viseme (phoneme shape) to blendshapes |
| `ebs_viseme_blend(a, b, t, out)` | Blend between two visemes |
| `ebs_clamp_blendshapes(bs)` | Clamp all weights to [0,1] |
| `ebs_scale_blendshapes(bs, factor)` | Scale all weights by a factor |
| `ebs_blendshape_distance(a, b)` | Euclidean distance between blendshape sets |

**Expressions:** `EBS_EXPR_NEUTRAL`, `EBS_EXPR_HAPPY`, `EBS_EXPR_SAD`, `EBS_EXPR_ANGRY`, `EBS_EXPR_SURPRISED`, `EBS_EXPR_FEAR`, `EBS_EXPR_DISGUST`.

**Visemes (16):** `sil`, `PP`, `FF`, `TH`, `DD`, `kk`, `CH`, `SS`, `nn`, `RR`, `aa`, `E`, `I`, `O`, `U`, `MB`.

### 4. lip_sync.h — Audio-Driven Lip Sync

Phoneme-to-viseme conversion and audio-driven animation.

| Function | Description |
|---|---|
| `ls_init_sync(state)` | Initialize sync state |
| `ls_text_to_phonemes(text, phonemes, count)` | Convert text to phoneme array |
| `ls_text_to_visemes(text, visemes, count)` | Convert text directly to visemes |
| `ls_phoneme_to_viseme(phoneme)` | Map a single phoneme to a viseme |
| `ls_build_viseme_sequence(visemes, n, durs, seq)` | Build timed viseme sequence |
| `ls_get_viseme_at_time(seq, time, viseme, weight)` | Sample viseme at specific time |
| `ls_interpolate_visemes(seq, time, out)` | Interpolate visemes at time (with transitions) |
| `ls_audio_features_to_viseme(audio, len, viseme, weight)` | Detect viseme from raw audio features |
| `ls_extract_formants(audio, len, rate, formants)` | Extract formant frequencies (simulated) |
| `ls_sync_update(state, viseme, weight, dt, out)` | Real-time sync step (smooth transition) |
| `ls_audio_drive(audio, len, rate, seq, sync, bs)` | Full audio-driven pipeline |
| `ls_coarticulation_apply(visemes, count, seq)` | Apply co-articulation to sequence |
| `ls_smooth_transition(state, viseme, w, out)` | Smooth articulation transition |
| `ls_compute_audio_energy(audio, len)` | Compute RMS energy |
| `ls_detect_voice_activity(audio, len, thresh)` | Voice activity detection |
| `ls_estimate_pitch(audio, len, rate, pitch, conf)` | Autocorrelation pitch estimation |

### 5. avatar_drive.h — Avatar Driving

Combined face+body+audio skeleton animation.

| Function | Description |
|---|---|
| `ad_init_driver(driver)` | Initialize full avatar driver |
| `ad_build_default_skeleton(skeleton)` | Build a 22-bone humanoid skeleton |
| `ad_ik_fabrik(chain, target, tol, iter)` | FABRIK inverse kinematics solver |
| `ad_ik_ccd(chain, target, max_iter)` | CCD inverse kinematics solver |
| `ad_fk_solve(skeleton, pos, rot, count)` | Forward kinematics (compute world transforms) |
| `ad_retarget_init(config)` | Initialize retargeting config with default body maps |
| `ad_retarget_apply(config, body, skeleton)` | Apply body landmark positions to skeleton bones |
| `ad_retarget_face(face, expression, skeleton)` | Apply face data to skeleton |
| `ad_drive_face_body(driver, face, body, expr, skel)` | Drive skeleton from face+body+expression |
| `ad_drive_audio(driver, audio, len, rate, skel)` | Drive skeleton from audio (lip sync) |
| `ad_drive_multimodal(driver, ...)` | Full multi-modal driving pipeline |
| `ad_blend_skeletons(a, b, t, out)` | Blend two skeletons (SLERP rotations) |
| `ad_smooth_skeleton(curr, smoothed, alpha)` | EMA smoothing over skeleton |
| `ad_idle_init/update/get_pose` | Idle animation system |
| `ad_landmark_to_bone_rotation(lm, parent, rot)` | Convert landmark direction to quaternion |
| `ad_bone_index_by_name(skeleton, name)` | Look up bone index by name |

**Math utilities:** `ad_vec3_add/sub/scale/length/dot/cross/normalize`, `ad_quat_identity/from_axis_angle/multiply/slerp`.

---

## Data Types

| Type | Description |
|---|---|
| `fl_vec3_t` | 3D vector (float) |
| `fl_face_mesh_t` | 468 3D landmarks + visibility flags |
| `fl_result_t` | Detection result (up to 4 faces) |
| `bp_landmark_t` | Body landmark (x, y, z, visibility, presence) |
| `bp_body_t` | 33 body landmarks |
| `bp_joint_angle_t` | Joint angle + axis + confidence |
| `ebs_blendshapes_t` | 52 blendshape weights |
| `ebs_face_rig_t` | Face rig with expression classification |
| `ls_viseme_sequence_t` | Timed viseme animation sequence |
| `ls_sync_state_t` | Real-time lip sync state |
| `ad_skeleton_t` | Hierarchical bone skeleton (max 128 bones) |
| `ad_ik_chain_t` | IK chain (joint positions, lengths) |
| `ad_retarget_config_t` | Landmark-to-bone mapping |
| `ad_driver_t` | Complete avatar driver state |
| `ad_quat_t` | Quaternion (x, y, z, w) |
| `ad_transform_t` | Translation + rotation + scale |

## Usage Pattern

```c
// 1. Init
ad_driver_t driver;
ad_init_driver(&driver);

// 2. Per-frame loop
fl_result_t face;
bp_result_t body;
// ... fill face, body from camera ...
ebs_blendshapes_t expr;
ebs_preset_expression(&expr, EBS_EXPR_HAPPY, 0.7f);

ad_skeleton_t output;
ad_drive_multimodal(&driver, &face.faces[0], &body.bodies[0],
                    &expr, audio, audio_len, sample_rate, dt, &output);

// 3. Render output skeleton
```
