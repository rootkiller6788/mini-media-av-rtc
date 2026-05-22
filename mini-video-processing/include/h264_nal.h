#ifndef H264_NAL_H
#define H264_NAL_H

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

#define NAL_START_CODE_3B 0x000001
#define NAL_START_CODE_4B 0x00000001
#define MAX_SPS_COUNT 32
#define MAX_PPS_COUNT 256

/* NAL 单元类型 (nal_unit_type) */
typedef enum {
    NAL_TYPE_UNSPECIFIED  = 0,
    NAL_TYPE_SLICE_NON_IDR = 1,
    NAL_TYPE_SLICE_PART_A = 2,
    NAL_TYPE_SLICE_PART_B = 3,
    NAL_TYPE_SLICE_PART_C = 4,
    NAL_TYPE_SLICE_IDR    = 5,
    NAL_TYPE_SEI          = 6,
    NAL_TYPE_SPS          = 7,
    NAL_TYPE_PPS          = 8,
    NAL_TYPE_AUD          = 9,
    NAL_TYPE_END_OF_SEQ   = 10,
    NAL_TYPE_END_OF_STREAM= 11,
    NAL_TYPE_FILLER       = 12,
    NAL_TYPE_SPS_EXT      = 13,
    NAL_TYPE_PREFIX       = 14,
    NAL_TYPE_SUBSET_SPS   = 15,
    NAL_TYPE_SLICE_AUX    = 19,
    NAL_TYPE_SLICE_EXT    = 20
} NALUnitType;

/* 片类型 */
typedef enum {
    SLICE_TYPE_P  = 0,
    SLICE_TYPE_B  = 1,
    SLICE_TYPE_I  = 2,
    SLICE_TYPE_SP = 3,
    SLICE_TYPE_SI = 4
} SliceType;

/* NAL 单元头部 (1 字节) */
typedef struct {
    uint8_t forbidden_zero_bit;   /* 1 bit, 必须为 0 */
    uint8_t nal_ref_idc;          /* 2 bits, 非 0 表示参考帧 */
    uint8_t nal_unit_type;        /* 5 bits, NAL 类型 */
} NALHeader;

/* NAL 单元 */
typedef struct {
    NALHeader header;
    const uint8_t *data;          /* 指向 NAL 载荷起始（不含起始码和头部） */
    int data_len;
    int ref_idc;
    int unit_type;
} NALUnit;

/* ── SPS (Sequence Parameter Set) ── */
typedef struct {
    int sps_id;
    int profile_idc;
    int constraint_set0_flag;
    int constraint_set1_flag;
    int constraint_set2_flag;
    int constraint_set3_flag;
    int constraint_set4_flag;
    int constraint_set5_flag;
    int level_idc;
    int seq_parameter_set_id;
    int chroma_format_idc;
    int bit_depth_luma_minus8;
    int bit_depth_chroma_minus8;
    int log2_max_frame_num_minus4;
    int pic_order_cnt_type;
    int log2_max_pic_order_cnt_lsb_minus4;
    int num_ref_frames;
    int gaps_in_frame_num_value_allowed_flag;
    int pic_width_in_mbs_minus1;
    int pic_height_in_map_units_minus1;
    int frame_mbs_only_flag;
    int mb_adaptive_frame_field_flag;
    int direct_8x8_inference_flag;
    int frame_cropping_flag;
    int frame_crop_left_offset;
    int frame_crop_right_offset;
    int frame_crop_top_offset;
    int frame_crop_bottom_offset;
    int pic_width;               /* 解码后宽度 */
    int pic_height;              /* 解码后高度 */
} SPS;

/* ── PPS (Picture Parameter Set) ── */
typedef struct {
    int pps_id;
    int sps_id;
    int entropy_coding_mode_flag;
    int pic_order_present_flag;
    int num_slice_groups_minus1;
    int num_ref_idx_l0_active_minus1;
    int num_ref_idx_l1_active_minus1;
    int weighted_pred_flag;
    int weighted_bipred_idc;
    int pic_init_qp_minus26;
    int pic_init_qs_minus26;
    int chroma_qp_index_offset;
    int deblocking_filter_control_present_flag;
    int constrained_intra_pred_flag;
    int redundant_pic_cnt_present_flag;
    int transform_8x8_mode_flag;
    int pic_scaling_matrix_present_flag;
} PPS;

/* ── Slice Header ── */
typedef struct {
    int first_mb_in_slice;
    SliceType slice_type;
    int pps_id;
    int frame_num;
    int field_pic_flag;
    int bottom_field_flag;
    int idr_pic_id;
    int pic_order_cnt_lsb;
    int delta_pic_order_cnt_bottom;
    int delta_pic_order_cnt[2];
    int redundant_pic_cnt;
    int direct_spatial_mv_pred_flag;
    int num_ref_idx_active_override_flag;
    int num_ref_idx_l0_active_minus1;
    int num_ref_idx_l1_active_minus1;
} SliceHeader;

/* ── API ── */

/* 在字节流中查找起始码，返回指向起始码后第一个字节的指针 */
const uint8_t *nal_find_start_code(const uint8_t *data, int len);
const uint8_t *nal_next_start_code(const uint8_t *data, int len);

/* 解析 NAL 头部 */
NALHeader nal_parse_header(uint8_t byte);

/* 从字节流提取一个 NAL 单元（含头部） */
int nal_extract_unit(const uint8_t *stream, int stream_len, NALUnit *unit);

/* 解析 SPS */
int nal_parse_sps(const uint8_t *data, int len, SPS *sps);

/* 解析 PPS */
int nal_parse_pps(const uint8_t *data, int len, PPS *pps);

/* 解析片头 */
int nal_parse_slice_header(const uint8_t *data, int len, SliceHeader *sh, const SPS *sps, const PPS *pps);

/* 计算解码后的图像尺寸（考虑裁剪） */
void sps_get_pic_size(const SPS *sps, int *width, int *height);

/* 打印 NAL 单元信息概述 */
void nal_print_info(const NALUnit *unit);
void nal_print_raw(const NALUnit *unit, int max_bytes);

/* NAL 类型名称 */
const char *nal_type_name(NALUnitType type);
const char *slice_type_name(SliceType type);

#endif /* H264_NAL_H */
