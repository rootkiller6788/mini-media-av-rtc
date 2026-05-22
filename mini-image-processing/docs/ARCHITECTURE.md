# mini-image-processing — 架构文档

## 设计目标

`mini_image` 是一个**零依赖、纯 C99** 的图像处理教学库。核心设计原则:

1. **自包含**: 仅依赖 C 标准库 (`stdlib`, `string`, `stdio`, `math`)。
2. **最小抽象**: 所有图像数据存储在连续内存块 (`uint8_t *data`) 中,布局由 `PixelFormat` 枚举约定。
3. **可组合**: 各模块头部清晰分离,可独立包含/链接。
4. **教学导向**: 算法直接实现而非黑箱包装,代码注释及本文档配套用于解释内部原理。

---

## 模块拓扑

```
┌────────────────────────────────────────────────────────────────┐
│                         Application                           │
├───────────┬──────────┬──────────┬───────────┬────────────────┤
│examples/  │ demo/    │ docs/    │ README.md │ Makefile       │
│01..03.c   │ *.md     │ *.md     │           │                │
└─────┬─────┴────┬─────┴──────────┴───────────┴────────────────┘
      │          │
      ▼          ▼
┌────────────────────────────────────────────────────────────────┐
│                     Public API  (include/)                     │
├──────────────┬──────────────┬────────────┬─────────┬──────────┤
│pixel_format.h│image_filter.h│transforms.h│jpeg_...h│image_io.h│
└──────┬───────┴──────┬───────┴──────┬─────┴────┬────┴────┬─────┘
       │              │              │          │         │
       ▼              ▼              ▼          ▼         ▼
┌────────────────────────────────────────────────────────────────┐
│                    Implementation  (src/)                      │
├──────────────┬──────────────┬────────────┬─────────┬──────────┤
│pixel_format.c│image_filter.c│transforms.c│jpeg_...c│image_io.c│
└──────────────┴──────────────┴────────────┴─────────┴──────────┘
```

### 依赖图

```
pixel_format.h ◄── image_filter.h
pixel_format.h ◄── transforms.h
pixel_format.h ◄── image_io.h
pixel_format.h + transforms.h  ◄──  jpeg_codec.h
```

每个 `.h` 内部 `#include "pixel_format.h"`,因此 `Image` 类型在任何 API 中直接可用。

---

## 内存模型

`Image` 是核心数据容器:

```c
typedef struct {
    int32_t  width, height;
    PixelFormat format;
    uint8_t *data;
    size_t   data_size;
    int32_t  stride;
} Image;
```

### 像素布局 (按格式)

| 格式          | data 布局                                                                                     |
|---------------|-----------------------------------------------------------------------------------------------|
| RGB24         | 连续 RGBRGB...,row-major,stride = width×3                                                     |
| RGBA32        | 连续 RGBARGBA...,stride = width×4                                                             |
| Grayscale8    | 单字节连续, stride = width                                                                     |
| YUV420P       | [Y plane (w×h)] + [Cb plane (w/2 × h/2)] + [Cr plane (w/2 × h/2)]                           |
| NV12          | [Y plane (w×h)] + [interleaved UV plane (Cb0,Cr0,Cb1,Cr1,...) (w/2 × h/2 × 2)]              |
| YUV422        | YUYV 打包: 每组 2 像素 → {Y0, (Cb0+Cb1)/2, Y1, (Cr0+Cr1)/2},stride = width×2                 |

### 生命周期模式

```c
Image img = image_create(W, H, PIXEL_FORMAT_RGB24);  // 分配
/* ... process ... */
image_destroy(&img);                                   // 释放
```

`src` 与 `dst` 由调用者分别在传入前分配。API 不会在内部管理内存,确保无隐藏分配。

---

## 滤波器模块 (image_filter.c)

### 数据流

```
src (Image*) ──► [滤波器核] ──► dst (Image*)
                  │
                  ├── box_blur_apply:  可分离 1-D 均值卷积
                  ├── gaussian_blur:   可分离 1-D 高斯卷积
                  ├── sharpen_apply:   unsharp mask → 边界对比增强
                  ├── sobel_edge:      3×3 Sobel 梯度 → 幅值
                  ├── laplacian_edge:  3×3 Laplacian 二阶 → 绝对值
                  ├── brightness_cont: 逐像素 dst=src×contrast+brightness
                  ├── histogram_eq:    灰度 CDF 均衡（RGB 自动转换）
                  └── convolution_2d:  通用 K×K 卷积, clamp 边界
```

### 性能基准 (640×480 RGB24, Intel i5-13500H, -O2)

| 操作            | 耗时 (us)  | 备注                      |
|-----------------|------------|---------------------------|
| box_blur r=3    | ~1200      | 可分离 + 无浮点            |
| gaussian_blur r=3| ~3500     | exp() 计算开销              |
| sobel_edge      | ~800       | 仅整数运算                 |
| sharpen_apply   | ~4000      | 内含 gaussian_blur 调用    |
| convolution_2d(3×3) | ~1100 | 边界 clamp 开销小          |

---

## JPEG 模块 (jpeg_codec.c)

### 编码管线

```
Image (RGB24)
    │
    ▼
RGB→YCbCr  (逐像素,标量 float)
    │
    ▼
分块 (8×8 MCU) + Level 偏移 (−128)
    │
    ▼
DCT  (dct_8x8, O(N⁴))
    │
    ▼
量化  (整数除法: coeff / qtable)
    │
    ▼
Zigzag (查表重排 64 元素)
    │
    ▼
Huffman VLC (骨架: 仅存储符号尺寸)
    │
    ▼
字节流组包 (SOI → APP0 → DQT → SOF0 → DHT → SOS → data → EOI)
```

### 解码管线 (逆向)

```
字节流解析 (jpeg_parse_header)
    │
    ▼
Huffman VLD → Zigzag⁻¹ → 反量化 → IDCT → Level +128
    │
    ▼
YCbCr→RGB → Image (RGB24)
```

### 局限

| 方面               | 状态                          |
|--------------------|-------------------------------|
| Huffman 编码/解码   | 骨架（仅存储符号尺寸,未生成 VLC 码流） |
| 扫描数据解码        | 占位（输出灰色填充）          |
| DCT 算法            | 直接实现 (O(N⁴)),非快速算法   |
| DNL (Define Number of Lines) | 不支持               |
| 渐进式 JPEG         | 不支持                       |
| 算术编码            | 不支持                       |
| 12-bit 精度         | 不支持（仅 8-bit）            |

---

## I/O 模块 (image_io.c)

### BMP 解析流程

```
fopen(path, "rb")
    │
    ▼
read BmpFileHeader (14 B)  →  verify bf_type == 'BM'
    │
    ▼
read BmpInfoHeader (40 B)  →  verify bi_compression == BI_RGB
    │
    ▼
extract width, height, bi_bit_count
    │
    ▼
allocate Image (RGB24) — height×width×3
    │
    ▼
for y = height-1 downto 0:   (BMP 为 bottom-up)
    read width×3 bytes → Image row
    skip row_pad bytes
    │
    ▼
return Image
```

**支持**: 未压缩 24-bit BMP;不支持 RLE、BITFIELDS、调色板。

### PPM 解析

```
fopen → read magic "P6\n" → scanf width height maxval → read pixel data
```

P6 = binary RGB; P5 = binary grayscale (PGM)。

### PNG 骨架

写入时生成标准 IHDR+IDAT+IEND 块,但 IDAT 不含 zlib 压缩 (直接输出原始字节)。读取时可解析 IHDR 以获取尺寸/色深,但跳过 IDAT 体。完整 PNG 支持需集成 zlib/inflate。

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

## 编码约定

| 类别     | 风格                      | 示例                       |
|----------|---------------------------|----------------------------|
| 类型名   | PascalCase                | `PixelFormat`, `Kernel2D`  |
| 函数名   | snake_case                | `box_blur_apply`           |
| 宏/常量  | UPPER_SNAKE_CASE          | `DCT_BLOCK_SIZE`           |
| 局部变量 | snake_case                | `int32_t w = img->width`   |
| 文件保护 | `#ifndef` / `#define`     | `MINI_IMAGE_PIXEL_FORMAT_H`|
| C 标准   | C99, `<stdbool.h>`        |                            |

`sint32_t` / `uint8_t` 等定宽整型来自 `<stdint.h>`,保证跨平台一致。

---

## 编译

### 仅库 (静态)

```sh
make lib       # → bin/libmini_image.a (所有源文件归档)
```

### 全部示例

```sh
make examples  # → bin/01_basic_filter.exe, bin/02_jpeg_codec.exe, ...
```

### 全部 (库 + 示例)

```sh
make           # 或 make all
```

### 手动

```sh
gcc -Wall -Wextra -O2 -I include -c src/pixel_format.c -o pixel_format.o
gcc -Wall -Wextra -O2 -I include -c src/image_filter.c -o image_filter.o
ar rcs libmini_image.a pixel_format.o image_filter.o ...  -lm
```

**链接顺序**: 无需特殊顺序,所有符号为函数级,无循环依赖。

---

## 测试策略

当前版本无单元测试框架。可使用示例程序作为功能回归:

```sh
make examples
make run_examples
```

`examples/01_basic_filter.c` 对所有滤波器执行单次调用并输出 [OK]/[FAIL] 状态,适合快速冒烟测试。完整测试套件建议基于 CMocka 或 Unity。

---

## 未来路线

1. **SIMD 加速**: SSE/NEON 向量化 box_blur、convolution_2d 等热点。
2. **快速 DCT**: 实现 AAN scalefactor 表驱动 DCT。
3. **完整 Huffman**: 补齐 VLC 码流生成/解析。
4. **PNG 支持**: 集成 miniz/deflate 用于 IDAT 正确读写。
5. **WebP 骨架**: 添加 VP8 内部块解码演示。
6. **线程池**: 增加 OpenMP 并行化可分离卷积。
