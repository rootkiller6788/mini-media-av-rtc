#ifndef RATE_CONTROL_H
#define RATE_CONTROL_H

#include <stdint.h>
#include <stddef.h>

#define GOP_MAX_SIZE       64
#define GOP_DEFAULT_SIZE   12
#define HRD_BUFFER_SIZE    (2 * 1024 * 1024)  /* 2 MB */
#define QP_DEFAULT         26
#define QP_MAX             51
#define QP_MIN              0

/* ── 码率控制模式 ── */
typedef enum {
    RC_MODE_CBR,    /* 恒定码率 */
    RC_MODE_VBR,    /* 可变码率 */
    RC_MODE_CQP     /* 固定 QP */
} RCMode;

/* ── 帧类型 ── */
typedef enum {
    FRAME_TYPE_I,
    FRAME_TYPE_P,
    FRAME_TYPE_B
} RCFrameType;

/* ── GOP 结构 ── */
typedef struct {
    RCFrameType pattern[GOP_MAX_SIZE];
    int size;              /* GOP 中的帧数 */
    int idr_interval;      /* IDR 帧间隔 */
} GOPStructure;

/* ── HRD (Hypothetical Reference Decoder) 缓冲 ── */
typedef struct {
    int64_t buffer_size;         /* 缓冲区大小 (bits) */
    int64_t buffer_fullness;     /* 当前缓冲占用量 (bits) */
    int64_t target_fullness;     /* 目标占用量 (bits) */
    int64_t bitrate;             /* 目标比特率 (bps) */
    double   frame_rate;         /* 帧率 */
    int64_t bits_per_frame;      /* 每帧平均比特数 */
    int64_t removal_time;        /* 移除时间 */
} HRDBuffer;

/* ── 码率控制状态 ── */
typedef struct {
    RCMode mode;
    int64_t target_bitrate;      /* bps */
    int64_t max_bitrate;         /* VBR 最大 bps */
    double  frame_rate;
    int     width;
    int     height;

    /* 当前帧 */
    int     frame_count;
    int     picture_order_count;
    int     current_qp;
    RCFrameType current_frame_type;

    /* GOP */
    GOPStructure gop;
    int     gop_index;           /* 当前在 GOP 中的位置 */
    int     gop_qp;
    int     gop_bits_allocated;
    int     gop_bits_used;

    /* HRD */
    HRDBuffer hrd;

    /* QP 统计 */
    double  qp_avg;
    int     qp_min;
    int     qp_max;

    /* 复杂度估计 */
    double  complexity_i;
    double  complexity_p;
    double  complexity_b;

    /* 帧比特统计 */
    int64_t bits_i;
    int64_t bits_p;
    int64_t bits_b;
    int64_t bits_total;

    /* 帧层级 QP 偏移 */
    int     qp_offset_i;
    int     qp_offset_b;
} RateControl;

/* ── API ── */

/* 初始化码率控制 */
void rc_init(RateControl *rc, RCMode mode, int64_t bitrate, double fps,
             int width, int height);

/* 设置 VBR 模式下最大码率 */
void rc_set_max_bitrate(RateControl *rc, int64_t max_br);

/* GOP 结构初始化 */
void gop_init(GOPStructure *gop, int size, int b_frames);
void gop_init_pattern(GOPStructure *gop, const RCFrameType *p, int count);

/* 获取 GOP 指定位置的帧类型 */
RCFrameType gop_get_frame_type(const GOPStructure *gop, int index);

/* 编码前调用：确定当前帧类型和 QP */
int rc_prepare_frame(RateControl *rc);

/* QP 计算 */
int rc_compute_qp(RateControl *rc, RCFrameType ftype);

/* 基于码率-失真的 QP 选择 */
int rc_select_qp_rd(RateControl *rc, double complexity, RCFrameType ftype);

/* 编码后调用：更新状态（传入实际编码比特数） */
void rc_update_frame(RateControl *rc, int bits_used);

/* 基于缓冲占满程度的 QP 调整 */
void rc_adjust_qp_by_buffer(RateControl *rc);

/* HRD 缓冲操作 */
void hrd_init(HRDBuffer *hrd, int64_t bitrate, double fps);
int  hrd_can_output(HRDBuffer *hrd, int bits);
void hrd_add_bits(HRDBuffer *hrd, int bits);
void hrd_remove_bits(HRDBuffer *hrd);
int  hrd_is_underflow(HRDBuffer *hrd);
int  hrd_is_overflow(HRDBuffer *hrd);
double hrd_buffer_fill_ratio(const HRDBuffer *hrd);

/* 帧类型判断 */
RCFrameType rc_get_frame_type(const RateControl *rc);
const char *rc_frame_type_name(RCFrameType t);

/* 状态查询 */
void rc_print_status(const RateControl *rc);
int  rc_get_qp(const RateControl *rc);

#endif /* RATE_CONTROL_H */
