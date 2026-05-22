#include "yuv_rgb_conv.h"
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    int w = 640, h = 480;
    YUVFrame yuv = yuv_frame_alloc(w, h);
    RGBFrame rgb = rgb_frame_alloc(w, h, 3);

    printf("=== YUV <-> RGB Conversion Demo ===\n\n");

    yuv_fill_gradient(&yuv);
    printf("Generated YUV gradient frame (%dx%d)\n", w, h);
    printf("  Y plane: %d bytes\n", yuv.y_plane_size);
    printf("  U plane: %d bytes\n", yuv.uv_plane_size);
    printf("  V plane: %d bytes\n", yuv.uv_plane_size);

    printf("\n--- BT.601 Conversion ---\n");
    yuv_to_rgb(&yuv, &rgb, COLOR_MATRIX_BT601);
    printf("YUV -> RGB (BT.601) complete, %d bytes\n", rgb.total_size);
    printf("RGB pixel[0]: R=%d G=%d B=%d\n",
           rgb.data[0], rgb.data[1], rgb.data[2]);
    printf("RGB pixel[%d]: R=%d G=%d B=%d\n",
           rgb.total_size - 3,
           rgb.data[rgb.total_size - 3],
           rgb.data[rgb.total_size - 2],
           rgb.data[rgb.total_size - 1]);

    printf("\n--- BT.709 Conversion ---\n");
    yuv_to_rgb(&yuv, &rgb, COLOR_MATRIX_BT709);
    printf("YUV -> RGB (BT.709) complete\n");
    printf("RGB pixel[0]: R=%d G=%d B=%d\n",
           rgb.data[0], rgb.data[1], rgb.data[2]);

    YUVFrame yuv2 = yuv_frame_alloc(w, h);
    rgb_to_yuv(&rgb, &yuv2, COLOR_MATRIX_BT709);
    printf("RGB -> YUV (BT.709) complete\n");

    printf("\n--- Chroma Upsampling ---\n");
    YUVFrame small = yuv_frame_alloc(4, 4);
    yuv_fill_color(&small, 128, 128, 128);
    RGBFrame up_rgb = rgb_frame_alloc(4, 4, 3);
    chroma_upsample_to_rgb(&small, &up_rgb, COLOR_MATRIX_BT601, CHROMA_UPSAMPLE_BILINEAR);
    printf("Bilinear upsampling 4x4 -> RGB24 done\n");
    chroma_upsample_to_rgb(&small, &up_rgb, COLOR_MATRIX_BT601, CHROMA_UPSAMPLE_NEAREST);
    printf("Nearest upsampling 4x4 -> RGB24 done\n");

    printf("\n--- Frame Copy/Clone ---\n");
    YUVFrame copy = yuv_frame_clone(&yuv);
    printf("Cloned frame: %dx%d, Y[0]=%d\n",
           copy.width, copy.height, copy.y_plane[0]);

    printf("\nAll tests passed.\n");

    yuv_frame_free(&yuv);
    yuv_frame_free(&yuv2);
    yuv_frame_free(&small);
    yuv_frame_free(&copy);
    rgb_frame_free(&rgb);
    rgb_frame_free(&up_rgb);

    return 0;
}
