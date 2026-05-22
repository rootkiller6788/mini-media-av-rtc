# Internals — mini-digital-human-media

## Architecture Overview

```
                        +------------+
Camera Frame ───────►   │ Face       │────► 468 Landmarks
                        │ Detection  │
                        +------------+
                              │
                        +------------+
                        │ Expression │────► 52 Blendshapes
                        │ Classifier │
                        +------------+
                              │
                        +------------+     +----------+
Audio Stream ─────────►│ Lip Sync   │────►│ Viseme   │
                        │ Engine     │     │ Sequence │
                        +------------+     +----------+
                              │                  │
                        +------------+           │
                        │ Body Pose  │────► 33 Landmarks
                        │ Estimator  │
                        +------------+
                              │
                        +------------+
                        │ Avatar     │────► Skeleton (22 bones)
                        │ Driver     │       + FK transforms
                        │ (IK+FK)    │       + Blendshape targets
                        +------------+
                              │
                              ▼
                        Render Engine
```

## Module Details

### face_landmark.c — Simulated Detection

Since this is a simulation library without a deep learning backend, `fl_detect()` generates a synthetic oval face mesh. The 468 landmarks are arranged in an ellipsoid pattern around a center point derived from the image dimensions. Each landmark has a sinusoidal offset to simulate 3D depth.

- **Smoothing:** Exponential moving average (EMA) with configurable alpha, operating on all 468 x/y/z coordinates.
- **EAR:** (|p2-p6| + |p3-p5|) / (2 * |p1-p4|) — using landmarks 159,145,33,133,144,163 for left eye and 386,374,362,263,373,380 for right eye.
- **MAR:** vertical distance (13→14) / horizontal distance (61→291).
- **Normalization:** Center-subtract then divide by bounding box diagonal.

### body_pose.c — Simulated Pose

`bp_detect()` generates a synthetic standing pose with fixed proportions scaled to image dimensions. The detection is stateless; the smoothing filter maintains a circular buffer of `BP_FILTER_WINDOW=5` frames.

- **Joint angles:** Computed via dot-product angle between three consecutive landmarks.
- **Pose classification:** Rule-based — compares wrist/shoulder/hip/knee y-coordinates to determine if hands are raised, if the body is sitting, walking (asymmetric knees), or crouching.
- **World coordinates:** All positions are relative to hip center (midpoint of left and right hips). Z is negated for right-handed coordinate system.

### expression_bs.c — 52 Blendshapes

Full ARKit 52 blendshape set with preset tables for 7 expressions. Each expression preset defines 3-8 key blendshapes and their target values (at full strength).

- **Classification:** Score-based — each expression has a weighted sum of its characteristic blendshapes. The highest scorer wins, with a minimum threshold to fall back to NEUTRAL.
- **Viseme mapping:** Each of the 16 visemes maps to 2-5 blendshape targets with specific weights. See the switch statement in `ebs_viseme_to_blendshapes()`.
- **Morph target rig:** Stores reference blendshape sets (morph targets). `ebs_rig_apply_weights()` computes a weighted blend of multiple targets.

### lip_sync.c — Audio-to-Viseme Pipeline

- **Phoneme mapping:** 39 ARPABET-style phonemes map to 16 viseme categories via a lookup table.
- **Text-to-viseme:** Character-by-character conversion using `ls_char_to_phoneme()` then `ls_phoneme_to_viseme()`.
- **Viseme sequence:** Timed array of viseme frames with transition-in/transition-out windows. `ls_interpolate_visemes()` blends overlapping frames using linear weights.
- **Audio features:** `ls_audio_features_to_viseme()` computes energy, zero-crossing rate, and spectral centroid from a single audio frame. The spectral centroid is used as a rough formant proxy: low frequencies → AA/O, mid → E, high → I, high ZCR → SS (fricatives).
- **Co-articulation:** `ls_coarticulation_apply()` extends transition windows by 2x and adds a 15ms overlap between adjacent visemes.
- **Real-time sync:** `ls_sync_update()` smoothly interpolates current blendshape state toward target with configurable transition speed.
- **Pitch estimation:** Simple autocorrelation method over period candidates [40..400] Hz. Uses normalized dot-product as confidence.

### avatar_drive.c — Skeleton Animation

- **Default skeleton:** 22 bones organized as: root → spine chain (4) → neck → head, plus left/right shoulder → arm → forearm → hand chains and left/right hip → upper leg → lower leg → foot chains. Parent indices follow a standard humanoid rig.
- **FABRIK IK:** Forward-and-backward reaching. Forward pass: set endpoint to target, propagate backwards maintaining bone lengths. Backward pass: anchor root, propagate forward. Iterates until convergence or max iterations. Suitable for limb chains.
- **CCD IK:** Cyclic Coordinate Descent — iteratively rotates each joint (from end to root) to point the end effector toward the target. Uses quaternion rotation from axis-angle formula.
- **FK:** Recursive world-space transform computation. Each bone's world rotation = parent world rotation * local rotation. Translation is rotated by parent quaternion then added to parent position.
- **Retargeting:** 13 default landmark-to-bone maps (shoulders, elbows, wrists, hips, knees, ankles, nose→head). `ad_retarget_apply()` applies EMA smoothing with per-axis masking.
- **Multimodal fusion:** `ad_drive_multimodal()` runs three parallel skeleton drives (body retarget, face/audio lip-sync, idle animation) and blends them: body+face at 50/50 blend, then optionally blends with idle pose.
- **Idle animation:** Timer-based system. When no input is received for `idle_threshold` seconds, switches to idle. Applies subtle sinusoidal sway to the spine bone.
- **Skeleton blend:** Linear interpolation of positions + SLERP of quaternions, bone by bone.
- **Math:** Vector operations (add, sub, scale, dot, cross, normalize, length). Quaternion operations (identity, from axis-angle, multiply, SLERP). Uses standard quaternion rotation formula for vector rotation.

## Memory Layout

| Structure | Size (bytes, approx) |
|---|---|
| `fl_face_mesh_t` | ~468 * (12+4+4) ≈ 9.4 KB |
| `fl_result_t` | ~4 * 9.4 KB + overhead ≈ 38 KB |
| `bp_body_t` | ~33 * (12+4+4) + 33*12 ≈ 1 KB |
| `ebs_blendshapes_t` | 52 * 4 = 208 B |
| `ls_viseme_sequence_t` | 1024 * ~20 ≈ 20 KB |
| `ad_skeleton_t` | 128 * ~120 ≈ 15 KB |
| `ad_driver_t` | ~20 KB total |

## Limitations (Simulated Backend)

- Face detection generates synthetic landmarks; no real camera/ML integration.
- Body pose is synthetic; classification is rule-based, not ML.
- Audio formant extraction is stubbed; uses simple spectral features instead of LPC.
- IK solves are limited to single chains; no full-body IK.
- No time-series modeling (RNN/LSTM) for expression dynamics or lip co-articulation.

These simulation stubs are designed to be replaced with real ML model backends while keeping the same API.
