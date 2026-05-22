#include "mini_image/pixel_format.h"
#include "mini_image/jpeg_codec.h"
#include "mini_image/transforms.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    printf("=== JPEG Codec Simulation Demo ===\n\n");

    /* ── 1. RGB → YCbCr colour transform ───────────────────────────── */
    printf("--- RGB -> YCbCr ---\n");
    {
        int32_t y, cb, cr;
        rgb_to_ycbcr(255, 0, 0, &y, &cb, &cr);
        printf("  RGB(255,0,0) -> Y=%d Cb=%d Cr=%d\n", y, cb, cr);
        rgb_to_ycbcr(0, 255, 0, &y, &cb, &cr);
        printf("  RGB(0,255,0) -> Y=%d Cb=%d Cr=%d\n", y, cb, cr);
        rgb_to_ycbcr(0, 0, 255, &y, &cb, &cr);
        printf("  RGB(0,0,255) -> Y=%d Cb=%d Cr=%d\n", y, cb, cr);
    }

    /* ── 2. YCbCr → RGB inverse transform ──────────────────────────── */
    printf("\n--- YCbCr -> RGB ---\n");
    {
        int32_t r, g, b;
        ycbcr_to_rgb(76, 84, 255, &r, &g, &b);
        printf("  YCbCr(76,84,255) -> RGB(%d,%d,%d)\n", r, g, b);
    }

    /* ── 3. Chroma subsampling (4:2:0) ─────────────────────────────── */
    printf("\n--- Chroma Subsampling 4:2:0 ---\n");
    {
        int32_t luma_w = 16, luma_h = 16;
        int32_t cw = (luma_w + 1) / 2, ch = (luma_h + 1) / 2;
        int32_t *y_plane  = (int32_t *)calloc((size_t)luma_w * luma_h, sizeof(int32_t));
        int32_t *cb_plane = (int32_t *)calloc((size_t)cw * ch, sizeof(int32_t));
        int32_t *cr_plane = (int32_t *)calloc((size_t)cw * ch, sizeof(int32_t));
        chroma_subsample_420(y_plane, luma_w, luma_h, cb_plane, cr_plane);
        printf("  Luma plane: %dx%d, Chroma plane: %dx%d\n",
               luma_w, luma_h, cw, ch);
        free(y_plane); free(cb_plane); free(cr_plane);
    }

    /* ── 4. 8×8 DCT ───────────────────────────────────────────────── */
    printf("\n--- 8x8 DCT (constant block) ---\n");
    {
        int16_t src[DCT_BLOCK_LEN];
        int16_t dst[DCT_BLOCK_LEN];
        for (int32_t i = 0; i < DCT_BLOCK_LEN; i++) src[i] = 128;
        dct_8x8(src, dst);
        printf("  Constant 128 -> DC coefficient = %d (expect large value)\n", dst[0]);
        for (int32_t i = 1; i < DCT_BLOCK_LEN; i++) {
            if (dst[i] != 0) {
                printf("  AC[%d] = %d (should be ~0)\n", i, dst[i]);
                break;
            }
            if (i == DCT_BLOCK_LEN - 1)
                printf("  All AC coefficients are 0 (as expected)\n");
        }
    }

    /* ── 5. IDCT round-trip ────────────────────────────────────────── */
    printf("\n--- IDCT round-trip ---\n");
    {
        int16_t orig[DCT_BLOCK_LEN];
        int16_t dct_coeff[DCT_BLOCK_LEN];
        int16_t recon[DCT_BLOCK_LEN];
        for (int32_t i = 0; i < DCT_BLOCK_LEN; i++)
            orig[i] = (int16_t)((i % 16) * 16);
        dct_8x8(orig, dct_coeff);
        idct_8x8(dct_coeff, recon);
        int32_t max_err = 0;
        for (int32_t i = 0; i < DCT_BLOCK_LEN; i++) {
            int32_t err = (int32_t)orig[i] - (int32_t)recon[i];
            if (err < 0) err = -err;
            if (err > max_err) max_err = err;
        }
        printf("  Round-trip max error = %d\n", max_err);
    }

    /* ── 6. Quantisation table generation ──────────────────────────── */
    printf("\n--- Quantisation Tables ---\n");
    {
        uint8_t ql[JPEG_BLOCK_LEN], qc[JPEG_BLOCK_LEN];
        generate_quant_table(ql, 75, true);
        generate_quant_table(qc, 75, false);
        printf("  Luma quant[0]   = %u (q=75)\n", ql[0]);
        printf("  Chroma quant[0] = %u (q=75)\n", qc[0]);

        uint8_t ql_low[JPEG_BLOCK_LEN];
        generate_quant_table(ql_low, 10, true);
        printf("  Luma quant[0]   = %u (q=10, low quality)\n", ql_low[0]);

        uint8_t ql_high[JPEG_BLOCK_LEN];
        generate_quant_table(ql_high, 95, true);
        printf("  Luma quant[0]   = %u (q=95, high quality)\n", ql_high[0]);
    }

    /* ── 7. Quantise / dequantise round-trip ───────────────────────── */
    printf("\n--- Quantise/Dequantise ---\n");
    {
        int16_t block[JPEG_BLOCK_LEN];
        uint8_t qtable[JPEG_BLOCK_LEN];
        generate_quant_table(qtable, 50, true);
        for (int32_t i = 0; i < JPEG_BLOCK_LEN; i++)
            block[i] = (int16_t)(i * 4);
        int16_t orig[JPEG_BLOCK_LEN];
        memcpy(orig, block, sizeof(orig));
        quantize_block(block, qtable);
        dequantize_block(block, qtable);
        int32_t max_err = 0;
        for (int32_t i = 0; i < JPEG_BLOCK_LEN; i++) {
            int32_t err = (int32_t)orig[i] - (int32_t)block[i];
            if (err < 0) err = -err;
            if (err > max_err) max_err = err;
        }
        printf("  Q/DQ round-trip max error = %d\n", max_err);
    }

    /* ── 8. Zig-zag scan ───────────────────────────────────────────── */
    printf("\n--- Zig-zag Scan ---\n");
    {
        int16_t block[JPEG_BLOCK_LEN];
        int16_t zigzag[JPEG_BLOCK_LEN];
        int16_t back[JPEG_BLOCK_LEN];
        for (int32_t i = 0; i < JPEG_BLOCK_LEN; i++) block[i] = (int16_t)i;
        zigzag_encode(block, zigzag);
        zigzag_decode(zigzag, back);
        int32_t ok = 1;
        for (int32_t i = 0; i < JPEG_BLOCK_LEN; i++) {
            if (block[i] != back[i]) { ok = 0; break; }
        }
        printf("  Zig-zag round-trip: %s\n", ok ? "OK" : "FAIL");
    }

    /* ── 9. JPEG header parsing ────────────────────────────────────── */
    printf("\n--- JPEG Header Parsing ---\n");
    {
        /* build minimal JPEG header */
        uint8_t minimal_jpeg[] = {
            0xFF, 0xD8, /* SOI */
            0xFF, 0xC0, 0x00, 0x0B, /* SOF0, len=11 */
            0x08, /* precision */
            0x01, 0x00, /* height = 256 */
            0x01, 0x00, /* width  = 256 */
            0x01, /* 1 component */
            0x01, 0x11, 0x00,
            0xFF, 0xD9  /* EOI */
        };
        JpegContext ctx;
        jpeg_context_init(&ctx);
        if (jpeg_parse_header(minimal_jpeg, sizeof(minimal_jpeg), &ctx) == 0) {
            printf("  Parsed: %dx%d, components=%d\n",
                   ctx.width, ctx.height, ctx.num_components);
        } else {
            printf("  Parse failed\n");
        }
        jpeg_context_free(&ctx);
    }

    /* ── 10. High-level encode / decode ────────────────────────────── */
    printf("\n--- JPEG Encode/Decode (simplified) ---\n");
    {
        Image src = image_create(64, 64, PIXEL_FORMAT_RGB24);
        for (int32_t y = 0; y < 64; y++)
            for (int32_t x = 0; x < 64; x++) {
                uint8_t *p = src.data + ((size_t)y * 64 + x) * 3;
                p[0] = (uint8_t)x; p[1] = (uint8_t)y; p[2] = (uint8_t)((x + y) / 2);
            }

        uint8_t *jpeg_buf = NULL;
        size_t   jpeg_sz   = 0;
        if (jpeg_encode_simple(&src, &jpeg_buf, &jpeg_sz, 80) == 0) {
            printf("  Encoded %dx%d -> %zu bytes\n", src.width, src.height, jpeg_sz);

            Image dst = image_create(64, 64, PIXEL_FORMAT_RGB24);
            if (jpeg_decode_simple(jpeg_buf, jpeg_sz, &dst) == 0) {
                printf("  Decoded -> %dx%d image\n", dst.width, dst.height);
            }
            image_destroy(&dst);
            free(jpeg_buf);
        }
        image_destroy(&src);
    }

    printf("\nAll JPEG codec examples completed.\n");
    return 0;
}
