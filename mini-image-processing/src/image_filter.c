#include "image_filter.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ── Kernel helpers ──────────────────────────────────────────────────── */
void kernel2d_init(Kernel2D *k, int32_t size)
{
    k->size   = size;
    k->stride = 1;
    k->data   = (float *)calloc((size_t)(size * size), sizeof(float));
}

void kernel2d_free(Kernel2D *k)
{
    if (k && k->data) { free(k->data); k->data = NULL; }
}

void kernel2d_fill(Kernel2D *k, const float *values)
{
    if (!k || !values) return;
    int32_t n = k->size * k->size;
    for (int32_t i = 0; i < n; i++) k->data[i] = values[i];
}

void kernel2d_normalize(Kernel2D *k)
{
    if (!k) return;
    float sum = 0.0f;
    int32_t n = k->size * k->size;
    for (int32_t i = 0; i < n; i++) sum += k->data[i];
    if (fabsf(sum) < 1e-9f) { k->data[0] = 1.0f; return; }
    if (sum < 0.0f && fabsf(sum) < 1e-6f) return;
    for (int32_t i = 0; i < n; i++) k->data[i] /= sum;
}

/* ── Box blur ────────────────────────────────────────────────────────── */
int box_blur_apply(const Image *src, Image *dst, int32_t radius)
{
    if (!src || !dst || radius < 1) return -1;
    if (src->format != PIXEL_FORMAT_GRAYSCALE8
        && src->format != PIXEL_FORMAT_RGB24) return -1;

    int32_t w = src->width, h = src->height;
    int32_t bpp = image_bytes_per_pixel(src->format);
    int32_t d = 2 * radius + 1;

    /* horizontal pass — temporary buffer */
    uint8_t *tmp = (uint8_t *)calloc((size_t)w * h, (size_t)bpp);
    if (!tmp) return -1;

    for (int32_t y = 0; y < h; y++) {
        for (int32_t x = 0; x < w; x++) {
            int32_t sum[4] = {0};
            int32_t cnt    = 0;
            for (int32_t dx = -radius; dx <= radius; dx++) {
                int32_t sx = x + dx;
                if (sx < 0 || sx >= w) continue;
                const uint8_t *sp = src->data + ((size_t)y * w + sx) * bpp;
                for (int32_t c = 0; c < bpp; c++) sum[c] += sp[c];
                cnt++;
            }
            uint8_t *tp = tmp + ((size_t)y * w + x) * bpp;
            for (int32_t c = 0; c < bpp; c++) tp[c] = (uint8_t)(sum[c] / cnt);
        }
    }

    /* vertical pass */
    for (int32_t y = 0; y < h; y++) {
        for (int32_t x = 0; x < w; x++) {
            int32_t sum[4] = {0};
            int32_t cnt    = 0;
            for (int32_t dy = -radius; dy <= radius; dy++) {
                int32_t sy = y + dy;
                if (sy < 0 || sy >= h) continue;
                const uint8_t *sp = tmp + ((size_t)sy * w + x) * bpp;
                for (int32_t c = 0; c < bpp; c++) sum[c] += sp[c];
                cnt++;
            }
            uint8_t *dp = dst->data + ((size_t)y * w + x) * bpp;
            for (int32_t c = 0; c < bpp; c++) dp[c] = (uint8_t)(sum[c] / cnt);
        }
    }
    free(tmp);
    (void)d;
    return 0;
}

/* ── Gaussian blur ───────────────────────────────────────────────────── */
int gaussian_blur_generate_kernel(Kernel2D *k, int32_t radius, float sigma)
{
    if (!k || radius < 1) return -1;
    int32_t size = 2 * radius + 1;
    kernel2d_init(k, size);
    if (!k->data) return -1;

    float s2 = 2.0f * sigma * sigma;
    float sum = 0.0f;
    for (int32_t y = -radius; y <= radius; y++) {
        for (int32_t x = -radius; x <= radius; x++) {
            int32_t idx = (y + radius) * size + (x + radius);
            float val = (float)exp((double)(-(x * x + y * y) / s2));
            k->data[idx] = val;
            sum += val;
        }
    }
    for (int32_t i = 0; i < size * size; i++) k->data[i] /= sum;
    k->stride = 1;
    return 0;
}

int gaussian_blur_apply(const Image *src, Image *dst, int32_t radius, float sigma)
{
    if (!src || !dst || radius < 1) return -1;
    Kernel2D k;
    if (gaussian_blur_generate_kernel(&k, radius, sigma) != 0) return -1;
    int ret = convolution_2d(src, dst, &k);
    kernel2d_free(&k);
    return ret;
}

/* ── Sharpen  (unsharp mask) ─────────────────────────────────────────── */
int sharpen_apply(const Image *src, Image *dst, float amount, float threshold)
{
    if (!src || !dst) return -1;
    if (src->format != PIXEL_FORMAT_RGB24
        && src->format != PIXEL_FORMAT_GRAYSCALE8) return -1;

    int32_t w = src->width, h = src->height;
    int32_t bpp = image_bytes_per_pixel(src->format);

    /* create blurred copy */
    Image blur = image_create(w, h, src->format);
    if (!blur.data) return -1;
    gaussian_blur_apply(src, &blur, 2, 1.4f);

    for (int32_t y = 0; y < h; y++) {
        for (int32_t x = 0; x < w; x++) {
            uint8_t *dp = dst->data + ((size_t)y * w + x) * bpp;
            const uint8_t *sp = src->data + ((size_t)y * w + x) * bpp;
            const uint8_t *bp = blur.data + ((size_t)y * w + x) * bpp;
            for (int32_t c = 0; c < bpp; c++) {
                float diff = (float)sp[c] - (float)bp[c];
                if (fabsf(diff) < threshold) { dp[c] = sp[c]; continue; }
                float val  = (float)sp[c] + amount * diff;
                if (val < 0.0f) val = 0.0f;
                if (val > 255.0f) val = 255.0f;
                dp[c] = (uint8_t)val;
            }
        }
    }
    image_destroy(&blur);
    return 0;
}

/* ── Sobel edge detection ────────────────────────────────────────────── */
int sobel_edge_detect(const Image *src, Image *dst)
{
    if (!src || !dst) return -1;
    if (src->format != PIXEL_FORMAT_GRAYSCALE8) return -1;

    int32_t w = src->width, h = src->height;
    const int32_t gx[3][3] = {{-1,0,1},{-2,0,2},{-1,0,1}};
    const int32_t gy[3][3] = {{-1,-2,-1},{0,0,0},{1,2,1}};

    for (int32_t y = 1; y < h - 1; y++) {
        for (int32_t x = 1; x < w - 1; x++) {
            int32_t sx = 0, sy = 0;
            for (int32_t dy = -1; dy <= 1; dy++) {
                for (int32_t dx = -1; dx <= 1; dx++) {
                    int32_t val = (int32_t)src->data[((size_t)(y+dy))*w + (x+dx)];
                    sx += val * gx[dy+1][dx+1];
                    sy += val * gy[dy+1][dx+1];
                }
            }
            int32_t mag = (int32_t)sqrt((double)(sx*sx + sy*sy));
            if (mag > 255) mag = 255;
            dst->data[(size_t)y * w + x] = (uint8_t)mag;
        }
    }
    return 0;
}

/* ── Laplacian edge detection ────────────────────────────────────────── */
int laplacian_edge_detect(const Image *src, Image *dst)
{
    if (!src || !dst) return -1;
    if (src->format != PIXEL_FORMAT_GRAYSCALE8) return -1;

    int32_t w = src->width, h = src->height;
    const int32_t lap[3][3] = {{0,-1,0},{-1,4,-1},{0,-1,0}};

    for (int32_t y = 1; y < h - 1; y++) {
        for (int32_t x = 1; x < w - 1; x++) {
            int32_t sum = 0;
            for (int32_t dy = -1; dy <= 1; dy++) {
                for (int32_t dx = -1; dx <= 1; dx++) {
                    int32_t val = (int32_t)src->data[((size_t)(y+dy))*w + (x+dx)];
                    sum += val * lap[dy+1][dx+1];
                }
            }
            if (sum < 0) sum = 0;
            if (sum > 255) sum = 255;
            dst->data[(size_t)y * w + x] = (uint8_t)sum;
        }
    }
    return 0;
}

/* ── Brightness / contrast ───────────────────────────────────────────── */
int brightness_contrast_adjust(const Image *src, Image *dst,
                               float brightness, float contrast)
{
    if (!src || !dst) return -1;
    int32_t w = src->width, h = src->height;
    int32_t bpp = image_bytes_per_pixel(src->format);

    for (int32_t y = 0; y < h; y++) {
        for (int32_t x = 0; x < w; x++) {
            const uint8_t *sp = src->data + ((size_t)y * w + x) * bpp;
            uint8_t       *dp = dst->data + ((size_t)y * w + x) * bpp;
            for (int32_t c = 0; c < bpp; c++) {
                float val = (float)sp[c];
                val = val * contrast + brightness;
                if (val < 0.0f) val = 0.0f;
                if (val > 255.0f) val = 255.0f;
                dp[c] = (uint8_t)val;
            }
        }
    }
    return 0;
}

/* ── Histogram equalisation ──────────────────────────────────────────── */
int histogram_equalize(const Image *src, Image *dst)
{
    if (!src || !dst) return -1;
    int32_t w = src->width, h = src->height;

    /* work on RGB24 by doing grayscale conversion internally */
    Image gray_src = image_create(w, h, PIXEL_FORMAT_GRAYSCALE8);
    Image gray_dst = image_create(w, h, PIXEL_FORMAT_GRAYSCALE8);
    if (!gray_src.data || !gray_dst.data) {
        image_destroy(&gray_src); image_destroy(&gray_dst);
        return -1;
    }
    if (src->format == PIXEL_FORMAT_RGB24)
        rgb24_to_grayscale(src, &gray_src);
    else
        memcpy(gray_src.data, src->data, src->data_size);

    /* build histogram */
    int32_t hist[256] = {0};
    for (int32_t i = 0; i < w * h; i++)
        hist[gray_src.data[i]]++;

    /* cumulative distribution */
    int32_t cdf[256];
    cdf[0] = hist[0];
    for (int32_t i = 1; i < 256; i++)
        cdf[i] = cdf[i - 1] + hist[i];

    int32_t min_cdf = 0;
    for (int32_t i = 0; i < 256; i++) {
        if (cdf[i] > 0) { min_cdf = cdf[i]; break; }
    }
    int32_t total = w * h;

    for (int32_t i = 0; i < w * h; i++) {
        int32_t v = (int32_t)(((float)(cdf[gray_src.data[i]] - min_cdf)
                     / (float)(total - min_cdf)) * 255.0f);
        if (v < 0) v = 0; if (v > 255) v = 255;
        gray_dst.data[i] = (uint8_t)v;
    }
    if (dst->format == PIXEL_FORMAT_RGB24)
        grayscale_to_rgb24(&gray_dst, dst);
    else
        memcpy(dst->data, gray_dst.data, gray_dst.data_size);

    image_destroy(&gray_src);
    image_destroy(&gray_dst);
    return 0;
}

/* ── Generic 2-D convolution ─────────────────────────────────────────── */
int convolution_2d(const Image *src, Image *dst, const Kernel2D *kernel)
{
    if (!src || !dst || !kernel || !kernel->data) return -1;
    int32_t w = src->width, h = src->height;
    int32_t bpp = image_bytes_per_pixel(src->format);

    int32_t ksz = kernel->size;
    int32_t half = ksz / 2;

    for (int32_t y = 0; y < h; y++) {
        for (int32_t x = 0; x < w; x++) {
            float acc[4] = {0.0f};
            for (int32_t ky = 0; ky < ksz; ky++) {
                for (int32_t kx = 0; kx < ksz; kx++) {
                    int32_t sx = x + kx - half;
                    int32_t sy = y + ky - half;
                    if (sx < 0) sx = 0;
                    if (sx >= w) sx = w - 1;
                    if (sy < 0) sy = 0;
                    if (sy >= h) sy = h - 1;
                    const uint8_t *sp = src->data + ((size_t)sy * w + sx) * bpp;
                    float kw = kernel->data[ky * ksz + kx];
                    for (int32_t c = 0; c < bpp; c++)
                        acc[c] += (float)sp[c] * kw;
                }
            }
            uint8_t *dp = dst->data + ((size_t)y * w + x) * bpp;
            for (int32_t c = 0; c < bpp; c++) {
                if (acc[c] < 0.0f) acc[c] = 0.0f;
                if (acc[c] > 255.0f) acc[c] = 255.0f;
                dp[c] = (uint8_t)acc[c];
            }
        }
    }
    return 0;
}
