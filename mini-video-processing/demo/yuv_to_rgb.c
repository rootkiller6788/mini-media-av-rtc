#include "yuv_rgb_conv.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define HEADER_LEN 54

static void write_bmp_header(FILE *f, int w, int h)
{
    uint8_t header[HEADER_LEN];
    int file_size = HEADER_LEN + w * h * 3;
    int padded_row = (w * 3 + 3) & ~3;
    file_size = HEADER_LEN + padded_row * h;

    memset(header, 0, HEADER_LEN);
    header[0] = 'B'; header[1] = 'M';
    header[2] = (uint8_t)(file_size);
    header[3] = (uint8_t)(file_size >> 8);
    header[4] = (uint8_t)(file_size >> 16);
    header[5] = (uint8_t)(file_size >> 24);
    header[10] = HEADER_LEN;
    header[14] = 40;
    header[18] = (uint8_t)(w);
    header[19] = (uint8_t)(w >> 8);
    header[20] = (uint8_t)(w >> 16);
    header[21] = (uint8_t)(w >> 24);
    header[22] = (uint8_t)(h);
    header[23] = (uint8_t)(h >> 8);
    header[24] = (uint8_t)(h >> 16);
    header[25] = (uint8_t)(h >> 24);
    header[26] = 1;
    header[28] = 24;
    fwrite(header, 1, HEADER_LEN, f);
}

static void convert_yuv_to_rgb_file(const char *yuv_path, const char *out_path,
                                    int w, int h, ColorMatrix matrix,
                                    ChromaUpsample upsample)
{
    FILE *fyuv = fopen(yuv_path, "rb");
    if (!fyuv) {
        printf("Error: Cannot open %s\n", yuv_path);
        return;
    }

    long y_size = (long)w * h;
    long uv_size = (long)(w / 2) * (h / 2);
    long frame_size = y_size + 2 * uv_size;

    fseek(fyuv, 0, SEEK_END);
    long file_size = ftell(fyuv);
    fseek(fyuv, 0, SEEK_SET);

    int num_frames = (int)(file_size / frame_size);
    printf("File: %s (%lld bytes, %d frame(s))\n", yuv_path,
           (long long)file_size, num_frames);
    printf("Resolution: %dx%d\n", w, h);
    printf("Color matrix: %s\n", matrix == COLOR_MATRIX_BT709 ? "BT.709" : "BT.601");
    printf("Chroma upsampling: %s\n",
           upsample == CHROMA_UPSAMPLE_BILINEAR ? "Bilinear" : "Nearest");

    FILE *fbmp = fopen(out_path, "wb");
    if (!fbmp) {
        printf("Error: Cannot create %s\n", out_path);
        fclose(fyuv);
        return;
    }

    write_bmp_header(fbmp, w, h);

    uint8_t *frame_buf = (uint8_t *)malloc(frame_size);
    YUVFrame yuv = yuv_frame_alloc(w, h);
    RGBFrame rgb = rgb_frame_alloc(w, h, 3);

    int frm;
    for (frm = 0; frm < num_frames; frm++) {
        size_t read_bytes = fread(frame_buf, 1, frame_size, fyuv);
        if (read_bytes != (size_t)frame_size) break;

        yuv_extract_planes(&yuv, frame_buf, w, h);

        if (upsample == CHROMA_UPSAMPLE_BILINEAR || upsample == CHROMA_UPSAMPLE_NEAREST)
            chroma_upsample_to_rgb(&yuv, &rgb, matrix, upsample);
        else
            yuv_to_rgb(&yuv, &rgb, matrix);

        int padded_row = (w * 3 + 3) & ~3;
        int y;
        for (y = h - 1; y >= 0; y--) {
            fwrite(rgb.data + y * rgb.stride, 1, w * 3, fbmp);
            if (padded_row > w * 3) {
                uint8_t pad[4] = {0, 0, 0, 0};
                fwrite(pad, 1, (size_t)(padded_row - w * 3), fbmp);
            }
        }

        printf("  Frame %d/%d converted\r", frm + 1, num_frames);
    }
    printf("\nDone. Output: %s\n", out_path);

    yuv_frame_free(&yuv);
    rgb_frame_free(&rgb);
    free(frame_buf);
    fclose(fyuv);
    fclose(fbmp);
}

static void generate_test_yuv(const char *path, int w, int h)
{
    FILE *f = fopen(path, "wb");
    if (!f) {
        printf("Error: Cannot create %s\n", path);
        return;
    }

    long y_size = (long)w * h;
    long uv_size = (long)(w / 2) * (h / 2);
    long frame_size = y_size + 2 * uv_size;

    YUVFrame yuv = yuv_frame_alloc(w, h);

    yuv_fill_gradient(&yuv);
    fwrite(yuv.y_plane, 1, yuv.y_plane_size, f);
    fwrite(yuv.u_plane, 1, yuv.uv_plane_size, f);
    fwrite(yuv.v_plane, 1, yuv.uv_plane_size, f);

    float kr, kg, kb;
    int y_off, uv_off;
    get_bt601_coeffs(&kr, &kg, &kb, &y_off, &uv_off);

    int x, y;
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            float r = ((float)x / w) * 255.0f;
            float g = ((float)y / h) * 255.0f;
            float b = 255.0f - ((float)(x + y) / (w + h)) * 255.0f;
            float yv = kr * r + kg * g + kb * b;
            float uv = 0.5f * (b - yv) / (1.0f - kb) + 128.0f;
            float vv = 0.5f * (r - yv) / (1.0f - kr) + 128.0f;
            yuv.y_plane[y * w + x] = (uint8_t)(yv + 0.5f);
            if (y % 2 == 0 && x % 2 == 0) {
                int uv_idx = (y/2) * (w/2) + (x/2);
                yuv.u_plane[uv_idx] = (uint8_t)(uv + 0.5f);
                yuv.v_plane[uv_idx] = (uint8_t)(vv + 0.5f);
            }
        }
    }

    fwrite(yuv.y_plane, 1, yuv.y_plane_size, f);
    fwrite(yuv.u_plane, 1, yuv.uv_plane_size, f);
    fwrite(yuv.v_plane, 1, yuv.uv_plane_size, f);

    yuv_frame_free(&yuv);
    fclose(f);
    printf("Generated test YUV: %s (%lld bytes, 2 frames)\n",
           path, (long long)(frame_size * 2));
}

static void print_usage(const char *prog)
{
    printf("Usage: %s <input.yuv> <width> <height> <output.bmp>\n", prog);
    printf("  Or:  %s --gen <width> <height> <output.yuv>\n", prog);
    printf("\nOptions:\n");
    printf("  --bt709       Use BT.709 color matrix (default: BT.601)\n");
    printf("  --bilinear    Use bilinear chroma upsampling (default: nearest)\n");
    printf("\nExamples:\n");
    printf("  %s input.yuv 1920 1080 output.bmp --bt709\n", prog);
    printf("  %s --gen 640 480 test.yuv\n", prog);
}

static void print_info(void)
{
    printf("=== YUV to RGB Converter ===\n");
    printf("This tool converts raw YUV420p files to BMP RGB images.\n");
    printf("Supports BT.601 and BT.709 color matrices.\n");
    printf("Supports nearest-neighbor and bilinear chroma upsampling.\n");
    printf("\n");
}

int main(int argc, char **argv)
{
    ColorMatrix matrix = COLOR_MATRIX_BT601;
    ChromaUpsample upsample = CHROMA_UPSAMPLE_NEAREST;

    if (argc < 2) {
        print_info();
        print_usage(argv[0]);
        return 1;
    }

    int ai = 1;

    if (strcmp(argv[1], "--gen") == 0) {
        if (argc < 5) {
            print_usage(argv[0]);
            return 1;
        }
        int gen_w = atoi(argv[2]);
        int gen_h = atoi(argv[3]);
        const char *gen_out = argv[4];
        if (gen_w <= 0 || gen_h <= 0 || (gen_w & 1) || (gen_h & 1)) {
            printf("Error: Width and height must be positive even numbers\n");
            return 1;
        }
        generate_test_yuv(gen_out, gen_w, gen_h);
        return 0;
    }

    int pos_args = 0;
    const char *input = NULL, *output = NULL;
    int in_w = 0, in_h = 0;

    for (; ai < argc; ai++) {
        if (strcmp(argv[ai], "--bt709") == 0) {
            matrix = COLOR_MATRIX_BT709;
        } else if (strcmp(argv[ai], "--bilinear") == 0) {
            upsample = CHROMA_UPSAMPLE_BILINEAR;
        } else if (pos_args == 0) {
            input = argv[ai]; pos_args++;
        } else if (pos_args == 1) {
            in_w = atoi(argv[ai]); pos_args++;
        } else if (pos_args == 2) {
            in_h = atoi(argv[ai]); pos_args++;
        } else if (pos_args == 3) {
            output = argv[ai]; pos_args++;
        }
    }

    if (!input || !output || in_w <= 0 || in_h <= 0) {
        print_usage(argv[0]);
        return 1;
    }

    if ((in_w & 1) || (in_h & 1)) {
        printf("Error: Width and height must be even for YUV420p\n");
        return 1;
    }

    print_info();
    printf("Input:  %s\nOutput: %s\n", input, output);
    printf("Size:   %dx%d\n", in_w, in_h);
    printf("Matrix: %s\n", matrix == COLOR_MATRIX_BT709 ? "BT.709 (HD)" : "BT.601 (SD)");
    printf("Upsample: %s\n\n", upsample == CHROMA_UPSAMPLE_BILINEAR ? "Bilinear" : "Nearest");

    convert_yuv_to_rgb_file(input, output, in_w, in_h, matrix, upsample);

    printf("Output file size: ");
    FILE *fs = fopen(output, "rb");
    if (fs) {
        fseek(fs, 0, SEEK_END);
        long sz = ftell(fs);
        printf("%ld bytes (%.1f KB)\n", sz, sz / 1024.0);
        fclose(fs);
    } else {
        printf("N/A\n");
    }

    return 0;
}
