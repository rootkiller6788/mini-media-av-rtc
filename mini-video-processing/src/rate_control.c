#include "rate_control.h"
#include <string.h>
#include <math.h>
#include <stdio.h>

void hrd_init(HRDBuffer *hrd, int64_t bitrate, double fps)
{
    hrd->buffer_size = HRD_BUFFER_SIZE * 8;
    hrd->buffer_fullness = hrd->buffer_size / 2;
    hrd->target_fullness = hrd->buffer_size / 2;
    hrd->bitrate = bitrate;
    hrd->frame_rate = fps;
    hrd->bits_per_frame = (int64_t)((double)bitrate / fps);
    hrd->removal_time = 0;
}

int hrd_can_output(HRDBuffer *hrd, int bits)
{
    if (bits > hrd->buffer_fullness) return 0;
    return 1;
}

void hrd_add_bits(HRDBuffer *hrd, int bits)
{
    hrd->buffer_fullness += bits;
    if (hrd->buffer_fullness > hrd->buffer_size)
        hrd->buffer_fullness = hrd->buffer_size;
}

void hrd_remove_bits(HRDBuffer *hrd)
{
    hrd->buffer_fullness -= hrd->bits_per_frame;
    if (hrd->buffer_fullness < 0)
        hrd->buffer_fullness = 0;
    hrd->removal_time++;
}

int hrd_is_underflow(HRDBuffer *hrd)
{
    return hrd->buffer_fullness <= 0 ? 1 : 0;
}

int hrd_is_overflow(HRDBuffer *hrd)
{
    return hrd->buffer_fullness >= hrd->buffer_size ? 1 : 0;
}

double hrd_buffer_fill_ratio(const HRDBuffer *hrd)
{
    return (double)hrd->buffer_fullness / (double)hrd->buffer_size;
}

void gop_init(GOPStructure *gop, int size, int b_frames)
{
    int i, pos = 0;
    if (size > GOP_MAX_SIZE) size = GOP_MAX_SIZE;
    gop->size = size;
    gop->idr_interval = size;

    gop->pattern[pos++] = FRAME_TYPE_I;
    for (i = 1; i < size; i++) {
        int seg = (i - 1) % (b_frames + 1);
        if (seg == 0)
            gop->pattern[pos++] = FRAME_TYPE_P;
        else
            gop->pattern[pos++] = FRAME_TYPE_B;
    }
    gop->size = pos;
}

void gop_init_pattern(GOPStructure *gop, const RCFrameType *p, int count)
{
    int i;
    if (count > GOP_MAX_SIZE) count = GOP_MAX_SIZE;
    gop->size = count;
    gop->idr_interval = count;
    for (i = 0; i < count; i++)
        gop->pattern[i] = p[i];
}

RCFrameType gop_get_frame_type(const GOPStructure *gop, int index)
{
    if (index < 0 || index >= gop->size)
        return FRAME_TYPE_I;
    return gop->pattern[index];
}

void rc_init(RateControl *rc, RCMode mode, int64_t bitrate, double fps,
             int width, int height)
{
    memset(rc, 0, sizeof(RateControl));
    rc->mode = mode;
    rc->target_bitrate = bitrate;
    rc->frame_rate = fps;
    rc->width = width;
    rc->height = height;
    rc->current_qp = QP_DEFAULT;
    rc->gop_qp = QP_DEFAULT;
    rc->qp_min = QP_MIN;
    rc->qp_max = QP_MAX;
    rc->complexity_i = 1.0;
    rc->complexity_p = 0.6;
    rc->complexity_b = 0.3;
    rc->qp_offset_i = -3;
    rc->qp_offset_b = 2;

    gop_init(&rc->gop, GOP_DEFAULT_SIZE, 2);
    hrd_init(&rc->hrd, bitrate, fps);
}

void rc_set_max_bitrate(RateControl *rc, int64_t max_br)
{
    rc->max_bitrate = max_br;
}

int rc_prepare_frame(RateControl *rc)
{
    rc->gop_index = rc->frame_count % rc->gop.size;
    rc->current_frame_type = gop_get_frame_type(&rc->gop, rc->gop_index);
    rc->current_qp = rc_compute_qp(rc, rc->current_frame_type);
    rc->picture_order_count = rc->frame_count;
    rc->frame_count++;
    return rc->current_qp;
}

int rc_compute_qp(RateControl *rc, RCFrameType ftype)
{
    int qp = rc->gop_qp;

    switch (ftype) {
        case FRAME_TYPE_I:
            qp += rc->qp_offset_i;
            break;
        case FRAME_TYPE_P:
            break;
        case FRAME_TYPE_B:
            qp += rc->qp_offset_b;
            break;
    }

    if (rc->mode == RC_MODE_CBR || rc->mode == RC_MODE_VBR) {
        double fill = hrd_buffer_fill_ratio(&rc->hrd);
        int buffer_offset;
        if (fill > 0.8)
            buffer_offset = 3;
        else if (fill > 0.6)
            buffer_offset = 1;
        else if (fill < 0.2)
            buffer_offset = -3;
        else if (fill < 0.4)
            buffer_offset = -1;
        else
            buffer_offset = 0;
        qp += buffer_offset;
    }

    if (qp < rc->qp_min) qp = rc->qp_min;
    if (qp > rc->qp_max) qp = rc->qp_max;
    return qp;
}

int rc_select_qp_rd(RateControl *rc, double complexity, RCFrameType ftype)
{
    int base_qp = rc_compute_qp(rc, ftype);
    double comp_factor;
    switch (ftype) {
        case FRAME_TYPE_I: comp_factor = rc->complexity_i; break;
        case FRAME_TYPE_P: comp_factor = rc->complexity_p; break;
        case FRAME_TYPE_B: comp_factor = rc->complexity_b; break;
        default: comp_factor = 1.0; break;
    }

    int delta = (int)((complexity / comp_factor - 1.0) * 6.0);
    int qp = base_qp + delta;
    if (qp < rc->qp_min) qp = rc->qp_min;
    if (qp > rc->qp_max) qp = rc->qp_max;
    return qp;
}

void rc_update_frame(RateControl *rc, int bits_used)
{
    rc->bits_total += bits_used;

    switch (rc->current_frame_type) {
        case FRAME_TYPE_I:
            rc->bits_i += bits_used;
            rc->complexity_i = rc->complexity_i * 0.9 +
                               (double)bits_used / rc->hrd.bits_per_frame * 0.1;
            break;
        case FRAME_TYPE_P:
            rc->bits_p += bits_used;
            rc->complexity_p = rc->complexity_p * 0.9 +
                               (double)bits_used / rc->hrd.bits_per_frame * 0.1;
            break;
        case FRAME_TYPE_B:
            rc->bits_b += bits_used;
            rc->complexity_b = rc->complexity_b * 0.9 +
                               (double)bits_used / rc->hrd.bits_per_frame * 0.1;
            break;
    }

    rc->qp_avg = rc->qp_avg * 0.9 + rc->current_qp * 0.1;
    if (rc->current_qp < rc->qp_min) rc->qp_min = rc->current_qp;
    if (rc->current_qp > rc->qp_max) rc->qp_max = rc->current_qp;

    hrd_add_bits(&rc->hrd, bits_used);
    hrd_remove_bits(&rc->hrd);

    if (rc->gop_index == 0) {
        rc->gop_bits_used = 0;
        rc->gop_qp = (int)rc->qp_avg;
        if (hrd_is_overflow(&rc->hrd))
            rc->gop_qp += 2;
        if (hrd_is_underflow(&rc->hrd))
            rc->gop_qp -= 2;
        if (rc->gop_qp < QP_MIN) rc->gop_qp = QP_MIN;
        if (rc->gop_qp > QP_MAX) rc->gop_qp = QP_MAX;
    }
    rc->gop_bits_used += bits_used;
}

void rc_adjust_qp_by_buffer(RateControl *rc)
{
    double fill = hrd_buffer_fill_ratio(&rc->hrd);

    if (fill > 0.9) {
        rc->current_qp += 4;
    } else if (fill > 0.75) {
        rc->current_qp += 2;
    } else if (fill < 0.1) {
        rc->current_qp -= 4;
    } else if (fill < 0.25) {
        rc->current_qp -= 2;
    }

    if (rc->current_qp < rc->qp_min) rc->current_qp = rc->qp_min;
    if (rc->current_qp > rc->qp_max) rc->current_qp = rc->qp_max;
}

RCFrameType rc_get_frame_type(const RateControl *rc)
{
    return rc->current_frame_type;
}

const char *rc_frame_type_name(RCFrameType t)
{
    switch (t) {
        case FRAME_TYPE_I: return "I";
        case FRAME_TYPE_P: return "P";
        case FRAME_TYPE_B: return "B";
        default:           return "?";
    }
}

int rc_get_qp(const RateControl *rc)
{
    return rc->current_qp;
}

void rc_print_status(const RateControl *rc)
{
    printf("=== Rate Control Status ===\n");
    printf("  Mode: %s\n", rc->mode == RC_MODE_CBR ? "CBR" :
                           rc->mode == RC_MODE_VBR ? "VBR" : "CQP");
    printf("  Target bitrate: %lld bps\n", (long long)rc->target_bitrate);
    printf("  Frame rate: %.2f fps\n", rc->frame_rate);
    printf("  Resolution: %dx%d\n", rc->width, rc->height);
    printf("  Frame count: %d\n", rc->frame_count);
    printf("  Current QP: %d\n", rc->current_qp);
    printf("  GOP size: %d, index: %d\n", rc->gop.size, rc->gop_index);
    printf("  Current frame type: %s\n", rc_frame_type_name(rc->current_frame_type));
    printf("  Bits total: %lld, I: %lld, P: %lld, B: %lld\n",
           (long long)rc->bits_total, (long long)rc->bits_i,
           (long long)rc->bits_p, (long long)rc->bits_b);
    printf("  QP avg: %.1f, range: [%d, %d]\n", rc->qp_avg, rc->qp_min, rc->qp_max);
    printf("  HRD fill: %.1f%% (%lld / %lld)\n",
           hrd_buffer_fill_ratio(&rc->hrd) * 100.0,
           (long long)rc->hrd.buffer_fullness,
           (long long)rc->hrd.buffer_size);
}
