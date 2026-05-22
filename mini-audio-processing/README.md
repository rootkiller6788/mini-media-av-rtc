# mini-audio-processing — 音频处理 (C 语言实现)

一个纯 C99 音频处理库，涵盖 PCM/WAV I/O、FFT 频谱分析、FIR/IIR 滤波器、AAC 编解码仿真以及采样率转换与多轨混音。

## 目录结构

```
mini-audio-processing/
├── pcm_wav.h / .c       — PCM 采样格式 · WAV 文件读写 · 音频缓冲混音
├── fft_core.h / .c      — 蝶形 FFT / IFFT · 窗函数 · STFT 语谱图
├── audio_filter.h / .c  — FIR/IIR 滤波器 · 卷积混响 · 回声延迟 · 限幅器
├── audio_codec.h / .c   — MDCT · Bark 心理声学模型 · AAC 量化仿真
├── resample_mix.h / .c  — 线性/Sinc 重采样 · 声道映射 · 多轨混音 · 缓冲池
├── example_wav_play.c   — 示例: 正弦波生成与 WAV 读写
├── example_spectrum.c   — 示例: FFT 频谱分析器
├── example_filter.c     — 示例: 滤波器套件测试
├── demo_noise_reduction.c — 演示: 谱减法降噪
├── demo_audio_mixer.c   — 演示: 8 轨立体声混音器
├── API.md               — API 参考文档
├── DESIGN.md            — 设计文档
├── README.md            — 本文件
└── Makefile             — 构建脚本
```

## 编译运行

```bash
make all          # 编译源码、示例和演示
make examples     # 仅编译示例
make demos        # 仅编译演示
make clean        # 清理构建产物

# 手动编译示例:
gcc -std=c99 -Wall -O2 -o example_wav_play example_wav_play.c pcm_wav.c -lm

# 运行:
./example_wav_play
./example_spectrum
./example_filter
./demo_noise_reduction
./demo_audio_mixer
```

## 模块概览

| 模块 | 功能 |
|---|---|
| **pcm_wav** | int16/float32 格式互转, WAV RIFF 读写, AudioBuffer 管理 |
| **fft_core** | Radix-2 Cooley-Tukey FFT/IFFT, Hann/Hamming/Blackman 窗, STFT |
| **audio_filter** | FIR 窗函数法设计, 7 种 Biquad 滤波器, 卷积/回声效果器 |
| **audio_codec** | 教育用 AAC: MDCT, Bark 尺度, 掩蔽阈值, ADTS 头 |
| **resample_mix** | 线性/Lanczos 重采样, 声道映射, 多轨混音器, 缓冲池 |

## 依赖

仅依赖 C99 标准库，无需任何第三方库。

## 许可

Educational use. No warranty.
