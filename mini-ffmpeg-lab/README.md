# mini-ffmpeg-lab — FFmpeg实验室 (C 语言实现)

轻量级多媒体处理框架，纯 C99 实现，模拟 FFmpeg 核心子系统的设计思想。

## 架构概览

```
┌──────────────────────────────────────────────────────┐
│                    Application                       │
├──────────────────────────────────────────────────────┤
│  demuxer.h    decoder_engine.h   encoder_engine.h    │
│  filter_graph.h          av_sync.h                   │
├──────────────────────────────────────────────────────┤
│  mp4/mkv/ts parser │ h264/aac codec │ scale/crop ... │
└──────────────────────────────────────────────────────┘
```

### 核心模块

| 模块 | 文件 | 功能 |
|------|------|------|
| **解封装** | `demuxer.h/c` | 格式探测、容器解析 (MP4/MKV/TS)、PTS/DTS 管理、Seek |
| **解码器** | `decoder_engine.h/c` | send_packet→receive_frame 模型, H.264/AAC 模拟解码, codec 注册表 |
| **编码器** | `encoder_engine.h/c` | 编码帧→输出包, I/P/B 帧类型, 码率控制 (CBR/VBR/CRF), presets |
| **滤镜图** | `filter_graph.h/c` | buffer source→filter chain→buffer sink, scale/crop/overlay/volume 等 |
| **音视频同步** | `av_sync.h/c` | 音频主时钟/视频主时钟, 丢帧/重复帧策略, 时间基转换, 唇音同步检测 |

### 补充文件

| 文件 | 说明 |
|------|------|
| `demo_player.c` | 完整播放器模拟 (>250 行) |
| `demo_transcoder.c` | 转码管线模拟 (>250 行) |
| `example_demux.c` | 解封装示例 |
| `example_decode.c` | 解码示例 |
| `example_transcode.c` | 转码示例 |
| `API.md` | API 参考文档 |
| `INTERNALS.md` | 内部实现文档 |
| `Makefile` | GNU Make 构建 |

## 快速开始

### 环境要求

- C99 编译器 (GCC / Clang / MSVC)
- GNU Make (或手动编译)

### 构建

```bash
make all
```

### 运行示例

```bash
make examples
./example_demux
./example_decode
./example_transcode

make demos
./demo_player
./demo_transcoder
```

### 运行测试

```bash
make test
```

### 清理

```bash
make clean
```

## 许可

仅供学习与研究使用。
