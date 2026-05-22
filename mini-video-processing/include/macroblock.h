#ifndef MACROBLOCK_H
#define MACROBLOCK_H

#include <stdint.h>
#include <stddef.h>

#define MB_SIZE        16
#define BLOCK_SIZE      4
#define BLOCK_SIZE_8    8
#define MACROBLOCK_PIXELS (MB_SIZE * MB_SIZE)

/* ── 量化参数宏 ── */
#define QP_MAX 51
#define QP_MIN  0

/* ── 帧内预测模式 ── */
typedef enum {
    INTRA_DC        = 0,   /* 直流预测 */
    INTRA_VERTICAL  = 1,   /* 垂直预测 */
    INTRA_HORIZONTAL= 2,   /* 水平预测 */
    INTRA_PLANE     = 3    /* 平面预测 */
} IntraPredMode;

/* ── 宏块类型 ── */
typedef enum {
    MB_TYPE_INTRA_4x4,
    MB_TYPE_INTRA_16x16,
    MB_TYPE_INTER_16x16,
    MB_TYPE_INTER_16x8,
    MB_TYPE_INTER_8x16,
    MB_TYPE_INTER_8x8,
    MB_TYPE_SKIP
} MacroblockType;

/* ── 运动矢量（四分之一像素精度） ── */
typedef struct {
    int x_qpel;   /* 四分之一像素单位 */
    int y_qpel;
} MotionVector;

/* ── 参考帧索引 ── */
typedef struct {
    uint8_t *y_plane;
    uint8_t *u_plane;
    uint8_t *v_plane;
    int width;
    int height;
    int y_stride;
    int uv_stride;
} ReferenceFrame;

/* ── 宏块数据 ── */
typedef struct {
    MacroblockType mb_type;
    IntraPredMode intra_mode_16x16;
    IntraPredMode intra_mode_4x4[MB_SIZE];  /* 16 个 4×4 块各一个模式 */
    uint8_t reconstructed[MB_SIZE][MB_SIZE];  /* 重建 Y 分量 */
    uint8_t residual[MB_SIZE][MB_SIZE];       /* 残差 */
    int16_t coeffs[BLOCK_SIZE][BLOCK_SIZE];   /* DCT 系数（用于 4×4 块） */
    MotionVector mv_l0;   /* list0 运动矢量 */
    MotionVector mv_l1;   /* list1 运动矢量 */
    int ref_idx_l0;       /* list0 参考帧索引 */
    int ref_idx_l1;       /* list1 参考帧索引 */
    int qp;
    int num_coeff;
    int coded_block_pattern;
} Macroblock;

/* ── CAVLC 编码单元 ── */
typedef struct {
    int level;
    int run;
} CAVLCLevelRun;

typedef struct {
    CAVLCLevelRun pairs[BLOCK_SIZE * BLOCK_SIZE];
    int total_coeff;         /* 非零系数总数 */
    int trailing_ones;       /* 拖尾 1 的数量 */
    int count;
} CAVLCBlock;

/* ── 帧内预测 ── */
void intra_predict_4x4(const uint8_t *above, const uint8_t *left,
                       uint8_t *pred, int stride, IntraPredMode mode);
void intra_predict_16x16(const uint8_t *above, const uint8_t *left,
                         uint8_t *pred, int stride, IntraPredMode mode);

/* ── 4×4 整数 DCT 变换 ── */
void dct_4x4(const int16_t *input, int16_t *output);
void idct_4x4(const int16_t *input, int16_t *output);

/* ── 量化 ── */
void quantize_4x4(const int16_t *dct, int16_t *qcoeff, int qp);
void dequantize_4x4(const int16_t *qcoeff, int16_t *dct, int qp);

/* ── Zigzag 扫描 ── */
extern const int zigzag_4x4[16];
void zigzag_scan_4x4(const int16_t *block, int16_t *scanned);

/* ── 运动补偿 ── */
void motion_compensate(const ReferenceFrame *ref, const MotionVector *mv,
                       uint8_t *pred, int mb_x, int mb_y, int block_w, int block_h);

/* ── CAVLC 编码/解码模拟 ── */
int cavlc_encode_block(const int16_t *scanned, int count, CAVLCBlock *out);
void cavlc_print_block(const CAVLCBlock *block);

/* ── 宏块编解码 ── */
void mb_init(Macroblock *mb);
void mb_compute_residual(const uint8_t *orig, const uint8_t *pred, uint8_t *res);
void mb_reconstruct(const uint8_t *pred, const uint8_t *res, uint8_t *recon);

/* 宏块编码流程：预测→残差→DCT→量化→CAVLC */
int mb_encode_intra(Macroblock *mb, const uint8_t *plane, const uint8_t *above,
                    const uint8_t *left, int stride, int qp);
int mb_decode_intra(Macroblock *mb, uint8_t *plane, int stride);

/* 辅助：计算 PSNR */
double compute_psnr(const uint8_t *a, const uint8_t *b, int count);

#endif /* MACROBLOCK_H */
