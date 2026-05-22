#ifndef MINI_IMAGE_PIXEL_FORMAT_H
#define MINI_IMAGE_PIXEL_FORMAT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PIXEL_FORMAT_RGB24,
    PIXEL_FORMAT_RGBA32,
    PIXEL_FORMAT_YUV420P,
    PIXEL_FORMAT_YUV422,   /* YUYV packed */
    PIXEL_FORMAT_NV12,
    PIXEL_FORMAT_GRAYSCALE8,
    PIXEL_FORMAT_COUNT
} PixelFormat;

typedef enum {
    CHROMA_SUBSAMPLING_444,
    CHROMA_SUBSAMPLING_422,
    CHROMA_SUBSAMPLING_420
} ChromaSubsampling;

typedef struct {
    int32_t  width;
    int32_t  height;
    PixelFormat format;
    uint8_t *data;
    size_t   data_size;
    int32_t  stride;
} Image;

/* ── RGB ↔ YUV conversion coefficients  (BT.601) ────────────────────── */
#define YUV_KR  0.299
#define YUV_KG  0.587
#define YUV_KB  0.114

/* Forward: RGB → YUV  (Y, Cb, Cr) */
#define YUV_Y_FROM_RGB(r,g,b)  ((int32_t)(0.299*(r) + 0.587*(g) + 0.114*(b)))
#define YUV_CB_FROM_RGB(r,g,b) ((int32_t)(-0.168736*(r) - 0.331264*(g) + 0.5*(b) + 128))
#define YUV_CR_FROM_RGB(r,g,b) ((int32_t)(0.5*(r) - 0.418688*(g) - 0.081312*(b) + 128))

/* Inverse: YUV → RGB  (R, G, B) */
#define YUV_R_FROM_YUV(y,cb,cr) ((int32_t)((y) + 1.402*(int32_t)((cr)-128)))
#define YUV_G_FROM_YUV(y,cb,cr) ((int32_t)((y) - 0.344136*(int32_t)((cb)-128) - 0.714136*(int32_t)((cr)-128)))
#define YUV_B_FROM_YUV(y,cb,cr) ((int32_t)((y) + 1.772*(int32_t)((cb)-128)))

/* ── Image lifecycle ─────────────────────────────────────────────────── */
Image   image_create(int32_t width, int32_t height, PixelFormat format);
void    image_destroy(Image *img);
size_t  image_calculate_size(int32_t width, int32_t height, PixelFormat format);
int32_t image_bytes_per_pixel(PixelFormat format);
int32_t image_chroma_plane_w(int32_t luma_w, ChromaSubsampling ss);
int32_t image_chroma_plane_h(int32_t luma_h, ChromaSubsampling ss);

/* ── Colour-space conversion helpers ─────────────────────────────────── */
void clamp_rgb(int32_t *r, int32_t *g, int32_t *b);
void yuv_to_rgb(int32_t y, int32_t cb, int32_t cr, int32_t *r, int32_t *g, int32_t *b);
void rgb_to_yuv(int32_t r, int32_t g, int32_t b, int32_t *y, int32_t *cb, int32_t *cr);

/* ── Public pixel-format converters ──────────────────────────────────── */
int rgb24_to_yuv420p(const Image *src, Image *dst);
int yuv420p_to_rgb24(const Image *src, Image *dst);
int rgb24_to_rgba32(const Image *src, Image *dst);
int rgba32_to_rgb24(const Image *src, Image *dst);
int rgb24_to_grayscale(const Image *src, Image *dst);
int grayscale_to_rgb24(const Image *src, Image *dst);
int nv12_to_rgb24(const Image *src, Image *dst);
int rgb24_to_nv12(const Image *src, Image *dst);
int rgb24_to_yuv422(const Image *src, Image *dst);
int yuv422_to_rgb24(const Image *src, Image *dst);

#ifdef __cplusplus
}
#endif

#endif /* MINI_IMAGE_PIXEL_FORMAT_H */
