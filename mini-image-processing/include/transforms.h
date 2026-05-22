#ifndef MINI_IMAGE_TRANSFORMS_H
#define MINI_IMAGE_TRANSFORMS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "pixel_format.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── DCT / IDCT  (8×8 block) ─────────────────────────────────────────── */
#define DCT_BLOCK_SIZE 8
#define DCT_BLOCK_LEN  (DCT_BLOCK_SIZE * DCT_BLOCK_SIZE)

/* Forward DCT — input:  8×8  i16  spatial samples
                 output: 8×8  i16  DCT coefficients                */
void dct_8x8(const int16_t src[DCT_BLOCK_LEN], int16_t dst[DCT_BLOCK_LEN]);

/* Inverse DCT */
void idct_8x8(const int16_t src[DCT_BLOCK_LEN], int16_t dst[DCT_BLOCK_LEN]);

/* ── Resize ──────────────────────────────────────────────────────────── */
int resize_bilinear(const Image *src, Image *dst);
int resize_nearest(const Image *src, Image *dst);

int resize_to(const Image *src, Image *dst, int32_t new_w, int32_t new_h,
              bool use_bilinear);

/* ── Rotation ────────────────────────────────────────────────────────── */
int rotate_90(const Image *src, Image *dst);
int rotate_180(const Image *src, Image *dst);
int rotate_270(const Image *src, Image *dst);
int rotate_arbitrary(const Image *src, Image *dst, int32_t degrees);

/* ── Flip ────────────────────────────────────────────────────────────── */
int flip_horizontal(const Image *src, Image *dst);
int flip_vertical(const Image *src, Image *dst);

/* ── Crop & pad ──────────────────────────────────────────────────────── */
int crop(const Image *src, Image *dst,
         int32_t x, int32_t y);
int pad(const Image *src, Image *dst,
        int32_t left, int32_t top, int32_t right, int32_t bottom,
        uint8_t r, uint8_t g, uint8_t b);

#ifdef __cplusplus
}
#endif

#endif /* MINI_IMAGE_TRANSFORMS_H */
