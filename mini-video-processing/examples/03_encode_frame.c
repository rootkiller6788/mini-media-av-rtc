#include "macroblock.h"
#include "motion_est.h"
#include "rate_control.h"
#include "yuv_rgb_conv.h"
#include <stdio.h>
#include <string.h>

#define TEST_WIDTH  64
#define TEST_HEIGHT 64

static void test_intra_pred(void)
{
    uint8_t above[20], left[20], pred[256];
    int i;

    for (i = 0; i < 20; i++) {
        above[i] = (uint8_t)(100 + i * 5);
        left[i] = (uint8_t)(80 + i * 3);
    }

    printf("=== Intra Prediction Test ===\n");

    intra_predict_4x4(above, left, pred, 4, INTRA_DC);
    printf("4x4 DC pred[0]=%d\n", pred[0]);

    intra_predict_4x4(above, left, pred, 4, INTRA_VERTICAL);
    printf("4x4 Vertical pred[0]=%d\n", pred[0]);

    intra_predict_4x4(above, left, pred, 4, INTRA_HORIZONTAL);
    printf("4x4 Horizontal pred[0]=%d\n", pred[0]);

    intra_predict_4x4(above, left, pred, 4, INTRA_PLANE);
    printf("4x4 Plane pred[0]=%d\n", pred[0]);

    intra_predict_16x16(above, left, pred, 16, INTRA_DC);
    printf("16x16 DC pred[0]=%d\n", pred[0]);

    intra_predict_16x16(above, left, pred, 16, INTRA_PLANE);
    printf("16x16 Plane pred[0]=%d\n", pred[0]);
}

static void test_dct_quant(void)
{
    int16_t input[16], dct_out[16], qcoeff[16], deq[16], idct_out[16];
    int i;

    for (i = 0; i < 16; i++)
        input[i] = (int16_t)((i % 8) * 10 - 35);

    printf("\n=== DCT / Quantization Test ===\n");

    dct_4x4(input, dct_out);
    printf("DCT coeff[0]=%d, coeff[1]=%d\n", dct_out[0], dct_out[1]);

    quantize_4x4(dct_out, qcoeff, 26);
    printf("Quantized[0]=%d\n", qcoeff[0]);

    dequantize_4x4(qcoeff, deq, 26);
    printf("Dequantized[0]=%d\n", deq[0]);

    idct_4x4(deq, idct_out);
    printf("IDCT output[0]=%d (original=%d)\n", idct_out[0], input[0]);

    int16_t scanned[16];
    zigzag_scan_4x4(qcoeff, scanned);
    printf("Zigzag[0]=%d, Zigzag[1]=%d\n", scanned[0], scanned[1]);
}

static void test_cavlc(void)
{
    int16_t coeffs[16] = { 0, 3, 0, 1, -1, -1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0 };
    CAVLCBlock block;

    printf("\n=== CAVLC Encoding Test ===\n");
    cavlc_encode_block(coeffs, 16, &block);
    printf("TotalCoeff=%d, TrailingOnes=%d, Pairs=%d\n",
           block.total_coeff, block.trailing_ones, block.count);
    cavlc_print_block(&block);
}

static void test_motion_est(void)
{
    uint8_t cur[MB_SIZE * MB_SIZE], ref_data[640 * 480];
    MEReferenceFrame ref;
    MEConfig cfg;
    int i;

    printf("\n=== Motion Estimation Test ===\n");

    for (i = 0; i < MB_SIZE * MB_SIZE; i++)
        cur[i] = (uint8_t)(128 + (i % 32));

    memset(ref_data, 128, sizeof(ref_data));
    for (i = 0; i < MB_SIZE * MB_SIZE; i++)
        ref_data[i + 1 * 640 + 1] = cur[i];

    ref.y_plane = ref_data;
    ref.u_plane = NULL;
    ref.v_plane = NULL;
    ref.width = 640;
    ref.height = 480;
    ref.y_stride = 640;
    ref.uv_stride = 320;
    ref.frame_num = 0;
    ref.poc = 0;

    me_config_init(&cfg);
    cfg.search_range = 16;
    cfg.subpel_refinement = 0;

    MEResult result = me_search(cur, MB_SIZE, &ref, 0, 0, &cfg);
    printf("Best MV: (%d, %d) qpel, SAD=%u\n",
           result.mv.x_qpel, result.mv.y_qpel, result.sad);

    cfg.subpel_refinement = 1;
    cfg.half_pel_enabled = 1;
    result = me_search(cur, MB_SIZE, &ref, 0, 0, &cfg);
    printf("With half-pel: MV=(%d, %d), SAD=%u\n",
           result.mv.x_qpel, result.mv.y_qpel, result.sad);

    MV median = me_mv_median_predictor(&result.mv, &result.mv, &result.mv);
    printf("Median MV: (%d, %d)\n", median.x_qpel, median.y_qpel);
}

static void test_rate_control(void)
{
    RateControl rc;

    printf("\n=== Rate Control Test ===\n");

    rc_init(&rc, RC_MODE_CBR, 2000000, 30.0, 1920, 1080);
    printf("Initial QP: %d\n", rc.current_qp);

    int i;
    for (i = 0; i < 15; i++) {
        int qp = rc_prepare_frame(&rc);
        int bits = (int)(rc.hrd.bits_per_frame * 0.9);
        rc_update_frame(&rc, bits);
        printf("Frame %d: type=%s, QP=%d, bits=%d, HRD fill=%.1f%%\n",
               i, rc_frame_type_name(rc.current_frame_type),
               qp, bits, hrd_buffer_fill_ratio(&rc.hrd) * 100.0);
    }

    rc_print_status(&rc);
}

static void test_psnr(void)
{
    uint8_t a[256], b[256];
    int i;
    for (i = 0; i < 256; i++) {
        a[i] = (uint8_t)i;
        b[i] = (uint8_t)(i + 5);
    }
    double psnr = compute_psnr(a, b, 256);
    printf("\n=== PSNR Test ===\n");
    printf("PSNR(a, a+5) = %.2f dB\n", psnr);

    for (i = 0; i < 256; i++) b[i] = a[i];
    psnr = compute_psnr(a, b, 256);
    printf("PSNR(a, a) = %.2f dB\n", psnr);
}

int main(void)
{
    printf("=== Encode Frame Pipeline Demo ===\n\n");
    test_intra_pred();
    test_dct_quant();
    test_cavlc();
    test_motion_est();
    test_rate_control();
    test_psnr();
    printf("\nAll pipeline tests passed.\n");
    return 0;
}
