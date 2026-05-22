#ifndef MINI_IMAGE_JPEG_CODEC_H
#define MINI_IMAGE_JPEG_CODEC_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "pixel_format.h"

#ifdef __cplusplus
extern "C" {
#endif

#define JPEG_BLOCK_SIZE   8
#define JPEG_BLOCK_LEN    (JPEG_BLOCK_SIZE * JPEG_BLOCK_SIZE)
#define JPEG_MAX_HUFF_LEN 256
#define JPEG_MAX_COMPONENTS 3

/* ── JPEG marker constants ───────────────────────────────────────────── */
#define JPEG_MARKER_SOI  0xFFD8
#define JPEG_MARKER_EOI  0xFFD9
#define JPEG_MARKER_APP0 0xFFE0
#define JPEG_MARKER_DQT  0xFFDB
#define JPEG_MARKER_SOF0 0xFFC0
#define JPEG_MARKER_DHT  0xFFC4
#define JPEG_MARKER_SOS  0xFFDA
#define JPEG_MARKER_DRI  0xFFDD
#define JPEG_MARKER_RST0 0xFFD0

/* ── Quantisation tables ──────────────────────────────────────────────── */
typedef struct {
    uint8_t table[2][JPEG_BLOCK_LEN]; /* [0]=luma [1]=chroma */
} JpegQuantTables;

/* ── Huffman table  (simplified, one symbol per code) ─────────────────── */
typedef struct {
    uint8_t  codes[JPEG_MAX_HUFF_LEN];
    uint8_t  sizes[JPEG_MAX_HUFF_LEN];
    int32_t  count;
} JpegHuffTable;

/* ── JPEG decoder state ──────────────────────────────────────────────── */
typedef struct {
    int32_t         width;
    int32_t         height;
    int32_t         num_components;
    int32_t         restart_interval;
    JpegQuantTables quant_tables;
    JpegHuffTable   dc_huff[2];
    JpegHuffTable   ac_huff[2];
    uint8_t        *raw_data;
    size_t          raw_size;
} JpegContext;

/* ── Colour transform:  RGB ↔ YCbCr ──────────────────────────────────── */
void rgb_to_ycbcr(int32_t r, int32_t g, int32_t b,
                  int32_t *y, int32_t *cb, int32_t *cr);
void ycbcr_to_rgb(int32_t y, int32_t cb, int32_t cr,
                  int32_t *r, int32_t *g, int32_t *b);

/* ── Chroma sub-sampling  (4:2:0) ────────────────────────────────────── */
void chroma_subsample_420(const int32_t *y_plane, int32_t luma_w, int32_t luma_h,
                          int32_t *cb_plane, int32_t *cr_plane);
void chroma_upsample_420(const int32_t *cb_plane, const int32_t *cr_plane,
                         int32_t chroma_w, int32_t chroma_h,
                         int32_t luma_w, int32_t luma_h,
                         int32_t *cb_full, int32_t *cr_full);

/* ── Quantisation  (quality 1–100 → scaling factor) ──────────────────── */
void quantize_block(int16_t block[JPEG_BLOCK_LEN],
                    const uint8_t qtable[JPEG_BLOCK_LEN]);
void dequantize_block(int16_t block[JPEG_BLOCK_LEN],
                      const uint8_t qtable[JPEG_BLOCK_LEN]);
void generate_quant_table(uint8_t qtable[JPEG_BLOCK_LEN],
                          int32_t quality, bool is_luma);

/* ── Zig-zag scan  (8×8) ─────────────────────────────────────────────── */
void zigzag_encode(const int16_t block[JPEG_BLOCK_LEN],
                   int16_t out[JPEG_BLOCK_LEN]);
void zigzag_decode(const int16_t in[JPEG_BLOCK_LEN],
                   int16_t block[JPEG_BLOCK_LEN]);

/* ── Simplified Huffman helper ────────────────────────────────────────── */
int  huffman_encode_symbol(int32_t symbol, uint8_t *bitstream,
                           int32_t bit_offset, const JpegHuffTable *table);
int  huffman_decode_symbol(const uint8_t *bitstream, int32_t *bit_offset,
                           const JpegHuffTable *table);

/* ── JPEG header parsing ─────────────────────────────────────────────── */
int jpeg_parse_header(const uint8_t *data, size_t size, JpegContext *ctx);
void jpeg_context_init(JpegContext *ctx);
void jpeg_context_free(JpegContext *ctx);

/* ── High-level encode / decode  (simplified) ────────────────────────── */
int jpeg_encode_simple(const Image *src, uint8_t **out, size_t *out_size,
                       int32_t quality);
int jpeg_decode_simple(const uint8_t *data, size_t size, Image *dst);

#ifdef __cplusplus
}
#endif

#endif /* MINI_IMAGE_JPEG_CODEC_H */
