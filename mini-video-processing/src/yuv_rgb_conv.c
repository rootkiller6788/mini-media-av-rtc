#include "yuv_rgb_conv.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

YUVFrame yuv_frame_alloc(int width, int height)
{
    YUVFrame f;
    f.width = width;
    f.height = height;
    f.y_stride = width;
    f.uv_stride = width / 2;
    f.y_plane_size = width * height;
    f.uv_plane_size = (width / 2) * (height / 2);
    f.total_size = f.y_plane_size + 2 * f.uv_plane_size;
    f.y_plane = (uint8_t *)malloc(f.y_plane_size);
    f.u_plane = (uint8_t *)malloc(f.uv_plane_size);
    f.v_plane = (uint8_t *)malloc(f.uv_plane_size);
    return f;
}

void yuv_frame_free(YUVFrame *frame)
{
    if (frame) {
        free(frame->y_plane);
        free(frame->u_plane);
        free(frame->v_plane);
        frame->y_plane = NULL;
        frame->u_plane = NULL;
        frame->v_plane = NULL;
    }
}

RGBFrame rgb_frame_alloc(int width, int height, int channels)
{
    RGBFrame f;
    f.width = width;
    f.height = height;
    f.channels = channels;
    f.stride = width * channels;
    f.total_size = width * height * channels;
    f.data = (uint8_t *)malloc(f.total_size);
    return f;
}

void rgb_frame_free(RGBFrame *frame)
{
    if (frame && frame->data) {
        free(frame->data);
        frame->data = NULL;
    }
}

void yuv_extract_planes(YUVFrame *frame, const uint8_t *packed, int width, int height)
{
    int y_size = width * height;
    int uv_size = (width / 2) * (height / 2);
    const uint8_t *src = packed;

    memcpy(frame->y_plane, src, y_size);
    src += y_size;
    memcpy(frame->u_plane, src, uv_size);
    src += uv_size;
    memcpy(frame->v_plane, src, uv_size);
}

void yuv_frame_copy(YUVFrame *dst, const YUVFrame *src)
{
    memcpy(dst->y_plane, src->y_plane, src->y_plane_size);
    memcpy(dst->u_plane, src->u_plane, src->uv_plane_size);
    memcpy(dst->v_plane, src->v_plane, src->uv_plane_size);
}

YUVFrame yuv_frame_clone(const YUVFrame *src)
{
    YUVFrame dst = yuv_frame_alloc(src->width, src->height);
    yuv_frame_copy(&dst, src);
    return dst;
}

void get_bt601_coeffs(float *kr, float *kg, float *kb, int *y_offset, int *uv_offset)
{
    *kr = 0.299f;
    *kg = 0.587f;
    *kb = 0.114f;
    *y_offset = 16;
    *uv_offset = 128;
}

void get_bt709_coeffs(float *kr, float *kg, float *kb, int *y_offset, int *uv_offset)
{
    *kr = 0.2126f;
    *kg = 0.7152f;
    *kb = 0.0722f;
    *y_offset = 16;
    *uv_offset = 128;
}

static void get_matrix_coeffs(ColorMatrix matrix, float *kr, float *kg, float *kb,
                              int *y_off, int *uv_off)
{
    if (matrix == COLOR_MATRIX_BT709)
        get_bt709_coeffs(kr, kg, kb, y_off, uv_off);
    else
        get_bt601_coeffs(kr, kg, kb, y_off, uv_off);
}

void yuv_to_rgb(const YUVFrame *yuv, RGBFrame *rgb, ColorMatrix matrix)
{
    int i, j;
    float kr, kg, kb;
    int y_off, uv_off;
    get_matrix_coeffs(matrix, &kr, &kg, &kb, &y_off, &uv_off);

    for (j = 0; j < yuv->height; j++) {
        int uv_row = j / 2;
        for (i = 0; i < yuv->width; i++) {
            int uv_col = i / 2;
            int yi = j * yuv->y_stride + i;
            int uvi = uv_row * yuv->uv_stride + uv_col;

            float y_val = (float)(yuv->y_plane[yi] - y_off);
            float u_val = (float)(yuv->u_plane[uvi] - uv_off);
            float v_val = (float)(yuv->v_plane[uvi] - uv_off);

            float r = y_val + 1.402f * v_val;
            float g = y_val - 0.344136f * u_val - 0.714136f * v_val;
            float b = y_val + 1.772f * u_val;

            int ri = (int)(r + 0.5f);
            int gi = (int)(g + 0.5f);
            int bi = (int)(b + 0.5f);

            if (ri < 0) ri = 0; if (ri > 255) ri = 255;
            if (gi < 0) gi = 0; if (gi > 255) gi = 255;
            if (bi < 0) bi = 0; if (bi > 255) bi = 255;

            int out_idx = j * rgb->stride + i * rgb->channels;
            rgb->data[out_idx + 0] = (uint8_t)ri;
            rgb->data[out_idx + 1] = (uint8_t)gi;
            rgb->data[out_idx + 2] = (uint8_t)bi;
            if (rgb->channels == 4)
                rgb->data[out_idx + 3] = 255;
        }
    }
}

void rgb_to_yuv(const RGBFrame *rgb, YUVFrame *yuv, ColorMatrix matrix)
{
    int i, j;
    float kr, kg, kb;
    int y_off, uv_off;
    get_matrix_coeffs(matrix, &kr, &kg, &kb, &y_off, &uv_off);

    for (j = 0; j < yuv->height; j++) {
        for (i = 0; i < yuv->width; i++) {
            int in_idx = j * rgb->stride + i * rgb->channels;
            float r = (float)rgb->data[in_idx + 0];
            float g = (float)rgb->data[in_idx + 1];
            float b = (float)rgb->data[in_idx + 2];

            float y_val = kr * r + kg * g + kb * b;
            float u_val = 0.5f * (b - y_val) / (1.0f - kb) + 128.0f;
            float v_val = 0.5f * (r - y_val) / (1.0f - kr) + 128.0f;

            int yi = (int)(y_val + 0.5f);
            int ui = (int)(u_val + 0.5f);
            int vi = (int)(v_val + 0.5f);

            if (yi < 0) yi = 0; if (yi > 255) yi = 255;
            if (ui < 0) ui = 0; if (ui > 255) ui = 255;
            if (vi < 0) vi = 0; if (vi > 255) vi = 255;

            yuv->y_plane[j * yuv->y_stride + i] = (uint8_t)yi;
            if (j % 2 == 0 && i % 2 == 0) {
                int uv_idx = (j/2) * yuv->uv_stride + (i/2);
                yuv->u_plane[uv_idx] = (uint8_t)ui;
                yuv->v_plane[uv_idx] = (uint8_t)vi;
            }
        }
    }
}

void chroma_upsample_nearest(const YUVFrame *src, uint8_t *u_full, uint8_t *v_full,
                             int width, int height)
{
    int x, y;
    for (y = 0; y < height; y++) {
        int src_y = y / 2;
        for (x = 0; x < width; x++) {
            int src_x = x / 2;
            int src_idx = src_y * src->uv_stride + src_x;
            int dst_idx = y * width + x;
            u_full[dst_idx] = src->u_plane[src_idx];
            v_full[dst_idx] = src->v_plane[src_idx];
        }
    }
}

void chroma_upsample_bilinear(const YUVFrame *src, uint8_t *u_full, uint8_t *v_full,
                              int width, int height)
{
    int x, y;
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            float fx = (float)x * 0.5f - 0.25f;
            float fy = (float)y * 0.5f - 0.25f;
            int x0 = (int)fx; if (x0 < 0) x0 = 0;
            int y0 = (int)fy; if (y0 < 0) y0 = 0;
            int x1 = x0 + 1; if (x1 >= width/2) x1 = width/2 - 1;
            int y1 = y0 + 1; if (y1 >= height/2) y1 = height/2 - 1;

            float dx = fx - x0;
            float dy = fy - y0;

            int idx00 = y0 * src->uv_stride + x0;
            int idx10 = y0 * src->uv_stride + x1;
            int idx01 = y1 * src->uv_stride + x0;
            int idx11 = y1 * src->uv_stride + x1;

            float u00 = src->u_plane[idx00], u10 = src->u_plane[idx10];
            float u01 = src->u_plane[idx01], u11 = src->u_plane[idx11];
            float v00 = src->v_plane[idx00], v10 = src->v_plane[idx10];
            float v01 = src->v_plane[idx01], v11 = src->v_plane[idx11];

            float u = (1-dx)*(1-dy)*u00 + dx*(1-dy)*u10 + (1-dx)*dy*u01 + dx*dy*u11;
            float v = (1-dx)*(1-dy)*v00 + dx*(1-dy)*v10 + (1-dx)*dy*v01 + dx*dy*v11;

            int d = y * width + x;
            u_full[d] = (uint8_t)(u + 0.5f);
            v_full[d] = (uint8_t)(v + 0.5f);
        }
    }
}

void chroma_upsample_to_rgb(const YUVFrame *src, RGBFrame *rgb, ColorMatrix matrix,
                            ChromaUpsample method)
{
    int wh = src->width * src->height;
    uint8_t *u_full = (uint8_t *)malloc(wh);
    uint8_t *v_full = (uint8_t *)malloc(wh);

    if (method == CHROMA_UPSAMPLE_BILINEAR)
        chroma_upsample_bilinear(src, u_full, v_full, src->width, src->height);
    else
        chroma_upsample_nearest(src, u_full, v_full, src->width, src->height);

    float kr, kg, kb;
    int y_off, uv_off;
    get_matrix_coeffs(matrix, &kr, &kg, &kb, &y_off, &uv_off);

    int x, y;
    for (y = 0; y < src->height; y++) {
        for (x = 0; x < src->width; x++) {
            int yi = y * src->y_stride + x;
            int uvi = y * src->width + x;
            float yv = (float)(src->y_plane[yi] - y_off);
            float uv = (float)(u_full[uvi] - uv_off);
            float vv = (float)(v_full[uvi] - uv_off);

            float r = yv + 1.402f * vv;
            float g = yv - 0.344136f * uv - 0.714136f * vv;
            float b = yv + 1.772f * uv;

            int ri = (int)(r + 0.5f), gi = (int)(g + 0.5f), bi = (int)(b + 0.5f);
            if (ri < 0) ri = 0; if (ri > 255) ri = 255;
            if (gi < 0) gi = 0; if (gi > 255) gi = 255;
            if (bi < 0) bi = 0; if (bi > 255) bi = 255;

            int out_idx = y * rgb->stride + x * rgb->channels;
            rgb->data[out_idx + 0] = (uint8_t)ri;
            rgb->data[out_idx + 1] = (uint8_t)gi;
            rgb->data[out_idx + 2] = (uint8_t)bi;
            if (rgb->channels == 4)
                rgb->data[out_idx + 3] = 255;
        }
    }

    free(u_full);
    free(v_full);
}

void yuv_fill_black(YUVFrame *frame)
{
    memset(frame->y_plane, 16, frame->y_plane_size);
    memset(frame->u_plane, 128, frame->uv_plane_size);
    memset(frame->v_plane, 128, frame->uv_plane_size);
}

void yuv_fill_color(YUVFrame *frame, uint8_t y, uint8_t u, uint8_t v)
{
    memset(frame->y_plane, y, frame->y_plane_size);
    memset(frame->u_plane, u, frame->uv_plane_size);
    memset(frame->v_plane, v, frame->uv_plane_size);
}

void yuv_fill_gradient(YUVFrame *frame)
{
    int x, y;
    for (y = 0; y < frame->height; y++) {
        for (x = 0; x < frame->width; x++) {
            frame->y_plane[y * frame->y_stride + x] = (uint8_t)((x * 255) / frame->width);
            if (y % 2 == 0 && x % 2 == 0) {
                int uv_idx = (y/2) * frame->uv_stride + (x/2);
                frame->u_plane[uv_idx] = (uint8_t)((y * 255) / frame->height);
                frame->v_plane[uv_idx] = 128;
            }
        }
    }
}
