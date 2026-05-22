#include "motion_est.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>

void me_config_init(MEConfig *cfg)
{
    cfg->algo = ME_DIAMOND_SEARCH;
    cfg->search_range = SEARCH_RANGE;
    cfg->subpel_refinement = 1;
    cfg->half_pel_enabled = 1;
    cfg->quarter_pel_enabled = 1;
    cfg->early_termination = 128;
    cfg->max_ref_frames = MAX_REF_FRAMES;
    cfg->block_width = BLOCK_MATCH_SIZE;
    cfg->block_height = BLOCK_MATCH_SIZE;
}

void me_config_set_algo(MEConfig *cfg, MESearchAlgo algo)
{
    cfg->algo = algo;
}

void me_result_init(MEResult *result)
{
    result->mv.x_qpel = 0;
    result->mv.y_qpel = 0;
    result->mv.ref_idx = 0;
    result->sad = UINT32_MAX;
    result->valid = 0;
}

uint32_t me_sad_16x16(const uint8_t *cur, int cur_stride,
                      const uint8_t *ref, int ref_stride)
{
    return me_subpel_sad(cur, cur_stride, ref, ref_stride, 16, 16);
}

uint32_t me_sad_8x8(const uint8_t *cur, int cur_stride,
                    const uint8_t *ref, int ref_stride)
{
    return me_subpel_sad(cur, cur_stride, ref, ref_stride, 8, 8);
}

uint32_t me_sad_4x4(const uint8_t *cur, int cur_stride,
                    const uint8_t *ref, int ref_stride)
{
    return me_subpel_sad(cur, cur_stride, ref, ref_stride, 4, 4);
}

uint32_t me_subpel_sad(const uint8_t *cur, int cur_stride,
                       const uint8_t *ref, int ref_stride,
                       int w, int h)
{
    uint32_t sad = 0;
    int y, x;
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            int a = cur[y * cur_stride + x];
            int b = ref[y * ref_stride + x];
            sad += (uint32_t)((a > b) ? (a - b) : (b - a));
        }
    }
    return sad;
}

MEResult me_full_search(const uint8_t *cur, int cur_stride,
                        const MEReferenceFrame *ref,
                        int mb_x, int mb_y,
                        const MEConfig *cfg)
{
    MEResult best;
    me_result_init(&best);
    best.valid = 1;

    int center_x = mb_x * cfg->block_width;
    int center_y = mb_y * cfg->block_height;
    int sr = cfg->search_range;
    int bx, by;

    for (by = -sr; by <= sr; by++) {
        for (bx = -sr; bx <= sr; bx++) {
            int rx = center_x + bx;
            int ry = center_y + by;

            if (rx < 0 || ry < 0 ||
                rx + cfg->block_width  > ref->width ||
                ry + cfg->block_height > ref->height)
                continue;

            const uint8_t *ref_ptr = ref->y_plane + ry * ref->y_stride + rx;
            uint32_t sad = me_subpel_sad(cur, cur_stride, ref_ptr, ref->y_stride,
                                         cfg->block_width, cfg->block_height);
            if (sad < best.sad) {
                best.sad = sad;
                best.mv.x_qpel = bx * 4;
                best.mv.y_qpel = by * 4;
                best.mv.ref_idx = 0;
            }
        }
    }
    return best;
}

static const int diamond_pattern[4][2] = {
    { 0, -1}, { 1,  0}, { 0,  1}, {-1,  0}
};

static const int large_diamond[8][2] = {
    { 0, -2}, { 2,  0}, { 0,  2}, {-2,  0},
    { 1, -1}, { 1,  1}, {-1,  1}, {-1, -1}
};

MEResult me_diamond_search(const uint8_t *cur, int cur_stride,
                           const MEReferenceFrame *ref,
                           int mb_x, int mb_y,
                           const MEConfig *cfg)
{
    MEResult best;
    me_result_init(&best);
    best.valid = 1;

    int cx = mb_x * cfg->block_width;
    int cy = mb_y * cfg->block_height;
    int step = (cfg->search_range > 2) ? cfg->search_range : 2;

    int bmv_x = 0, bmv_y = 0;
    const uint8_t *r0 = ref->y_plane + cy * ref->y_stride + cx;
    best.sad = me_subpel_sad(cur, cur_stride, r0, ref->y_stride,
                             cfg->block_width, cfg->block_height);

    while (step >= 1) {
        int found = 0;
        int points = (step >= 2) ? 8 : 4;
        const int (*pattern)[2] = (step >= 2) ? large_diamond : diamond_pattern;
        int k;
        for (k = 0; k < points; k++) {
            int tx = bmv_x + pattern[k][0] * step;
            int ty = bmv_y + pattern[k][1] * step;
            int rx = cx + tx;
            int ry = cy + ty;
            if (rx < 0 || ry < 0 ||
                rx + cfg->block_width  > ref->width ||
                ry + cfg->block_height > ref->height)
                continue;
            const uint8_t *rp = ref->y_plane + ry * ref->y_stride + rx;
            uint32_t sad = me_subpel_sad(cur, cur_stride, rp, ref->y_stride,
                                         cfg->block_width, cfg->block_height);
            if (sad < best.sad) {
                best.sad = sad;
                bmv_x = tx;
                bmv_y = ty;
                found = 1;
            }
        }
        if (!found) step /= 2;
    }

    best.mv.x_qpel = bmv_x * 4;
    best.mv.y_qpel = bmv_y * 4;
    best.mv.ref_idx = 0;
    return best;
}

MEResult me_search(const uint8_t *cur, int cur_stride,
                   const MEReferenceFrame *ref,
                   int mb_x, int mb_y,
                   const MEConfig *cfg)
{
    MEResult result;
    switch (cfg->algo) {
        case ME_FULL_SEARCH:
            result = me_full_search(cur, cur_stride, ref, mb_x, mb_y, cfg);
            break;
        case ME_HEXAGON_SEARCH:
        case ME_LOG_SEARCH:
        case ME_DIAMOND_SEARCH:
        default:
            result = me_diamond_search(cur, cur_stride, ref, mb_x, mb_y, cfg);
            break;
    }

    if (cfg->subpel_refinement && cfg->half_pel_enabled)
        result = me_half_pel_refine(cur, cur_stride, ref, mb_x, mb_y,
                                    &result.mv, cfg);
    if (cfg->subpel_refinement && cfg->quarter_pel_enabled)
        result = me_quarter_pel_refine(cur, cur_stride, ref, mb_x, mb_y,
                                       &result.mv, cfg);

    return result;
}

MEResult me_half_pel_refine(const uint8_t *cur, int cur_stride,
                            const MEReferenceFrame *ref,
                            int mb_x, int mb_y,
                            const MV *integer_mv,
                            const MEConfig *cfg)
{
    MEResult best;
    me_result_init(&best);
    best.mv = *integer_mv;
    best.valid = 1;

    int cx = mb_x * cfg->block_width + integer_mv->x_qpel / 4;
    int cy = mb_y * cfg->block_height + integer_mv->y_qpel / 4;
    const uint8_t *rbase = ref->y_plane + cy * ref->y_stride + cx;

    best.sad = me_subpel_sad(cur, cur_stride, rbase, ref->y_stride,
                             cfg->block_width, cfg->block_height);

    int hp[9][2] = {{0,0},{0,2},{0,-2},{2,0},{-2,0},{2,2},{2,-2},{-2,2},{-2,-2}};
    int k;
    for (k = 1; k < 9; k++) {
        int hx = hp[k][0], hy = hp[k][1];
        int rx = cx + hx / 4, ry = cy + hy / 4;
        if (rx < 0 || ry < 0 ||
            rx + cfg->block_width  > ref->width ||
            ry + cfg->block_height > ref->height)
            continue;
        const uint8_t *rp = ref->y_plane + ry * ref->y_stride + rx;
        uint32_t sad = me_subpel_sad(cur, cur_stride, rp, ref->y_stride,
                                     cfg->block_width, cfg->block_height);
        if (sad < best.sad) {
            best.sad = sad;
            best.mv.x_qpel += hx;
            best.mv.y_qpel += hy;
        }
    }
    return best;
}

MEResult me_quarter_pel_refine(const uint8_t *cur, int cur_stride,
                               const MEReferenceFrame *ref,
                               int mb_x, int mb_y,
                               const MV *half_mv,
                               const MEConfig *cfg)
{
    MEResult best;
    me_result_init(&best);
    best.mv = *half_mv;
    best.valid = 1;

    int cx_qpel = mb_x * cfg->block_width * 4 + half_mv->x_qpel;
    int cy_qpel = mb_y * cfg->block_height * 4 + half_mv->y_qpel;
    int base_x = cx_qpel / 4, base_y = cy_qpel / 4;
    const uint8_t *rbase = ref->y_plane + base_y * ref->y_stride + base_x;

    best.sad = me_subpel_sad(cur, cur_stride, rbase, ref->y_stride,
                             cfg->block_width, cfg->block_height);

    int qp[9][2] = {{0,0},{0,1},{0,-1},{1,0},{-1,0},{1,1},{1,-1},{-1,1},{-1,-1}};
    int k;
    for (k = 1; k < 9; k++) {
        int qx = cx_qpel + qp[k][0];
        int qy = cy_qpel + qp[k][1];
        int rx = qx / 4, ry = qy / 4;
        if (rx < 0 || ry < 0 ||
            rx + cfg->block_width  > ref->width ||
            ry + cfg->block_height > ref->height)
            continue;
        const uint8_t *rp = ref->y_plane + ry * ref->y_stride + rx;
        uint32_t sad = me_subpel_sad(cur, cur_stride, rp, ref->y_stride,
                                     cfg->block_width, cfg->block_height);
        if (sad < best.sad) {
            best.sad = sad;
            best.mv.x_qpel += qp[k][0];
            best.mv.y_qpel += qp[k][1];
        }
    }
    return best;
}

void me_half_pel_interpolate(const MEReferenceFrame *src, uint8_t *half_pel, int out_stride)
{
    int x, y;
    int w = src->width * 2 - 1, h = src->height * 2 - 1;
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            int sx = x / 2, sy = y / 2;
            int sum, count;
            if (x % 2 == 0 && y % 2 == 0) {
                half_pel[y * out_stride + x] = src->y_plane[sy * src->y_stride + sx];
            } else if (x % 2 == 1 && y % 2 == 0) {
                int a = src->y_plane[sy * src->y_stride + sx];
                int b = (sx + 1 < src->width) ? src->y_plane[sy * src->y_stride + sx + 1] : a;
                half_pel[y * out_stride + x] = (uint8_t)((a + b + 1) / 2);
            } else if (x % 2 == 0 && y % 2 == 1) {
                int a = src->y_plane[sy * src->y_stride + sx];
                int b = (sy + 1 < src->height) ? src->y_plane[(sy+1)*src->y_stride+sx] : a;
                half_pel[y * out_stride + x] = (uint8_t)((a + b + 1) / 2);
            } else {
                sum = 0; count = 0;
                if (sy < src->height && sx < src->width) { sum += src->y_plane[sy*src->y_stride+sx]; count++; }
                if (sy < src->height && sx+1 < src->width) { sum += src->y_plane[sy*src->y_stride+sx+1]; count++; }
                if (sy+1 < src->height && sx < src->width) { sum += src->y_plane[(sy+1)*src->y_stride+sx]; count++; }
                if (sy+1 < src->height && sx+1 < src->width) { sum += src->y_plane[(sy+1)*src->y_stride+sx+1]; count++; }
                half_pel[y * out_stride + x] = (uint8_t)((sum + count/2) / (count > 0 ? count : 1));
            }
        }
    }
    (void)sum; (void)count;
}

void me_quarter_pel_interpolate(const uint8_t *half_pel, int stride, int w, int h,
                                uint8_t *quarter_pel, int out_stride)
{
    int x, y;
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            int hx = x / 2, hy = y / 2;
            int a = half_pel[hy * stride + hx];
            int b = (x % 2 == 1 && hx+1 < w) ? half_pel[hy*stride+hx+1] : a;
            quarter_pel[y * out_stride + x] = (uint8_t)((a + b + 1) / 2);
        }
    }
}

MV me_mv_median_predictor(const MV *mv_a, const MV *mv_b, const MV *mv_c)
{
    return me_mv_median(mv_a, mv_b, mv_c);
}

MV me_mv_median(const MV *left_mv, const MV *top_mv, const MV *topright_mv)
{
    MV result;
    int ax = left_mv->x_qpel, ay = left_mv->y_qpel;
    int bx = top_mv->x_qpel, by = top_mv->y_qpel;
    int cx = topright_mv->x_qpel, cy = topright_mv->y_qpel;

    int med_x = ax + bx + cx - (ax > bx ? (ax > cx ? ax : cx) : (bx > cx ? bx : cx))
                - (ax < bx ? (ax < cx ? ax : cx) : (bx < cx ? bx : cx));
    int med_y = ay + by + cy - (ay > by ? (ay > cy ? ay : cy) : (by > cy ? by : cy))
                - (ay < by ? (ay < cy ? ay : cy) : (by < cy ? by : cy));

    result.x_qpel = med_x;
    result.y_qpel = med_y;
    result.ref_idx = left_mv->ref_idx;
    return result;
}

MEResult me_multi_ref_search(const uint8_t *cur, int cur_stride,
                             const MEReferenceFrame *refs, int num_refs,
                             int mb_x, int mb_y,
                             const MEConfig *cfg)
{
    MEResult best, tmp;
    me_result_init(&best);
    int r;

    for (r = 0; r < num_refs && r < cfg->max_ref_frames; r++) {
        tmp = me_search(cur, cur_stride, &refs[r], mb_x, mb_y, cfg);
        tmp.mv.ref_idx = r;
        if (tmp.sad < best.sad) {
            best = tmp;
        }
    }
    return best;
}

void me_b_frame_predict(const uint8_t *cur, int cur_stride,
                        const MEReferenceFrame *ref_fwd,
                        const MEReferenceFrame *ref_bwd,
                        int mb_x, int mb_y,
                        const MEConfig *cfg,
                        MEResult *result_fwd,
                        MEResult *result_bwd,
                        MEResult *result_bidir)
{
    if (result_fwd)
        *result_fwd = me_search(cur, cur_stride, ref_fwd, mb_x, mb_y, cfg);

    if (result_bwd)
        *result_bwd = me_search(cur, cur_stride, ref_bwd, mb_x, mb_y, cfg);

    if (result_bidir) {
        me_result_init(result_bidir);
        result_bidir->valid = 1;
        if (result_fwd && result_bwd) {
            result_bidir->sad = (result_fwd->sad + result_bwd->sad) / 2;
            result_bidir->mv = result_fwd->mv;
        }
    }
}
