# mini-digital-human-media — 数字人与媒体 (C 语言实现)

C99 library for virtual digital human animation: face landmark detection, body pose estimation, expression blendshapes, audio-driven lip sync, and full avatar skeleton driving.

## Modules

| Module | Header | Source | Description |
|---|---|---|---|
| Face Landmark | `face_landmark.h` | `face_landmark.c` | 468 3D face landmarks (MediaPipe Face Mesh style) |
| Body Pose | `body_pose.h` | `body_pose.c` | 33 body landmarks (MediaPipe Pose style) |
| Expression BS | `expression_bs.h` | `expression_bs.c` | ARKit 52 blendshapes, expression classification |
| Lip Sync | `lip_sync.h` | `lip_sync.c` | Audio→viseme mapping, phoneme detection |
| Avatar Drive | `avatar_drive.h` | `avatar_drive.c` | IK/FK skeleton, retargeting, multi-modal drive |

## Quick Start

```bash
make
./demo_face_landmark
./demo_avatar_full
```

## Examples

```bash
make examples
./example_face_detect
./example_lip_sync
./example_avatar_drive
```

## API Summary

```c
// Face: detect 468 3D face landmarks
fl_tracker_t tracker;
fl_config_t config;
fl_init(&tracker, 0.4f);
fl_config_default(&config);
fl_result_t result;
fl_detect(&config, &tracker, image, w, h, &result);
float ear = fl_eye_aspect_ratio(&result.faces[0]);  // blink detection

// Body: detect 33 body landmarks
bp_result_t body;
bp_detect(NULL, w, h, &body);
bp_pose_class_t cls = bp_classify_pose(&body.bodies[0]);

// Expression: ARKit 52 blendshapes
ebs_blendshapes_t bs;
ebs_preset_expression(&bs, EBS_EXPR_HAPPY, 0.7f);
ebs_expression_t expr;
float conf;
ebs_classify_expression(&bs, &expr, &conf);

// Lip Sync: audio → viseme → blendshapes
ls_sync_state_t sync;
ls_init_sync(&sync);
ebs_blendshapes_t lip_bs;
ls_sync_update(&sync, EBS_VISEME_AA, 1.0f, 0.016f, &lip_bs);

// Avatar: multi-modal skeleton drive
ad_driver_t driver;
ad_init_driver(&driver);
ad_skeleton_t skel;
ad_drive_multimodal(&driver, face, body, &bs, audio, len, rate, dt, &skel);
```

## Building

Requirements: C99 compiler (gcc/clang/msvc).

```bash
make          # build demos + examples
make clean    # remove built files
```

## File List

| File | Lines | Description |
|---|---|---|
| `face_landmark.h` | ~120 | Face landmark API |
| `face_landmark.c` | ~300 | Face landmark simulation |
| `body_pose.h` | ~130 | Body pose API |
| `body_pose.c` | ~280 | Body pose simulation |
| `expression_bs.h` | ~140 | Expression blendshape API |
| `expression_bs.c` | ~280 | Expression + viseme logic |
| `lip_sync.h` | ~120 | Lip sync API |
| `lip_sync.c` | ~330 | Phoneme/viseme/audio pipeline |
| `avatar_drive.h` | ~190 | Avatar driving API |
| `avatar_drive.c` | ~430 | IK/FK/retargeting/driving |
| `example_face_detect.c` | ~130 | Face landmark example |
| `example_lip_sync.c` | ~230 | Lip sync example |
| `example_avatar_drive.c` | ~240 | Avatar drive example |
| `demo_face_landmark.c` | ~280 | Interactive face demo |
| `demo_avatar_full.c` | ~340 | Full pipeline demo |
| `API_REFERENCE.md` | ~250 | API documentation |
| `INTERNALS.md` | ~200 | Architecture & internals |
| `README.md` | ~100 | This file |
| `Makefile` | ~60 | Build system |

## License

MIT
