# mini-image-processing — API 参考

## 数据类型

### PixelFormat

```c
typedef enum {
    PIXEL_FORMAT_RGB24,
    PIXEL_FORMAT_RGBA32,
    PIXEL_FORMAT_YUV420P,
    PIXEL_FORMAT_YUV422,
    PIXEL_FORMAT_NV12,
    PIXEL_FORMAT_GRAYSCALE8,
    PIXEL_FORMAT_COUNT
} PixelFormat;
```

### ChromaSubsampling

```c
typedef enum {
    CHROMA_SUBSAMPLING_444,
    CHROMA_SUBSAMPLING_422,
    CHROMA_SUBSAMPLING_420
} ChromaSubsampling;
```

### Image

```c
typedef struct {
    int32_t  width;
    int32_t  height;
    PixelFormat format;
    uint8_t *data;
    size_t   data_size;
    int32_t  stride;
} Image;
```

### Kernel2D

```c
typedef struct {
    int32_t  size;
    float   *data;
    int32_t  stride;
} Kernel2D;
```

---

## 像素格式 — pixel_format.h

### 生命周期

| 函数                           | 说明                                      |
|--------------------------------|-------------------------------------------|
| `image_create(w,h,fmt)`        | 分配 Image,`data` 由 calloc 初始化       |
| `image_destroy(img)`           | 释放 `data` 并置零                       |
| `image_calculate_size(w,h,fmt)`| 按格式计算字节数                         |
| `image_bytes_per_pixel(fmt)`   | 返回每像素字节数（planar 格式返回 0）     |
| `image_chroma_plane_w(lw,ss)`  | 给定亮度和采样方案,返回色度平面宽度       |
| `image_chroma_plane_h(lh,ss)`  | 返回色度平面高度                          |

### 色彩空间转换

| 函数                     | 说明                     |
|--------------------------|--------------------------|
| `clamp_rgb(r,g,b)`       | 将 RGB clamp 到 [0,255] |
| `yuv_to_rgb(y,cb,cr,...)`| YUV→RGB,含 clamp         |
| `rgb_to_yuv(r,g,b,...)`  | RGB→YUV,含 clamp         |

### 像素格式转换

所有转换返回 0 表示成功,-1 表示错误。

| 函数                     | 从            | 到              |
|--------------------------|---------------|-----------------|
| `rgb24_to_yuv420p`       | RGB24         | YUV420P         |
| `yuv420p_to_rgb24`       | YUV420P       | RGB24           |
| `rgb24_to_rgba32`        | RGB24         | RGBA32          |
| `rgba32_to_rgb24`        | RGBA32        | RGB24           |
| `rgb24_to_grayscale`     | RGB24         | Grayscale8      |
| `grayscale_to_rgb24`     | Grayscale8    | RGB24           |
| `nv12_to_rgb24`          | NV12          | RGB24           |
| `rgb24_to_nv12`          | RGB24         | NV12            |
| `rgb24_to_yuv422`        | RGB24         | YUV422          |
| `yuv422_to_rgb24`        | YUV422        | RGB24           |

### BT.601 宏

| 宏                      | 说明                 |
|-------------------------|----------------------|
| `YUV_Y_FROM_RGB(r,g,b)` | 亮度分量             |
| `YUV_CB_FROM_RGB(...)`  | Cb 色度分量          |
| `YUV_CR_FROM_RGB(...)`  | Cr 色度分量          |
| `YUV_R_FROM_YUV(y,cb,cr)`| R 还原              |
| `YUV_G_FROM_YUV(...)`   | G 还原               |
| `YUV_B_FROM_YUV(...)`   | B 还原               |

---

## 图像滤波 — image_filter.h

| 函数                              | 说明                                     |
|-----------------------------------|------------------------------------------|
| `box_blur_apply(src,dst,radius)`  | 方框模糊,先行后列可分离卷积              |
| `gaussian_blur_generate_kernel`   | 生成高斯核权重                           |
| `gaussian_blur_apply(src,dst,r,σ)`| 高斯模糊                                 |
| `sharpen_apply(src,dst,amt,thrs)` | Unsharp Mask 锐化                        |
| `sobel_edge_detect(src,dst)`      | Sobel 3×3 边缘检测 (Grayscale8)          |
| `laplacian_edge_detect(src,dst)`  | Laplacian 3×3 边缘检测 (Grayscale8)     |
| `brightness_contrast_adjust(...)` | 线性亮度/对比度调整                      |
| `histogram_equalize(src,dst)`     | 直方图均衡 (RGB 自动转灰度)             |
| `convolution_2d(src,dst,kernel)`  | 通用 2-D 卷积 (含 clamp 边界)            |

### Kernel2D 辅助

| 函数                     | 说明                      |
|--------------------------|---------------------------|
| `kernel2d_init(k,size)`  | 分配 size×size 浮点核    |
| `kernel2d_free(k)`       | 释放内存                  |
| `kernel2d_fill(k,vals)`  | 从数组填充                 |
| `kernel2d_normalize(k)`  | 归一化（除以权重和）        |

---

## 几何变换 — transforms.h

### DCT

| 函数                             | 说明                        |
|----------------------------------|-----------------------------|
| `dct_8x8(src[64],dst[64])`       | 2-D DCT  (8×8,i16)          |
| `idct_8x8(src[64],dst[64])`      | 2-D IDCT (8×8,i16)          |

### 缩放

| 函数                            | 说明                            |
|---------------------------------|---------------------------------|
| `resize_nearest(src,dst)`       | 最近邻缩放                      |
| `resize_bilinear(src,dst)`      | 双线性插值缩放                  |
| `resize_to(src,dst,w,h,bool)`   | 按指定尺寸缩放,可选择插值方法    |

### 旋转

| 函数                             | 说明                          |
|----------------------------------|-------------------------------|
| `rotate_90(src,dst)`             | 顺时针 90°                     |
| `rotate_180(src,dst)`            | 180°                           |
| `rotate_270(src,dst)`            | 顺时针 270° (逆时针 90°)       |
| `rotate_arbitrary(src,dst,deg)`  | 任意 90°倍角 (0/90/180/270)    |

### 翻转

| 函数                     | 说明        |
|--------------------------|-------------|
| `flip_horizontal(src,dst)`| 水平镜像   |
| `flip_vertical(src,dst)` | 垂直镜像    |

### 裁剪与填充

| 函数                                    | 说明                                    |
|-----------------------------------------|-----------------------------------------|
| `crop(src,dst,x,y)`                     | 从 (x,y) 开始,裁剪大小为 dst 的尺寸      |
| `pad(src,dst,l,t,r,b, R,G,B)`          | 四周填充,fill colour 指定               |

---

## JPEG 编解码 — jpeg_codec.h

### 色彩变换

| 函数                                | 说明             |
|-------------------------------------|------------------|
| `rgb_to_ycbcr(r,g,b, &y,&cb,&cr)`   | RGB→YCbCr       |
| `ycbcr_to_rgb(y,cb,cr, &r,&g,&b)`   | YCbCr→RGB       |

### 色度下采样

| 函数                                                | 说明                |
|-----------------------------------------------------|---------------------|
| `chroma_subsample_420(y,cb,cr, lw,lh)`              | 4:2:0 下采样       |
| `chroma_upsample_420(cb,cr,cw,ch,lw,lh, cb_f,cr_f)`| 4:2:0 上采样       |

### 量化

| 函数                                       | 说明                     |
|--------------------------------------------|--------------------------|
| `quantize_block(block,qtable)`             | 除法量化                  |
| `dequantize_block(block,qtable)`           | 乘法反量化                |
| `generate_quant_table(qtable,q,is_luma)`   | 按质量生成量化表          |

### Zig-zag

| 函数                            | 说明                 |
|---------------------------------|----------------------|
| `zigzag_encode(block,out)`      | 按 zig-zag 顺序重排   |
| `zigzag_decode(in,block)`       | 逆 zig-zag           |

### Huffman (简化)

| 函数                                                | 说明               |
|-----------------------------------------------------|--------------------|
| `huffman_encode_symbol(sym,buf,off,table)`          | 编码单个符号       |
| `huffman_decode_symbol(buf,&off,table)`             | 解码单个符号       |

### JPEG 解析

| 函数                                     | 说明                        |
|------------------------------------------|-----------------------------|
| `jpeg_parse_header(data,size,ctx)`       | 解析 JPEG 标记段             |
| `jpeg_context_init(ctx)` / `_free(ctx)`  | 上下文生命周期               |

### 高级 API

| 函数                                            | 说明              |
|-------------------------------------------------|-------------------|
| `jpeg_encode_simple(src,&out,&size,quality)`    | 简化的 JPEG 编码   |
| `jpeg_decode_simple(data,size,dst)`             | 简化的 JPEG 解码   |

---

## 图像 I/O — image_io.h

### BMP

| 函数                        | 说明                 |
|-----------------------------|----------------------|
| `bmp_read_file(path,img)`   | 读取 24-bit BMP      |
| `bmp_write_file(path,img)`  | 写入 24-bit BMP      |
| `bmp_read_memory(data,n,img)`| 从内存缓冲区读取    |

**结构体**: `BmpFileHeader` (14 bytes), `BmpInfoHeader` (40 bytes), `#pragma pack(push,1)` 确保对齐。

### PPM / PGM

| 函数                     | 说明                     |
|--------------------------|--------------------------|
| `ppm_read_file(path,img)`| 读取 P6 二进制 PPM       |
| `ppm_write_file(path,img)`| 写入 P6 二进制 PPM       |
| `pgm_read_file(path,img)`| 读取 P5 二进制 PGM       |
| `pgm_write_file(path,img)`| 写入 P5 二进制 PGM       |

### PNG

| 函数                     | 说明                                        |
|--------------------------|---------------------------------------------|
| `png_read_file(path,img)`| 读取 PNG (IHDR 解析,IDAT 跳过)               |
| `png_write_file(path,img)`| 写入最小 PNG (IHDR+IDAT+IEND,无压缩)        |

常量: `PNG_SIGNATURE[8]` = `{ 137, 80, 78, 71, 13, 10, 26, 10 }`.

### 原始像素缓冲区

| 函数                                            | 说明                    |
|-------------------------------------------------|-------------------------|
| `raw_pixel_buffer_create(w,h,fmt)`              | 分配并返回 Image         |
| `raw_pixel_buffer_free(img)`                    | 释放 Image               |
| `raw_pixel_buffer_read(data,n,w,h,fmt,img)`     | 从缓冲区拷贝            |
| `raw_pixel_buffer_write(img,buf,n)`             | 写入到缓冲区            |

---

## 返回值约定

所有函数:
- `0`  = 成功。
- `-1` = 错误（空指针、格式不匹配、内存不足、I/O 失败等）。

---

## 线程安全性

代码未使用全局可变状态,所有操作通过参数传递数据。`Image` 中的 `data` 内存由调用者管理,**非线程安全**的并发创建/销毁需自行同步。

---

## 包含顺序

推荐顺序 (解决依赖):

```c
#include "mini_image/pixel_format.h"   /* 基础类型声明 */
#include "mini_image/image_filter.h"    /* 依赖 pixel_format */
#include "mini_image/transforms.h"      /* 依赖 pixel_format */
#include "mini_image/jpeg_codec.h"      /* 依赖 pixel_format + transforms */
#include "mini_image/image_io.h"        /* 依赖 pixel_format */
```

实际每个头文件各自 `#include "pixel_format.h"`,可安全独立包含。
