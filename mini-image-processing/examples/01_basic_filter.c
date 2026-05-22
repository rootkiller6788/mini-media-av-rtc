#include "mini_image/pixel_format.h"
#include "mini_image/image_filter.h"
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    const int32_t W = 640, H = 480;

    /* ── create source image  (RGB24 filled with a gradient) ─────────── */
    Image src = image_create(W, H, PIXEL_FORMAT_RGB24);
    if (!src.data) { fprintf(stderr, "Failed to create source image\n"); return 1; }
    for (int32_t y = 0; y < H; y++) {
        for (int32_t x = 0; x < W; x++) {
            uint8_t *p = src.data + ((size_t)y * W + x) * 3;
            p[0] = (uint8_t)(x * 255 / W);
            p[1] = (uint8_t)(y * 255 / H);
            p[2] = (uint8_t)(128);
        }
    }
    printf("[OK] Source RGB24 %dx%d created\n", W, H);

    /* ── destination images ─────────────────────────────────────────────── */
    Image dst_blur    = image_create(W, H, PIXEL_FORMAT_RGB24);
    Image dst_gauss   = image_create(W, H, PIXEL_FORMAT_RGB24);
    Image dst_sharpen = image_create(W, H, PIXEL_FORMAT_RGB24);
    Image dst_sobel   = image_create(W, H, PIXEL_FORMAT_GRAYSCALE8);
    Image dst_laplace = image_create(W, H, PIXEL_FORMAT_GRAYSCALE8);
    Image dst_bc      = image_create(W, H, PIXEL_FORMAT_RGB24);
    Image dst_hist    = image_create(W, H, PIXEL_FORMAT_RGB24);
    if (!dst_blur.data || !dst_gauss.data || !dst_sharpen.data
        || !dst_sobel.data || !dst_laplace.data || !dst_bc.data
        || !dst_hist.data) {
        fprintf(stderr, "Failed to allocate destination images\n"); return 1;
    }

    /* ── Box blur  (radius 5) ───────────────────────────────────────────── */
    if (box_blur_apply(&src, &dst_blur, 5) == 0)
        printf("[OK] Box blur (r=5) applied\n");
    else
        printf("[FAIL] Box blur\n");

    /* ── Gaussian blur  (radius 3, sigma 1.6) ───────────────────────────── */
    if (gaussian_blur_apply(&src, &dst_gauss, 3, 1.6f) == 0)
        printf("[OK] Gaussian blur (r=3, sigma=1.6) applied\n");
    else
        printf("[FAIL] Gaussian blur\n");

    /* ── Sharpen  (amount 1.5, threshold 2.0) ───────────────────────────── */
    if (sharpen_apply(&src, &dst_sharpen, 1.5f, 2.0f) == 0)
        printf("[OK] Sharpen (amount=1.5, threshold=2.0) applied\n");
    else
        printf("[FAIL] Sharpen\n");

    /* ── Sobel edge detection ────────────────────────────────────────────── */
    {
        Image gray = image_create(W, H, PIXEL_FORMAT_GRAYSCALE8);
        rgb24_to_grayscale(&src, &gray);
        if (sobel_edge_detect(&gray, &dst_sobel) == 0)
            printf("[OK] Sobel edge detection applied\n");
        else
            printf("[FAIL] Sobel\n");
        image_destroy(&gray);
    }

    /* ── Laplacian edge detection ───────────────────────────────────────── */
    {
        Image gray = image_create(W, H, PIXEL_FORMAT_GRAYSCALE8);
        rgb24_to_grayscale(&src, &gray);
        if (laplacian_edge_detect(&gray, &dst_laplace) == 0)
            printf("[OK] Laplacian edge detection applied\n");
        else
            printf("[FAIL] Laplacian\n");
        image_destroy(&gray);
    }

    /* ── Brightness / contrast ──────────────────────────────────────────── */
    if (brightness_contrast_adjust(&src, &dst_bc, 10.0f, 1.2f) == 0)
        printf("[OK] Brightness/contrast applied (b=10, c=1.2)\n");
    else
        printf("[FAIL] Brightness/contrast\n");

    /* ── Histogram equalisation ─────────────────────────────────────────── */
    if (histogram_equalize(&src, &dst_hist) == 0)
        printf("[OK] Histogram equalisation applied\n");
    else
        printf("[FAIL] Histogram equalisation\n");

    /* ── Custom convolution  (emboss kernel) ────────────────────────────── */
    {
        Image dst_conv = image_create(W, H, PIXEL_FORMAT_RGB24);
        Kernel2D k;
        kernel2d_init(&k, 3);
        float emboss[9] = {-2,-1,0, -1,1,1, 0,1,2};
        kernel2d_fill(&k, emboss);
        if (convolution_2d(&src, &dst_conv, &k) == 0)
            printf("[OK] Custom convolution (emboss 3x3) applied\n");
        else
            printf("[FAIL] Custom convolution\n");
        kernel2d_free(&k);
        image_destroy(&dst_conv);
    }

    /* ── cleanup ────────────────────────────────────────────────────────── */
    image_destroy(&src);
    image_destroy(&dst_blur);
    image_destroy(&dst_gauss);
    image_destroy(&dst_sharpen);
    image_destroy(&dst_sobel);
    image_destroy(&dst_laplace);
    image_destroy(&dst_bc);
    image_destroy(&dst_hist);

    printf("All filter examples completed.\n");
    return 0;
}
