# mini-image-processing — 滤波器参考手册

## 概览

`mini_image` 提供了一套轻量级 2-D 图像滤波器,包括平滑（模糊）、锐化、边缘检测、点操作与直方图均衡。
所有滤波器均工作在 `Image` 结构上,支持 **RGB24** 与 **Grayscale8** 像素格式。

---

## 1. 方框模糊  (Box Blur)

```
int box_blur_apply(const Image *src, Image *dst, int32_t radius);
```

方框模糊是最简单、最快的模糊方法。它以给定半径在每个像素周围取一个正方形窗口,用 `radius` 指定半窗宽（窗口尺寸 = `2×radius + 1`）。
窗口内所有像素权重相同,求平均后写入目标图像。

**参数**

| 参数     | 说明                          |
|----------|-------------------------------|
| `src`    | 源图像（RGB24 或 Grayscale8） |
| `dst`    | 目标图像,需预先分配           |
| `radius` | 模糊半径,≥ 1                  |

**示例**
```c
Image src = image_create(640, 480, PIXEL_FORMAT_RGB24);
Image dst = image_create(640, 480, PIXEL_FORMAT_RGB24);
box_blur_apply(&src, &dst, 5);
```

**性能提示**
- 实现为**可分离卷积**（先行、后列）,复杂度 O(W·H·R) 而非 O(W·H·R²)。
- 大半径建议改用高斯模糊以获更自然效果。

---

## 2. 高斯模糊  (Gaussian Blur)

```
int gaussian_blur_generate_kernel(Kernel2D *k, int32_t radius, float sigma);
int gaussian_blur_apply(const Image *src, Image *dst, int32_t radius, float sigma);
```

高斯模糊使用钟形权重分布,边缘过渡比 box blur 更平滑。核权重由 `sigma` 参数控制扩散程度。

**参数**

| 参数     | 说明                                           |
|----------|------------------------------------------------|
| `radius` | 核半径（窗口 = 2×radius+1）,通常 radius ≈ 3σ |
| `sigma`  | 标准差,越大越模糊                              |

**常用组合**

| 用途           | radius | sigma |
|----------------|--------|-------|
| 轻微去噪       | 2      | 1.0   |
| 一般平滑       | 3      | 1.4   |
| 强模糊（背景） | 5      | 3.0   |

**示例**
```c
Image src = image_create(800, 600, PIXEL_FORMAT_RGB24);
Image dst = image_create(800, 600, PIXEL_FORMAT_RGB24);
gaussian_blur_apply(&src, &dst, 3, 1.6f);
```

`gaussian_blur_generate_kernel` 允许手动获取核权重后通过 `convolution_2d` 应用。

---

## 3. 锐化  (Sharpen — Unsharp Mask)

```
int sharpen_apply(const Image *src, Image *dst, float amount, float threshold);
```

使用 **Unsharp Mask** 算法增强边缘。原理为:从原图减去高斯模糊后的图像,再按 `amount` 加权叠加回原图。
仅当像素差超过 `threshold` 时才应用锐化,用于抑制噪声放大。

**数学公式**
```
sharp = src + amount × (src − blur)
```

**参数**

| 参数        | 说明                           |
|-------------|--------------------------------|
| `amount`    | 锐化强度,通常 0.5–2.0          |
| `threshold` | 阈值（灰度差,0–255）,≤ 此值不处理 |

**示例**
```c
Image src = image_create(1024, 768, PIXEL_FORMAT_RGB24);
Image dst = image_create(1024, 768, PIXEL_FORMAT_RGB24);
sharpen_apply(&src, &dst, 1.5f, 2.0f);  // 柔和锐化
sharpen_apply(&src, &dst, 3.0f, 5.0f);  // 强锐化（可能产生光晕）
```

---

## 4. 边缘检测: Sobel

```
int sobel_edge_detect(const Image *src, Image *dst);
```

对灰度图像应用 **3×3 Sobel 算子**,计算 x/y 方向梯度后取幅度。输出为 0–255 的梯度强度图。

**Sobel 核**

```
Gx = [[-1,  0, +1],      Gy = [[-1, -2, -1],
      [-2,  0, +2],            [ 0,  0,  0],
      [-1,  0, +1]]            [+1, +2, +1]]
```

`|∇| ≈ √(Gx² + Gy²)`

**注意**: 仅接受 **Grayscale8** 输入;RGB 图像需先调用 `rgb24_to_grayscale`。

**示例**
```c
Image gray = image_create(640, 480, PIXEL_FORMAT_GRAYSCALE8);
rgb24_to_grayscale(&src_rgb, &gray);
Image edges = image_create(640, 480, PIXEL_FORMAT_GRAYSCALE8);
sobel_edge_detect(&gray, &edges);
```

---

## 5. 边缘检测: Laplacian

```
int laplacian_edge_detect(const Image *src, Image *dst);
```

使用 **3×3 Laplacian 算子** 检测二阶导数过零点。核固定为:

```
[[ 0, -1,  0],
 [-1, +4, -1],
 [ 0, -1,  0]]
```

同样仅接受 Grayscale8。对比 Sobel,Laplacian 对噪声更敏感但能捕捉各向同性边缘。

---

## 6. 亮度 / 对比度调整

```
int brightness_contrast_adjust(const Image *src, Image *dst,
                               float brightness, float contrast);
```

逐像素线性变换:
```
dst = src × contrast + brightness
```

**参数**
- `brightness`: 加性偏移,0 = 不变,正数增亮,负数减暗。
- `contrast`: 乘性因子,1.0 = 不变,>1 增强对比,<1 降低对比。

**示例**
```c
brightness_contrast_adjust(&src, &dst, -30.0f, 1.3f);  // 增对比、变暗
brightness_contrast_adjust(&src, &dst, +50.0f, 0.8f);  // 降对比、提亮
```

---

## 7. 直方图均衡化

```
int histogram_equalize(const Image *src, Image *dst);
```

通过累积分布函数（CDF）将像素的灰度直方图拉伸为均匀分布,可有效增强低对比度图像的细节可见性。

RGB 输入会自动转为灰度处理,再回写为 RGB;灰度输入原地处理。

**使用场景**: 医学影像、低光照片、X 光片增强。

---

## 8. 通用 2-D 卷积

```
int convolution_2d(const Image *src, Image *dst, const Kernel2D *kernel);
```

提供一个灵活的可编程卷积框架。用户可以:

1. 用 `kernel2d_init` 创建核。
2. `kernel2d_fill` 赋值。
3. `kernel2d_normalize` 为归一化。
4. 传入 `convolution_2d` 应用。

**核辅助 API**

| 函数                    | 说明                           |
|-------------------------|--------------------------------|
| `kernel2d_init`         | 分配 `size×size` 浮点核       |
| `kernel2d_free`         | 释放核内存                     |
| `kernel2d_fill`         | 从浮点数组填充核权重           |
| `kernel2d_normalize`    | 除以所有权重之和（归一化）     |

**自定义卷积示例** （浮雕核）
```c
Kernel2D k;
kernel2d_init(&k, 3);
float emboss[9] = {-2, -1, 0, -1, 1, 1, 0, 1, 2};
kernel2d_fill(&k, emboss);
convolution_2d(&src, &dst, &k);
kernel2d_free(&k);
```

**常用核参考**

| 效果   | 3×3 核                                             |
|--------|----------------------------------------------------|
| 锐化   | ` 0, -1,  0, -1, 5, -1,  0, -1,  0`              |
| 浮雕   | `-2, -1, 0, -1, 1, 1, 0, 1, 2`                   |
| 均值   | 全 `1/9` （`kernel2d_normalize` 后）              |
| 边缘   | `-1, -1, -1, -1, 8, -1, -1, -1, -1`              |

---

## 边界处理

所有滤波器对超出图像边界的像素采样采用 **夹取（clamp）** 策略:
- 负坐标用 0 替换
- 超宽/超高坐标用 `width−1` / `height−1` 替换

不支持 wrap / mirror / reflect 等其它模式。

---

## 格式兼容性一览

| 函数                          | RGB24 | RGBA32 | Grayscale8 | YUV420p | NV12 | YUV422 |
|-------------------------------|-------|--------|------------|---------|------|--------|
| `box_blur_apply`              | ✓     | —      | ✓          | —       | —    | —      |
| `gaussian_blur_apply`         | ✓     | —      | ✓          | —       | —    | —      |
| `sharpen_apply`               | ✓     | —      | ✓          | —       | —    | —      |
| `sobel_edge_detect`           | —     | —      | ✓          | —       | —    | —      |
| `laplacian_edge_detect`       | —     | —      | ✓          | —       | —    | —      |
| `brightness_contrast_adjust`  | ✓     | ✓      | ✓          | —       | —    | —      |
| `histogram_equalize`          | ✓     | —      | ✓          | —       | —    | —      |
| `convolution_2d`              | ✓     | ✓      | ✓          | —       | —    | —      |

---

## 编译与链接

```sh
gcc -Iinclude -c src/image_filter.c -o image_filter.o
gcc -Iinclude -c src/pixel_format.c -o pixel_format.o
gcc -o my_app my_app.c image_filter.o pixel_format.o -lm
```

也可通过顶层 Makefile 一键构建:
```sh
make examples
```

详细信息请参阅项目 README.md。
