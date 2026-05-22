#include "image_io.h"
#include <stdlib.h>
#include <string.h>

const uint8_t PNG_SIGNATURE[PNG_SIG_SIZE] = {
    0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A
};

/* ── BMP read ────────────────────────────────────────────────────────── */
int bmp_read_file(const char *path, Image *img)
{
    if (!path || !img) return -1;
    FILE *f = fopen(path, "rb");
    if (!f) return -1;

    BmpFileHeader fh;
    BmpInfoHeader ih;
    if (fread(&fh, sizeof(fh), 1, f) != 1 || fh.bf_type != BMP_MAGIC) {
        fclose(f); return -1;
    }
    if (fread(&ih, sizeof(ih), 1, f) != 1 || ih.bi_compression != BMP_BI_RGB) {
        fclose(f); return -1;
    }

    int32_t w = ih.bi_width, h = (ih.bi_height > 0) ? ih.bi_height : -ih.bi_height;
    img->width  = w;
    img->height = h;
    img->format = PIXEL_FORMAT_RGB24;
    img->data_size = (size_t)w * h * 3;
    img->stride   = w * 3;
    img->data     = (uint8_t *)malloc(img->data_size);
    if (!img->data) { fclose(f); return -1; }

    /* BMP rows are padded to 4-byte boundary */
    int32_t row_pad = (4 - (w * 3) % 4) % 4;

    fseek(f, fh.bf_off_bits, SEEK_SET);
    for (int32_t y = h - 1; y >= 0; y--) {
        size_t off = (size_t)y * w * 3;
        if (fread(img->data + off, 3, (size_t)w, f) != (size_t)w) {
            free(img->data); fclose(f); return -1;
        }
        fseek(f, row_pad, SEEK_CUR);
    }
    fclose(f);
    return 0;
}

/* ── BMP write ───────────────────────────────────────────────────────── */
int bmp_write_file(const char *path, const Image *img)
{
    if (!path || !img || img->format != PIXEL_FORMAT_RGB24) return -1;
    FILE *f = fopen(path, "wb");
    if (!f) return -1;

    int32_t w = img->width, h = img->height;
    int32_t row_pad = (4 - (w * 3) % 4) % 4;
    uint32_t data_size = (uint32_t)(w * 3 + row_pad) * (uint32_t)h;
    uint32_t file_size = data_size + sizeof(BmpFileHeader) + sizeof(BmpInfoHeader);

    BmpFileHeader fh;
    memset(&fh, 0, sizeof(fh));
    fh.bf_type    = BMP_MAGIC;
    fh.bf_size    = file_size;
    fh.bf_off_bits = sizeof(BmpFileHeader) + sizeof(BmpInfoHeader);

    BmpInfoHeader ih;
    memset(&ih, 0, sizeof(ih));
    ih.bi_size        = sizeof(BmpInfoHeader);
    ih.bi_width       = w;
    ih.bi_height      = h;
    ih.bi_planes      = 1;
    ih.bi_bit_count   = 24;
    ih.bi_compression = BMP_BI_RGB;
    ih.bi_size_image  = data_size;

    fwrite(&fh, sizeof(fh), 1, f);
    fwrite(&ih, sizeof(ih), 1, f);

    uint8_t padding[3] = {0};
    for (int32_t y = h - 1; y >= 0; y--) {
        fwrite(img->data + (size_t)y * w * 3, 3, (size_t)w, f);
        fwrite(padding, 1, (size_t)row_pad, f);
    }
    fclose(f);
    return 0;
}

int bmp_read_memory(const uint8_t *data, size_t size, Image *img)
{
    if (!data || !img || size < sizeof(BmpFileHeader) + sizeof(BmpInfoHeader))
        return -1;
    const BmpFileHeader *fh = (const BmpFileHeader *)data;
    const BmpInfoHeader *ih = (const BmpInfoHeader *)(data + sizeof(BmpFileHeader));

    if (fh->bf_type != BMP_MAGIC || ih->bi_compression != BMP_BI_RGB)
        return -1;

    int32_t w   = ih->bi_width;
    int32_t h   = (ih->bi_height > 0) ? ih->bi_height : -ih->bi_height;
    img->width  = w;
    img->height = h;
    img->format = PIXEL_FORMAT_RGB24;
    img->data_size = (size_t)w * h * 3;
    img->stride   = w * 3;
    img->data     = (uint8_t *)malloc(img->data_size);
    if (!img->data) return -1;

    int32_t row_pad = (4 - (w * 3) % 4) % 4;
    const uint8_t *pix = data + fh->bf_off_bits;
    for (int32_t y = h - 1; y >= 0; y--) {
        memcpy(img->data + (size_t)y * w * 3, pix, (size_t)w * 3);
        pix += (size_t)w * 3 + (size_t)row_pad;
    }
    return 0;
}

/* ── PPM  (P6 binary) ────────────────────────────────────────────────── */
int ppm_read_file(const char *path, Image *img)
{
    if (!path || !img) return -1;
    FILE *f = fopen(path, "rb");
    if (!f) return -1;

    char magic[3];
    if (!fgets(magic, sizeof(magic), f) || magic[0] != 'P' || magic[1] != '6') {
        fclose(f); return -1;
    }

    int32_t w, h, maxval;
    if (fscanf(f, "%d %d\n%d\n", &w, &h, &maxval) != 3) { fclose(f); return -1; }
    (void)maxval;

    img->width  = w;
    img->height = h;
    img->format = PIXEL_FORMAT_RGB24;
    img->data_size = (size_t)w * h * 3;
    img->stride   = w * 3;
    img->data     = (uint8_t *)malloc(img->data_size);
    if (!img->data) { fclose(f); return -1; }

    if (fread(img->data, 1, img->data_size, f) != img->data_size) {
        free(img->data); fclose(f); return -1;
    }
    fclose(f);
    return 0;
}

int ppm_write_file(const char *path, const Image *img)
{
    if (!path || !img || img->format != PIXEL_FORMAT_RGB24) return -1;
    FILE *f = fopen(path, "wb");
    if (!f) return -1;

    fprintf(f, "P6\n%d %d\n255\n", img->width, img->height);
    fwrite(img->data, 1, img->data_size, f);
    fclose(f);
    return 0;
}

int pgm_read_file(const char *path, Image *img)
{
    if (!path || !img) return -1;
    FILE *f = fopen(path, "rb");
    if (!f) return -1;

    char magic[3];
    if (!fgets(magic, sizeof(magic), f) || magic[0] != 'P' || magic[1] != '5') {
        fclose(f); return -1;
    }

    int32_t w, h, maxval;
    if (fscanf(f, "%d %d\n%d\n", &w, &h, &maxval) != 3) { fclose(f); return -1; }
    (void)maxval;

    img->width  = w;
    img->height = h;
    img->format = PIXEL_FORMAT_GRAYSCALE8;
    img->data_size = (size_t)w * h;
    img->stride   = w;
    img->data     = (uint8_t *)malloc(img->data_size);
    if (!img->data) { fclose(f); return -1; }

    if (fread(img->data, 1, img->data_size, f) != img->data_size) {
        free(img->data); fclose(f); return -1;
    }
    fclose(f);
    return 0;
}

int pgm_write_file(const char *path, const Image *img)
{
    if (!path || !img || img->format != PIXEL_FORMAT_GRAYSCALE8) return -1;
    FILE *f = fopen(path, "wb");
    if (!f) return -1;

    fprintf(f, "P5\n%d %d\n255\n", img->width, img->height);
    fwrite(img->data, 1, img->data_size, f);
    fclose(f);
    return 0;
}

/* ── Simple PNG  (signature + IHDR only) ─────────────────────────────── */
int png_read_file(const char *path, Image *img)
{
    if (!path || !img) return -1;
    FILE *f = fopen(path, "rb");
    if (!f) return -1;

    uint8_t sig[PNG_SIG_SIZE];
    if (fread(sig, 1, PNG_SIG_SIZE, f) != PNG_SIG_SIZE
        || memcmp(sig, PNG_SIGNATURE, PNG_SIG_SIZE) != 0) {
        fclose(f); return -1;
    }

    /* read IHDR — simplified: assume fixed-length chunk header */
    uint8_t buf[8 + 13 + 4]; /* len(4) + type(4) + IHDR(13) + crc(4) */
    if (fread(buf, 1, sizeof(buf), f) != sizeof(buf)) {
        fclose(f); return -1;
    }
    /* IHDR fields at offset 8: w(4) h(4) bitd(1) coltype(1) ... */
    int32_t w = ((int32_t)buf[8] << 24) | ((int32_t)buf[9] << 16)
              | ((int32_t)buf[10] << 8) | (int32_t)buf[11];
    int32_t h = ((int32_t)buf[12] << 24) | ((int32_t)buf[13] << 16)
              | ((int32_t)buf[14] << 8) | (int32_t)buf[15];
    uint8_t coltype = buf[17];

    img->width  = w;
    img->height = h;
    if (coltype == 2) img->format = PIXEL_FORMAT_RGB24;
    else img->format = PIXEL_FORMAT_GRAYSCALE8;

    img->data_size = image_calculate_size(w, h, img->format);
    img->stride    = w * image_bytes_per_pixel(img->format);
    img->data      = (uint8_t *)calloc(1, img->data_size);
    fclose(f);
    /* Note: full decompression not implemented — IDAT body skipped */
    return 0;
}

int png_write_file(const char *path, const Image *img)
{
    if (!path || !img) return -1;
    FILE *f = fopen(path, "wb");
    if (!f) return -1;

    fwrite(PNG_SIGNATURE, 1, PNG_SIG_SIZE, f);

    /* IHDR */
    int32_t w = img->width, h = img->height;
    uint8_t ihdr[13];
    memset(ihdr, 0, 13);
    ihdr[0] = (uint8_t)(w >> 24); ihdr[1] = (uint8_t)(w >> 16);
    ihdr[2] = (uint8_t)(w >> 8);  ihdr[3] = (uint8_t)(w);
    ihdr[4] = (uint8_t)(h >> 24); ihdr[5] = (uint8_t)(h >> 16);
    ihdr[6] = (uint8_t)(h >> 8);  ihdr[7] = (uint8_t)(h);
    ihdr[8] = 8; /* bit depth */
    if (img->format == PIXEL_FORMAT_RGB24) ihdr[9] = 2;
    else ihdr[9] = 0;
    ihdr[10] = 0; /* compression */
    ihdr[11] = 0; /* filter */
    ihdr[12] = 0; /* interlace */

    /* chunk: len(4) "IHDR" data(13) crc(4) */
    uint8_t chunk_type[4] = {'I','H','D','R'};
    uint32_t crc_val = 0; /* simplified — CRC omitted */
    uint32_t len_be = 13;
    uint8_t  len_bytes[4] = {
        (uint8_t)(len_be >> 24), (uint8_t)(len_be >> 16),
        (uint8_t)(len_be >> 8),  (uint8_t)(len_be)
    };
    fwrite(len_bytes, 1, 4, f);
    fwrite(chunk_type, 1, 4, f);
    fwrite(ihdr, 1, 13, f);
    fwrite(&crc_val, 1, 4, f); /* stub CRC */

    /* IDAT placeholder — raw pixels written uncompressed */
    uint32_t data_len = (uint32_t)img->data_size;
    uint8_t  dlen_bytes[4] = {
        (uint8_t)(data_len >> 24), (uint8_t)(data_len >> 16),
        (uint8_t)(data_len >> 8),  (uint8_t)(data_len)
    };
    uint8_t idat_type[4] = {'I','D','A','T'};
    fwrite(dlen_bytes, 1, 4, f);
    fwrite(idat_type, 1, 4, f);
    fwrite(img->data, 1, img->data_size, f);
    fwrite(&crc_val, 1, 4, f); /* stub CRC */

    /* IEND */
    uint8_t iend_type[4] = {'I','E','N','D'};
    uint32_t zero = 0;
    fwrite(&zero, 1, 4, f);
    fwrite(iend_type, 1, 4, f);
    fwrite(&zero, 1, 4, f);

    fclose(f);
    return 0;
}

/* ── Raw pixel buffer ────────────────────────────────────────────────── */
Image raw_pixel_buffer_create(int32_t width, int32_t height, PixelFormat format)
{
    return image_create(width, height, format);
}

void raw_pixel_buffer_free(Image *img)
{
    image_destroy(img);
}

int raw_pixel_buffer_read(const uint8_t *data, size_t size,
                          int32_t width, int32_t height,
                          PixelFormat format, Image *img)
{
    if (!data || !img) return -1;
    size_t needed = image_calculate_size(width, height, format);
    if (size < needed) return -1;

    img->width  = width;
    img->height = height;
    img->format = format;
    img->stride = width * image_bytes_per_pixel(format);
    img->data_size = needed;
    img->data = (uint8_t *)malloc(needed);
    if (!img->data) return -1;
    memcpy(img->data, data, needed);
    return 0;
}

int raw_pixel_buffer_write(const Image *img, uint8_t *buf, size_t buf_size)
{
    if (!img || !buf || buf_size < img->data_size) return -1;
    memcpy(buf, img->data, img->data_size);
    return 0;
}
