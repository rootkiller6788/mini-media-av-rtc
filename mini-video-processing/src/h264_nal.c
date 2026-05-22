#include "h264_nal.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

const uint8_t *nal_find_start_code(const uint8_t *data, int len)
{
    int i;
    for (i = 0; i < len - 3; i++) {
        if (data[i] == 0x00 && data[i+1] == 0x00) {
            if (data[i+2] == 0x01)
                return data + i + 3;
            if (i < len - 4 && data[i+2] == 0x00 && data[i+3] == 0x01)
                return data + i + 4;
        }
    }
    return NULL;
}

const uint8_t *nal_next_start_code(const uint8_t *data, int len)
{
    int i;
    for (i = 0; i < len - 3; i++) {
        if (data[i] == 0x00 && data[i+1] == 0x00) {
            if (data[i+2] == 0x01)
                return data + i;
            if (i < len - 4 && data[i+2] == 0x00 && data[i+3] == 0x01)
                return data + i;
        }
    }
    return NULL;
}

NALHeader nal_parse_header(uint8_t byte)
{
    NALHeader h;
    h.forbidden_zero_bit = (byte >> 7) & 0x01;
    h.nal_ref_idc       = (byte >> 5) & 0x03;
    h.nal_unit_type     = byte & 0x1F;
    return h;
}

int nal_extract_unit(const uint8_t *stream, int stream_len, NALUnit *unit)
{
    const uint8_t *start = nal_find_start_code(stream, stream_len);
    if (!start) return 0;

    unit->header = nal_parse_header(*start);
    unit->ref_idc = unit->header.nal_ref_idc;
    unit->unit_type = unit->header.nal_unit_type;
    unit->data = start + 1;

    int offset = (int)(start + 1 - stream);
    int remaining = stream_len - offset;
    const uint8_t *next = nal_next_start_code(start + 1, remaining);
    if (next)
        unit->data_len = (int)(next - (start + 1));
    else
        unit->data_len = remaining;

    return 1;
}

static int read_exp_golomb(const uint8_t *data, int *bit_pos, int len)
{
    int leading_zeros = 0;
    int byte_pos, bit_in_byte;

    while (*bit_pos < len * 8) {
        byte_pos = *bit_pos / 8;
        bit_in_byte = 7 - (*bit_pos % 8);
        int bit = (data[byte_pos] >> bit_in_byte) & 1;
        (*bit_pos)++;
        if (bit == 1) break;
        leading_zeros++;
    }

    int value = 1;
    int i;
    for (i = 0; i < leading_zeros; i++) {
        byte_pos = *bit_pos / 8;
        bit_in_byte = 7 - (*bit_pos % 8);
        int bit = (data[byte_pos] >> bit_in_byte) & 1;
        (*bit_pos)++;
        value = (value << 1) | bit;
    }
    return value - 1;
}

static int read_bits(const uint8_t *data, int *bit_pos, int num_bits, int len)
{
    int value = 0;
    int i;
    for (i = 0; i < num_bits; i++) {
        if (*bit_pos >= len * 8) break;
        int byte_pos = *bit_pos / 8;
        int bit_in_byte = 7 - (*bit_pos % 8);
        int bit = (data[byte_pos] >> bit_in_byte) & 1;
        (*bit_pos)++;
        value = (value << 1) | bit;
    }
    return value;
}

static int read_signed_eg(const uint8_t *data, int *bit_pos, int len)
{
    int val = read_exp_golomb(data, bit_pos, len);
    if (val % 2 == 0)
        return -(val >> 1);
    else
        return (val + 1) >> 1;
}

int nal_parse_sps(const uint8_t *data, int len, SPS *sps)
{
    int bit_pos = 0;

    (void)len;
    memset(sps, 0, sizeof(SPS));

    sps->profile_idc = read_bits(data, &bit_pos, 8, len);
    sps->constraint_set0_flag = read_bits(data, &bit_pos, 1, len);
    sps->constraint_set1_flag = read_bits(data, &bit_pos, 1, len);
    sps->constraint_set2_flag = read_bits(data, &bit_pos, 1, len);
    sps->constraint_set3_flag = read_bits(data, &bit_pos, 1, len);
    sps->constraint_set4_flag = read_bits(data, &bit_pos, 1, len);
    sps->constraint_set5_flag = read_bits(data, &bit_pos, 1, len);
    read_bits(data, &bit_pos, 2, len); /* reserved_zero_2bits */
    sps->level_idc = read_bits(data, &bit_pos, 8, len);
    sps->seq_parameter_set_id = read_exp_golomb(data, &bit_pos, len);

    if (sps->profile_idc == 100 || sps->profile_idc == 110 ||
        sps->profile_idc == 122 || sps->profile_idc == 244 ||
        sps->profile_idc == 44  || sps->profile_idc == 83  ||
        sps->profile_idc == 86  || sps->profile_idc == 118 ||
        sps->profile_idc == 128) {
        sps->chroma_format_idc = read_exp_golomb(data, &bit_pos, len);
        if (sps->chroma_format_idc == 3)
            read_bits(data, &bit_pos, 1, len); /* separate_colour_plane_flag */
        sps->bit_depth_luma_minus8   = read_exp_golomb(data, &bit_pos, len);
        sps->bit_depth_chroma_minus8 = read_exp_golomb(data, &bit_pos, len);
        read_bits(data, &bit_pos, 1, len); /* qpprime_y_zero_transform_bypass_flag */
        int seq_scaling = read_bits(data, &bit_pos, 1, len);
        if (seq_scaling) {
            int i;
            for (i = 0; i < 8; i++) {
                int seq_scaling_list_present = read_bits(data, &bit_pos, 1, len);
                if (seq_scaling_list_present) {
                    (void)seq_scaling_list_present;
                }
            }
        }
    } else {
        sps->chroma_format_idc = 1;
    }

    sps->log2_max_frame_num_minus4 = read_exp_golomb(data, &bit_pos, len);
    sps->pic_order_cnt_type = read_exp_golomb(data, &bit_pos, len);

    if (sps->pic_order_cnt_type == 0) {
        sps->log2_max_pic_order_cnt_lsb_minus4 = read_exp_golomb(data, &bit_pos, len);
    } else if (sps->pic_order_cnt_type == 1) {
        read_bits(data, &bit_pos, 1, len); /* delta_pic_order_always_zero_flag */
        read_signed_eg(data, &bit_pos, len); /* offset_for_non_ref_pic */
        read_signed_eg(data, &bit_pos, len); /* offset_for_top_to_bottom_field */
        int num_ref_frames_in_poc_cycle = read_exp_golomb(data, &bit_pos, len);
        int i;
        for (i = 0; i < num_ref_frames_in_poc_cycle; i++)
            read_signed_eg(data, &bit_pos, len);
    }

    sps->num_ref_frames = read_exp_golomb(data, &bit_pos, len);
    sps->gaps_in_frame_num_value_allowed_flag = read_bits(data, &bit_pos, 1, len);
    sps->pic_width_in_mbs_minus1 = read_exp_golomb(data, &bit_pos, len);
    sps->pic_height_in_map_units_minus1 = read_exp_golomb(data, &bit_pos, len);
    sps->frame_mbs_only_flag = read_bits(data, &bit_pos, 1, len);

    if (!sps->frame_mbs_only_flag)
        sps->mb_adaptive_frame_field_flag = read_bits(data, &bit_pos, 1, len);

    sps->direct_8x8_inference_flag = read_bits(data, &bit_pos, 1, len);
    sps->frame_cropping_flag = read_bits(data, &bit_pos, 1, len);

    if (sps->frame_cropping_flag) {
        sps->frame_crop_left_offset   = read_exp_golomb(data, &bit_pos, len);
        sps->frame_crop_right_offset  = read_exp_golomb(data, &bit_pos, len);
        sps->frame_crop_top_offset    = read_exp_golomb(data, &bit_pos, len);
        sps->frame_crop_bottom_offset = read_exp_golomb(data, &bit_pos, len);
    }

    sps_get_pic_size(sps, &sps->pic_width, &sps->pic_height);
    return 0;
}

int nal_parse_pps(const uint8_t *data, int len, PPS *pps)
{
    int bit_pos = 0;
    (void)len;
    memset(pps, 0, sizeof(PPS));

    pps->pps_id = read_exp_golomb(data, &bit_pos, len);
    pps->sps_id = read_exp_golomb(data, &bit_pos, len);
    pps->entropy_coding_mode_flag = read_bits(data, &bit_pos, 1, len);
    pps->pic_order_present_flag = read_bits(data, &bit_pos, 1, len);
    pps->num_slice_groups_minus1 = read_exp_golomb(data, &bit_pos, len);

    if (pps->num_slice_groups_minus1 > 0) {
        int slice_group_map_type = read_exp_golomb(data, &bit_pos, len);
        (void)slice_group_map_type;
    }

    pps->num_ref_idx_l0_active_minus1 = read_exp_golomb(data, &bit_pos, len);
    pps->num_ref_idx_l1_active_minus1 = read_exp_golomb(data, &bit_pos, len);
    pps->weighted_pred_flag = read_bits(data, &bit_pos, 1, len);
    pps->weighted_bipred_idc = read_bits(data, &bit_pos, 2, len);
    pps->pic_init_qp_minus26 = read_signed_eg(data, &bit_pos, len);
    pps->pic_init_qs_minus26 = read_signed_eg(data, &bit_pos, len);
    pps->chroma_qp_index_offset = read_signed_eg(data, &bit_pos, len);
    pps->deblocking_filter_control_present_flag = read_bits(data, &bit_pos, 1, len);
    pps->constrained_intra_pred_flag = read_bits(data, &bit_pos, 1, len);
    pps->redundant_pic_cnt_present_flag = read_bits(data, &bit_pos, 1, len);

    return 0;
}

int nal_parse_slice_header(const uint8_t *data, int len, SliceHeader *sh,
                           const SPS *sps, const PPS *pps)
{
    int bit_pos = 0;
    (void)len;
    (void)sps;
    (void)pps;
    memset(sh, 0, sizeof(SliceHeader));

    sh->first_mb_in_slice = read_exp_golomb(data, &bit_pos, len);
    int st = read_exp_golomb(data, &bit_pos, len);
    sh->slice_type = (SliceType)(st % 5);
    sh->pps_id = read_exp_golomb(data, &bit_pos, len);
    sh->frame_num = read_bits(data, &bit_pos,
                              sps->log2_max_frame_num_minus4 + 4, len);

    if (!sps->frame_mbs_only_flag) {
        sh->field_pic_flag = read_bits(data, &bit_pos, 1, len);
        if (sh->field_pic_flag)
            sh->bottom_field_flag = read_bits(data, &bit_pos, 1, len);
    }

    if (sh->slice_type == SLICE_TYPE_I || sh->slice_type == SLICE_TYPE_SI) {
        if (sh->slice_type == SLICE_TYPE_SI)
            sh->idr_pic_id = read_exp_golomb(data, &bit_pos, len);
    }

    if (sps->pic_order_cnt_type == 0) {
        sh->pic_order_cnt_lsb = read_bits(data, &bit_pos,
                                          sps->log2_max_pic_order_cnt_lsb_minus4 + 4, len);
        if (pps->pic_order_present_flag && !sh->field_pic_flag)
            sh->delta_pic_order_cnt_bottom = read_signed_eg(data, &bit_pos, len);
    }

    if (sps->pic_order_cnt_type == 1) {
        if (!sps->gaps_in_frame_num_value_allowed_flag) {
        }
        read_signed_eg(data, &bit_pos, len);
    }

    return 0;
}

void sps_get_pic_size(const SPS *sps, int *width, int *height)
{
    int sub_w = 2, sub_h = 2;
    if (sps->chroma_format_idc == 0) { sub_w = 1; sub_h = 1; }
    if (sps->chroma_format_idc == 3) { sub_w = 1; sub_h = 1; }

    *width  = (sps->pic_width_in_mbs_minus1 + 1) * 16;
    *height = (sps->pic_height_in_map_units_minus1 + 1) * 16 * (2 - sps->frame_mbs_only_flag);

    if (sps->frame_cropping_flag) {
        *width  -= sub_w * (sps->frame_crop_left_offset + sps->frame_crop_right_offset);
        *height -= sub_h * (sps->frame_crop_top_offset  + sps->frame_crop_bottom_offset) * (2 - sps->frame_mbs_only_flag);
    }
}

void nal_print_info(const NALUnit *unit)
{
    printf("NAL Unit:\n");
    printf("  Type: %s (%d)\n", nal_type_name((NALUnitType)unit->unit_type), unit->unit_type);
    printf("  Ref IDC: %d\n", unit->ref_idc);
    printf("  Forbidden: %d\n", unit->header.forbidden_zero_bit);
    printf("  Data length: %d bytes\n", unit->data_len);
}

void nal_print_raw(const NALUnit *unit, int max_bytes)
{
    int i, limit = unit->data_len < max_bytes ? unit->data_len : max_bytes;
    printf("  Raw (%d bytes): ", limit);
    for (i = 0; i < limit; i++)
        printf("%02X ", unit->data[i]);
    printf("\n");
}

const char *nal_type_name(NALUnitType type)
{
    switch (type) {
        case NAL_TYPE_SLICE_NON_IDR: return "Non-IDR Slice";
        case NAL_TYPE_SLICE_PART_A:  return "Slice Data Partition A";
        case NAL_TYPE_SLICE_PART_B:  return "Slice Data Partition B";
        case NAL_TYPE_SLICE_PART_C:  return "Slice Data Partition C";
        case NAL_TYPE_SLICE_IDR:     return "IDR Slice";
        case NAL_TYPE_SEI:           return "SEI";
        case NAL_TYPE_SPS:           return "SPS";
        case NAL_TYPE_PPS:           return "PPS";
        case NAL_TYPE_AUD:           return "Access Unit Delimiter";
        case NAL_TYPE_END_OF_SEQ:    return "End of Sequence";
        case NAL_TYPE_END_OF_STREAM: return "End of Stream";
        case NAL_TYPE_FILLER:        return "Filler Data";
        case NAL_TYPE_SPS_EXT:       return "SPS Extension";
        case NAL_TYPE_PREFIX:        return "Prefix NAL Unit";
        case NAL_TYPE_SUBSET_SPS:    return "Subset SPS";
        case NAL_TYPE_SLICE_AUX:     return "Auxiliary Slice";
        case NAL_TYPE_SLICE_EXT:     return "Slice Extension";
        default:                     return "Unknown";
    }
}

const char *slice_type_name(SliceType type)
{
    switch (type) {
        case SLICE_TYPE_P:  return "P-Slice";
        case SLICE_TYPE_B:  return "B-Slice";
        case SLICE_TYPE_I:  return "I-Slice";
        case SLICE_TYPE_SP: return "SP-Slice";
        case SLICE_TYPE_SI: return "SI-Slice";
        default:            return "Unknown";
    }
}
