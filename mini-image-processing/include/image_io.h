#ifndef MINI_IMAGE_IMAGE_IO_H
#define MINI_IMAGE_IMAGE_IO_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include "pixel_format.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── BMP ─────────────────────────────────────────────────────────────── */
#define BMP_MAGIC    0x4D42   /* 'B' 'M' */
#define BMP_BI_RGB   0
#define BMP_BI_BITFIELDS 3

#pragma pack(push, 1)
typedef struct {
    uint16_t bf_type;
    uint32_t bf_size;
    uint16_t bf_reserved1;
    uint16_t bf_reserved2;
    uint32_t bf_off_bits;
} BmpFileHeader;

typedef struct {
    uint32_t bi_size;
    int32_t  bi_width;
    int32_t  bi_height;       /* >0 = bottom-up */
    uint16_t bi_planes;
    uint16_t bi_bit_count;
    uint32_t bi_compression;
    uint32_t bi_size_image;
    int32_t  bi_x_pels_per_meter;
    int32_t  bi_y_pels_per_meter;
    uint32_t bi_clr_used;
    uint32_t bi_clr_important;
} BmpInfoHeader;
#pragma pack(pop)

int bmp_read_file(const char *path, Image *img);
int bmp_write_file(const char *path, const Image *img);
int bmp_read_memory(const uint8_t *data, size_t size, Image *img);

/* ── PPM / PGM ───────────────────────────────────────────────────────── */
int ppm_read_file(const char *path, Image *img);
int ppm_write_file(const char *path, const Image *img);
int pgm_read_file(const char *path, Image *img);
int pgm_write_file(const char *path, const Image *img);

/* ── simple PNG  (deflate decompressed, IDAT concatenated) ───────────── */
#define PNG_SIG_SIZE 8
extern const uint8_t PNG_SIGNATURE[PNG_SIG_SIZE];

int png_read_file(const char *path, Image *img);
int png_write_file(const char *path, const Image *img);

/* ── Raw pixel buffer  (memory-only image) ───────────────────────────── */
Image raw_pixel_buffer_create(int32_t width, int32_t height, PixelFormat format);
void  raw_pixel_buffer_free(Image *img);
int   raw_pixel_buffer_read(const uint8_t *data, size_t size,
                            int32_t width, int32_t height,
                            PixelFormat format, Image *img);
int   raw_pixel_buffer_write(const Image *img, uint8_t *buf, size_t buf_size);

#ifdef __cplusplus
}
#endif

#endif /* MINI_IMAGE_IMAGE_IO_H */
