#ifndef YUV_RGB_CONV_H
#define YUV_RGB_CONV_H

#include <stdint.h>
#include <stddef.h>

typedef enum {
    COLOR_MATRIX_BT601,
    COLOR_MATRIX_BT709
} ColorMatrix;

typedef enum {
    CHROMA_UPSAMPLE_NEAREST,
    CHROMA_UPSAMPLE_BILINEAR
} ChromaUpsample;

typedef struct {
    int width;
    int height;
    int y_stride;
    int uv_stride;
    uint8_t *y_plane;
    uint8_t *u_plane;
    uint8_t *v_plane;
    int y_plane_size;
    int uv_plane_size;
    int total_size;
} YUVFrame;

typedef struct {
    int width;
    int height;
    int stride;
    int total_size;
    uint8_t *data;   /* RGB24 或 RGBA32 */
    int channels;    /* 3 = RGB24, 4 = RGBA32 */
} RGBFrame;

/* 分配/释放 YUV 帧 */
YUVFrame yuv_frame_alloc(int width, int height);
void yuv_frame_free(YUVFrame *frame);

/* 分配/释放 RGB 帧 */
RGBFrame rgb_frame_alloc(int width, int height, int channels);
void rgb_frame_free(RGBFrame *frame);

/* YUV420p 平面提取：将 packed YUV 拆分为 Y, U, V 三个平面 */
void yuv_extract_planes(YUVFrame *frame, const uint8_t *packed, int width, int height);

/* 复制帧数据 */
void yuv_frame_copy(YUVFrame *dst, const YUVFrame *src);
YUVFrame yuv_frame_clone(const YUVFrame *src);

/* YUV→RGB 转换（定点计算） */
void yuv_to_rgb(const YUVFrame *yuv, RGBFrame *rgb, ColorMatrix matrix);

/* RGB→YUV 转换 */
void rgb_to_yuv(const RGBFrame *rgb, YUVFrame *yuv, ColorMatrix matrix);

/* 色度上采样：将 4:2:0 色度平面匹配到全分辨率 */
void chroma_upsample_nearest(const YUVFrame *src, uint8_t *u_full, uint8_t *v_full, int width, int height);
void chroma_upsample_bilinear(const YUVFrame *src, uint8_t *u_full, uint8_t *v_full, int width, int height);
void chroma_upsample_to_rgb(const YUVFrame *src, RGBFrame *rgb, ColorMatrix matrix, ChromaUpsample method);

/* 色彩矩阵系数获取 */
void get_bt601_coeffs(float *kr, float *kg, float *kb, int *y_offset, int *uv_offset);
void get_bt709_coeffs(float *kr, float *kg, float *kb, int *y_offset, int *uv_offset);

/* 平面填充（黑白/纯色测试帧） */
void yuv_fill_black(YUVFrame *frame);
void yuv_fill_color(YUVFrame *frame, uint8_t y, uint8_t u, uint8_t v);
void yuv_fill_gradient(YUVFrame *frame);

#endif /* YUV_RGB_CONV_H */
