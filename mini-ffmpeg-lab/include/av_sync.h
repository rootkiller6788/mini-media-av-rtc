#ifndef MINI_FFMPEG_AV_SYNC_H
#define MINI_FFMPEG_AV_SYNC_H

#include <stdint.h>
#include <stddef.h>
#include "demuxer.h"

#define MINI_FFMPEG_SYNC_VIDEO_MASTER   0
#define MINI_FFMPEG_SYNC_AUDIO_MASTER   1
#define MINI_FFMPEG_SYNC_EXTERNAL_MASTER 2

#define MINI_FFMPEG_AV_SYNC_THRESHOLD_MIN   0.04
#define MINI_FFMPEG_AV_SYNC_THRESHOLD_MAX   0.1
#define MINI_FFMPEG_AV_SYNC_FRAMEDROP_THRESHOLD 0.2

#define MINI_FFMPEG_AUDIO_MIN_BUFFER_SIZE   1024
#define MINI_FFMPEG_AUDIO_MAX_BUFFER_SIZE   (1024 * 1024)
#define MINI_FFMPEG_VIDEO_MAX_FRAME_DELAY   10
#define MINI_FFMPEG_MAX_SYNC_SAMPLES        64

enum AVSyncStrategy {
    MINI_FFMPEG_SYNC_STRATEGY_REPEAT_LAST,
    MINI_FFMPEG_SYNC_STRATEGY_DROP_FRAME,
    MINI_FFMPEG_SYNC_STRATEGY_INTERPOLATE,
    MINI_FFMPEG_SYNC_STRATEGY_AUDIO_RESAMPLE,
};

typedef struct MiniFFmpegClock {
    double   pts;
    double   pts_drift;
    double   last_updated;
    double   speed;
    int     *queue_serial;
    int      paused;
    int     *paused_ptr;
} MiniFFmpegClock;

typedef struct MiniFFmpegAVSyncContext {
    MiniFFmpegClock audio_clock;
    MiniFFmpegClock video_clock;
    MiniFFmpegClock external_clock;

    MiniFFmpegRational audio_time_base;
    MiniFFmpegRational video_time_base;
    MiniFFmpegRational output_time_base;

    double  audio_clock_value;
    double  video_clock_value;

    int     master_sync_type;

    double  sync_threshold;
    double  frame_drop_threshold;

    int     nb_dropped_frames;
    int     nb_repeated_frames;
    int     nb_audio_buffer_refills;
    int     nb_audio_buffer_drains;

    double  avg_frame_duration;
    double  audio_buffer_level;
    double  last_video_pts;
    double  target_video_pts;
    double  drift;

    int     frame_timer_valid;
    double  frame_timer;
    int     force_refresh;

    int     sync_strategy;

    int64_t audio_callback_time;
    int     audio_hw_buf_size;

    int     smooth_enabled;
    double  smooth_factor;
    double  smoothed_diff;

    double  lip_sync_quality;
    double  lip_sync_max_error;
    double  lip_sync_total_error;
    int     lip_sync_samples;

    void   *opaque;
} MiniFFmpegAVSyncContext;

MiniFFmpegAVSyncContext *mini_ffmpeg_av_sync_alloc(void);

void mini_ffmpeg_av_sync_free(MiniFFmpegAVSyncContext *sync);

void mini_ffmpeg_av_sync_init(MiniFFmpegAVSyncContext *sync);

void mini_ffmpeg_clock_init(MiniFFmpegClock *clock, int *queue_serial);

void mini_ffmpeg_clock_set(MiniFFmpegClock *clock, double pts, int serial);

double mini_ffmpeg_clock_get(MiniFFmpegClock *clock);

void mini_ffmpeg_clock_set_speed(MiniFFmpegClock *clock, double speed);

double mini_ffmpeg_clock_get_speed(MiniFFmpegClock *clock);

int mini_ffmpeg_av_sync_compute_video_delay(MiniFFmpegAVSyncContext *sync,
                                             double pts);

int mini_ffmpeg_av_sync_should_drop(MiniFFmpegAVSyncContext *sync,
                                     double pts);

int mini_ffmpeg_av_sync_should_repeat(MiniFFmpegAVSyncContext *sync,
                                       double pts);

double mini_ffmpeg_av_sync_get_master_clock(MiniFFmpegAVSyncContext *sync);

int mini_ffmpeg_av_sync_set_master(MiniFFmpegAVSyncContext *sync, int type);

int64_t mini_ffmpeg_av_sync_pts_to_output(MiniFFmpegAVSyncContext *sync,
                                           int64_t pts,
                                           MiniFFmpegRational *src_tb);

void mini_ffmpeg_av_sync_update_audio_clock(MiniFFmpegAVSyncContext *sync,
                                              double pts, int serial);

void mini_ffmpeg_av_sync_update_video_clock(MiniFFmpegAVSyncContext *sync,
                                              double pts, int serial);

void mini_ffmpeg_av_sync_update_external_clock(MiniFFmpegAVSyncContext *sync,
                                                double pts);

int mini_ffmpeg_av_sync_adjust_audio(MiniFFmpegAVSyncContext *sync,
                                      int nb_samples);

double mini_ffmpeg_av_sync_get_lip_sync_quality(MiniFFmpegAVSyncContext *sync);

int mini_ffmpeg_av_sync_set_strategy(MiniFFmpegAVSyncContext *sync,
                                      int strategy);

void mini_ffmpeg_av_sync_reset(MiniFFmpegAVSyncContext *sync);

int mini_ffmpeg_av_sync_schedule_frame(MiniFFmpegAVSyncContext *sync,
                                        double pts, int serial);

void mini_ffmpeg_av_sync_smooth_timestamps(MiniFFmpegAVSyncContext *sync,
                                            double *pts, int count);

#endif /* MINI_FFMPEG_AV_SYNC_H */
