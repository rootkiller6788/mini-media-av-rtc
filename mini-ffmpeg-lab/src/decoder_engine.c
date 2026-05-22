#include "decoder_engine.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

typedef struct MiniFFmpegCodecEntry {
    MiniFFmpegAVCodec codec;
    struct MiniFFmpegCodecEntry *next;
} MiniFFmpegCodecEntry;

static MiniFFmpegCodecEntry *g_codec_list = NULL;

typedef struct MiniFFmpegH264DecoderPriv {
    int     width;
    int     height;
    int     pix_fmt;
    uint8_t sps_data[256];
    int     sps_size;
    uint8_t pps_data[256];
    int     pps_size;
    int     mb_width;
    int     mb_height;
    int     mb_x;
    int     mb_y;
    int     slice_type;
    int     nal_ref_idc;
    int     nal_unit_type;
    int     decoded_frames;
    int     ref_count;
    MiniFFmpegAVFrame *ref_frames[MINI_FFMPEG_MAX_REF_FRAMES];
    uint8_t *internal_buffer;
    int      internal_buffer_size;
} MiniFFmpegH264DecoderPriv;

typedef struct MiniFFmpegAACDecoderPriv {
    int     sample_rate;
    int     channels;
    int     frame_size;
    int     decoded_samples;
    uint8_t extra_data[64];
    int     extra_size;
    float  *overlap_buffer;
    int     overlap_size;
    int     aot;
    int     sampling_index;
} MiniFFmpegAACDecoderPriv;

typedef struct MiniFFmpegPCMDecoderPriv {
    int     sample_rate;
    int     channels;
    int     bit_depth;
    int     decoded_samples;
} MiniFFmpegPCMDecoderPriv;

typedef struct MiniFFmpegDecoderEngine {
    MiniFFmpegAVCodecContext codec_ctx;
    MiniFFmpegAVPacket       current_pkt;
    int    packet_consumed;
    int    draining;
    void  *codec_priv;
    int    initialized;
    int    frames_decoded;
    int    errors;
} MiniFFmpegDecoderEngine;

static int mini_ffmpeg_h264_parse_nal(MiniFFmpegDecoderEngine *engine,
                                       const uint8_t *data, int size) {
    MiniFFmpegH264DecoderPriv *priv = (MiniFFmpegH264DecoderPriv *)engine->codec_priv;
    int nal_type;
    (void)size;
    if (size < 4) return -1;
    nal_type = data[0] & 0x1F;
    priv->nal_unit_type = nal_type;
    priv->nal_ref_idc = (data[0] >> 5) & 0x03;
    switch (nal_type) {
    case 7:
        if (size > 4 && size <= 260) {
            memcpy(priv->sps_data, data + 4, size - 4);
            priv->sps_size = size - 4;
            priv->width = ((priv->sps_data[1] << 8) | priv->sps_data[2]) & 0xFFFC;
        }
        break;
    case 8:
        if (size > 4 && size <= 260) {
            memcpy(priv->pps_data, data + 4, size - 4);
            priv->pps_size = size - 4;
        }
        break;
    case 1: case 5:
        priv->slice_type = (data[4] & 0x1F);
        break;
    default: break;
    }
    return 0;
}

static int mini_ffmpeg_h264_reconstruct_frame(MiniFFmpegDecoderEngine *engine,
                                                MiniFFmpegAVFrame *frame) {
    MiniFFmpegH264DecoderPriv *priv = (MiniFFmpegH264DecoderPriv *)engine->codec_priv;
    int y_size, uv_size, i;
    uint8_t *y_plane, *u_plane, *v_plane;
    if (!priv->width || !priv->height) {
        priv->width = engine->codec_ctx.width ? engine->codec_ctx.width : 1920;
        priv->height = engine->codec_ctx.height ? engine->codec_ctx.height : 1080;
    }
    frame->width = priv->width;
    frame->height = priv->height;
    frame->format = MINI_FFMPEG_PIX_FMT_YUV420P;
    y_size = priv->width * priv->height;
    uv_size = (priv->width / 2) * (priv->height / 2);
    frame->data[0] = (uint8_t *)malloc(y_size);
    frame->data[1] = (uint8_t *)malloc(uv_size);
    frame->data[2] = (uint8_t *)malloc(uv_size);
    frame->linesize[0] = priv->width;
    frame->linesize[1] = priv->width / 2;
    frame->linesize[2] = priv->width / 2;
    y_plane = frame->data[0];
    u_plane = frame->data[1];
    v_plane = frame->data[2];
    memset(y_plane, 128, y_size);
    memset(u_plane, 128, uv_size);
    memset(v_plane, 128, uv_size);
    frame->key_frame = (priv->nal_unit_type == 5) ? 1 : 0;
    frame->pict_type = frame->key_frame ? 1 : 2;
    frame->pts = engine->current_pkt.pts;
    frame->pkt_dts = engine->current_pkt.dts;
    priv->decoded_frames++;
    frame->coded_picture_number = priv->decoded_frames;
    frame->display_picture_number = priv->decoded_frames;
    return 0;
}

static int mini_ffmpeg_aac_decode_frame(MiniFFmpegDecoderEngine *engine,
                                         MiniFFmpegAVFrame *frame) {
    MiniFFmpegAACDecoderPriv *priv = (MiniFFmpegAACDecoderPriv *)engine->codec_priv;
    int nb_samples, frame_bytes, i;
    if (!priv->sample_rate) priv->sample_rate = 48000;
    if (!priv->channels) priv->channels = 2;
    nb_samples = 1024;
    frame->nb_samples = nb_samples;
    frame->sample_rate = priv->sample_rate;
    frame->channels = priv->channels;
    frame->format = MINI_FFMPEG_SAMPLE_FMT_S16;
    frame_bytes = nb_samples * priv->channels * 2;
    frame->data[0] = (uint8_t *)malloc(frame_bytes);
    frame->linesize[0] = frame_bytes;
    for (i = 0; i < nb_samples * priv->channels; i++) {
        int16_t *samples = (int16_t *)frame->data[0];
        samples[i] = (int16_t)((rand() % 65536) - 32768);
    }
    frame->pts = engine->current_pkt.pts;
    priv->decoded_samples += nb_samples;
    return 0;
}

static int mini_ffmpeg_pcm_decode_frame(MiniFFmpegDecoderEngine *engine,
                                         MiniFFmpegAVFrame *frame) {
    MiniFFmpegPCMDecoderPriv *priv = (MiniFFmpegPCMDecoderPriv *)engine->codec_priv;
    int nb_samples, frame_bytes;
    if (!priv->sample_rate) priv->sample_rate = 44100;
    if (!priv->channels) priv->channels = 2;
    nb_samples = engine->current_pkt.size / (priv->channels * 2);
    if (nb_samples <= 0) nb_samples = 1024;
    frame->nb_samples = nb_samples;
    frame->sample_rate = priv->sample_rate;
    frame->channels = priv->channels;
    frame->format = MINI_FFMPEG_SAMPLE_FMT_S16;
    frame_bytes = nb_samples * priv->channels * 2;
    frame->data[0] = (uint8_t *)malloc(frame_bytes);
    if (engine->current_pkt.data && engine->current_pkt.size > 0) {
        int copy = engine->current_pkt.size < frame_bytes ?
                   engine->current_pkt.size : frame_bytes;
        memcpy(frame->data[0], engine->current_pkt.data, copy);
        if (copy < frame_bytes)
            memset(frame->data[0] + copy, 0, frame_bytes - copy);
    } else {
        memset(frame->data[0], 0, frame_bytes);
    }
    frame->linesize[0] = frame_bytes;
    frame->pts = engine->current_pkt.pts;
    priv->decoded_samples += nb_samples;
    return 0;
}

static void mini_ffmpeg_register_builtin_codecs(void) {
    MiniFFmpegCodecEntry *entry;
    static const MiniFFmpegAVCodec builtins[] = {
        { "h264",    "H.264 / AVC / MPEG-4 AVC", MINI_FFMPEG_MEDIA_TYPE_VIDEO, MINI_FFMPEG_CODEC_ID_H264, 0 },
        { "h265",    "H.265 / HEVC",             MINI_FFMPEG_MEDIA_TYPE_VIDEO, MINI_FFMPEG_CODEC_ID_H265, 0 },
        { "vp8",     "On2 VP8",                  MINI_FFMPEG_MEDIA_TYPE_VIDEO, MINI_FFMPEG_CODEC_ID_VP8,  0 },
        { "vp9",     "Google VP9",               MINI_FFMPEG_MEDIA_TYPE_VIDEO, MINI_FFMPEG_CODEC_ID_VP9,  0 },
        { "av1",     "Alliance for Open Media AV1", MINI_FFMPEG_MEDIA_TYPE_VIDEO, MINI_FFMPEG_CODEC_ID_AV1, 0 },
        { "aac",     "AAC (Advanced Audio Coding)", MINI_FFMPEG_MEDIA_TYPE_AUDIO, MINI_FFMPEG_CODEC_ID_AAC, 0 },
        { "mp3",     "MP3 (MPEG audio layer 3)",    MINI_FFMPEG_MEDIA_TYPE_AUDIO, MINI_FFMPEG_CODEC_ID_MP3, 0 },
        { "opus",    "Opus Interactive Audio Codec",MINI_FFMPEG_MEDIA_TYPE_AUDIO, MINI_FFMPEG_CODEC_ID_OPUS,0 },
        { "pcm_s16le","PCM signed 16-bit little-endian", MINI_FFMPEG_MEDIA_TYPE_AUDIO, MINI_FFMPEG_CODEC_ID_PCM_S16LE, 0 },
        { NULL, NULL, 0, 0, 0 }
    };
    int i;
    for (i = 0; builtins[i].name; i++) {
        entry = (MiniFFmpegCodecEntry *)calloc(1, sizeof(*entry));
        entry->codec = builtins[i];
        entry->next = g_codec_list;
        g_codec_list = entry;
    }
}

void mini_ffmpeg_codec_register_all(void) {
    static int registered = 0;
    if (!registered) {
        mini_ffmpeg_register_builtin_codecs();
        registered = 1;
    }
}

MiniFFmpegAVCodec *mini_ffmpeg_codec_find_decoder(int codec_id) {
    MiniFFmpegCodecEntry *entry;
    mini_ffmpeg_codec_register_all();
    for (entry = g_codec_list; entry; entry = entry->next) {
        if (entry->codec.id == codec_id) return &entry->codec;
    }
    return NULL;
}

MiniFFmpegAVCodec *mini_ffmpeg_codec_find_encoder(int codec_id) {
    return mini_ffmpeg_codec_find_decoder(codec_id);
}

MiniFFmpegAVCodec *mini_ffmpeg_codec_find_by_name(const char *name) {
    MiniFFmpegCodecEntry *entry;
    mini_ffmpeg_codec_register_all();
    for (entry = g_codec_list; entry; entry = entry->next) {
        if (strcmp(entry->codec.name, name) == 0) return &entry->codec;
        if (entry->codec.long_name && strstr(entry->codec.long_name, name))
            return &entry->codec;
    }
    return NULL;
}

MiniFFmpegAVCodec *mini_ffmpeg_codec_iterate(void **opaque) {
    MiniFFmpegCodecEntry *entry;
    mini_ffmpeg_codec_register_all();
    if (!*opaque) {
        *opaque = g_codec_list;
        return g_codec_list ? &g_codec_list->codec : NULL;
    }
    entry = (MiniFFmpegCodecEntry *)*opaque;
    *opaque = entry->next;
    return entry->next ? &entry->next->codec : NULL;
}

MiniFFmpegDecoderEngine *mini_ffmpeg_decoder_alloc(void) {
    MiniFFmpegDecoderEngine *engine = (MiniFFmpegDecoderEngine *)
        calloc(1, sizeof(MiniFFmpegDecoderEngine));
    if (engine) {
        mini_ffmpeg_packet_init(&engine->current_pkt);
    }
    return engine;
}

void mini_ffmpeg_decoder_free(MiniFFmpegDecoderEngine *engine) {
    if (!engine) return;
    mini_ffmpeg_packet_unref(&engine->current_pkt);
    if (engine->codec_priv) free(engine->codec_priv);
    free(engine);
}

int mini_ffmpeg_decoder_open(MiniFFmpegDecoderEngine *engine,
                              MiniFFmpegAVCodecContext *codec_ctx) {
    if (!engine || !codec_ctx) return -1;
    engine->codec_ctx = *codec_ctx;
    engine->packet_consumed = 1;
    engine->draining = 0;
    engine->initialized = 1;
    switch (codec_ctx->codec_id) {
    case MINI_FFMPEG_CODEC_ID_H264:
    case MINI_FFMPEG_CODEC_ID_H265:
        engine->codec_priv = calloc(1, sizeof(MiniFFmpegH264DecoderPriv));
        if (engine->codec_priv) {
            MiniFFmpegH264DecoderPriv *p = (MiniFFmpegH264DecoderPriv *)engine->codec_priv;
            p->width = codec_ctx->width;
            p->height = codec_ctx->height;
            p->pix_fmt = codec_ctx->pix_fmt;
            p->mb_width = (codec_ctx->width + 15) / 16;
            p->mb_height = (codec_ctx->height + 15) / 16;
        }
        break;
    case MINI_FFMPEG_CODEC_ID_AAC:
    case MINI_FFMPEG_CODEC_ID_MP3:
        engine->codec_priv = calloc(1, sizeof(MiniFFmpegAACDecoderPriv));
        if (engine->codec_priv) {
            MiniFFmpegAACDecoderPriv *p = (MiniFFmpegAACDecoderPriv *)engine->codec_priv;
            p->sample_rate = codec_ctx->sample_rate;
            p->channels = codec_ctx->channels;
            p->frame_size = 1024;
        }
        break;
    case MINI_FFMPEG_CODEC_ID_PCM_S16LE:
        engine->codec_priv = calloc(1, sizeof(MiniFFmpegPCMDecoderPriv));
        if (engine->codec_priv) {
            MiniFFmpegPCMDecoderPriv *p = (MiniFFmpegPCMDecoderPriv *)engine->codec_priv;
            p->sample_rate = codec_ctx->sample_rate;
            p->channels = codec_ctx->channels;
            p->bit_depth = 16;
        }
        break;
    default:
        engine->codec_priv = calloc(128, 1);
        break;
    }
    return 0;
}

int mini_ffmpeg_decoder_send_packet(MiniFFmpegDecoderEngine *engine,
                                     MiniFFmpegAVPacket *pkt) {
    if (!engine || !engine->initialized) return -1;
    if (!engine->packet_consumed) return -2;
    mini_ffmpeg_packet_unref(&engine->current_pkt);
    if (pkt && pkt->data && pkt->size > 0) {
        engine->current_pkt.data = (uint8_t *)malloc(pkt->size);
        if (!engine->current_pkt.data) return -1;
        memcpy(engine->current_pkt.data, pkt->data, pkt->size);
        engine->current_pkt.size = pkt->size;
        engine->current_pkt.pts = pkt->pts;
        engine->current_pkt.dts = pkt->dts;
        engine->current_pkt.stream_index = pkt->stream_index;
        engine->current_pkt.flags = pkt->flags;
        engine->packet_consumed = 0;
        engine->draining = 0;
    } else {
        engine->draining = 1;
        engine->packet_consumed = 0;
    }
    return 0;
}

int mini_ffmpeg_decoder_receive_frame(MiniFFmpegDecoderEngine *engine,
                                       MiniFFmpegAVFrame *frame) {
    int ret = 0;
    if (!engine || !engine->initialized || !frame) return -1;
    if (engine->packet_consumed && !engine->draining) return -3;
    mini_ffmpeg_frame_init(frame);
    if (engine->draining && engine->codec_ctx.delay <= 0 &&
        engine->codec_ctx.has_b_frames <= 0) {
        engine->packet_consumed = 1;
        return -4;
    }
    switch (engine->codec_ctx.codec_id) {
    case MINI_FFMPEG_CODEC_ID_H264:
    case MINI_FFMPEG_CODEC_ID_H265:
    case MINI_FFMPEG_CODEC_ID_VP8:
    case MINI_FFMPEG_CODEC_ID_VP9:
    case MINI_FFMPEG_CODEC_ID_AV1:
        mini_ffmpeg_h264_parse_nal(engine,
                                    engine->current_pkt.data,
                                    engine->current_pkt.size);
        ret = mini_ffmpeg_h264_reconstruct_frame(engine, frame);
        break;
    case MINI_FFMPEG_CODEC_ID_AAC:
    case MINI_FFMPEG_CODEC_ID_MP3:
    case MINI_FFMPEG_CODEC_ID_OPUS:
        ret = mini_ffmpeg_aac_decode_frame(engine, frame);
        break;
    case MINI_FFMPEG_CODEC_ID_PCM_S16LE:
        ret = mini_ffmpeg_pcm_decode_frame(engine, frame);
        break;
    default:
        ret = -5;
        break;
    }
    if (engine->draining) {
        engine->codec_ctx.delay--;
        if (engine->codec_ctx.delay < 0) {
            engine->packet_consumed = 1;
            return -4;
        }
    } else {
        engine->packet_consumed = 1;
    }
    engine->frames_decoded++;
    return ret;
}

int mini_ffmpeg_decoder_flush(MiniFFmpegDecoderEngine *engine) {
    if (!engine) return -1;
    engine->draining = 1;
    engine->packet_consumed = 0;
    engine->codec_ctx.delay = engine->codec_ctx.has_b_frames;
    return 0;
}

void mini_ffmpeg_frame_init(MiniFFmpegAVFrame *frame) {
    if (!frame) return;
    memset(frame, 0, sizeof(*frame));
    frame->pts = MINI_FFMPEG_NO_PTS_VALUE;
    frame->pkt_dts = MINI_FFMPEG_NO_PTS_VALUE;
    frame->format = -1;
}

void mini_ffmpeg_frame_unref(MiniFFmpegAVFrame *frame) {
    int i;
    if (!frame) return;
    for (i = 0; i < 8; i++) {
        if (frame->data[i]) {
            free(frame->data[i]);
            frame->data[i] = NULL;
        }
        frame->linesize[i] = 0;
    }
    if (frame->priv_data) { free(frame->priv_data); frame->priv_data = NULL; }
    if (frame->opaque) { free(frame->opaque); frame->opaque = NULL; }
}

int mini_ffmpeg_frame_alloc_buffer(MiniFFmpegAVFrame *frame, int align) {
    int y_size, uv_size;
    (void)align;
    if (!frame || frame->width <= 0 || frame->height <= 0) return -1;
    y_size = frame->width * frame->height;
    uv_size = (frame->width / 2) * (frame->height / 2);
    frame->data[0] = (uint8_t *)malloc(y_size);
    frame->data[1] = (uint8_t *)malloc(uv_size);
    frame->data[2] = (uint8_t *)malloc(uv_size);
    frame->linesize[0] = frame->width;
    frame->linesize[1] = frame->width / 2;
    frame->linesize[2] = frame->width / 2;
    return 0;
}

MiniFFmpegAVFrame *mini_ffmpeg_frame_alloc(void) {
    MiniFFmpegAVFrame *frame = (MiniFFmpegAVFrame *)calloc(1, sizeof(*frame));
    if (frame) mini_ffmpeg_frame_init(frame);
    return frame;
}

void mini_ffmpeg_frame_free(MiniFFmpegAVFrame *frame) {
    if (!frame) return;
    mini_ffmpeg_frame_unref(frame);
    free(frame);
}

int mini_ffmpeg_codec_context_copy(MiniFFmpegAVCodecContext *dst,
                                    const MiniFFmpegAVCodecContext *src) {
    if (!dst || !src) return -1;
    memcpy(dst, src, sizeof(*dst));
    if (src->extradata && src->extradata_size > 0) {
        dst->extradata = (uint8_t *)malloc(src->extradata_size);
        if (!dst->extradata) return -1;
        memcpy(dst->extradata, src->extradata, src->extradata_size);
    }
    return 0;
}
