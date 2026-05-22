#include "pixel_format.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

Image image_create(int32_t width, int32_t height, PixelFormat format)
{
    Image img;
    img.width  = width;
    img.height = height;
    img.format = format;
    img.stride = width * image_bytes_per_pixel(format);
    img.data_size = image_calculate_size(width, height, format);

    if (format == PIXEL_FORMAT_YUV420P) {
        img.data_size = (size_t)width * height
                      + (size_t)((width + 1) / 2) * ((height + 1) / 2) * 2;
    } else if (format == PIXEL_FORMAT_NV12) {
        img.data_size = (size_t)width * height
                      + (size_t)((width + 1) / 2) * ((height + 1) / 2) * 2;
    } else if (format == PIXEL_FORMAT_YUV422) {
        img.data_size = (size_t)width * height * 2;
    }

    img.data = (uint8_t *)calloc(1, img.data_size);
    return img;
}

void image_destroy(Image *img)
{
    if (img && img->data) {
        free(img->data);
        img->data = NULL;
        img->data_size = 0;
    }
}

size_t image_calculate_size(int32_t width, int32_t height, PixelFormat format)
{
    switch (format) {
    case PIXEL_FORMAT_RGB24:       return (size_t)(width * height * 3);
    case PIXEL_FORMAT_RGBA32:      return (size_t)(width * height * 4);
    case PIXEL_FORMAT_GRAYSCALE8:  return (size_t)(width * height);
    case PIXEL_FORMAT_YUV422:      return (size_t)(width * height * 2);
    case PIXEL_FORMAT_YUV420P:     return (size_t)width * height
                                         + (size_t)((width + 1) / 2) * ((height + 1) / 2) * 2;
    case PIXEL_FORMAT_NV12:        return (size_t)width * height
                                         + (size_t)((width + 1) / 2) * ((height + 1) / 2) * 2;
    default:                        return 0;
    }
}

int32_t image_bytes_per_pixel(PixelFormat format)
{
    switch (format) {
    case PIXEL_FORMAT_RGB24:       return 3;
    case PIXEL_FORMAT_RGBA32:      return 4;
    case PIXEL_FORMAT_GRAYSCALE8:  return 1;
    case PIXEL_FORMAT_YUV422:      return 2;
    default:                        return 0;
    }
}

int32_t image_chroma_plane_w(int32_t luma_w, ChromaSubsampling ss)
{
    switch (ss) {
    case CHROMA_SUBSAMPLING_444: return luma_w;
    case CHROMA_SUBSAMPLING_422: return (luma_w + 1) / 2;
    case CHROMA_SUBSAMPLING_420: return (luma_w + 1) / 2;
    default:                     return luma_w;
    }
}

int32_t image_chroma_plane_h(int32_t luma_h, ChromaSubsampling ss)
{
    switch (ss) {
    case CHROMA_SUBSAMPLING_444: return luma_h;
    case CHROMA_SUBSAMPLING_422: return luma_h;
    case CHROMA_SUBSAMPLING_420: return (luma_h + 1) / 2;
    default:                     return luma_h;
    }
}

void clamp_rgb(int32_t *r, int32_t *g, int32_t *b)
{
    if (*r < 0) *r = 0; if (*r > 255) *r = 255;
    if (*g < 0) *g = 0; if (*g > 255) *g = 255;
    if (*b < 0) *b = 0; if (*b > 255) *b = 255;
}

void yuv_to_rgb(int32_t y, int32_t cb, int32_t cr, int32_t *r, int32_t *g, int32_t *b)
{
    *r = YUV_R_FROM_YUV(y, cb, cr);
    *g = YUV_G_FROM_YUV(y, cb, cr);
    *b = YUV_B_FROM_YUV(y, cb, cr);
    clamp_rgb(r, g, b);
}

void rgb_to_yuv(int32_t r, int32_t g, int32_t b, int32_t *y, int32_t *cb, int32_t *cr)
{
    *y  = YUV_Y_FROM_RGB(r, g, b);
    *cb = YUV_CB_FROM_RGB(r, g, b);
    *cr = YUV_CR_FROM_RGB(r, g, b);
    if (*y  < 0) *y  = 0; if (*y  > 255) *y  = 255;
    if (*cb < 0) *cb = 0; if (*cb > 255) *cb = 255;
    if (*cr < 0) *cr = 0; if (*cr > 255) *cr = 255;
}

/* ── RGB24 → YUV420P ─────────────────────────────────────────────────── */
int rgb24_to_yuv420p(const Image *src, Image *dst)
{
    if (!src || !dst || src->format != PIXEL_FORMAT_RGB24
        || dst->format != PIXEL_FORMAT_YUV420P) return -1;

    int32_t w = src->width, h = src->height;
    int32_t cw = (w + 1) / 2, ch = (h + 1) / 2;
    uint8_t *y_plane  = dst->data;
    uint8_t *cb_plane = y_plane + (size_t)w * h;
    uint8_t *cr_plane = cb_plane + (size_t)cw * ch;

    for (int32_t row = 0; row < h; row++) {
        for (int32_t col = 0; col < w; col++) {
            const uint8_t *pix = src->data + ((size_t)row * w + col) * 3;
            int32_t r = pix[0], g = pix[1], b = pix[2];
            int32_t yv, cbv, crv;
            rgb_to_yuv(r, g, b, &yv, &cbv, &crv);

            y_plane[(size_t)row * w + col] = (uint8_t)yv;
            /* 4:2:0 — average 2×2 block */
            if ((row & 1) == 0 && (col & 1) == 0) {
                int32_t idx = ((size_t)row / 2) * cw + col / 2;
                cb_plane[idx] = (uint8_t)cbv;
                cr_plane[idx] = (uint8_t)crv;
            }
        }
    }
    return 0;
}

/* ── YUV420P → RGB24 ─────────────────────────────────────────────────── */
int yuv420p_to_rgb24(const Image *src, Image *dst)
{
    if (!src || !dst || src->format != PIXEL_FORMAT_YUV420P
        || dst->format != PIXEL_FORMAT_RGB24) return -1;

    int32_t w = src->width, h = src->height;
    int32_t cw = (w + 1) / 2, ch = (h + 1) / 2;
    const uint8_t *y_plane  = src->data;
    const uint8_t *cb_plane = y_plane + (size_t)w * h;
    const uint8_t *cr_plane = cb_plane + (size_t)cw * ch;

    for (int32_t row = 0; row < h; row++) {
        for (int32_t col = 0; col < w; col++) {
            int32_t idx = ((size_t)row / 2) * cw + col / 2;
            int32_t yv = y_plane[(size_t)row * w + col];
            int32_t cbv = cb_plane[idx];
            int32_t crv = cr_plane[idx];
            int32_t r, g, b_val;
            yuv_to_rgb(yv, cbv, crv, &r, &g, &b_val);

            uint8_t *pix = dst->data + ((size_t)row * w + col) * 3;
            pix[0] = (uint8_t)r;
            pix[1] = (uint8_t)g;
            pix[2] = (uint8_t)b_val;
        }
    }
    return 0;
}

int rgb24_to_rgba32(const Image *src, Image *dst)
{
    if (!src || !dst || src->format != PIXEL_FORMAT_RGB24
        || dst->format != PIXEL_FORMAT_RGBA32) return -1;

    int32_t w = src->width, h = src->height;
    for (int32_t row = 0; row < h; row++) {
        for (int32_t col = 0; col < w; col++) {
            const uint8_t *s = src->data + ((size_t)row * w + col) * 3;
            uint8_t *d = dst->data + ((size_t)row * w + col) * 4;
            d[0] = s[0]; d[1] = s[1]; d[2] = s[2]; d[3] = 255;
        }
    }
    return 0;
}

int rgba32_to_rgb24(const Image *src, Image *dst)
{
    if (!src || !dst || src->format != PIXEL_FORMAT_RGBA32
        || dst->format != PIXEL_FORMAT_RGB24) return -1;

    int32_t w = src->width, h = src->height;
    for (int32_t row = 0; row < h; row++) {
        for (int32_t col = 0; col < w; col++) {
            const uint8_t *s = src->data + ((size_t)row * w + col) * 4;
            uint8_t *d = dst->data + ((size_t)row * w + col) * 3;
            d[0] = s[0]; d[1] = s[1]; d[2] = s[2];
        }
    }
    return 0;
}

int rgb24_to_grayscale(const Image *src, Image *dst)
{
    if (!src || !dst || src->format != PIXEL_FORMAT_RGB24
        || dst->format != PIXEL_FORMAT_GRAYSCALE8) return -1;

    int32_t w = src->width, h = src->height;
    for (int32_t row = 0; row < h; row++) {
        for (int32_t col = 0; col < w; col++) {
            const uint8_t *pix = src->data + ((size_t)row * w + col) * 3;
            int32_t r = pix[0], g = pix[1], b_val = pix[2];
            uint8_t gy = (uint8_t)YUV_Y_FROM_RGB(r, g, b_val);
            dst->data[(size_t)row * w + col] = gy;
        }
    }
    return 0;
}

int grayscale_to_rgb24(const Image *src, Image *dst)
{
    if (!src || !dst || src->format != PIXEL_FORMAT_GRAYSCALE8
        || dst->format != PIXEL_FORMAT_RGB24) return -1;

    int32_t w = src->width, h = src->height;
    for (int32_t row = 0; row < h; row++) {
        for (int32_t col = 0; col < w; col++) {
            uint8_t gy = src->data[(size_t)row * w + col];
            uint8_t *pix = dst->data + ((size_t)row * w + col) * 3;
            pix[0] = gy; pix[1] = gy; pix[2] = gy;
        }
    }
    return 0;
}

int nv12_to_rgb24(const Image *src, Image *dst)
{
    if (!src || !dst || src->format != PIXEL_FORMAT_NV12
        || dst->format != PIXEL_FORMAT_RGB24) return -1;

    int32_t w = src->width, h = src->height;
    int32_t cw = (w + 1) / 2, ch = (h + 1) / 2;
    const uint8_t *y_plane  = src->data;
    const uint8_t *uv_plane = y_plane + (size_t)w * h;

    for (int32_t row = 0; row < h; row++) {
        for (int32_t col = 0; col < w; col++) {
            int32_t uv_idx = ((size_t)row / 2) * cw * 2 + (col / 2) * 2;
            int32_t yv  = y_plane[(size_t)row * w + col];
            int32_t cbv = uv_plane[uv_idx];
            int32_t crv = uv_plane[uv_idx + 1];
            int32_t r, g, b_val;
            yuv_to_rgb(yv, cbv, crv, &r, &g, &b_val);

            uint8_t *pix = dst->data + ((size_t)row * w + col) * 3;
            pix[0] = (uint8_t)r;
            pix[1] = (uint8_t)g;
            pix[2] = (uint8_t)b_val;
        }
    }
    return 0;
}

int rgb24_to_nv12(const Image *src, Image *dst)
{
    if (!src || !dst || src->format != PIXEL_FORMAT_RGB24
        || dst->format != PIXEL_FORMAT_NV12) return -1;

    int32_t w = src->width, h = src->height;
    int32_t cw = (w + 1) / 2, ch = (h + 1) / 2;
    uint8_t *y_plane  = dst->data;
    uint8_t *uv_plane = y_plane + (size_t)w * h;

    for (int32_t row = 0; row < h; row++) {
        for (int32_t col = 0; col < w; col++) {
            const uint8_t *pix = src->data + ((size_t)row * w + col) * 3;
            int32_t yv, cbv, crv;
            rgb_to_yuv(pix[0], pix[1], pix[2], &yv, &cbv, &crv);
            y_plane[(size_t)row * w + col] = (uint8_t)yv;

            if ((row & 1) == 0 && (col & 1) == 0) {
                int32_t uv_idx = ((size_t)row / 2) * cw * 2 + (col / 2) * 2;
                uv_plane[uv_idx]     = (uint8_t)cbv;
                uv_plane[uv_idx + 1] = (uint8_t)crv;
            }
        }
    }
    (void)ch;
    return 0;
}

int rgb24_to_yuv422(const Image *src, Image *dst)
{
    if (!src || !dst || src->format != PIXEL_FORMAT_RGB24
        || dst->format != PIXEL_FORMAT_YUV422) return -1;

    int32_t w = src->width, h = src->height;
    for (int32_t row = 0; row < h; row++) {
        for (int32_t col = 0; col < w; col += 2) {
            /* YUYV packed: Y0 U01 Y1 V01 */
            const uint8_t *p0 = src->data + ((size_t)row * w + col) * 3;
            const uint8_t *p1 = src->data + ((size_t)row * w + col + 1) * 3;
            int32_t y0, cb0, cr0, y1, cb1, cr1;
            rgb_to_yuv(p0[0], p0[1], p0[2], &y0, &cb0, &cr0);
            rgb_to_yuv(p1[0], p1[1], p1[2], &y1, &cb1, &cr1);

            uint8_t *d = dst->data + ((size_t)row * w + col) * 2;
            d[0] = (uint8_t)y0;
            d[1] = (uint8_t)((cb0 + cb1) / 2);
            d[2] = (uint8_t)y1;
            d[3] = (uint8_t)((cr0 + cr1) / 2);
        }
    }
    return 0;
}

int yuv422_to_rgb24(const Image *src, Image *dst)
{
    if (!src || !dst || src->format != PIXEL_FORMAT_YUV422
        || dst->format != PIXEL_FORMAT_RGB24) return -1;

    int32_t w = src->width, h = src->height;
    for (int32_t row = 0; row < h; row++) {
        for (int32_t col = 0; col < w; col += 2) {
            const uint8_t *s = src->data + ((size_t)row * w + col) * 2;
            int32_t y0 = s[0], cb = s[1], y1 = s[2], cr = s[3];
            int32_t r0, g0, b0, r1, g1, b1;
            yuv_to_rgb(y0, cb, cr, &r0, &g0, &b0);
            yuv_to_rgb(y1, cb, cr, &r1, &g1, &b1);

            uint8_t *d0 = dst->data + ((size_t)row * w + col) * 3;
            uint8_t *d1 = dst->data + ((size_t)row * w + col + 1) * 3;
            d0[0] = (uint8_t)r0; d0[1] = (uint8_t)g0; d0[2] = (uint8_t)b0;
            d1[0] = (uint8_t)r1; d1[1] = (uint8_t)g1; d1[2] = (uint8_t)b1;
        }
    }
    return 0;
}
