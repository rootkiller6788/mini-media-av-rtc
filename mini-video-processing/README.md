# mini-video-processing — 视频处理 (C 语言实现)

纯 C99 实现的视频处理核心模块，涵盖 YUV/RGB 色彩空间转换、H.264 NAL 单元解析、宏块编码、运动估计、码率控制等基础视频编解码算法。

## 模块结构

| 模块 | 文件 | 说明 |
|------|------|------|
| YUV/RGB 转换 | `include/yuv_rgb_conv.h`, `src/yuv_rgb_conv.c` | YUV420p 平面提取、BT.601/709 色彩矩阵、色度上采样 |
| H.264 NAL 解析 | `include/h264_nal.h`, `src/h264_nal.c` | NAL 单元结构、SPS/PPS 解析、片头解析 |
| 宏块编码 | `include/macroblock.h`, `src/macroblock.c` | 帧内/帧间预测、4×4 DCT、量化、Zigzag、CAVLC |
| 运动估计 | `include/motion_est.h`, `src/motion_est.c` | 块匹配搜索、亚像素精化、MV 预测 |
| 码率控制 | `include/rate_control.h`, `src/rate_control.c` | CBR/VBR、GOP 结构、HRD 缓冲模拟 |

## 构建

```bash
make
```

## 示例

```bash
# YUV ↔ RGB 转换示例
./build/01_yuv_convert

# NAL 单元解析示例
./build/02_nal_parse

# 帧编码示例
./build/03_encode_frame
```

## 演示

```bash
# YUV 文件转 RGB 位图
./build/yuv_to_rgb input.yuv 1920 1080 output.rgb

# H.264 编码器演示
./build/h264_encoder input.rgb 1920 1080 30 output.264
```

## 文档

- `docs/yuv_rgb_guide.md` — YUV/RGB 色彩空间转换指南
- `docs/nal_format.md` — H.264 NAL 单元格式说明

## 要求

- C99 编译器 (GCC / Clang / MSVC)
- 无外部依赖
