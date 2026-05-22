#include "macroblock.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

const int zigzag_4x4[16] = {
    0,  1,  4,  8,  5,  2,  3,  6,
    9, 12, 13, 10,  7, 11, 14, 15
};

void intra_predict_4x4(const uint8_t *above, const uint8_t *left,
                       uint8_t *pred, int stride, IntraPredMode mode)
{
    int i, j;
    int stride_actual = stride > 0 ? stride : 1;
    (void)stride_actual;
    (void)stride;

    switch (mode) {
    case INTRA_DC: {
        int sum = 0;
        for (i = 0; i < 4; i++) sum += above[i];
        for (i = 0; i < 4; i++) sum += left[i];
        int dc = (sum + 4) >> 3;
        for (j = 0; j < 4; j++)
            for (i = 0; i < 4; i++)
                pred[j * 4 + i] = (uint8_t)dc;
        break;
    }
    case INTRA_VERTICAL:
        for (j = 0; j < 4; j++)
            for (i = 0; i < 4; i++)
                pred[j * 4 + i] = above[i];
        break;
    case INTRA_HORIZONTAL:
        for (j = 0; j < 4; j++)
            for (i = 0; i < 4; i++)
                pred[j * 4 + i] = left[j];
        break;
    case INTRA_PLANE: {
        int H = 0, V = 0;
        for (i = 0; i < 4; i++) { H += (i+1) * (above[i] - above[3]); }
        for (i = 0; i < 4; i++) { V += (i+1) * (left[i] - left[3]); }
        int a = 16 * (above[3] + left[3]);
        for (j = 0; j < 4; j++) {
            for (i = 0; i < 4; i++) {
                int val = (a + (i-1)*H + (j-1)*V + 16) >> 5;
                if (val < 0) val = 0; if (val > 255) val = 255;
                pred[j * 4 + i] = (uint8_t)val;
            }
        }
        break;
    }
    }
}

void intra_predict_16x16(const uint8_t *above, const uint8_t *left,
                         uint8_t *pred, int stride, IntraPredMode mode)
{
    int i, j;
    (void)stride;

    switch (mode) {
    case INTRA_DC: {
        int sum = 0;
        for (i = 0; i < 16; i++) sum += above[i];
        for (i = 0; i < 16; i++) sum += left[i];
        int dc = (sum + 16) >> 5;
        for (j = 0; j < 16; j++)
            for (i = 0; i < 16; i++)
                pred[j * 16 + i] = (uint8_t)dc;
        break;
    }
    case INTRA_VERTICAL:
        for (j = 0; j < 16; j++)
            for (i = 0; i < 16; i++)
                pred[j * 16 + i] = above[i];
        break;
    case INTRA_HORIZONTAL:
        for (j = 0; j < 16; j++)
            for (i = 0; i < 16; i++)
                pred[j * 16 + i] = left[j];
        break;
    case INTRA_PLANE: {
        int H = 0, V = 0;
        for (i = 0; i < 7; i++)  H += (i+1) * (above[8+i] - above[6-i]);
        for (i = 0; i < 7; i++)  V += (i+1) * (left[8+i] - left[6-i]);
        H += 8 * (above[15] - above[7]);
        V += 8 * (left[15] - left[7]);
        int a = 16 * (above[15] + left[15]);
        int b = (5 * H + 32) >> 6;
        int c = (5 * V + 32) >> 6;
        for (j = 0; j < 16; j++) {
            for (i = 0; i < 16; i++) {
                int val = (a + b*(i-7) + c*(j-7) + 16) >> 5;
                if (val < 0) val = 0; if (val > 255) val = 255;
                pred[j * 16 + i] = (uint8_t)val;
            }
        }
        break;
    }
    }
}

static const int dct4_matrix[4][4] = {
    { 1,  1,  1,  1},
    { 2,  1, -1, -2},
    { 1, -1, -1,  1},
    { 1, -2,  2, -1}
};

void dct_4x4(const int16_t *input, int16_t *output)
{
    int i0, i1, i2, i3;
    int out[4][4], tmp[4][4];
    for (i0 = 0; i0 < 4; i0++) {
        for (i1 = 0; i1 < 4; i1++) {
            tmp[i0][i1] = dct4_matrix[i0][0] * input[i1*4+0]
                        + dct4_matrix[i0][1] * input[i1*4+1]
                        + dct4_matrix[i0][2] * input[i1*4+2]
                        + dct4_matrix[i0][3] * input[i1*4+3];
        }
    }
    for (i2 = 0; i2 < 4; i2++) {
        for (i3 = 0; i3 < 4; i3++) {
            out[i3][i2] = tmp[i3][0] * dct4_matrix[i2][0]
                        + tmp[i3][1] * dct4_matrix[i2][1]
                        + tmp[i3][2] * dct4_matrix[i2][2]
                        + tmp[i3][3] * dct4_matrix[i2][3];
        }
    }
    for (i0 = 0; i0 < 16; i0++)
        output[i0] = (int16_t)((out[i0/4][i0%4] + 1) >> 1);
}

void idct_4x4(const int16_t *input, int16_t *output)
{
    int i0, i2;
    int out[4][4], tmp[4][4];
    static const int idct4_matrix[4][4] = {
        {1, 1, 1, 1},
        {1, 1, -1, -1},
        {1, -1, -1, 1},
        {1, -1, 1, -1}
    };
    for (i0 = 0; i0 < 4; i0++) {
        for (i2 = 0; i2 < 4; i2++) {
            tmp[i0][i2] = idct4_matrix[0][i0] * input[i2*4+0] / 2
                        + idct4_matrix[1][i0] * input[i2*4+1] / 2
                        + idct4_matrix[2][i0] * input[i2*4+2] / 2
                        + idct4_matrix[3][i0] * input[i2*4+3] / 2;
        }
    }
    for (i0 = 0; i0 < 4; i0++) {
        for (i2 = 0; i2 < 4; i2++) {
            out[i0][i2] = tmp[i0][0] * idct4_matrix[0][i2] / 2
                        + tmp[i0][1] * idct4_matrix[1][i2] / 2
                        + tmp[i0][2] * idct4_matrix[2][i2] / 2
                        + tmp[i0][3] * idct4_matrix[3][i2] / 2;
        }
    }
    for (i0 = 0; i0 < 16; i0++)
        output[i0] = (int16_t)out[i0/4][i0%4];
}

static const int qp_scale_tbl[6] = { 10, 11, 13, 14, 16, 18 };

void quantize_4x4(const int16_t *dct, int16_t *qcoeff, int qp)
{
    int i;
    int qp_per = qp / 6;
    int qp_rem = qp % 6;
    int q_bits = 15 + qp_per;
    int scale = qp_scale_tbl[qp_rem];

    for (i = 0; i < 16; i++) {
        int coeff = dct[i];
        int sign = coeff < 0 ? -1 : 1;
        coeff = (coeff < 0) ? -coeff : coeff;
        int qval = (coeff * scale + (1 << (q_bits - 1))) >> q_bits;
        qcoeff[i] = (int16_t)(sign * qval);
    }
}

void dequantize_4x4(const int16_t *qcoeff, int16_t *dct, int qp)
{
    int i;
    int qp_per = qp / 6;
    int qp_rem = qp % 6;
    int q_bits = qp_per;
    int scale = qp_scale_tbl[qp_rem];

    for (i = 0; i < 16; i++)
        dct[i] = (int16_t)(qcoeff[i] * scale * (1 << q_bits));
}

void zigzag_scan_4x4(const int16_t *block, int16_t *scanned)
{
    int i;
    for (i = 0; i < 16; i++)
        scanned[i] = block[zigzag_4x4[i]];
}

void motion_compensate(const ReferenceFrame *ref, const MotionVector *mv,
                       uint8_t *pred, int mb_x, int mb_y, int block_w, int block_h)
{
    int px = mb_x * MB_SIZE + (mv->x_qpel >> 2);
    int py = mb_y * MB_SIZE + (mv->y_qpel >> 2);
    int x, y;

    for (y = 0; y < block_h; y++) {
        for (x = 0; x < block_w; x++) {
            int rx = px + x, ry = py + y;
            if (rx >= 0 && rx < ref->width && ry >= 0 && ry < ref->height)
                pred[y * block_w + x] = ref->y_plane[ry * ref->y_stride + rx];
            else
                pred[y * block_w + x] = 128;
        }
    }
}

int cavlc_encode_block(const int16_t *scanned, int count, CAVLCBlock *out)
{
    int i;
    int non_zero = 0;
    int trailing = 0;
    int last_non_zero = -1;

    for (i = 0; i < count; i++) {
        if (scanned[i] != 0) {
            non_zero++;
            last_non_zero = i;
        }
    }

    if (non_zero > 0) {
        int val = scanned[last_non_zero];
        trailing = (val == 1 || val == -1) ? 1 : 0;
        int k;
        for (k = last_non_zero - 1; k >= 0 && trailing < 3; k--) {
            int v = scanned[k];
            if (v == 1 || v == -1) trailing++;
            else break;
        }
    }

    out->total_coeff = non_zero;
    out->trailing_ones = trailing;
    out->count = 0;

    int run = 0;
    int written = 0;
    for (i = 0; i < count && written < non_zero; i++) {
        if (scanned[i] != 0) {
            out->pairs[out->count].level = scanned[i];
            out->pairs[out->count].run = run;
            out->count++;
            run = 0;
            written++;
        } else {
            run++;
        }
    }

    return out->count;
}

void cavlc_print_block(const CAVLCBlock *block)
{
    int i;
    printf("  CAVLC: TotalCoeff=%d, TrailingOnes=%d\n",
           block->total_coeff, block->trailing_ones);
    for (i = 0; i < block->count; i++)
        printf("    [%d] level=%d, run=%d\n", i,
               block->pairs[i].level, block->pairs[i].run);
}

void mb_init(Macroblock *mb)
{
    memset(mb, 0, sizeof(Macroblock));
    mb->mb_type = MB_TYPE_INTRA_4x4;
    mb->intra_mode_16x16 = INTRA_DC;
    mb->qp = 26;
}

void mb_compute_residual(const uint8_t *orig, const uint8_t *pred, uint8_t *res)
{
    int i;
    for (i = 0; i < MACROBLOCK_PIXELS; i++) {
        int diff = (int)orig[i] - (int)pred[i];
        diff = (diff + 128) & 0xFF;
        diff -= 128;
        res[i] = (uint8_t)(diff & 0xFF);
    }
}

void mb_reconstruct(const uint8_t *pred, const uint8_t *res, uint8_t *recon)
{
    int i;
    for (i = 0; i < MACROBLOCK_PIXELS; i++) {
        int val = (int)pred[i] + (int)((int8_t)res[i]);
        if (val < 0) val = 0;
        if (val > 255) val = 255;
        recon[i] = (uint8_t)val;
    }
}

int mb_encode_intra(Macroblock *mb, const uint8_t *plane, const uint8_t *above,
                    const uint8_t *left, int stride, int qp)
{
    uint8_t pred_block[BLOCK_SIZE][BLOCK_SIZE];
    uint8_t pred[MACROBLOCK_PIXELS];
    uint8_t residual[MACROBLOCK_PIXELS];
    int16_t coeff_in[BLOCK_SIZE*BLOCK_SIZE];
    int16_t coeff_out[BLOCK_SIZE*BLOCK_SIZE];
    int16_t scanned[BLOCK_SIZE*BLOCK_SIZE];
    CAVLCBlock cavlc;
    int bx, by, b, idx;
    int total_bits = 0;
    int sp = stride > 0 ? stride : 1;
    (void)sp;

    intra_predict_16x16(above, left, pred, MB_SIZE, mb->intra_mode_16x16);
    mb_compute_residual(plane, pred, residual);

    for (by = 0; by < 4; by++) {
        for (bx = 0; bx < 4; bx++) {
            for (b = 0; b < 4; b++) {
                for (idx = 0; idx < 4; idx++) {
                    int py = by * 4 + b;
                    int px = bx * 4 + idx;
                    coeff_in[b * 4 + idx] = (int16_t)((int8_t)residual[py * MB_SIZE + px]);
                }
            }
            dct_4x4(coeff_in, coeff_out);
            quantize_4x4(coeff_out, coeff_out, qp);
            zigzag_scan_4x4(coeff_out, scanned);
            total_bits += cavlc_encode_block(scanned, 16, &cavlc) * 8;
        }
    }

    mb_reconstruct(pred, residual, mb->reconstructed[0]);
    memcpy(mb->residual[0], residual, MACROBLOCK_PIXELS);
    mb->num_coeff = total_bits;
    return total_bits;
}

int mb_decode_intra(Macroblock *mb, uint8_t *plane, int stride)
{
    uint8_t pred[MACROBLOCK_PIXELS];
    uint8_t above[MB_SIZE], left[MB_SIZE];
    memset(above, 128, sizeof(above));
    memset(left, 128, sizeof(left));
    (void)stride;

    intra_predict_16x16(above, left, pred, MB_SIZE, mb->intra_mode_16x16);
    mb_reconstruct(pred, mb->residual[0], plane);
    return 0;
}

double compute_psnr(const uint8_t *a, const uint8_t *b, int count)
{
    double mse = 0.0;
    int i;
    for (i = 0; i < count; i++) {
        double d = (double)a[i] - (double)b[i];
        mse += d * d;
    }
    mse /= (double)count;
    if (mse == 0.0) return 99.0;
    return 10.0 * log10(255.0 * 255.0 / mse);
}
