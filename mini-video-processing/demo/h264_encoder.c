#include "h264_nal.h"
#include "macroblock.h"
#include "motion_est.h"
#include "rate_control.h"
#include "yuv_rgb_conv.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MAX_FRAME_SHOW 30
#define ENC_WIDTH      64
#define ENC_HEIGHT     48
#define MB_X           (ENC_WIDTH / MB_SIZE)
#define MB_Y           (ENC_HEIGHT / MB_SIZE)

typedef struct {
    uint8_t y[ENC_HEIGHT][ENC_WIDTH];
    uint8_t u[ENC_HEIGHT/2][ENC_WIDTH/2];
    uint8_t v[ENC_HEIGHT/2][ENC_WIDTH/2];
} EncFrame;

typedef struct {
    RateControl rc;
    GOPStructure gop;
    int frame_count;
    int total_bytes;
    int total_coeffs;
    double avg_psnr;

    FILE *output_file;

    EncFrame ref_frames[4];
    int ref_count;

    int sps_written;
    int pps_written;
} H264Encoder;

static void enc_init(H264Encoder *enc, int64_t bitrate, double fps,
                     const char *out_path)
{
    memset(enc, 0, sizeof(H264Encoder));

    rc_init(&enc->rc, RC_MODE_CBR, bitrate, fps, ENC_WIDTH, ENC_HEIGHT);
    gop_init(&enc->gop, GOP_DEFAULT_SIZE, 2);
    memcpy(&enc->rc.gop, &enc->gop, sizeof(GOPStructure));

    enc->output_file = fopen(out_path, "wb");
}

static void enc_close(H264Encoder *enc)
{
    if (enc->output_file)
        fclose(enc->output_file);
}

static void write_start_code(FILE *f)
{
    uint8_t sc[4] = { 0x00, 0x00, 0x00, 0x01 };
    fwrite(sc, 1, 4, f);
}

static void write_nal_byte(FILE *f, int ref_idc, int type)
{
    uint8_t byte = (uint8_t)((ref_idc << 5) | (type & 0x1F));
    fwrite(&byte, 1, 1, f);
}

static void write_bits(uint8_t *buf, int *bp, int val, int nbits)
{
    while (nbits > 0) {
        int avail = 8 - (*bp % 8);
        int write_n = nbits < avail ? nbits : avail;
        int shift = nbits - write_n;
        int byte_idx = *bp / 8;
        buf[byte_idx] |= (uint8_t)(((val >> shift) & ((1 << write_n) - 1))
                                    << (avail - write_n));
        *bp += write_n;
        nbits -= write_n;
    }
}

static void write_sps(FILE *f, int width, int height)
{
    int mb_w = (width + 15) / 16;
    int mb_h = (height + 15) / 16;
    uint8_t sps[32];
    int bit_pos = 0;

    memset(sps, 0, sizeof(sps));

    write_bits(sps, &bit_pos, 0, 1);            /* forbidden_zero_bit */
    write_bits(sps, &bit_pos, 3, 2);            /* nal_ref_idc */
    write_bits(sps, &bit_pos, 7, 5);            /* nal_unit_type = SPS */
    write_bits(sps, &bit_pos, 66, 8);           /* profile_idc = Baseline */
    write_bits(sps, &bit_pos, 0, 1);            /* constraint_set0 */
    write_bits(sps, &bit_pos, 0, 1);            /* constraint_set1 */
    write_bits(sps, &bit_pos, 0, 1);            /* constraint_set2 */
    write_bits(sps, &bit_pos, 0, 1);            /* constraint_set3 */
    write_bits(sps, &bit_pos, 0, 4);            /* reserved */
    write_bits(sps, &bit_pos, 30, 8);           /* level_idc = 3.0 */
    write_bits(sps, &bit_pos, 0, 1);            /* seq_param_set_id (ue=0) */
    write_bits(sps, &bit_pos, 0, 1);            /* log2_max_frame_num (ue=0) */
    write_bits(sps, &bit_pos, 0, 1);            /* pic_order_cnt_type (ue=0) */
    write_bits(sps, &bit_pos, 0, 1);            /* log2_max_poc_lsb (ue=0) */
    write_bits(sps, &bit_pos, 0, 1);            /* num_ref_frames (ue=0) */
    write_bits(sps, &bit_pos, 0, 1);            /* gaps_allowed */
    write_bits(sps, &bit_pos, mb_w - 1, 0);     /* pic_width_in_mbs (ue) */
    write_bits(sps, &bit_pos, mb_h - 1, 0);     /* pic_height_in_mbs (ue) */
    write_bits(sps, &bit_pos, 1, 1);            /* frame_mbs_only */
    write_bits(sps, &bit_pos, 0, 1);            /* direct_8x8 */
    write_bits(sps, &bit_pos, 0, 1);            /* frame_cropping */

    write_start_code(f);
    write_nal_byte(f, 3, NAL_TYPE_SPS);

    int byte_count = (bit_pos + 7) / 8;
    fwrite(sps, 1, byte_count, f);
}

static void write_pps(FILE *f)
{
    uint8_t pps[8] = { 0xCE, 0x3C, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00 };
    write_start_code(f);
    write_nal_byte(f, 3, NAL_TYPE_PPS);
    fwrite(pps, 1, 4, f);
}

static void write_idr_header(FILE *f, int frame_num)
{
    uint8_t slice[16];
    memset(slice, 0, sizeof(slice));
    slice[0] = (uint8_t)((SLICE_TYPE_I << 5) | ((frame_num >> 8) & 0x0F));
    slice[1] = (uint8_t)(frame_num & 0xFF);

    write_start_code(f);
    write_nal_byte(f, 3, NAL_TYPE_SLICE_IDR);
    fwrite(slice, 1, 8, f);
}

static void write_non_idr_header(FILE *f, int frame_num, SliceType st)
{
    uint8_t slice[16];
    memset(slice, 0, sizeof(slice));
    slice[0] = (uint8_t)((st << 5) | ((frame_num >> 8) & 0x0F));
    slice[1] = (uint8_t)(frame_num & 0xFF);

    write_start_code(f);
    write_nal_byte(f, 1, NAL_TYPE_SLICE_NON_IDR);
    fwrite(slice, 1, 8, f);
}

static void write_coded_mb(FILE *f, const Macroblock *mb, SliceType st)
{
    uint8_t mb_data[128];
    int offset = 0;
    int i;

    mb_data[offset++] = (uint8_t)(mb->mb_type & 0x0F);
    mb_data[offset++] = (uint8_t)mb->qp;

    if (st == SLICE_TYPE_I) {
        mb_data[offset++] = (uint8_t)mb->intra_mode_16x16;
    } else {
        mb_data[offset++] = (uint8_t)((mb->mv_l0.x_qpel >> 2) & 0xFF);
        mb_data[offset++] = (uint8_t)((mb->mv_l0.y_qpel >> 2) & 0xFF);
        mb_data[offset++] = (uint8_t)mb->ref_idx_l0;
    }

    int coeff_idx = 0;
    for (i = 0; i < 16 && coeff_idx < mb->num_coeff / 8; i++) {
        mb_data[offset++] = (uint8_t)((mb->coeffs[i / 4][i % 4] + 128) & 0xFF);
        coeff_idx++;
    }

    mb_data[offset++] = 0x00;

    fwrite(mb_data, 1, offset, f);
}

static void encode_frame(H264Encoder *enc, EncFrame *frame)
{
    int fnum = enc->frame_count;
    int qp = rc_prepare_frame(&enc->rc);
    RCFrameType ftype = rc_get_frame_type(&enc->rc);
    SliceType st;
    int is_idr = 0;

    if (!enc->sps_written) {
        write_sps(enc->output_file, ENC_WIDTH, ENC_HEIGHT);
        enc->sps_written = 1;
    }
    if (!enc->pps_written) {
        write_pps(enc->output_file);
        enc->pps_written = 1;
    }

    if (ftype == FRAME_TYPE_I) {
        st = SLICE_TYPE_I;
        is_idr = 1;
    } else if (ftype == FRAME_TYPE_B) {
        st = SLICE_TYPE_B;
    } else {
        st = SLICE_TYPE_P;
    }

    if (is_idr)
        write_idr_header(enc->output_file, fnum);
    else
        write_non_idr_header(enc->output_file, fnum, st);

    Macroblock mb;
    int total_frame_bits = 0;
    int by, bx;

    uint8_t above[MB_SIZE], left[MB_SIZE];

    for (by = 0; by < MB_Y; by++) {
        for (bx = 0; bx < MB_X; bx++) {
            memset(above, 128, sizeof(above));
            memset(left, 128, sizeof(left));

            mb_init(&mb);
            mb.mb_type = MB_TYPE_INTRA_16x16;
            mb.intra_mode_16x16 = INTRA_DC;
            mb.qp = qp;

            uint8_t block_src[MB_SIZE * MB_SIZE];
            int sx, sy;
            for (sy = 0; sy < MB_SIZE; sy++) {
                for (sx = 0; sx < MB_SIZE; sx++) {
                    int fy = by * MB_SIZE + sy;
                    int fx = bx * MB_SIZE + sx;
                    block_src[sy * MB_SIZE + sx] = frame->y[fy][fx];
                }
            }

            int bits = mb_encode_intra(&mb, block_src, above, left, MB_SIZE, qp);
            total_frame_bits += bits;

            write_coded_mb(enc->output_file, &mb, st);
            enc->total_coeffs += mb.num_coeff / 8;
        }
    }

    rc_update_frame(&enc->rc, total_frame_bits);
    enc->total_bytes += total_frame_bits / 8;
    enc->frame_count++;

    if (enc->ref_count < 4) {
        int ri = enc->ref_count;
        for (sy = 0; sy < ENC_HEIGHT; sy++)
            for (sx = 0; sx < ENC_WIDTH; sx++)
                enc->ref_frames[ri].y[sy][sx] = frame->y[sy][sx];
        enc->ref_count++;
    }
}

static void generate_frame(EncFrame *frame, int fnum)
{
    int x, y;
    double fx = fnum * 0.1;
    for (y = 0; y < ENC_HEIGHT; y++) {
        for (x = 0; x < ENC_WIDTH; x++) {
            double val = 128.0 + 60.0 * sin(fx + (double)x * 0.1)
                         * cos(fx + (double)y * 0.05);
            int iv = (int)(val + 0.5);
            if (iv < 0) iv = 0; if (iv > 255) iv = 255;
            frame->y[y][x] = (uint8_t)iv;
        }
    }
    for (y = 0; y < ENC_HEIGHT/2; y++) {
        for (x = 0; x < ENC_WIDTH/2; x++) {
            frame->u[y][x] = 128;
            frame->v[y][x] = 128;
        }
    }
}

static void print_encoder_stats(const H264Encoder *enc)
{
    printf("\n=== Encoder Statistics ===\n");
    printf("Total frames: %d\n", enc->frame_count);
    printf("Total bytes: %d\n", enc->total_bytes);
    printf("Total coeffs: %d\n", enc->total_coeffs);
    printf("Average bits per frame: %d\n",
           enc->frame_count > 0 ? enc->total_bytes * 8 / enc->frame_count : 0);
    rc_print_status(&enc->rc);
}

static void print_usage(const char *prog)
{
    printf("Usage: %s <input.rgb> <width> <height> <fps> <output.264>\n", prog);
    printf("  Or:  %s --demo <output.264>\n", prog);
    printf("\n  --demo       Generate synthetic frames and encode them\n");
    printf("\nExamples:\n");
    printf("  %s input.rgb 1920 1080 30 output.264\n", prog);
    printf("  %s --demo test.264\n", prog);
}

static void print_info(void)
{
    printf("=== H.264 Baseline Encoder Demo ===\n");
    printf("This tool demonstrates a simplified H.264 encoding pipeline.\n");
    printf("Features: I/P/B prediction, 4x4 DCT, quantization, CAVLC, rate control.\n");
    printf("Note: Generates a non-standard bitstream for educational purposes.\n");
}

int main(int argc, char **argv)
{
    int demo_mode = 0;
    const char *out_path = NULL;
    int in_w = 0, in_h = 0, fps = 0;
    const char *input = NULL;
    int num_gen_frames = MAX_FRAME_SHOW;

    if (argc < 2) {
        print_info();
        print_usage(argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "--demo") == 0) {
        demo_mode = 1;
        if (argc < 3) {
            print_usage(argv[0]);
            return 1;
        }
        out_path = argv[2];
        if (argc > 3) num_gen_frames = atoi(argv[3]);
    } else {
        if (argc < 6) {
            print_usage(argv[0]);
            return 1;
        }
        input  = argv[1];
        in_w   = atoi(argv[2]);
        in_h   = atoi(argv[3]);
        fps    = atoi(argv[4]);
        out_path = argv[5];
    }

    print_info();
    printf("\n");

    if (!demo_mode && (in_w <= 0 || in_h <= 0 || fps <= 0 || !input)) {
        print_usage(argv[0]);
        return 1;
    }

    if (demo_mode) {
        H264Encoder enc;
        EncFrame frame;
        int i;

        enc_init(&enc, 500000, 30.0, out_path);

        printf("Encoding %d synthetic frames to %s ...\n", num_gen_frames, out_path);
        printf("Resolution: %dx%d\n", ENC_WIDTH, ENC_HEIGHT);

        for (i = 0; i < num_gen_frames; i++) {
            generate_frame(&frame, i);
            encode_frame(&enc, &frame);

            if (i % 10 == 0 || i == num_gen_frames - 1) {
                RCFrameType ft = rc_get_frame_type(&enc.rc);
                printf("  Frame %4d: %s, QP=%d, bits=%d, HRD=%.1f%%\r",
                       i, rc_frame_type_name(ft),
                       rc_get_qp(&enc.rc),
                       enc.total_bytes * 8,
                       hrd_buffer_fill_ratio(&enc.rc.hrd) * 100.0);
            }
        }
        printf("\n");

        print_encoder_stats(&enc);
        enc_close(&enc);
        printf("\nEncoded bitstream written to: %s\n", out_path);
    } else {
        printf("File-based encoding mode.\n");
        printf("Input: %s (%dx%d @ %d fps) -> Output: %s\n",
               input, in_w, in_h, fps, out_path);
        printf("(File reading not implemented in demo mode)\n");
    }

    return 0;
}
