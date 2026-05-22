#include "transforms.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ── 2-D DCT  (8×8 block) ────────────────────────────────────────────── */
void dct_8x8(const int16_t src[DCT_BLOCK_LEN], int16_t dst[DCT_BLOCK_LEN])
{
    for (int32_t v = 0; v < DCT_BLOCK_SIZE; v++) {
        for (int32_t u = 0; u < DCT_BLOCK_SIZE; u++) {
            float sum = 0.0f;
            for (int32_t y = 0; y < DCT_BLOCK_SIZE; y++) {
                for (int32_t x = 0; x < DCT_BLOCK_SIZE; x++) {
                    float val = (float)src[y * DCT_BLOCK_SIZE + x];
                    sum += val
                         * (float)cos(((double)x + 0.5) * (double)u * M_PI / 8.0)
                         * (float)cos(((double)y + 0.5) * (double)v * M_PI / 8.0);
                }
            }
            float cu = (u == 0) ? (float)(1.0 / sqrt(2.0)) : 1.0f;
            float cv = (v == 0) ? (float)(1.0 / sqrt(2.0)) : 1.0f;
            sum *= cu * cv * 0.25f;
            dst[v * DCT_BLOCK_SIZE + u] = (int16_t)roundf(sum);
        }
    }
}

/* ── 2-D IDCT  (8×8 block) ───────────────────────────────────────────── */
void idct_8x8(const int16_t src[DCT_BLOCK_LEN], int16_t dst[DCT_BLOCK_LEN])
{
    for (int32_t y = 0; y < DCT_BLOCK_SIZE; y++) {
        for (int32_t x = 0; x < DCT_BLOCK_SIZE; x++) {
            float sum = 0.0f;
            for (int32_t v = 0; v < DCT_BLOCK_SIZE; v++) {
                for (int32_t u = 0; u < DCT_BLOCK_SIZE; u++) {
                    float cu = (u == 0) ? (float)(1.0 / sqrt(2.0)) : 1.0f;
                    float cv = (v == 0) ? (float)(1.0 / sqrt(2.0)) : 1.0f;
                    float coeff = (float)src[v * DCT_BLOCK_SIZE + u];
                    sum += cu * cv * coeff
                         * (float)cos(((double)x + 0.5) * (double)u * M_PI / 8.0)
                         * (float)cos(((double)y + 0.5) * (double)v * M_PI / 8.0);
                }
            }
            sum *= 0.25f;
            dst[y * DCT_BLOCK_SIZE + x] = (int16_t)roundf(sum);
        }
    }
}

/* ── Resize: nearest-neighbour ───────────────────────────────────────── */
int resize_nearest(const Image *src, Image *dst)
{
    if (!src || !dst) return -1;
    int32_t sw = src->width, sh = src->height;
    int32_t dw = dst->width, dh = dst->height;
    int32_t bpp = image_bytes_per_pixel(src->format);

    for (int32_t dy = 0; dy < dh; dy++) {
        int32_t sy = dy * sh / dh;
        if (sy >= sh) sy = sh - 1;
        for (int32_t dx = 0; dx < dw; dx++) {
            int32_t sx = dx * sw / dw;
            if (sx >= sw) sx = sw - 1;
            const uint8_t *sp = src->data + ((size_t)sy * sw + sx) * bpp;
            uint8_t       *dp = dst->data + ((size_t)dy * dw + dx) * bpp;
            for (int32_t c = 0; c < bpp; c++) dp[c] = sp[c];
        }
    }
    return 0;
}

/* ── Resize: bilinear ────────────────────────────────────────────────── */
int resize_bilinear(const Image *src, Image *dst)
{
    if (!src || !dst) return -1;
    int32_t sw = src->width, sh = src->height;
    int32_t dw = dst->width, dh = dst->height;
    int32_t bpp = image_bytes_per_pixel(src->format);

    for (int32_t dy = 0; dy < dh; dy++) {
        float fy = ((float)dy + 0.5f) * (float)sh / (float)dh - 0.5f;
        if (fy < 0.0f) fy = 0.0f;
        int32_t y0  = (int32_t)fy;
        int32_t y1  = (y0 + 1 < sh) ? y0 + 1 : y0;
        float   wy1 = fy - (float)y0;
        float   wy0 = 1.0f - wy1;

        for (int32_t dx = 0; dx < dw; dx++) {
            float fx = ((float)dx + 0.5f) * (float)sw / (float)dw - 0.5f;
            if (fx < 0.0f) fx = 0.0f;
            int32_t x0  = (int32_t)fx;
            int32_t x1  = (x0 + 1 < sw) ? x0 + 1 : x0;
            float   wx1 = fx - (float)x0;
            float   wx0 = 1.0f - wx1;

            uint8_t *dp = dst->data + ((size_t)dy * dw + dx) * bpp;
            for (int32_t c = 0; c < bpp; c++) {
                float v00 = (float)src->data[((size_t)y0 * sw + x0) * bpp + c];
                float v01 = (float)src->data[((size_t)y0 * sw + x1) * bpp + c];
                float v10 = (float)src->data[((size_t)y1 * sw + x0) * bpp + c];
                float v11 = (float)src->data[((size_t)y1 * sw + x1) * bpp + c];
                float val = wy0 * (wx0 * v00 + wx1 * v01)
                          + wy1 * (wx0 * v10 + wx1 * v11);
                if (val < 0.0f) val = 0.0f;
                if (val > 255.0f) val = 255.0f;
                dp[c] = (uint8_t)val;
            }
        }
    }
    return 0;
}

int resize_to(const Image *src, Image *dst, int32_t new_w, int32_t new_h,
              bool use_bilinear)
{
    if (!src || !dst) return -1;
    /* dst must be pre-allocated with target dimensions */
    if (dst->width != new_w || dst->height != new_h) return -1;
    return use_bilinear ? resize_bilinear(src, dst)
                        : resize_nearest(src, dst);
}

/* ── Rotation ────────────────────────────────────────────────────────── */
int rotate_90(const Image *src, Image *dst)
{
    if (!src || !dst) return -1;
    int32_t w = src->width, h = src->height;
    int32_t bpp = image_bytes_per_pixel(src->format);
    /* dst must have: dst->width == h, dst->height == w */
    for (int32_t y = 0; y < h; y++) {
        for (int32_t x = 0; x < w; x++) {
            const uint8_t *sp = src->data + ((size_t)y * w + x) * bpp;
            uint8_t *dp = dst->data + ((size_t)x * h + (h - 1 - y)) * bpp;
            for (int32_t c = 0; c < bpp; c++) dp[c] = sp[c];
        }
    }
    return 0;
}

int rotate_180(const Image *src, Image *dst)
{
    if (!src || !dst) return -1;
    int32_t w = src->width, h = src->height;
    int32_t bpp = image_bytes_per_pixel(src->format);
    for (int32_t y = 0; y < h; y++) {
        for (int32_t x = 0; x < w; x++) {
            const uint8_t *sp = src->data + ((size_t)y * w + x) * bpp;
            uint8_t *dp = dst->data + ((size_t)(h - 1 - y) * w + (w - 1 - x)) * bpp;
            for (int32_t c = 0; c < bpp; c++) dp[c] = sp[c];
        }
    }
    return 0;
}

int rotate_270(const Image *src, Image *dst)
{
    if (!src || !dst) return -1;
    int32_t w = src->width, h = src->height;
    int32_t bpp = image_bytes_per_pixel(src->format);
    for (int32_t y = 0; y < h; y++) {
        for (int32_t x = 0; x < w; x++) {
            const uint8_t *sp = src->data + ((size_t)y * w + x) * bpp;
            uint8_t *dp = dst->data + ((size_t)(w - 1 - x) * h + y) * bpp;
            for (int32_t c = 0; c < bpp; c++) dp[c] = sp[c];
        }
    }
    return 0;
}

int rotate_arbitrary(const Image *src, Image *dst, int32_t degrees)
{
    if (!src || !dst) return -1;
    switch (degrees % 360) {
    case 0:   memcpy(dst->data, src->data, src->data_size); return 0;
    case 90:  return rotate_90(src, dst);
    case 180: return rotate_180(src, dst);
    case 270: return rotate_270(src, dst);
    default:  return -1;
    }
}

/* ── Flip ────────────────────────────────────────────────────────────── */
int flip_horizontal(const Image *src, Image *dst)
{
    if (!src || !dst) return -1;
    int32_t w = src->width, h = src->height;
    int32_t bpp = image_bytes_per_pixel(src->format);
    for (int32_t y = 0; y < h; y++) {
        for (int32_t x = 0; x < w; x++) {
            const uint8_t *sp = src->data + ((size_t)y * w + x) * bpp;
            uint8_t *dp = dst->data + ((size_t)y * w + (w - 1 - x)) * bpp;
            for (int32_t c = 0; c < bpp; c++) dp[c] = sp[c];
        }
    }
    return 0;
}

int flip_vertical(const Image *src, Image *dst)
{
    if (!src || !dst) return -1;
    int32_t w = src->width, h = src->height;
    int32_t bpp = image_bytes_per_pixel(src->format);
    for (int32_t y = 0; y < h; y++) {
        for (int32_t x = 0; x < w; x++) {
            const uint8_t *sp = src->data + ((size_t)y * w + x) * bpp;
            uint8_t *dp = dst->data + ((size_t)(h - 1 - y) * w + x) * bpp;
            for (int32_t c = 0; c < bpp; c++) dp[c] = sp[c];
        }
    }
    return 0;
}

/* ── Crop ────────────────────────────────────────────────────────────── */
int crop(const Image *src, Image *dst, int32_t x, int32_t y)
{
    if (!src || !dst) return -1;
    int32_t dw = dst->width, dh = dst->height;
    int32_t sw = src->width, sh = src->height;
    int32_t bpp = image_bytes_per_pixel(src->format);

    for (int32_t dy = 0; dy < dh; dy++) {
        int32_t sy = y + dy;
        if (sy < 0 || sy >= sh) continue;
        for (int32_t dx = 0; dx < dw; dx++) {
            int32_t sx = x + dx;
            if (sx < 0 || sx >= sw) continue;
            const uint8_t *sp = src->data + ((size_t)sy * sw + sx) * bpp;
            uint8_t *dp = dst->data + ((size_t)dy * dw + dx) * bpp;
            for (int32_t c = 0; c < bpp; c++) dp[c] = sp[c];
        }
    }
    return 0;
}

/* ── Pad ─────────────────────────────────────────────────────────────── */
int pad(const Image *src, Image *dst,
        int32_t left, int32_t top, int32_t right, int32_t bottom,
        uint8_t r, uint8_t g, uint8_t b)
{
    if (!src || !dst) return -1;
    int32_t dw = dst->width, dh = dst->height;
    int32_t sw = src->width, sh = src->height;
    int32_t bpp = image_bytes_per_pixel(src->format);

    uint8_t fill[4] = {r, g, b, 0};
    /* fill entire dst first */
    for (int32_t dy = 0; dy < dh; dy++) {
        for (int32_t dx = 0; dx < dw; dx++) {
            uint8_t *dp = dst->data + ((size_t)dy * dw + dx) * bpp;
            for (int32_t c = 0; c < bpp; c++) dp[c] = fill[c];
        }
    }
    /* copy src at (left, top) */
    for (int32_t sy = 0; sy < sh; sy++) {
        int32_t dy = top + sy;
        if (dy < 0 || dy >= dh) continue;
        for (int32_t sx = 0; sx < sw; sx++) {
            int32_t dx = left + sx;
            if (dx < 0 || dx >= dw) continue;
            const uint8_t *sp = src->data + ((size_t)sy * sw + sx) * bpp;
            uint8_t *dp = dst->data + ((size_t)dy * dw + dx) * bpp;
            for (int32_t c = 0; c < bpp; c++) dp[c] = sp[c];
        }
    }
    return 0;
}
