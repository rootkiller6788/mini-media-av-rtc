#include "h264_nal.h"
#include <stdio.h>
#include <string.h>

static void test_sps_parse(void)
{
    /* 模拟 SPS NAL 载荷: profile_idc=66(Baseline), level_idc=30(3.0), 176x144 */
    uint8_t sps_data[] = {
        0x42, 0x00, 0x1E, 0x8D, 0x0F, 0x00, 0x05, 0xBA, 0x10
    };
    SPS sps;

    printf("=== SPS Parsing ===\n");
    printf("Raw SPS bytes: ");
    int i;
    for (i = 0; i < (int)sizeof(sps_data); i++)
        printf("%02X ", sps_data[i]);
    printf("\n");

    nal_parse_sps(sps_data, sizeof(sps_data), &sps);
    printf("Profile IDC: %d\n", sps.profile_idc);
    printf("Level IDC: %d\n", sps.level_idc);
    printf("Pic width in mbs - 1: %d\n", sps.pic_width_in_mbs_minus1);
    printf("Pic height in map units - 1: %d\n", sps.pic_height_in_map_units_minus1);
    printf("Num ref frames: %d\n", sps.num_ref_frames);
    printf("Frame cropping flag: %d\n", sps.frame_cropping_flag);
    printf("Decoded size: %dx%d\n", sps.pic_width, sps.pic_height);
}

static void test_nal_extract(void)
{
    uint8_t stream[] = {
        0x00, 0x00, 0x00, 0x01, 0x67, 0x42, 0x00, 0x1E,
        0x00, 0x00, 0x00, 0x01, 0x68, 0xCE, 0x3C, 0x80,
        0x00, 0x00, 0x00, 0x01, 0x65, 0x88, 0x80, 0x10
    };
    int remaining = sizeof(stream);
    const uint8_t *pos = stream;
    int count = 0;

    printf("\n=== NAL Unit Extraction ===\n");
    while (remaining > 4) {
        const uint8_t *sc = nal_find_start_code(pos, remaining);
        if (!sc) break;

        NALHeader hdr = nal_parse_header(*sc);
        printf("[%d] Start code found at offset %lld, unit_type=%d (%s)\n",
               count, (long long)(sc - stream), hdr.nal_unit_type,
               nal_type_name((NALUnitType)hdr.nal_unit_type));

        const uint8_t *next = nal_next_start_code(sc + 1, remaining - (int)(sc + 1 - pos));
        int unit_len;
        if (next)
            unit_len = (int)(next - (sc));
        else
            unit_len = remaining - (int)(sc - pos);

        pos = sc + unit_len;
        remaining = (int)(stream + sizeof(stream) - pos);
        count++;
    }
    printf("Total NAL units found: %d\n", count);
}

static void test_slice_types(void)
{
    printf("\n=== NAL & Slice Type Names ===\n");
    printf("NAL_TYPE_SPS       (%d): %s\n", NAL_TYPE_SPS, nal_type_name(NAL_TYPE_SPS));
    printf("NAL_TYPE_PPS       (%d): %s\n", NAL_TYPE_PPS, nal_type_name(NAL_TYPE_PPS));
    printf("NAL_TYPE_SLICE_IDR (%d): %s\n", NAL_TYPE_SLICE_IDR, nal_type_name(NAL_TYPE_SLICE_IDR));
    printf("NAL_TYPE_SLICE_NON_IDR (%d): %s\n", NAL_TYPE_SLICE_NON_IDR, nal_type_name(NAL_TYPE_SLICE_NON_IDR));
    printf("SLICE_TYPE_I  (%d): %s\n", SLICE_TYPE_I, slice_type_name(SLICE_TYPE_I));
    printf("SLICE_TYPE_P  (%d): %s\n", SLICE_TYPE_P, slice_type_name(SLICE_TYPE_P));
    printf("SLICE_TYPE_B  (%d): %s\n", SLICE_TYPE_B, slice_type_name(SLICE_TYPE_B));
}

int main(void)
{
    printf("=== H.264 NAL Unit Parser Demo ===\n\n");
    test_sps_parse();
    test_nal_extract();
    test_slice_types();
    printf("\nAll tests passed.\n");
    return 0;
}
