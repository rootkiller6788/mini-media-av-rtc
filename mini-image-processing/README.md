# mini-image-processing — 图像处理 (C 语言实现)

**mini_image** 是一个零依赖、纯 C99 的图像处理教学库,涵盖像素格式转换、图像滤波、几何变换、JPEG 编解码骨架与基础图像文件 I/O。

---

## 目录

- [快速开始](#快速开始)
- [模块概览](#模块概览)
- [API 示例](#api-示例)
- [支持格式](#支持格式)
- [滤波器列表](#滤波器列表)
- [JPEG 数据路径](#jpeg-数据路径)
- [文件 I/O](#文件-io)
- [构建](#构建)
- [示例程序](#示例程序)
- [编码约定](#编码约定)
- [依赖](#依赖)
- [目录结构](#目录结构)
- [许可证](#许可证)

---

## 快速开始

```sh
git clone <repo-url>
cd mini-image-processing
make          # 编译库 + 所有示例
make run_examples  # 运行示例
```

---

## 模块概览

| 模块           | 头文件              | 功能                                       |
|----------------|---------------------|--------------------------------------------|
| **像素格式**   | `pixel_format.h`    | 6 种像素格式、RGB↔YUV 转换、Image 生命周期 |
| **图像滤波**   | `image_filter.h`    | 模糊、锐化、边缘检测、亮度/对比度、直方图  |
| **几何变换**   | `transforms.h`      | DCT/IDCT、缩放、旋转、翻转、裁剪、填充     |
| **JPEG 编解码**| `jpeg_codec.h`      | YCbCr 色彩变换、量化、Zigzag、Huffman、解析 |
| **图像 I/O**   | `image_io.h`        | BMP、PPM/PGM、PNG 骨架、原始像素缓冲区     |

---

## API 示例

### 创建图像

```c
#include "mini_image/pixel_format.h"

Image img = image_create(640, 480, PIXEL_FORMAT_RGB24);
// ... process ...
image_destroy(&img);
```

### 高斯模糊

```c
#include "mini_image/image_filter.h"

Image src = image_create(800, 600, PIXEL_FORMAT_RGB24);
Image dst = image_create(800, 600, PIXEL_FORMAT_RGB24);
gaussian_blur_apply(&src, &dst, 3, 1.4f);
```

### 格式转换

```c
Image rgb  = image_create(1920, 1080, PIXEL_FORMAT_RGB24);
Image yuv  = image_create(1920, 1080, PIXEL_FORMAT_YUV420P);
rgb24_to_yuv420p(&rgb, &yuv);
```

### 旋转

```c
#include "mini_image/transforms.h"

Image src  = image_create(640, 480, PIXEL_FORMAT_RGB24);
Image dst  = image_create(480, 640, PIXEL_FORMAT_RGB24); /* 尺寸互换 */
rotate_90(&src, &dst);
```

### BMP 读写

```c
#include "mini_image/image_io.h"

Image img;
bmp_read_file("input.bmp", &img);
box_blur_apply(&img, &img, 3);
bmp_write_file("output.bmp", &img);
image_destroy(&img);
```

---

## 支持格式

| 格式         | 枚举值                  | 通道           | 字节/像素 |
|--------------|-------------------------|----------------|-----------|
| RGB24        | `PIXEL_FORMAT_RGB24`    | R,G,B 交错      | 3         |
| RGBA32       | `PIXEL_FORMAT_RGBA32`   | R,G,B,A 交错    | 4         |
| Grayscale8   | `PIXEL_FORMAT_GRAYSCALE8`| 单通道亮度       | 1         |
| YUV420P      | `PIXEL_FORMAT_YUV420P`  | planar 三平面   | 1.5       |
| YUV422       | `PIXEL_FORMAT_YUV422`   | YUYV 打包       | 2         |
| NV12         | `PIXEL_FORMAT_NV12`     | Y 平面 + UV 交错 | 1.5       |

---

## 滤波器列表

| 滤波器              | 函数                       | 说明                           |
|---------------------|----------------------------|--------------------------------|
| Box Blur            | `box_blur_apply`           | 可分离均值卷积,radius 控制半径 |
| Gaussian Blur       | `gaussian_blur_apply`      | 高斯权重核,sigma 控制扩散      |
| Sharpen (Unsharp)   | `sharpen_apply`            | 原图 + amount×(原图−模糊)      |
| Sobel Edge          | `sobel_edge_detect`        | 3×3 梯度算子 (仅灰度)          |
| Laplacian Edge      | `laplacian_edge_detect`    | 3×3 二阶导数 (仅灰度)          |
| Brightness/Contrast | `brightness_contrast_adjust`| 线性 dst=src×c + b            |
| Histogram Equalise  | `histogram_equalize`       | 灰度 CDF 拉伸                  |
| 2-D Convolution     | `convolution_2d`           | 通用 K×K 卷积,clamp 边界        |

详细使用说明见 `demo/README_FILTERS.md`。

---

## JPEG 数据路径

```
RGB Image
  → RGB→YCbCr
  → Chroma Subsampling (4:2:0)
  → 8×8 Block Split + Level Shift (−128)
  → DCT
  → Quantisation
  → Zig-zag Scan
  → DPCM + RLE + Huffman Encoding
  → Byte-stream Packing (SOI..EOI)
```

解码为反向操作链。当前 Huffman 部分为骨架实现 (教学用途)。详见 `demo/README_CODECS.md`。

---

## 文件 I/O

| 格式 | 读取                     | 写入                     | 备注                               |
|------|--------------------------|--------------------------|------------------------------------|
| BMP  | `bmp_read_file`          | `bmp_write_file`         | 未压缩 24-bit                      |
| PPM  | `ppm_read_file`          | `ppm_write_file`         | P6 binary RGB                      |
| PGM  | `pgm_read_file`          | `pgm_write_file`         | P5 binary grayscale                |
| PNG  | `png_read_file` (header) | `png_write_file` (无压缩) | IDAT 不含 zlib,骨架实现            |
| RAW  | `raw_pixel_buffer_read`  | `raw_pixel_buffer_write`  | 内存 buffer ↩  Image               |

---

## 构建

### 要求

- **编译器**: GCC (或兼容 C99 的 C 编译器)
- **依赖**: 仅 C 标准库 (libc + libm)
- **Make**: GNU Make

### 目标

```sh
make            # 编译库 + 所有示例
make lib        # 仅编译静态库 bin/libmini_image.a
make examples   # 仅编译示例程序
make run_examples  # 运行所有示例
make clean      # 清理构建产物
```

### 编译选项

```
CC       = gcc
CFLAGS   = -Wall -Wextra -O2 -I include
LDFLAGS  = -lm
OUTDIR   = bin
```

---

## 示例程序

| 示例文件                     | 演示内容                                              |
|------------------------------|-------------------------------------------------------|
| `examples/01_basic_filter.c` | 所有 7 种滤波器 + 自定义卷积 + 核生成                  |
| `examples/02_jpeg_codec.c`   | RGB↔YCbCr、DCT/IDCT、量化、Zigzag、Header 解析、编解码 |
| `examples/03_image_io.c`     | BMP/PPM/PGM/PNG 读写 + 内存读取 + 原始缓冲区           |

运行:
```sh
make examples
./bin/01_basic_filter
./bin/02_jpeg_codec
./bin/03_image_io
```

---

## 编码约定

| 项目     | 风格                | 示例                          |
|----------|---------------------|-------------------------------|
| 类型     | PascalCase          | `PixelFormat`                 |
| 函数     | snake_case          | `box_blur_apply`             |
| 宏/常量  | UPPER_SNAKE_CASE    | `DCT_BLOCK_SIZE`             |
| 文件保护 | `#ifndef` guards    | `MINI_IMAGE_PIXEL_FORMAT_H`   |
| 标准     | C99                 | `#include <stdbool.h>`       |
| 变量     | snake_case          | `int32_t row_offset`          |

---

## 依赖

```
┌─────────────────────┐
│ <stdint.h>           │  定宽整数类型
│ <stdbool.h>          │  bool / true / false
│ <stddef.h>           │  size_t, NULL
│ <stdio.h>            │  FILE*, fopen, fread, fprintf
│ <stdlib.h>           │  malloc, free, calloc
│ <string.h>           │  memcpy, memset, memcmp
│ <math.h>             │  cos, sqrt, exp, roundf, fabsf
└─────────────────────┘
```

无第三方库依赖。

---

## 目录结构

```
mini-image-processing/
├── Makefile
├── README.md
├── include/
│   └── mini_image/
│       ├── pixel_format.h
│       ├── image_filter.h
│       ├── transforms.h
│       ├── jpeg_codec.h
│       └── image_io.h
├── src/
│   ├── pixel_format.c
│   ├── image_filter.c
│   ├── transforms.c
│   ├── jpeg_codec.c
│   └── image_io.c
├── examples/
│   ├── 01_basic_filter.c
│   ├── 02_jpeg_codec.c
│   └── 03_image_io.c
├── demo/
│   ├── README_FILTERS.md
│   └── README_CODECS.md
└── docs/
    ├── API_REFERENCE.md
    └── ARCHITECTURE.md
```

---

## 许可证

MIT License — 详见仓库根目录 LICENSE 文件 (如存在) 或源代码头部注释。
