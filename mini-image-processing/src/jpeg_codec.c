#include "jpeg_codec.h"
#include "transforms.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ── RGB ↔ YCbCr  (JPEG colour transform) ───────────────────────────── */
void rgb_to_ycbcr(int32_t r, int32_t g, int32_t b,
                  int32_t *y, int32_t *cb, int32_t *cr)
{
    *y  = (int32_t)( 0.2990 * r + 0.5870 * g + 0.1140 * b);
    *cb = (int32_t)(-0.1687 * r - 0.3313 * g + 0.5000 * b + 128);
    *cr = (int32_t)( 0.5000 * r - 0.4187 * g - 0.0813 * b + 128);
}

void ycbcr_to_rgb(int32_t y, int32_t cb, int32_t cr,
                  int32_t *r, int32_t *g, int32_t *b)
{
    *r = (int32_t)(y + 1.402 * (cr - 128));
    *g = (int32_t)(y - 0.34414 * (cb - 128) - 0.71414 * (cr - 128));
    *b = (int32_t)(y + 1.772 * (cb - 128));
}

/* ── Chroma sub-sampling  (4:2:0) ────────────────────────────────────── */
void chroma_subsample_420(const int32_t *y_plane, int32_t luma_w, int32_t luma_h,
                          int32_t *cb_plane, int32_t *cr_plane)
{
    (void)y_plane;
    int32_t cw = (luma_w + 1) / 2, ch = (luma_h + 1) / 2;
    for (int32_t row = 0; row < ch; row++) {
        for (int32_t col = 0; col < cw; col++) {
            cb_plane[row * cw + col] = 0;
            cr_plane[row * cw + col] = 0;
        }
    }
}

void chroma_upsample_420(const int32_t *cb_plane, const int32_t *cr_plane,
                         int32_t chroma_w, int32_t chroma_h,
                         int32_t luma_w, int32_t luma_h,
                         int32_t *cb_full, int32_t *cr_full)
{
    (void)chroma_w; (void)chroma_h;
    for (int32_t row = 0; row < luma_h; row++) {
        for (int32_t col = 0; col < luma_w; col++) {
            int32_t cr = (row / 2) * ((luma_w + 1) / 2) + col / 2;
            cb_full[row * luma_w + col] = cb_plane[cr];
            cr_full[row * luma_w + col] = cr_plane[cr];
        }
    }
}

/* ── Quantisation ────────────────────────────────────────────────────── */
void quantize_block(int16_t block[JPEG_BLOCK_LEN],
                    const uint8_t qtable[JPEG_BLOCK_LEN])
{
    for (int32_t i = 0; i < JPEG_BLOCK_LEN; i++) {
        if (qtable[i] == 0) continue;
        int32_t val = (int32_t)block[i];
        block[i] = (int16_t)(val / (int32_t)qtable[i]);
    }
}

void dequantize_block(int16_t block[JPEG_BLOCK_LEN],
                      const uint8_t qtable[JPEG_BLOCK_LEN])
{
    for (int32_t i = 0; i < JPEG_BLOCK_LEN; i++) {
        block[i] = (int16_t)((int32_t)block[i] * (int32_t)qtable[i]);
    }
}

void generate_quant_table(uint8_t qtable[JPEG_BLOCK_LEN],
                          int32_t quality, bool is_luma)
{
    if (quality < 1) quality = 1;
    if (quality > 100) quality = 100;

    /* base luminance quant table (standard JPEG example) */
    static const uint8_t base_luma[JPEG_BLOCK_LEN] = {
        16, 11, 10, 16, 24,  40,  51,  61,
        12, 12, 14, 19, 26,  58,  60,  55,
        14, 13, 16, 24, 40,  57,  69,  56,
        14, 17, 22, 29, 51,  87,  80,  62,
        18, 22, 37, 56, 68,  109, 103, 77,
        24, 35, 55, 64, 81,  104, 113, 92,
        49, 64, 78, 87, 103, 121, 120, 101,
        72, 92, 95, 98, 112, 100, 103, 99
    };
    static const uint8_t base_chroma[JPEG_BLOCK_LEN] = {
        17, 18, 24, 47, 99, 99, 99, 99,
        18, 21, 26, 66, 99, 99, 99, 99,
        24, 26, 56, 99, 99, 99, 99, 99,
        47, 66, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99
    };
    const uint8_t *base = is_luma ? base_luma : base_chroma;

    float scale;
    if (quality < 50)
        scale = 5000.0f / (float)quality;
    else
        scale = 200.0f - 2.0f * (float)quality;

    for (int32_t i = 0; i < JPEG_BLOCK_LEN; i++) {
        int32_t qv = (int32_t)(((float)base[i] * scale + 50.0f) / 100.0f);
        if (qv < 1) qv = 1;
        if (qv > 255) qv = 255;
        qtable[i] = (uint8_t)qv;
    }
}

/* ── Zig-zag ─────────────────────────────────────────────────────────── */
static const int32_t zigzag_order[JPEG_BLOCK_LEN] = {
     0,  1,  8, 16,  9,  2,  3, 10,
    17, 24, 32, 25, 18, 11,  4,  5,
    12, 19, 26, 33, 40, 48, 41, 34,
    27, 20, 13,  6,  7, 14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36,
    29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46,
    53, 60, 61, 54, 47, 55, 62, 63
};

void zigzag_encode(const int16_t block[JPEG_BLOCK_LEN],
                   int16_t out[JPEG_BLOCK_LEN])
{
    for (int32_t i = 0; i < JPEG_BLOCK_LEN; i++)
        out[i] = block[zigzag_order[i]];
}

void zigzag_decode(const int16_t in[JPEG_BLOCK_LEN],
                   int16_t block[JPEG_BLOCK_LEN])
{
    for (int32_t i = 0; i < JPEG_BLOCK_LEN; i++)
        block[zigzag_order[i]] = in[i];
}

/* ── Simplified Huffman (placeholder) ─────────────────────────────────── */
int huffman_encode_symbol(int32_t symbol, uint8_t *bitstream,
                          int32_t bit_offset, const JpegHuffTable *table)
{
    (void)bitstream; (void)bit_offset; (void)table;
    if (symbol < 0 || symbol >= table->count) return 0;
    return (int32_t)table->sizes[symbol];
}

int huffman_decode_symbol(const uint8_t *bitstream, int32_t *bit_offset,
                          const JpegHuffTable *table)
{
    (void)bitstream; (void)bit_offset; (void)table;
    return 0;
}

/* ── JPEG context ────────────────────────────────────────────────────── */
void jpeg_context_init(JpegContext *ctx)
{
    if (!ctx) return;
    memset(ctx, 0, sizeof(*ctx));
    ctx->dc_huff[0].count = 12;
    ctx->ac_huff[0].count = 12;
    ctx->dc_huff[1].count = 12;
    ctx->ac_huff[1].count = 12;
}

void jpeg_context_free(JpegContext *ctx)
{
    if (!ctx) return;
    if (ctx->raw_data) { free(ctx->raw_data); ctx->raw_data = NULL; }
    ctx->raw_size = 0;
}

/* ── JPEG header parsing (simplified) ────────────────────────────────── */
int jpeg_parse_header(const uint8_t *data, size_t size, JpegContext *ctx)
{
    if (!data || !ctx || size < 2) return -1;
    if (data[0] != 0xFF || data[1] != 0xD8) return -1; /* SOI */

    size_t pos = 2;
    while (pos + 2 <= size) {
        if (data[pos] != 0xFF) break;
        uint8_t marker = data[pos + 1];

        if (marker == 0xD9) break; /* EOI */
        if (marker == 0x00) { pos++; continue; } /* stuffed byte */

        if (marker == 0xDA) { /* SOS — stop parsing header */
            break;
        }

        if (pos + 4 > size) break;
        uint16_t seg_len = ((uint16_t)data[pos + 2] << 8) | data[pos + 3];

        switch (marker) {
        case 0xE0: /* APP0 */ break;
        case 0xC0: { /* SOF0 */
            if (seg_len >= 8) {
                ctx->height        = ((int32_t)data[pos + 5] << 8) | data[pos + 6];
                ctx->width         = ((int32_t)data[pos + 7] << 8) | data[pos + 8];
                ctx->num_components = data[pos + 9];
            }
            break;
        }
        case 0xDB: { /* DQT */
            break;
        }
        case 0xC4: /* DHT */ break;
        case 0xDD: { /* DRI */
            if (seg_len >= 4)
                ctx->restart_interval = ((int32_t)data[pos + 4] << 8) | data[pos + 5];
            break;
        }
        default: break;
        }
        pos += 2 + seg_len;
    }
    return 0;
}

/* ── High-level encode  (simplified skeleton) ────────────────────────── */
int jpeg_encode_simple(const Image *src, uint8_t **out, size_t *out_size,
                       int32_t quality)
{
    if (!src || !out || !out_size) return -1;

    /* allocate output buffer (over-estimate) */
    size_t buf_sz = src->data_size + 1024;
    *out = (uint8_t *)malloc(buf_sz);
    if (!*out) return -1;
    size_t pos = 0;

    /* SOI */
    (*out)[pos++] = 0xFF; (*out)[pos++] = 0xD8;

    /* APP0 (JFIF) */
    (*out)[pos++] = 0xFF; (*out)[pos++] = 0xE0;
    (*out)[pos++] = 0x00; (*out)[pos++] = 0x10;
    memcpy(*out + pos, "JFIF\0", 5); pos += 5;
    (*out)[pos++] = 0x01; (*out)[pos++] = 0x01; /* version 1.1 */
    (*out)[pos++] = 0x00; (*out)[pos++] = 0x01; /* units */
    (*out)[pos++] = 0x00; (*out)[pos++] = 0x48; /* x density */
    (*out)[pos++] = 0x00; (*out)[pos++] = 0x48; /* y density */
    (*out)[pos++] = 0x00; (*out)[pos++] = 0x00;

    /* DQT — luma */
    uint8_t qt_luma[JPEG_BLOCK_LEN], qt_chroma[JPEG_BLOCK_LEN];
    generate_quant_table(qt_luma, quality, true);
    generate_quant_table(qt_chroma, quality, false);

    (*out)[pos++] = 0xFF; (*out)[pos++] = 0xDB;
    (*out)[pos++] = 0x00; (*out)[pos++] = 0x43; /* len = 2 + 65 */
    (*out)[pos++] = 0x00; /* table ID 0 */
    for (int32_t i = 0; i < JPEG_BLOCK_LEN; i++)
        (*out)[pos++] = qt_luma[zigzag_order[i]];

    /* DQT — chroma */
    (*out)[pos++] = 0xFF; (*out)[pos++] = 0xDB;
    (*out)[pos++] = 0x00; (*out)[pos++] = 0x43;
    (*out)[pos++] = 0x01; /* table ID 1 */
    for (int32_t i = 0; i < JPEG_BLOCK_LEN; i++)
        (*out)[pos++] = qt_chroma[zigzag_order[i]];

    /* SOF0 */
    (*out)[pos++] = 0xFF; (*out)[pos++] = 0xC0;
    (*out)[pos++] = 0x00; (*out)[pos++] = 0x11; /* len */
    (*out)[pos++] = 0x08; /* precision */
    (*out)[pos++] = (uint8_t)(src->height >> 8);
    (*out)[pos++] = (uint8_t)(src->height & 0xFF);
    (*out)[pos++] = (uint8_t)(src->width >> 8);
    (*out)[pos++] = (uint8_t)(src->width & 0xFF);
    (*out)[pos++] = 0x03; /* 3 components */
    (*out)[pos++] = 0x01; (*out)[pos++] = 0x22; (*out)[pos++] = 0x00; /* Y 4:2:0 */
    (*out)[pos++] = 0x02; (*out)[pos++] = 0x11; (*out)[pos++] = 0x01; /* Cb */
    (*out)[pos++] = 0x03; (*out)[pos++] = 0x11; (*out)[pos++] = 0x01; /* Cr */

    /* DHT placeholder — skipped in simple mode */

    /* SOS */
    (*out)[pos++] = 0xFF; (*out)[pos++] = 0xDA;
    (*out)[pos++] = 0x00; (*out)[pos++] = 0x0C;
    (*out)[pos++] = 0x03;
    (*out)[pos++] = 0x01; (*out)[pos++] = 0x00;
    (*out)[pos++] = 0x02; (*out)[pos++] = 0x11;
    (*out)[pos++] = 0x03; (*out)[pos++] = 0x11;
    (*out)[pos++] = 0x00; (*out)[pos++] = 0x3F; (*out)[pos++] = 0x00;

    /* scan data placeholder */
    (*out)[pos++] = 0x12; (*out)[pos++] = 0x34;
    (*out)[pos++] = 0x56; (*out)[pos++] = 0x78;

    /* EOI */
    (*out)[pos++] = 0xFF; (*out)[pos++] = 0xD9;
    *out_size = pos;
    return 0;
}

/* ── High-level decode  (simplified skeleton) ────────────────────────── */
int jpeg_decode_simple(const uint8_t *data, size_t size, Image *dst)
{
    if (!data || !dst) return -1;
    JpegContext ctx;
    jpeg_context_init(&ctx);

    int ret = jpeg_parse_header(data, size, &ctx);
    if (ret != 0) { jpeg_context_free(&ctx); return -1; }

    if (ctx.width <= 0 || ctx.height <= 0) {
        jpeg_context_free(&ctx); return -1;
    }
    /* just fill with a test pattern */
    if (dst->data && dst->width == ctx.width && dst->height == ctx.height) {
        memset(dst->data, 128, dst->data_size);
    }
    jpeg_context_free(&ctx);
    return 0;
}
