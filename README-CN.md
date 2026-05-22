# Mini Media AV RTC（迷你音视频与实时通信）

**从零开始、零依赖的 C 语言实现**，涵盖音视频处理、媒体编解码、流媒体、实时通信和数字人/媒体技术。每个模块覆盖图像/音频/视频编解码器内部原理、类 FFmpeg 管线、WebRTC 和数字人渲染。

## 模块总览

| 模块 | 主题 | 参考标准 |
|--------|--------|----------------|
| [mini-image-processing](mini-image-processing/) | 像素格式（RGB/YUV）、滤波器（模糊/锐化/边缘）、变换（DCT）、JPEG 编解码仿真 | ITU-T.81, LibJPEG |
| [mini-audio-processing](mini-audio-processing/) | PCM/WAV、FFT、滤波器（FIR/IIR）、音频编解码（AAC 仿真）、重采样、混音 | FFmpeg, LibAV |
| [mini-video-processing](mini-video-processing/) | YUV/RGB 转换、H.264 仿真（NAL、Slice、宏块、DCT+量化）、运动估计 | H.264/AVC, FFmpeg |
| [mini-ffmpeg-lab](mini-ffmpeg-lab/) | 解封装器/封装器、解码器/编码器图、滤镜图、音视频同步、PTS/DTS 模型 | FFmpeg 架构 |
| [mini-media-stream](mini-media-stream/) | HLS/DASH 分段器、ABR 流媒体、CDN 源站/边缘、DRM 仿真（Widevine）、直播 | HLS RFC 8216, MPEG-DASH |
| [mini-rtc-lab](mini-rtc-lab/) | WebRTC：SDP Offer/Answer、ICE（STUN/TURN）、DTLS-SRTP、媒体轨道（音频/视频）、数据通道、Simulcast、SFU/MCU | WebRTC 1.0, libwebrtc |
| [mini-digital-human-media](mini-digital-human-media/) | 人脸关键点（MediaPipe 仿真）、人体姿态、表情 BlendShape、唇音同步、虚拟形象驱动 | MediaPipe, ARKit |

## 设计理念

- **零外部依赖** — 纯 C（C99/C11），仅使用 `libc` 和 `libm`
- **模块自包含** — 每个目录自带 `Makefile`、`include/`、`src/`、`examples/`、`demos/`、`tests/`
- **用户态媒体管线** — 对编解码器内部原理、流媒体协议和实时通信的教学级建模
- **理论到代码的映射** — 每个模块包含 `docs/` 目录，内有标准对齐说明
- **实用演示程序** — JPEG 编解码器、H.264 仿真器、FFmpeg 风格管线、WebRTC SFU 等

## 构建方式

每个模块相互独立。进入模块目录后运行：

```bash
cd mini-image-processing
make all    # 构建全部
make test   # 运行测试
```

需要 **GCC** 和 **GNU Make**。

## 项目结构

```
mini-media-av-rtc/
├── mini-image-processing/       # 图像处理
├── mini-audio-processing/       # 音频处理
├── mini-video-processing/       # 视频处理
├── mini-ffmpeg-lab/             # FFmpeg 实验室
├── mini-media-stream/           # 媒体流
├── mini-rtc-lab/                # 实时通信实验室
└── mini-digital-human-media/    # 数字人与媒体
```

## 许可证

MIT
