#ifndef MINI_FFMPEG_ENCODER_ENGINE_H
#define MINI_FFMPEG_ENCODER_ENGINE_H

#include <stdint.h>
#include <stddef.h>
#include "decoder_engine.h"

#define MINI_FFMPEG_MAX_ENC_DELAY      16
#define MINI_FFMPEG_RC_BUFFER_SIZE     256
#define MINI_FFMPEG_MAX_GOPS           64
#define MINI_FFMPEG_MAX_SLICES_ENC     32

enum AVEncPictureType {
    MINI_FFMPEG_ENC_PIC_TYPE_I = 1,
    MINI_FFMPEG_ENC_PIC_TYPE_P = 2,
    MINI_FFMPEG_ENC_PIC_TYPE_B = 3,
};

enum AVEncPreset {
    MINI_FFMPEG_ENC_PRESET_ULTRAFAST,
    MINI_FFMPEG_ENC_PRESET_VERYFAST,
    MINI_FFMPEG_ENC_PRESET_FAST,
    MINI_FFMPEG_ENC_PRESET_MEDIUM,
    MINI_FFMPEG_ENC_PRESET_SLOW,
    MINI_FFMPEG_ENC_PRESET_VERYSLOW,
    MINI_FFMPEG_ENC_PRESET_PLACEBO,
};

enum AVEncRateControl {
    MINI_FFMPEG_RC_CQP = 0,
    MINI_FFMPEG_RC_CBR,
    MINI_FFMPEG_RC_VBR,
    MINI_FFMPEG_RC_ABR,
    MINI_FFMPEG_RC_CRF,
};

enum AVEncFlag {
    MINI_FFMPEG_ENC_FLAG_CLOSED_GOP     = 0x0001,
    MINI_FFMPEG_ENC_FLAG_LOW_DELAY      = 0x0002,
    MINI_FFMPEG_ENC_FLAG_GLOBAL_HEADER  = 0x0004,
    MINI_FFMPEG_ENC_FLAG_PSNR           = 0x0010,
    MINI_FFMPEG_ENC_FLAG_SSIM           = 0x0020,
};

typedef struct MiniFFmpegEncParams {
    int   width;
    int   height;
    int   pix_fmt;
    int   sample_rate;
    int   channels;
    int   sample_fmt;
    int   bit_rate;
    int   gop_size;
    int   max_b_frames;
    int   keyint_min;
    int   refs;
    int   preset;
    int   rate_control;
    int   qp_constant;
    int   crf_value;
    int   profile;
    int   level;
    int   threads;
    int   flags;
    int   frame_rate_num;
    int   frame_rate_den;
    char  preset_name[32];
    char  tune[32];
    char  profile_name[32];
    int   scene_change_detection;
    int   adaptive_quantization;
    int   lookahead_frames;
    int   rc_buffer_size;
    int   rc_initial_buffer_occupancy;
    int   min_bitrate;
    int   max_bitrate;
    float qp_min_i;
    float qp_max_i;
    float qp_min_p;
    float qp_max_p;
} MiniFFmpegEncParams;

typedef struct MiniFFmpegRateControlContext {
    int   mode;
    int   bit_rate;
    float qp;
    int   buffer_size;
    int   buffer_level;
    int   frame_count;
    int64_t last_time;
    float accumulated_bits;
    float target_bits_per_frame;
    float average_qp;
    int   scene_change_threshold;
    int   reactive_qp;
} MiniFFmpegRateControlContext;

typedef struct MiniFFmpegEncoderEngine MiniFFmpegEncoderEngine;

MiniFFmpegEncoderEngine *mini_ffmpeg_encoder_alloc(void);

void mini_ffmpeg_encoder_free(MiniFFmpegEncoderEngine *engine);

int mini_ffmpeg_encoder_open(MiniFFmpegEncoderEngine *engine,
                             MiniFFmpegAVCodecContext *codec_ctx,
                             const MiniFFmpegEncParams *params);

int mini_ffmpeg_encoder_send_frame(MiniFFmpegEncoderEngine *engine,
                                   const MiniFFmpegAVFrame *frame);

int mini_ffmpeg_encoder_receive_packet(MiniFFmpegEncoderEngine *engine,
                                       MiniFFmpegAVPacket *pkt);

int mini_ffmpeg_encoder_flush(MiniFFmpegEncoderEngine *engine);

int mini_ffmpeg_encoder_set_bitrate(MiniFFmpegEncoderEngine *engine,
                                     int bit_rate);

int mini_ffmpeg_encoder_set_gop_size(MiniFFmpegEncoderEngine *engine,
                                      int gop_size);

int mini_ffmpeg_encoder_set_preset(MiniFFmpegEncoderEngine *engine,
                                    int preset);

int mini_ffmpeg_encoder_set_rate_control(MiniFFmpegEncoderEngine *engine,
                                          int mode);

int mini_ffmpeg_encoder_force_keyframe(MiniFFmpegEncoderEngine *engine);

int mini_ffmpeg_encoder_get_stats(MiniFFmpegEncoderEngine *engine,
                                   int *frame_count,
                                   float *avg_qp,
                                   int64_t *total_bits);

void mini_ffmpeg_enc_params_default(MiniFFmpegEncParams *params, int codec_id);

void mini_ffmpeg_rate_control_init(MiniFFmpegRateControlContext *rc,
                                    const MiniFFmpegEncParams *params);

void mini_ffmpeg_rate_control_update(MiniFFmpegRateControlContext *rc,
                                      int frame_type, int frame_bits);

float mini_ffmpeg_rate_control_get_qp(MiniFFmpegRateControlContext *rc,
                                       int frame_type);

#endif /* MINI_FFMPEG_ENCODER_ENGINE_H */
