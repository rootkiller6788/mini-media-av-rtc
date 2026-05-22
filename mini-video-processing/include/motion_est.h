#ifndef MOTION_EST_H
#define MOTION_EST_H

#include <stdint.h>
#include <stddef.h>

#define SEARCH_RANGE      16
#define MAX_REF_FRAMES     4
#define BLOCK_MATCH_SIZE  16

/* ── 运动矢量（四分之一像素精度） ── */
typedef struct {
    int x_qpel;     /* 四分之一像素单位 */
    int y_qpel;
    int ref_idx;    /* 参考帧索引 */
} MV;

/* ── 搜索算法 ── */
typedef enum {
    ME_FULL_SEARCH,       /* 全搜索 */
    ME_DIAMOND_SEARCH,    /* 菱形搜索 */
    ME_HEXAGON_SEARCH,    /* 六边形搜索 */
    ME_LOG_SEARCH         /* 对数搜索 */
} MESearchAlgo;

/* ── 预测方向 ── */
typedef enum {
    ME_PRED_FORWARD,      /* 前向预测 (P-frame) */
    ME_PRED_BACKWARD,     /* 后向预测 (B-frame) */
    ME_PRED_BI_DIR        /* 双向预测 (B-frame) */
} MEPredDir;

/* ── 参考帧 ── */
typedef struct {
    uint8_t *y_plane;
    uint8_t *u_plane;
    uint8_t *v_plane;
    int width;
    int height;
    int y_stride;
    int uv_stride;
    int frame_num;
    int poc;               /* 图像顺序计数 */
} MEReferenceFrame;

/* ── 运动估计配置 ── */
typedef struct {
    MESearchAlgo algo;
    int search_range;          /* 整数像素搜索范围 */
    int subpel_refinement;     /* 是否进行亚像素精化 */
    int half_pel_enabled;      /* 半像素搜索 */
    int quarter_pel_enabled;   /* 四分之一像素搜索 */
    int early_termination;     /* 提前终止阈值 */
    int max_ref_frames;
    int block_width;
    int block_height;
} MEConfig;

/* ── 运动估计结果 ── */
typedef struct {
    MV mv;                     /* 最佳运动矢量 */
    uint32_t sad;              /* SAD 值 */
    int valid;                 /* 结果有效 */
} MEResult;

/* ── API ── */

/* 初始化运动估计配置 */
void me_config_init(MEConfig *cfg);
void me_config_set_algo(MEConfig *cfg, MESearchAlgo algo);

/* 计算 SAD (Sum of Absolute Differences) */
uint32_t me_sad_16x16(const uint8_t *cur, int cur_stride,
                      const uint8_t *ref, int ref_stride);
uint32_t me_sad_8x8(const uint8_t *cur, int cur_stride,
                    const uint8_t *ref, int ref_stride);
uint32_t me_sad_4x4(const uint8_t *cur, int cur_stride,
                    const uint8_t *ref, int ref_stride);

/* 整像素运动搜索（全搜索） */
MEResult me_full_search(const uint8_t *cur, int cur_stride,
                        const MEReferenceFrame *ref,
                        int mb_x, int mb_y,
                        const MEConfig *cfg);

/* 菱形搜索（快速） */
MEResult me_diamond_search(const uint8_t *cur, int cur_stride,
                           const MEReferenceFrame *ref,
                           int mb_x, int mb_y,
                           const MEConfig *cfg);

/* 运动搜索主入口（根据配置选择算法） */
MEResult me_search(const uint8_t *cur, int cur_stride,
                   const MEReferenceFrame *ref,
                   int mb_x, int mb_y,
                   const MEConfig *cfg);

/* 亚像素精化：半像素 */
MEResult me_half_pel_refine(const uint8_t *cur, int cur_stride,
                            const MEReferenceFrame *ref,
                            int mb_x, int mb_y,
                            const MV *integer_mv,
                            const MEConfig *cfg);

/* 亚像素精化：四分之一像素 */
MEResult me_quarter_pel_refine(const uint8_t *cur, int cur_stride,
                               const MEReferenceFrame *ref,
                               int mb_x, int mb_y,
                               const MV *half_mv,
                               const MEConfig *cfg);

/* 亚像素插值 */
void me_half_pel_interpolate(const MEReferenceFrame *src, uint8_t *half_pel, int out_stride);
void me_quarter_pel_interpolate(const uint8_t *half_pel, int stride, int w, int h,
                                uint8_t *quarter_pel, int out_stride);

/* MV 预测：取左、上、右上邻居的中值 */
MV me_mv_median_predictor(const MV *mv_a, const MV *mv_b, const MV *mv_c);
MV me_mv_median(const MV *left_mv, const MV *top_mv, const MV *topright_mv);

/* Multi-reference 搜索：在多个参考帧中搜索最佳匹配 */
MEResult me_multi_ref_search(const uint8_t *cur, int cur_stride,
                             const MEReferenceFrame *refs, int num_refs,
                             int mb_x, int mb_y,
                             const MEConfig *cfg);

/* 前向/后向预测 (B-frame) */
void me_b_frame_predict(const uint8_t *cur, int cur_stride,
                        const MEReferenceFrame *ref_fwd,
                        const MEReferenceFrame *ref_bwd,
                        int mb_x, int mb_y,
                        const MEConfig *cfg,
                        MEResult *result_fwd,
                        MEResult *result_bwd,
                        MEResult *result_bidir);

/* 辅助函数 */
uint32_t me_subpel_sad(const uint8_t *cur, int cur_stride,
                       const uint8_t *ref, int ref_stride,
                       int w, int h);
void me_result_init(MEResult *result);

#endif /* MOTION_EST_H */
