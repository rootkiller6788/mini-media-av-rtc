#ifndef MINI_FFMPEG_DECODER_ENGINE_H
#define MINI_FFMPEG_DECODER_ENGINE_H

#include <stdint.h>
#include <stddef.h>
#include "demuxer.h"

#define MINI_FFMPEG_MAX_CODECS        64
#define MINI_FFMPEG_MAX_EXTRA_DATA    256
#define MINI_FFMPEG_MAX_SLICES        128
#define MINI_FFMPEG_MB_SIZE           16
#define MINI_FFMPEG_MAX_REF_FRAMES    16

enum AVPixelFormat {
    MINI_FFMPEG_PIX_FMT_NONE = -1,
    MINI_FFMPEG_PIX_FMT_YUV420P,
    MINI_FFMPEG_PIX_FMT_YUV422P,
    MINI_FFMPEG_PIX_FMT_YUV444P,
    MINI_FFMPEG_PIX_FMT_NV12,
    MINI_FFMPEG_PIX_FMT_RGB24,
    MINI_FFMPEG_PIX_FMT_BGRA,
    MINI_FFMPEG_PIX_FMT_GRAY8,
};

enum AVSampleFormat {
    MINI_FFMPEG_SAMPLE_FMT_NONE = -1,
    MINI_FFMPEG_SAMPLE_FMT_U8,
    MINI_FFMPEG_SAMPLE_FMT_S16,
    MINI_FFMPEG_SAMPLE_FMT_S32,
    MINI_FFMPEG_SAMPLE_FMT_FLT,
    MINI_FFMPEG_SAMPLE_FMT_DBL,
    MINI_FFMPEG_SAMPLE_FMT_U8P,
    MINI_FFMPEG_SAMPLE_FMT_S16P,
    MINI_FFMPEG_SAMPLE_FMT_S32P,
    MINI_FFMPEG_SAMPLE_FMT_FLTP,
    MINI_FFMPEG_SAMPLE_FMT_DBLP,
};

typedef struct MiniFFmpegAVFrame {
    uint8_t *data[8];
    int      linesize[8];
    int      width;
    int      height;
    int      format;
    int      sample_rate;
    int      channels;
    int      channel_layout;
    int      nb_samples;
    int64_t  pts;
    int64_t  pkt_dts;
    int64_t  duration;
    int      key_frame;
    int      pict_type;
        /* I=1, P=2, B=3 */
    int      coded_picture_number;
    int      display_picture_number;
    int      interlaced_frame;
    int      top_field_first;
    int      repeat_pict;
    int      quality;
    void    *opaque;
    void    *priv_data;
    int    (*buf_unref[8])(void *buf);
    void     *buf[8];
} MiniFFmpegAVFrame;

typedef struct MiniFFmpegAVCodec {
    const char *name;
    const char *long_name;
    int         type;
    int         id;
    int         caps;
} MiniFFmpegAVCodec;

typedef struct MiniFFmpegAVCodecContext {
    const MiniFFmpegAVCodec *codec;
    int   codec_type;
    int   codec_id;
    int   width;
    int   height;
    int   pix_fmt;
    int   sample_rate;
    int   channels;
    int   sample_fmt;
    int   channel_layout;
    int   bit_rate;
    int   gop_size;
    int   max_b_frames;
    int   refs;
    uint8_t *extradata;
    int      extradata_size;
    int   flags;
    int   frame_number;
    int   delay;
    int   has_b_frames;
    void *priv_data;
    void *hw_device_ctx;
    int   active_thread_type;
    int   debug;
    MiniFFmpegRational time_base;
} MiniFFmpegAVCodecContext;

typedef struct MiniFFmpegDecoderEngine MiniFFmpegDecoderEngine;

void mini_ffmpeg_codec_register_all(void);

MiniFFmpegAVCodec *mini_ffmpeg_codec_find_decoder(int codec_id);

MiniFFmpegAVCodec *mini_ffmpeg_codec_find_encoder(int codec_id);

MiniFFmpegAVCodec *mini_ffmpeg_codec_find_by_name(const char *name);

MiniFFmpegDecoderEngine *mini_ffmpeg_decoder_alloc(void);

void mini_ffmpeg_decoder_free(MiniFFmpegDecoderEngine *engine);

int mini_ffmpeg_decoder_open(MiniFFmpegDecoderEngine *engine,
                             MiniFFmpegAVCodecContext *codec_ctx);

int mini_ffmpeg_decoder_send_packet(MiniFFmpegDecoderEngine *engine,
                                    MiniFFmpegAVPacket *pkt);

int mini_ffmpeg_decoder_receive_frame(MiniFFmpegDecoderEngine *engine,
                                      MiniFFmpegAVFrame *frame);

int mini_ffmpeg_decoder_flush(MiniFFmpegDecoderEngine *engine);

void mini_ffmpeg_frame_init(MiniFFmpegAVFrame *frame);

void mini_ffmpeg_frame_unref(MiniFFmpegAVFrame *frame);

int mini_ffmpeg_frame_alloc_buffer(MiniFFmpegAVFrame *frame, int align);

MiniFFmpegAVFrame *mini_ffmpeg_frame_alloc(void);

void mini_ffmpeg_frame_free(MiniFFmpegAVFrame *frame);

int mini_ffmpeg_codec_context_copy(MiniFFmpegAVCodecContext *dst,
                                    const MiniFFmpegAVCodecContext *src);

MiniFFmpegAVCodec *mini_ffmpeg_codec_iterate(void **opaque);

#endif /* MINI_FFMPEG_DECODER_ENGINE_H */
