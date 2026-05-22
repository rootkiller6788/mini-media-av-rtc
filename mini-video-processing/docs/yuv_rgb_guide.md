# YUV/RGB 色彩空间转换指南

## 概述

YUV 色彩空间广泛应用于视频编解码领域，与 RGB 色彩空间相比，YUV 分离了亮度（Y）和色度（U、V）信息，可实现色度子采样以压缩数据量。

## YUV420p 平面格式

YUV420p（planar YUV 4:2:0）的三个平面独立存储：

```
Y 平面：width × height 像素（每个像素 1 字节）
U 平面：(width/2) × (height/2) 像素
V 平面：(width/2) × (height/2) 像素

内存布局：
[ Y0 Y1 Y2 ... Yn-1 ][ U0 U1 ... Um-1 ][ V0 V1 ... Vm-1 ]
```

## 色彩矩阵

### BT.601（标清 SD）

用于标清电视（720×576 及以下分辨率）：

```
Y  =  0.299*R + 0.587*G + 0.114*B
U  = -0.169*R - 0.331*G + 0.500*B + 128
V  =  0.500*R - 0.419*G - 0.081*B + 128

R  = Y + 1.402*(V-128)
G  = Y - 0.344*(U-128) - 0.714*(V-128)
B  = Y + 1.772*(U-128)
```

### BT.709（高清 HD）

用于高清电视（1280×720 及以上分辨率）：

```
Y  =  0.2126*R + 0.7152*G + 0.0722*B
U  = -0.1146*R - 0.3854*G + 0.5000*B + 128
V  =  0.5000*R - 0.4542*G - 0.0458*B + 128

R  = Y + 1.5748*(V-128)
G  = Y - 0.1873*(U-128) - 0.4681*(V-128)
B  = Y + 1.8556*(U-128)
```

## 色度上采样

### 最近邻（Nearest Neighbor）

对于目标像素 (x, y)，直接使用最近的色度样本：

```c
src_x = x / 2;
src_y = y / 2;
u[x][y] = u_420[src_y][src_x];
v[x][y] = v_420[src_y][src_x];
```

### 双线性（Bilinear）

通过周围 4 个色度样本的加权平均进行插值：

```c
fx = x * 0.5 - 0.25;
fy = y * 0.5 - 0.25;
x0 = floor(fx); y0 = floor(fy);
x1 = x0 + 1;    y1 = y0 + 1;
dx = fx - x0;   dy = fy - y0;

value = (1-dx)*(1-dy)*sample[x0][y0] +
        dx*(1-dy)*sample[x1][y0] +
        (1-dx)*dy*sample[x0][y1] +
        dx*dy*sample[x1][y1];
```

## API 使用

### 分配帧

```c
YUVFrame yuv = yuv_frame_alloc(1920, 1080);
RGBFrame rgb = rgb_frame_alloc(1920, 1080, 3);  /* RGB24 */
```

### 基本转换

```c
yuv_to_rgb(&yuv, &rgb, COLOR_MATRIX_BT709);
rgb_to_yuv(&rgb, &yuv, COLOR_MATRIX_BT601);
```

### 上采样转换

```c
chroma_upsample_to_rgb(&yuv, &rgb, COLOR_MATRIX_BT601, CHROMA_UPSAMPLE_BILINEAR);
```

## 注意事项

- 宽度和高度必须为偶数（YUV420p 要求）
- 转换使用定点近似计算，存在可接受的精度损失
- 及时释放通过 `yuv_frame_alloc` / `rgb_frame_alloc` 分配的帧
