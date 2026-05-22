#include "mini_image/pixel_format.h"
#include "mini_image/image_io.h"
#include "mini_image/image_filter.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int create_test_bmp(const char *path)
{
    Image img = image_create(4, 4, PIXEL_FORMAT_RGB24);
    if (!img.data) return -1;

    /* simple red gradient */
    for (int32_t y = 0; y < 4; y++) {
        for (int32_t x = 0; x < 4; x++) {
            uint8_t *p = img.data + ((size_t)y * 4 + x) * 3;
            p[0] = (uint8_t)((x + 1) * 64);
            p[1] = (uint8_t)(y * 64);
            p[2] = 0;
        }
    }
    int ret = bmp_write_file(path, &img);
    image_destroy(&img);
    return ret;
}

static int create_test_ppm(const char *path)
{
    Image img = image_create(4, 4, PIXEL_FORMAT_RGB24);
    if (!img.data) return -1;

    for (int32_t y = 0; y < 4; y++) {
        for (int32_t x = 0; x < 4; x++) {
            uint8_t *p = img.data + ((size_t)y * 4 + x) * 3;
            p[0] = 0;
            p[1] = (uint8_t)((x + 1) * 64);
            p[2] = (uint8_t)(y * 64);
        }
    }
    int ret = ppm_write_file(path, &img);
    image_destroy(&img);
    return ret;
}

int main(void)
{
    printf("=== Image I/O Demo ===\n\n");

    /* ── 1. BMP write / read ──────────────────────────────────────── */
    printf("--- BMP ---\n");
    {
        const char *bmp_path = "_test_bmp_write.bmp";
        if (create_test_bmp(bmp_path) == 0) {
            printf("[OK] BMP file written: %s\n", bmp_path);

            Image img;
            memset(&img, 0, sizeof(img));
            if (bmp_read_file(bmp_path, &img) == 0) {
                printf("  Read back: %dx%d, format=%d, data_size=%zu\n",
                       img.width, img.height, img.format, img.data_size);
                printf("  Pixel(0,0): R=%u G=%u B=%u\n",
                       img.data[0], img.data[1], img.data[2]);
                image_destroy(&img);
            } else {
                printf("[FAIL] BMP read\n");
            }
        } else {
            printf("[FAIL] BMP write\n");
        }
    }

    /* ── 2. PPM write / read ──────────────────────────────────────── */
    printf("\n--- PPM (P6 binary) ---\n");
    {
        const char *ppm_path = "_test_ppm_write.ppm";
        if (create_test_ppm(ppm_path) == 0) {
            printf("[OK] PPM file written: %s\n", ppm_path);

            Image img;
            memset(&img, 0, sizeof(img));
            if (ppm_read_file(ppm_path, &img) == 0) {
                printf("  Read back: %dx%d, format=%d, data_size=%zu\n",
                       img.width, img.height, img.format, img.data_size);
                image_destroy(&img);
            } else {
                printf("[FAIL] PPM read\n");
            }
        } else {
            printf("[FAIL] PPM write\n");
        }
    }

    /* ── 3. PGM write / read ──────────────────────────────────────── */
    printf("\n--- PGM (P5 binary, grayscale) ---\n");
    {
        Image gray = image_create(8, 8, PIXEL_FORMAT_GRAYSCALE8);
        for (int32_t y = 0; y < 8; y++)
            for (int32_t x = 0; x < 8; x++)
                gray.data[(size_t)y * 8 + x] = (uint8_t)((y + x) * 16);

        const char *pgm_path = "_test_pgm_write.pgm";
        if (pgm_write_file(pgm_path, &gray) == 0) {
            printf("[OK] PGM file written: %s\n", pgm_path);

            Image img;
            memset(&img, 0, sizeof(img));
            if (pgm_read_file(pgm_path, &img) == 0) {
                printf("  Read back: %dx%d, format=%d, pixel[0]=%u\n",
                       img.width, img.height, img.format, img.data[0]);
                image_destroy(&img);
            } else {
                printf("[FAIL] PGM read\n");
            }
        } else {
            printf("[FAIL] PGM write\n");
        }
        image_destroy(&gray);
    }

    /* ── 4. BMP read from memory ──────────────────────────────────── */
    printf("\n--- BMP read from memory ---\n");
    {
        const char *bmp_path = "_test_bmp_write.bmp";
        FILE *f = fopen(bmp_path, "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            long fsize = ftell(f);
            fseek(f, 0, SEEK_SET);
            uint8_t *buf = (uint8_t *)malloc((size_t)fsize);
            if (buf && fread(buf, 1, (size_t)fsize, f) == (size_t)fsize) {
                Image img;
                memset(&img, 0, sizeof(img));
                if (bmp_read_memory(buf, (size_t)fsize, &img) == 0) {
                    printf("[OK] BMP read from memory: %dx%d\n",
                           img.width, img.height);
                    image_destroy(&img);
                } else {
                    printf("[FAIL] BMP memory read\n");
                }
            }
            free(buf);
            fclose(f);
        }
    }

    /* ── 5. PNG write / read ──────────────────────────────────────── */
    printf("\n--- Simple PNG ---\n");
    {
        Image img = image_create(16, 16, PIXEL_FORMAT_RGB24);
        for (int32_t y = 0; y < 16; y++)
            for (int32_t x = 0; x < 16; x++) {
                uint8_t *p = img.data + ((size_t)y * 16 + x) * 3;
                p[0] = (uint8_t)(x * 16);
                p[1] = (uint8_t)(y * 16);
                p[2] = 128;
            }

        const char *png_path = "_test_png_write.png";
        if (png_write_file(png_path, &img) == 0) {
            printf("[OK] PNG file written: %s\n", png_path);

            Image read_img;
            memset(&read_img, 0, sizeof(read_img));
            if (png_read_file(png_path, &read_img) == 0) {
                printf("  Read back: %dx%d, format=%d (header only)\n",
                       read_img.width, read_img.height, read_img.format);
                image_destroy(&read_img);
            } else {
                printf("[FAIL] PNG read\n");
            }
        } else {
            printf("[FAIL] PNG write\n");
        }
        image_destroy(&img);
    }

    /* ── 6. Raw pixel buffer ──────────────────────────────────────── */
    printf("\n--- Raw Pixel Buffer ---\n");
    {
        int32_t w = 32, h = 32;
        Image raw = raw_pixel_buffer_create(w, h, PIXEL_FORMAT_RGBA32);
        /* fill with simple pattern */
        for (int32_t y = 0; y < h; y++)
            for (int32_t x = 0; x < w; x++) {
                uint8_t *p = raw.data + ((size_t)y * w + x) * 4;
                p[0] = (uint8_t)x;
                p[1] = (uint8_t)y;
                p[2] = 0;
                p[3] = 255;
            }

        size_t buf_sz = raw.data_size;
        uint8_t *copy_buf = (uint8_t *)malloc(buf_sz);
        if (raw_pixel_buffer_write(&raw, copy_buf, buf_sz) == 0) {
            printf("[OK] Raw buffer write: %zu bytes\n", buf_sz);
        }
        free(copy_buf);

        Image raw2;
        memset(&raw2, 0, sizeof(raw2));
        if (raw_pixel_buffer_read(raw.data, raw.data_size,
                                   w, h, PIXEL_FORMAT_RGBA32, &raw2) == 0) {
            printf("  Raw buffer read: %dx%d\n", raw2.width, raw2.height);
            image_destroy(&raw2);
        }
        raw_pixel_buffer_free(&raw);
    }

    printf("\nAll Image I/O examples completed.\n");
    return 0;
}
