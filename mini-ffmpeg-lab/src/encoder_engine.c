#include "encoder_engine.h"
#include "decoder_engine.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

typedef struct MiniFFmpegEncH264Priv {
    int     width;
    int     height;
    int     pix_fmt;
    int     gop_size;
    int     gop_index;
    int     max_b_frames;
    int     b_frame_strategy;
    int     keyint_min;
    int     refs;
    int     preset;
    int     profile;
    int     level;
    int     frame_count;
    int     encoded_frames;
    int     last_keyframe;
    int     scene_change_detected;
    uint8_t sps_pps[128];
    int     sps_pps_size;
    int     slice_count;
    int     current_slice;
    int     threads;
    MiniFFmpegAVFrame *lookahead_frames[MINI_FFMPEG_MAX_ENC_DELAY];
    int     lookahead_count;
    int     lookahead_capacity;
    float   psnr_sum;
    int     psnr_count;
} MiniFFmpegEncH264Priv;

typedef struct MiniFFmpegEncAACPriv {
    int     sample_rate;
    int     channels;
    int     bit_rate;
    int     frame_size;
    int     encoded_frames;
    int     total_samples;
    int     aot;
    int     sampling_index;
    int     window_type;
    int     sbr_enabled;
    int     ps_enabled;
    uint8_t adts_header_bytes[7];
    int     write_adts;
} MiniFFmpegEncAACPriv;

typedef struct MiniFFmpegEncoderEngine {
    MiniFFmpegAVCodecContext  codec_ctx;
    MiniFFmpegEncParams       params;
    MiniFFmpegRateControlContext rate_control;
    int    initialized;
    int    flushing;
    int    eof;
    int    frames_sent;
    int    packets_received;
    void  *codec_priv;
    MiniFFmpegAVFrame *current_frame;
    MiniFFmpegAVPacket buffered_packets[MINI_FFMPEG_MAX_ENC_DELAY];
    int    buffered_count;
    int    buffered_index;
    int    stats_frame_count;
    float  stats_avg_qp;
    int64_t stats_total_bits;
} MiniFFmpegEncoderEngine;

static void mini_ffmpeg_enc_generate_sps_pps(MiniFFmpegEncoderEngine *engine) {
    MiniFFmpegEncH264Priv *priv = (MiniFFmpegEncH264Priv *)engine->codec_priv;
    priv->sps_pps[0] = 0x00; priv->sps_pps[1] = 0x00;
    priv->sps_pps[2] = 0x00; priv->sps_pps[3] = 0x01;
    priv->sps_pps[4] = 0x67;
    priv->sps_pps[5] = (uint8_t)(priv->profile);
    priv->sps_pps[6] = 0x42;
    priv->sps_pps[7] = 0x00;
    priv->sps_pps[8] = 0x1E;
    priv->sps_pps[9] = (uint8_t)(priv->level);
    priv->sps_pps[10] = 0x8C;
    priv->sps_pps_size = 32;
}

static void mini_ffmpeg_enc_generate_adts_header(MiniFFmpegEncoderEngine *engine) {
    MiniFFmpegEncAACPriv *priv = (MiniFFmpegEncAACPriv *)engine->codec_priv;
    int profile = priv->aot - 1;
    int sampling_index_val = 3;
    int channel_config = priv->channels;
    int aac_frame_size = priv->frame_size + 7;
    priv->adts_header_bytes[0] = 0xFF;
    priv->adts_header_bytes[1] = 0xF9;
    priv->adts_header_bytes[2] = (uint8_t)(((profile - 1) << 6) +
        (sampling_index_val << 2) + ((channel_config >> 2) & 0x01));
    priv->adts_header_bytes[3] = (uint8_t)(((channel_config & 0x03) << 6) +
        ((aac_frame_size >> 11) & 0x03));
    priv->adts_header_bytes[4] = (uint8_t)((aac_frame_size >> 3) & 0xFF);
    priv->adts_header_bytes[5] = (uint8_t)(((aac_frame_size & 0x07) << 5) + 0x1F);
    priv->adts_header_bytes[6] = 0xFC;
}

static int mini_ffmpeg_enc_encode_h264(MiniFFmpegEncoderEngine *engine,
                                        MiniFFmpegAVPacket *pkt) {
    MiniFFmpegEncH264Priv *priv = (MiniFFmpegEncH264Priv *)engine->codec_priv;
    int frame_type, pkt_size;
    int is_keyframe;
    if (engine->flushing && priv->encoded_frames > priv->frame_count)
        return -11;
    priv->gop_index++;
    is_keyframe = 0;
    if (priv->gop_index >= priv->gop_size ||
        priv->encoded_frames == 0 ||
        priv->scene_change_detected) {
        frame_type = MINI_FFMPEG_ENC_PIC_TYPE_I;
        priv->gop_index = 0;
        priv->last_keyframe = priv->encoded_frames;
        is_keyframe = 1;
    } else if (priv->max_b_frames > 0 &&
               priv->gop_index > (priv->gop_size - priv->max_b_frames - 1)) {
        frame_type = MINI_FFMPEG_ENC_PIC_TYPE_B;
    } else {
        frame_type = MINI_FFMPEG_ENC_PIC_TYPE_P;
    }
    pkt_size = 4096 + (rand() % 32768);
    if (frame_type == MINI_FFMPEG_ENC_PIC_TYPE_I)
        pkt_size *= 3;
    mini_ffmpeg_packet_alloc(pkt, pkt_size);
    if (priv->encoded_frames == 0) {
        memcpy(pkt->data, priv->sps_pps,
               priv->sps_pps_size < pkt->size ? priv->sps_pps_size : pkt->size);
    } else {
        pkt->data[0] = 0x00; pkt->data[1] = 0x00;
        pkt->data[2] = 0x00; pkt->data[3] = 0x01;
        if (is_keyframe) pkt->data[4] = 0x65;
        else if (frame_type == MINI_FFMPEG_ENC_PIC_TYPE_P) pkt->data[4] = 0x41;
        else pkt->data[4] = 0x01;
    }
    pkt->flags = is_keyframe ? MINI_FFMPEG_PKT_FLAG_KEY : 0;
    pkt->pts = (int64_t)priv->encoded_frames *
               engine->params.frame_rate_den * 90000 /
               engine->params.frame_rate_num;
    pkt->dts = pkt->pts;
    if (frame_type == MINI_FFMPEG_ENC_PIC_TYPE_B)
        pkt->dts -= 3000;
    mini_ffmpeg_rate_control_update(&engine->rate_control,
                                     frame_type, pkt_size * 8);
    engine->stats_total_bits += pkt_size * 8;
    engine->stats_avg_qp = mini_ffmpeg_rate_control_get_qp(
        &engine->rate_control, frame_type);
    engine->stats_frame_count++;
    priv->encoded_frames++;
    return 0;
}

static int mini_ffmpeg_enc_encode_aac(MiniFFmpegEncoderEngine *engine,
                                       MiniFFmpegAVPacket *pkt) {
    MiniFFmpegEncAACPriv *priv = (MiniFFmpegEncAACPriv *)engine->codec_priv;
    int pkt_size;
    if (engine->flushing && priv->encoded_frames > engine->frames_sent)
        return -11;
    pkt_size = (priv->bit_rate / 8) / 50;
    if (pkt_size < 64) pkt_size = 64;
    mini_ffmpeg_packet_alloc(pkt, pkt_size + 7);
    if (priv->write_adts) {
        memcpy(pkt->data, priv->adts_header_bytes, 7);
        memset(pkt->data + 7, 0xAB, pkt_size);
    } else {
        memset(pkt->data, 0xCC, pkt_size);
    }
    pkt->size = pkt_size + (priv->write_adts ? 7 : 0);
    pkt->flags = MINI_FFMPEG_PKT_FLAG_KEY;
    pkt->pts = (int64_t)priv->encoded_frames * priv->frame_size *
               1000000 / priv->sample_rate;
    pkt->dts = pkt->pts;
    engine->stats_total_bits += pkt_size * 8;
    engine->stats_frame_count++;
    priv->encoded_frames++;
    priv->total_samples += priv->frame_size;
    return 0;
}

void mini_ffmpeg_enc_params_default(MiniFFmpegEncParams *params, int codec_id) {
    if (!params) return;
    memset(params, 0, sizeof(*params));
    params->gop_size = 250;
    params->max_b_frames = 2;
    params->keyint_min = 25;
    params->refs = 3;
    params->preset = MINI_FFMPEG_ENC_PRESET_MEDIUM;
    params->rate_control = MINI_FFMPEG_RC_VBR;
    params->crf_value = 23;
    params->qp_constant = 26;
    params->qp_min_i = 10.0f;
    params->qp_max_i = 51.0f;
    params->qp_min_p = 10.0f;
    params->qp_max_p = 51.0f;
    params->bit_rate = 2000000;
    params->frame_rate_num = 30;
    params->frame_rate_den = 1;
    params->rc_buffer_size = 4000000;
    params->rc_initial_buffer_occupancy = 2000000;
    params->scene_change_detection = 1;
    params->adaptive_quantization = 1;
    params->lookahead_frames = 20;
    params->threads = 4;
    if (codec_id == MINI_FFMPEG_CODEC_ID_AAC ||
        codec_id == MINI_FFMPEG_CODEC_ID_MP3) {
        params->sample_rate = 48000;
        params->channels = 2;
        params->bit_rate = 128000;
        params->gop_size = 1;
        params->max_b_frames = 0;
    }
}

void mini_ffmpeg_rate_control_init(MiniFFmpegRateControlContext *rc,
                                    const MiniFFmpegEncParams *params) {
    if (!rc || !params) return;
    memset(rc, 0, sizeof(*rc));
    rc->mode = params->rate_control;
    rc->bit_rate = params->bit_rate;
    rc->buffer_size = params->rc_buffer_size > 0 ? params->rc_buffer_size : params->bit_rate * 2;
    rc->buffer_level = params->rc_initial_buffer_occupancy > 0 ?
                       params->rc_initial_buffer_occupancy : rc->buffer_size / 2;
    rc->qp = (float)params->crf_value;
    rc->target_bits_per_frame = (float)params->bit_rate /
                                (float)params->frame_rate_num;
    rc->average_qp = (float)params->crf_value;
    rc->scene_change_threshold = 40;
    rc->reactive_qp = 0;
}

void mini_ffmpeg_rate_control_update(MiniFFmpegRateControlContext *rc,
                                      int frame_type, int frame_bits) {
    int target_bits;
    float deviation;
    if (!rc) return;
    target_bits = (int)rc->target_bits_per_frame;
    if (frame_type == MINI_FFMPEG_ENC_PIC_TYPE_I)
        target_bits *= 5;
    else if (frame_type == MINI_FFMPEG_ENC_PIC_TYPE_P)
        target_bits = (int)(target_bits * 2.5f);
    else
        target_bits = (int)(target_bits * 0.5f);
    deviation = (float)(frame_bits - target_bits) / (float)(target_bits + 1);
    rc->buffer_level += (frame_bits - target_bits);
    if (rc->buffer_level < 0) rc->buffer_level = 0;
    if (rc->buffer_level > rc->buffer_size) rc->buffer_level = rc->buffer_size;
    rc->accumulated_bits += (float)frame_bits;
    rc->qp += deviation * 2.0f;
    if (rc->qp < 0.0f) rc->qp = 0.0f;
    if (rc->qp > 51.0f) rc->qp = 51.0f;
    rc->average_qp = rc->average_qp * 0.95f + rc->qp * 0.05f;
    rc->frame_count++;
    rc->last_time += 1;
    if (frame_type == MINI_FFMPEG_ENC_PIC_TYPE_I &&
        frame_bits > target_bits * 3) {
        rc->scene_change_threshold = 50;
    } else {
        rc->scene_change_threshold = 40;
    }
}

float mini_ffmpeg_rate_control_get_qp(MiniFFmpegRateControlContext *rc,
                                       int frame_type) {
    float qp;
    (void)frame_type;
    if (!rc) return 23.0f;
    qp = rc->qp;
    switch (rc->mode) {
    case MINI_FFMPEG_RC_CQP:
        qp = rc->qp;
        break;
    case MINI_FFMPEG_RC_CBR:
        qp = rc->qp + (float)(rc->buffer_level - rc->buffer_size / 2) /
             (float)(rc->buffer_size / 10 + 1);
        break;
    case MINI_FFMPEG_RC_VBR:
        qp = rc->average_qp;
        break;
    case MINI_FFMPEG_RC_CRF:
        qp = rc->average_qp;
        break;
    default: break;
    }
    if (qp < 0.0f) qp = 0.0f;
    if (qp > 51.0f) qp = 51.0f;
    return qp;
}

MiniFFmpegEncoderEngine *mini_ffmpeg_encoder_alloc(void) {
    MiniFFmpegEncoderEngine *engine = (MiniFFmpegEncoderEngine *)
        calloc(1, sizeof(MiniFFmpegEncoderEngine));
    if (engine) {
        int i;
        for (i = 0; i < MINI_FFMPEG_MAX_ENC_DELAY; i++)
            mini_ffmpeg_packet_init(&engine->buffered_packets[i]);
    }
    return engine;
}

void mini_ffmpeg_encoder_free(MiniFFmpegEncoderEngine *engine) {
    int i;
    if (!engine) return;
    for (i = 0; i < MINI_FFMPEG_MAX_ENC_DELAY; i++)
        mini_ffmpeg_packet_unref(&engine->buffered_packets[i]);
    if (engine->codec_priv) {
        MiniFFmpegEncH264Priv *priv = (MiniFFmpegEncH264Priv *)engine->codec_priv;
        int j;
        for (j = 0; j < MINI_FFMPEG_MAX_ENC_DELAY; j++) {
            if (priv->lookahead_frames[j])
                mini_ffmpeg_frame_free(priv->lookahead_frames[j]);
        }
        free(engine->codec_priv);
    }
    if (engine->current_frame) mini_ffmpeg_frame_free(engine->current_frame);
    free(engine);
}

int mini_ffmpeg_encoder_open(MiniFFmpegEncoderEngine *engine,
                              MiniFFmpegAVCodecContext *codec_ctx,
                              const MiniFFmpegEncParams *params) {
    if (!engine || !codec_ctx) return -1;
    if (params) {
        engine->params = *params;
    } else {
        mini_ffmpeg_enc_params_default(&engine->params, codec_ctx->codec_id);
    }
    engine->codec_ctx = *codec_ctx;
    engine->codec_ctx.width  = engine->params.width;
    engine->codec_ctx.height = engine->params.height;
    engine->codec_ctx.sample_rate = engine->params.sample_rate;
    engine->codec_ctx.channels    = engine->params.channels;
    engine->codec_ctx.bit_rate    = engine->params.bit_rate;
    engine->codec_ctx.gop_size    = engine->params.gop_size;
    engine->codec_ctx.max_b_frames = engine->params.max_b_frames;
    mini_ffmpeg_rate_control_init(&engine->rate_control, &engine->params);
    switch (codec_ctx->codec_id) {
    case MINI_FFMPEG_CODEC_ID_H264:
    case MINI_FFMPEG_CODEC_ID_H265:
        engine->codec_priv = calloc(1, sizeof(MiniFFmpegEncH264Priv));
        if (engine->codec_priv) {
            MiniFFmpegEncH264Priv *priv = (MiniFFmpegEncH264Priv *)engine->codec_priv;
            priv->width = engine->params.width;
            priv->height = engine->params.height;
            priv->pix_fmt = engine->params.pix_fmt;
            priv->gop_size = engine->params.gop_size;
            priv->gop_index = engine->params.gop_size;
            priv->max_b_frames = engine->params.max_b_frames;
            priv->keyint_min = engine->params.keyint_min;
            priv->refs = engine->params.refs;
            priv->preset = engine->params.preset;
            priv->profile = engine->params.profile > 0 ? engine->params.profile : 77;
            priv->level = engine->params.level > 0 ? engine->params.level : 40;
            priv->threads = engine->params.threads > 0 ? engine->params.threads : 1;
            mini_ffmpeg_enc_generate_sps_pps(engine);
        }
        break;
    case MINI_FFMPEG_CODEC_ID_AAC:
        engine->codec_priv = calloc(1, sizeof(MiniFFmpegEncAACPriv));
        if (engine->codec_priv) {
            MiniFFmpegEncAACPriv *priv = (MiniFFmpegEncAACPriv *)engine->codec_priv;
            priv->sample_rate = engine->params.sample_rate;
            priv->channels = engine->params.channels;
            priv->bit_rate = engine->params.bit_rate;
            priv->frame_size = 1024;
            priv->aot = 2;
            priv->sampling_index = 3;
            priv->write_adts = 1;
            mini_ffmpeg_enc_generate_adts_header(engine);
        }
        break;
    default:
        engine->codec_priv = calloc(256, 1);
        break;
    }
    engine->initialized = 1;
    return 0;
}

int mini_ffmpeg_encoder_send_frame(MiniFFmpegEncoderEngine *engine,
                                    const MiniFFmpegAVFrame *frame) {
    if (!engine || !engine->initialized) return -1;
    if (engine->flushing) return -3;
    if (!frame) {
        engine->flushing = 1;
        return 0;
    }
    if (engine->current_frame)
        mini_ffmpeg_frame_unref(engine->current_frame);
    if (!engine->current_frame)
        engine->current_frame = mini_ffmpeg_frame_alloc();
    if (!engine->current_frame) return -1;
    engine->current_frame->width = frame->width;
    engine->current_frame->height = frame->height;
    engine->current_frame->format = frame->format;
    engine->current_frame->pts = frame->pts;
    engine->current_frame->pict_type = frame->pict_type;
    engine->current_frame->key_frame = frame->key_frame;
    engine->current_frame->sample_rate = frame->sample_rate;
    engine->current_frame->channels = frame->channels;
    engine->current_frame->nb_samples = frame->nb_samples;
    engine->frames_sent++;
    {
        MiniFFmpegEncH264Priv *priv = (MiniFFmpegEncH264Priv *)engine->codec_priv;
        if (priv && priv->lookahead_count < priv->lookahead_capacity) {
            priv->lookahead_count++;
        }
    }
    return 0;
}

int mini_ffmpeg_encoder_receive_packet(MiniFFmpegEncoderEngine *engine,
                                        MiniFFmpegAVPacket *pkt) {
    if (!engine || !engine->initialized || !pkt) return -1;
    if (engine->eof) return -11;
    if (engine->buffered_count > 0) {
        *pkt = engine->buffered_packets[engine->buffered_index];
        engine->buffered_index++;
        engine->buffered_count--;
        engine->packets_received++;
        return 0;
    }
    switch (engine->codec_ctx.codec_id) {
    case MINI_FFMPEG_CODEC_ID_H264:
    case MINI_FFMPEG_CODEC_ID_H265:
        return mini_ffmpeg_enc_encode_h264(engine, pkt);
    case MINI_FFMPEG_CODEC_ID_AAC:
    case MINI_FFMPEG_CODEC_ID_MP3:
    case MINI_FFMPEG_CODEC_ID_OPUS:
        return mini_ffmpeg_enc_encode_aac(engine, pkt);
    default: return -5;
    }
}

int mini_ffmpeg_encoder_flush(MiniFFmpegEncoderEngine *engine) {
    int ret;
    MiniFFmpegAVPacket pkt;
    if (!engine) return -1;
    engine->flushing = 1;
    mini_ffmpeg_packet_init(&pkt);
    ret = mini_ffmpeg_encoder_receive_packet(engine, &pkt);
    if (ret >= 0) {
        if (engine->buffered_count < MINI_FFMPEG_MAX_ENC_DELAY) {
            engine->buffered_packets[engine->buffered_count] = pkt;
            engine->buffered_count++;
        }
    } else {
        engine->eof = 1;
    }
    mini_ffmpeg_packet_unref(&pkt);
    return ret;
}

int mini_ffmpeg_encoder_set_bitrate(MiniFFmpegEncoderEngine *engine,
                                     int bit_rate) {
    if (!engine) return -1;
    engine->params.bit_rate = bit_rate;
    engine->rate_control.bit_rate = bit_rate;
    return 0;
}

int mini_ffmpeg_encoder_set_gop_size(MiniFFmpegEncoderEngine *engine,
                                      int gop_size) {
    if (!engine) return -1;
    engine->params.gop_size = gop_size;
    if (engine->codec_priv) {
        MiniFFmpegEncH264Priv *priv = (MiniFFmpegEncH264Priv *)engine->codec_priv;
        priv->gop_size = gop_size;
    }
    return 0;
}

int mini_ffmpeg_encoder_set_preset(MiniFFmpegEncoderEngine *engine,
                                    int preset) {
    if (!engine || preset < 0 || preset > MINI_FFMPEG_ENC_PRESET_PLACEBO)
        return -1;
    engine->params.preset = preset;
    return 0;
}

int mini_ffmpeg_encoder_set_rate_control(MiniFFmpegEncoderEngine *engine,
                                          int mode) {
    if (!engine || mode < 0 || mode > MINI_FFMPEG_RC_CRF) return -1;
    engine->params.rate_control = mode;
    engine->rate_control.mode = mode;
    return 0;
}

int mini_ffmpeg_encoder_force_keyframe(MiniFFmpegEncoderEngine *engine) {
    MiniFFmpegEncH264Priv *priv;
    if (!engine || !engine->codec_priv) return -1;
    priv = (MiniFFmpegEncH264Priv *)engine->codec_priv;
    priv->gop_index = priv->gop_size;
    return 0;
}

int mini_ffmpeg_encoder_get_stats(MiniFFmpegEncoderEngine *engine,
                                   int *frame_count,
                                   float *avg_qp,
                                   int64_t *total_bits) {
    if (!engine) return -1;
    if (frame_count) *frame_count = engine->stats_frame_count;
    if (avg_qp) *avg_qp = engine->stats_avg_qp;
    if (total_bits) *total_bits = engine->stats_total_bits;
    return 0;
}
