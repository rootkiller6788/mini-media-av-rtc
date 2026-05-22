#ifndef MINI_IMAGE_IMAGE_FILTER_H
#define MINI_IMAGE_IMAGE_FILTER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "pixel_format.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Kernel descriptor ───────────────────────────────────────────────── */
typedef struct {
    int32_t  size;      /* kernel dimension (e.g. 3 for 3×3) */
    float   *data;      /* row-major kernel weights */
    int32_t  stride;    /* default 1 */
} Kernel2D;

/* ── Blur ────────────────────────────────────────────────────────────── */
int box_blur_apply(const Image *src, Image *dst, int32_t radius);

/* Gaussian blur — separable: horizontal pass then vertical pass */
int gaussian_blur_generate_kernel(Kernel2D *k, int32_t radius, float sigma);
int gaussian_blur_apply(const Image *src, Image *dst, int32_t radius, float sigma);

/* ── Sharpen ─────────────────────────────────────────────────────────── */
int sharpen_apply(const Image *src, Image *dst, float amount, float threshold);

/* ── Edge detection ──────────────────────────────────────────────────── */
int sobel_edge_detect(const Image *src, Image *dst);
int laplacian_edge_detect(const Image *src, Image *dst);

/* ── Point operations ────────────────────────────────────────────────── */
int brightness_contrast_adjust(const Image *src, Image *dst,
                               float brightness, float contrast);

/* Histogram equalisation — works on Grayscale8; RGB is converted internally */
int histogram_equalize(const Image *src, Image *dst);

/* ── Generic 2-D convolution ─────────────────────────────────────────── */
int convolution_2d(const Image *src, Image *dst,
                   const Kernel2D *kernel);

/* ── Kernel helpers ──────────────────────────────────────────────────── */
void kernel2d_init(Kernel2D *k, int32_t size);
void kernel2d_free(Kernel2D *k);
void kernel2d_fill(Kernel2D *k, const float *values);
void kernel2d_normalize(Kernel2D *k);

#ifdef __cplusplus
}
#endif

#endif /* MINI_IMAGE_IMAGE_FILTER_H */
