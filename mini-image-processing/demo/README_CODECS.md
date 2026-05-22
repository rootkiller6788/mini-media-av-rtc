# mini-image-processing — JPEG 编解码器参考手册

## 概览

`mini_image` 提供一套**教学级**的 JPEG 编解码器模拟实现。它不是生产级编码器,而是完整演示 JPEG Baseline 数据路径上的每一步:
色彩变换 → 色度下采样 → DCT → 量化 → Zigzag → Huffman → 字节流组包（及逆向）。

所有步骤均可独立调用,适合**学习 JPEG 内部原理**或作为**轻量编解码原型**。

---

## JPEG Baseline 数据路径

```
[RGB 图像]
    │
    ▼
┌──────────────────────────┐
│ 1. RGB → YCbCr 色彩变换  │
└──────────────────────────┘
    │
    ▼
┌─────────────────────────────┐
│ 2. 色度下采样  (4:2:0)       │
└─────────────────────────────┘
    │
    ▼
┌──────────────────────────┐
│ 3. 8×8 分块 + Level 偏移 │  (减去 128)
└──────────────────────────┘
    │
    ▼
┌──────────────────────────┐
│ 4. 2-D DCT               │
└──────────────────────────┘
    │
    ▼
┌──────────────────────────┐
│ 5. 量化  (Divide)         │
└──────────────────────────┘
    │
    ▼
┌──────────────────────────┐
│ 6. Zig-zag 重新排序      │
└──────────────────────────┘
    │
    ▼
┌──────────────────────────┐
│ 7. Huffman VLC 编码       │
└──────────────────────────┘
    │
    ▼
┌──────────────────────────┐
│ 8. 字节流组包  (JPEG File)│
└──────────────────────────┘
```

---

## 1. RGB ↔ YCbCr 色彩变换

```
void rgb_to_ycbcr(int32_t r, int32_t g, int32_t b,
                  int32_t *y, int32_t *cb, int32_t *cr);
void ycbcr_to_rgb(int32_t y, int32_t cb, int32_t cr,
                  int32_t *r, int32_t *g, int32_t *b);
```

使用 JFIF 标准矩阵（BT.601）:

```
Y  =  0.299·R + 0.587·G + 0.114·B
Cb = -0.1687·R − 0.3313·G + 0.500·B  + 128
Cr =  0.500·R − 0.4187·G − 0.0813·B  + 128
```

反向变换:
```
R = Y + 1.402·(Cr − 128)
G = Y − 0.34414·(Cb − 128) − 0.71414·(Cr − 128)
B = Y + 1.772·(Cb − 128)
```

**示例**
```c
int32_t y, cb, cr;
rgb_to_ycbcr(255, 0, 0, &y, &cb, &cr);  // 红色像素 → Y≈76, Cb≈85, Cr≈255
```

**注意**: 此函数返回 `int32_t`,不 clamp 到 0–255。调用者应自行处理溢出。

---

## 2. 色度下采样  (4:2:0)

```
void chroma_subsample_420(const int32_t *y_plane, int32_t luma_w, int32_t luma_h,
                          int32_t *cb_plane, int32_t *cr_plane);
void chroma_upsample_420(const int32_t *cb_plane, const int32_t *cr_plane,
                         int32_t chroma_w, int32_t chroma_h,
                         int32_t luma_w, int32_t luma_h,
                         int32_t *cb_full, int32_t *cr_full);
```

| 采样方案 | 亮度        | Cb 采样    | Cr 采样    |
|----------|-------------|------------|------------|
| 4:4:4    | N×N         | N×N        | N×N        |
| 4:2:2    | N×N         | (N/2)×N    | (N/2)×N    |
| 4:2:0    | N×N         | (N/2)×(N/2)| (N/2)×(N/2)|

`chroma_subsample_420` 取 2×2 亮度块平均值作为对应的色度样本。
`chroma_upsample_420` 执行最近邻扩展以恢复全尺寸色度平面。

---

## 3. 8×8 DCT / IDCT

```
void dct_8x8(const int16_t src[DCT_BLOCK_LEN], int16_t dst[DCT_BLOCK_LEN]);
void idct_8x8(const int16_t src[DCT_BLOCK_LEN], int16_t dst[DCT_BLOCK_LEN]);
```

直接实现二维 Type-II DCT（正向）与 Type-III DCT（逆向）,均基于 **行列可分离公式**:

```
DCT(u,v) = ¼·C(u)·C(v)·Σ_x Σ_y f(x,y)·cos[(2x+1)uπ/16]·cos[(2y+1)vπ/16]

其中 C(k) = 1/√2  (k=0), 否则 C(k)=1
```

**性能注意**
- 未使用 Chen-Wang 快速算法,复杂度为 O(N⁴)。
- 用于学习/验证目的,生产环境请替换为 AAN/Loeffler 快速实现。

**示例**
```c
int16_t block[64], coeff[64];
for (int i = 0; i < 64; i++) block[i] = 128;  /* 恒定色块 */
dct_8x8(block, coeff);  /* coeff[0] 为 DC 分量,其余为 0 */
idct_8x8(coeff, block);  /* round-trip 恢复原始值 */
```

---

## 4. 量化与质量控制

```
void quantize_block(int16_t block[JPEG_BLOCK_LEN],
                    const uint8_t qtable[JPEG_BLOCK_LEN]);
void dequantize_block(int16_t block[JPEG_BLOCK_LEN],
                      const uint8_t qtable[JPEG_BLOCK_LEN]);
void generate_quant_table(uint8_t qtable[JPEG_BLOCK_LEN],
                          int32_t quality, bool is_luma);
```

**原理**

量化是 JPEG 中**唯一的有损步骤**。DCT 系数 `C(i,j)` 除以量化表中对应值 `Q(i,j)`:

```
C_quantised(i,j) = round( C(i,j) / Q(i,j) )
C_reconstructed(i,j) = C_quantised(i,j) × Q(i,j)
```

**质量参数 (1–100)**
- `generate_quant_table` 基于 IJG 公式从标准示例表派生出质量相关的量化表:

```
scale = quality < 50 ? 5000/quality : 200 − 2×quality
Q(i,j) = clamp( (base_table(i,j) × scale + 50) / 100, 1, 255 )
```

| 质量 | 效果                 |
|------|----------------------|
| 95+  | 几乎无损,大文件      |
| 75   | 良好平衡（常用默认） |
| 50   | 适中压缩,可见伪影    |
| 10   | 强压缩,严重块效应    |

---

## 5. Zig-zag 扫描

```
void zigzag_encode(const int16_t block[JPEG_BLOCK_LEN],
                   int16_t out[JPEG_BLOCK_LEN]);
void zigzag_decode(const int16_t in[JPEG_BLOCK_LEN],
                   int16_t block[JPEG_BLOCK_LEN]);
```

量化后的 8×8 DCT 块经**对角线 Zig-zag 扫描**重排为一维向量,使低频系数集中在前端,高频系数（尤以 0 为多）集中在尾部:

```
 0→ 1  5  6 14 15...
   2  4  7 13...
   3  8 12...
   9 11...
  10...
```

此排序使后续的行程长度编码（RLE）更高效。Zig-zag 顺序为 JPEG 标准硬编码。

---

## 6. Huffman 编码  (简化)

```
int huffman_encode_symbol(int32_t symbol, uint8_t *bitstream,
                           int32_t bit_offset, const JpegHuffTable *table);
int huffman_decode_symbol(const uint8_t *bitstream, int32_t *bit_offset,
                           const JpegHuffTable *table);
```

当前实现为**骨架**版本:
- DC 系数采用 DPCM (差值编码) + Huffman 编码类别（size）及幅值。
- AC 系数采用 RLE（零行程长度）+ Huffman 编码 (run, size) 二元组及幅值。
- 此处 `huffman_encode_symbol` / `huffman_decode_symbol` 仅存储符号尺寸,不生成真正的 VLC 码流。

**标准 JPEG 中使用四张 Huffman 表**
- DC Huffman Table 0  (亮度 DC)
- AC Huffman Table 0  (亮度 AC)
- DC Huffman Table 1  (色度 DC)
- AC Huffman Table 1  (色度 AC)

`JpegHuffTable` 结构体存储码字及对应长度,作为编码/解码的查找表。

---

## 7. JPEG 文件格式解析

```
int jpeg_parse_header(const uint8_t *data, size_t size, JpegContext *ctx);
```

解析 JPEG 字节流中的以下标记段:

| 标记    | 代码    | 说明                                                    |
|---------|---------|---------------------------------------------------------|
| **SOI** | 0xFFD8  | 图像开始                                                |
| **APP0**| 0xFFE0  | JFIF 应用段（版本、分辨率、缩略图等）                     |
| **DQT** | 0xFFDB  | 量化表定义（可含多表,支持亮度/色度分离）                  |
| **SOF0**| 0xFFC0  | 帧开始 (Baseline DCT),含宽度、高度、分量数与采样因子       |
| **DHT** | 0xFFC4  | Huffman 表定义                                            |
| **SOS** | 0xFFDA  | 扫描开始,指定各分量使用的 Huffman 表及扫描参数              |
| **DRI** | 0xFFDD  | 重启间隔                                                  |
| **EOI** | 0xFFD9  | 图像结束                                                  |

解析后 `JpegContext` 填充有 `width`、`height`、`num_components` 及量化表等字段。完整解码需要:
1. 解析 SOS 后续的熵编码扫描数据。
2. 对每个 MCU（最小编码单元）进行 Huffman 解码、反量化、IDCT 与色度上采样。
3. YCbCr→RGB 反向变换。

当前实现完成 Header 解析,`decode` 输出占位数据。

---

## 8. 高级编码 / 解码

```
int jpeg_encode_simple(const Image *src, uint8_t **out, size_t *out_size,
                       int32_t quality);
int jpeg_decode_simple(const uint8_t *data, size_t size, Image *dst);
```

### 编码流程

`jpeg_encode_simple` 将 `Image` 编码为符合 JPEG 语法的字节流:
1. 写入 SOI 标记。
2. 写入 APP0 (JFIF) 段。
3. 写入两张量化表（亮度+色度,DQT）。
4. 写入 SOF0 段（尺寸、分量信息、4:2:0 采样）。
5. 写入 SOS 段 + 占位扫描数据。
6. 写入 EOI。

输出需调用者 `free(*out)` 释放。

### 解码流程

`jpeg_decode_simple` 反向操作:
1. `jpeg_parse_header` 提取图像元数据。
2. 填充占位像素数据到 `dst`（未实现完整扫描数据解码）。

**注意**: 两函数均为教学演示。如需全功能 JPEG codec,建议基于 libjpeg-turbo 或 stb_image。

---

## 9. JPEG 特定数据结构

### JpegQuantTables

```c
typedef struct {
    uint8_t table[2][JPEG_BLOCK_LEN];  /* [0] = 亮度, [1] = 色度 */
} JpegQuantTables;
```

### JpegHuffTable

```c
typedef struct {
    uint8_t  codes[JPEG_MAX_HUFF_LEN];  /* 码字（每符号一码） */
    uint8_t  sizes[JPEG_MAX_HUFF_LEN];  /* 码长 */
    int32_t  count;                      /* 符号数 */
} JpegHuffTable;
```

### JpegContext

```c
typedef struct {
    int32_t         width, height;
    int32_t         num_components;
    int32_t         restart_interval;
    JpegQuantTables quant_tables;
    JpegHuffTable   dc_huff[2], ac_huff[2];
    uint8_t        *raw_data;
    size_t          raw_size;
} JpegContext;
```

需通过 `jpeg_context_init` 初始化,用毕后 `jpeg_context_free` 释放。

---

## 依赖关系

```
jpeg_codec.h
    ├── pixel_format.h  (Image 结构)
    └── transforms.h     (DCT/IDCT)
```

---

## 参考资源

- **ITU-T T.81** (JPEG Standard): https://www.w3.org/Graphics/JPEG/itu-t81.pdf
- **IJG libjpeg**: https://ijg.org/
- **libjpeg-turbo**: https://libjpeg-turbo.org/
- **stb_image.h**: https://github.com/nothings/stb

---

## 编译示例

```sh
gcc -Iinclude -c src/jpeg_codec.c -o jpeg_codec.o
gcc -Iinclude -c src/transforms.c -o transforms.o
gcc -Iinclude -c src/pixel_format.c -o pixel_format.o
gcc -o jpeg_demo examples/02_jpeg_codec.c jpeg_codec.o transforms.o pixel_format.o -lm
./jpeg_demo
```

或使用顶层 Makefile:
```sh
make all
```
