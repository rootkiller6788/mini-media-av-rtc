#include "av_sync.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

static double mini_ffmpeg_get_clock_time(void) {
    return (double)clock() / (double)CLOCKS_PER_SEC;
}

void mini_ffmpeg_clock_init(MiniFFmpegClock *clock, int *queue_serial) {
    if (!clock) return;
    memset(clock, 0, sizeof(*clock));
    clock->speed = 1.0;
    clock->paused = 0;
    clock->queue_serial = queue_serial;
    clock->pts = 0.0;
    clock->pts_drift = 0.0;
    clock->last_updated = 0.0;
}

void mini_ffmpeg_clock_set(MiniFFmpegClock *clock, double pts, int serial) {
    double time = mini_ffmpeg_get_clock_time();
    if (!clock) return;
    if (clock->queue_serial && serial != *clock->queue_serial)
        return;
    clock->pts = pts;
    clock->pts_drift = pts - time;
    clock->last_updated = time;
}

double mini_ffmpeg_clock_get(MiniFFmpegClock *clock) {
    double time;
    if (!clock) return 0.0;
    if (clock->paused_ptr && *clock->paused_ptr) {
        return clock->pts;
    }
    time = mini_ffmpeg_get_clock_time();
    return clock->pts_drift + time -
           (clock->last_updated - clock->pts_drift - clock->pts) *
           (1.0 - clock->speed);
}

void mini_ffmpeg_clock_set_speed(MiniFFmpegClock *clock, double speed) {
    if (!clock) return;
    mini_ffmpeg_clock_set(clock, mini_ffmpeg_clock_get(clock),
                          clock->queue_serial ? *clock->queue_serial : 0);
    clock->speed = speed;
}

double mini_ffmpeg_clock_get_speed(MiniFFmpegClock *clock) {
    return clock ? clock->speed : 1.0;
}

MiniFFmpegAVSyncContext *mini_ffmpeg_av_sync_alloc(void) {
    MiniFFmpegAVSyncContext *sync = (MiniFFmpegAVSyncContext *)
        calloc(1, sizeof(MiniFFmpegAVSyncContext));
    if (sync) {
        mini_ffmpeg_av_sync_init(sync);
    }
    return sync;
}

void mini_ffmpeg_av_sync_free(MiniFFmpegAVSyncContext *sync) {
    if (!sync) return;
    free(sync);
}

void mini_ffmpeg_av_sync_init(MiniFFmpegAVSyncContext *sync) {
    if (!sync) return;
    memset(sync, 0, sizeof(*sync));
    sync->master_sync_type = MINI_FFMPEG_SYNC_AUDIO_MASTER;
    sync->sync_threshold = MINI_FFMPEG_AV_SYNC_THRESHOLD_MIN;
    sync->frame_drop_threshold = MINI_FFMPEG_AV_SYNC_FRAMEDROP_THRESHOLD;
    sync->sync_strategy = MINI_FFMPEG_SYNC_STRATEGY_DROP_FRAME;
    sync->audio_time_base.num = 1; sync->audio_time_base.den = 48000;
    sync->video_time_base.num = 1; sync->video_time_base.den = 90000;
    sync->output_time_base.num = 1; sync->output_time_base.den = 90000;
    sync->smooth_enabled = 1;
    sync->smooth_factor = 0.9;
    sync->frame_timer_valid = 0;
    sync->force_refresh = 0;
    sync->nb_dropped_frames = 0;
    sync->nb_repeated_frames = 0;
    sync->nb_audio_buffer_refills = 0;
    sync->nb_audio_buffer_drains = 0;
    sync->avg_frame_duration = 0.0;
    sync->audio_buffer_level = 0.0;
    sync->last_video_pts = 0.0;
    sync->target_video_pts = 0.0;
    sync->drift = 0.0;
    sync->lip_sync_quality = 0.0;
    sync->lip_sync_max_error = 0.0;
    sync->lip_sync_total_error = 0.0;
    sync->lip_sync_samples = 0;
}

double mini_ffmpeg_av_sync_get_master_clock(MiniFFmpegAVSyncContext *sync) {
    if (!sync) return 0.0;
    switch (sync->master_sync_type) {
    case MINI_FFMPEG_SYNC_VIDEO_MASTER:
        return sync->video_clock_value;
    case MINI_FFMPEG_SYNC_EXTERNAL_MASTER:
        return mini_ffmpeg_clock_get(&sync->external_clock);
    case MINI_FFMPEG_SYNC_AUDIO_MASTER:
    default:
        return sync->audio_clock_value;
    }
}

int mini_ffmpeg_av_sync_compute_video_delay(MiniFFmpegAVSyncContext *sync,
                                             double pts) {
    double master_clock, diff, sync_threshold, delay;
    if (!sync) return 0;
    master_clock = mini_ffmpeg_av_sync_get_master_clock(sync);
    diff = pts - master_clock;
    delay = pts - sync->last_video_pts;
    if (delay <= 0 || delay > 1.0) {
        delay = sync->last_video_pts > 0 ?
               pts - sync->last_video_pts : sync->avg_frame_duration;
    }
    sync->last_video_pts = pts;
    sync->avg_frame_duration = sync->avg_frame_duration * 0.95 + delay * 0.05;
    sync_threshold = sync->sync_threshold > 0 ?
                     sync->sync_threshold : MINI_FFMPEG_AV_SYNC_THRESHOLD_MIN;
    if (fabs(diff) < sync_threshold) {
        diff = 0;
    }
    sync->drift = diff;
    sync->smoothed_diff = sync->smoothed_diff * sync->smooth_factor +
                          diff * (1.0 - sync->smooth_factor);
    sync->target_video_pts = pts;
    delay += diff * sync->avg_frame_duration;
    sync->lip_sync_total_error += fabs(diff);
    sync->lip_sync_samples++;
    if (sync->lip_sync_samples > 0) {
        sync->lip_sync_quality = 1.0 - (sync->lip_sync_total_error /
            (sync->lip_sync_samples * sync->avg_frame_duration + 0.001));
        if (sync->lip_sync_quality < 0.0) sync->lip_sync_quality = 0.0;
        if (sync->lip_sync_quality > 1.0) sync->lip_sync_quality = 1.0;
    }
    if (fabs(diff) > sync->lip_sync_max_error)
        sync->lip_sync_max_error = fabs(diff);
    if (delay < 0) delay = 0;
    return (int)(delay * 1000.0);
}

int mini_ffmpeg_av_sync_should_drop(MiniFFmpegAVSyncContext *sync,
                                     double pts) {
    double diff;
    if (!sync) return 0;
    if (sync->master_sync_type == MINI_FFMPEG_SYNC_VIDEO_MASTER)
        return 0;
    diff = pts - mini_ffmpeg_av_sync_get_master_clock(sync);
    if (diff < -(sync->frame_drop_threshold)) {
        sync->nb_dropped_frames++;
        return 1;
    }
    return 0;
}

int mini_ffmpeg_av_sync_should_repeat(MiniFFmpegAVSyncContext *sync,
                                       double pts) {
    double diff;
    if (!sync) return 0;
    if (sync->master_sync_type == MINI_FFMPEG_SYNC_VIDEO_MASTER)
        return 0;
    diff = pts - mini_ffmpeg_av_sync_get_master_clock(sync);
    if (diff > sync->sync_threshold) {
        sync->nb_repeated_frames++;
        return 1;
    }
    return 0;
}

int mini_ffmpeg_av_sync_set_master(MiniFFmpegAVSyncContext *sync, int type) {
    if (!sync) return -1;
    sync->master_sync_type = type;
    return 0;
}

int64_t mini_ffmpeg_av_sync_pts_to_output(MiniFFmpegAVSyncContext *sync,
                                           int64_t pts,
                                           MiniFFmpegRational *src_tb) {
    double seconds;
    if (!sync || !src_tb || pts == MINI_FFMPEG_NO_PTS_VALUE)
        return MINI_FFMPEG_NO_PTS_VALUE;
    if (src_tb->den == 0) return MINI_FFMPEG_NO_PTS_VALUE;
    seconds = (double)pts * (double)src_tb->num / (double)src_tb->den;
    if (sync->output_time_base.den == 0) return MINI_FFMPEG_NO_PTS_VALUE;
    return (int64_t)(seconds * (double)sync->output_time_base.den /
                     (double)sync->output_time_base.num);
}

void mini_ffmpeg_av_sync_update_audio_clock(MiniFFmpegAVSyncContext *sync,
                                              double pts, int serial) {
    if (!sync) return;
    sync->audio_clock_value = pts;
    mini_ffmpeg_clock_set(&sync->audio_clock, pts, serial);
}

void mini_ffmpeg_av_sync_update_video_clock(MiniFFmpegAVSyncContext *sync,
                                              double pts, int serial) {
    if (!sync) return;
    sync->video_clock_value = pts;
    mini_ffmpeg_clock_set(&sync->video_clock, pts, serial);
}

void mini_ffmpeg_av_sync_update_external_clock(MiniFFmpegAVSyncContext *sync,
                                                double pts) {
    if (!sync) return;
    mini_ffmpeg_clock_set(&sync->external_clock, pts,
        sync->external_clock.queue_serial ?
        *sync->external_clock.queue_serial : 0);
}

int mini_ffmpeg_av_sync_adjust_audio(MiniFFmpegAVSyncContext *sync,
                                      int nb_samples) {
    double diff, max_diff, ratio;
    int adj;
    if (!sync) return nb_samples;
    if (sync->master_sync_type == MINI_FFMPEG_SYNC_AUDIO_MASTER)
        return nb_samples;
    diff = sync->audio_clock_value -
           mini_ffmpeg_av_sync_get_master_clock(sync);
    max_diff = (double)nb_samples / (double)sync->audio_time_base.den;
    if (fabs(diff) < sync->sync_threshold) return nb_samples;
    ratio = diff / max_diff;
    adj = nb_samples;
    if (diff < 0) {
        adj = (int)(nb_samples * (1.0 + ratio * 0.05));
        if (adj > nb_samples * 2) adj = nb_samples * 2;
        sync->nb_audio_buffer_drains++;
    } else {
        adj = (int)(nb_samples * (1.0 - ratio * 0.05));
        if (adj < nb_samples / 2) adj = nb_samples / 2;
        sync->nb_audio_buffer_refills++;
    }
    return adj;
}

double mini_ffmpeg_av_sync_get_lip_sync_quality(MiniFFmpegAVSyncContext *sync) {
    return sync ? sync->lip_sync_quality : 0.0;
}

int mini_ffmpeg_av_sync_set_strategy(MiniFFmpegAVSyncContext *sync,
                                      int strategy) {
    if (!sync) return -1;
    sync->sync_strategy = strategy;
    return 0;
}

void mini_ffmpeg_av_sync_reset(MiniFFmpegAVSyncContext *sync) {
    if (!sync) return;
    sync->audio_clock_value = 0.0;
    sync->video_clock_value = 0.0;
    sync->last_video_pts = 0.0;
    sync->target_video_pts = 0.0;
    sync->drift = 0.0;
    sync->frame_timer_valid = 0;
    sync->frame_timer = 0.0;
    sync->force_refresh = 0;
    sync->nb_dropped_frames = 0;
    sync->nb_repeated_frames = 0;
    sync->avg_frame_duration = 0.0;
    sync->lip_sync_quality = 0.0;
    sync->lip_sync_max_error = 0.0;
    sync->lip_sync_total_error = 0.0;
    sync->lip_sync_samples = 0;
    mini_ffmpeg_clock_init(&sync->audio_clock, NULL);
    mini_ffmpeg_clock_init(&sync->video_clock, NULL);
    mini_ffmpeg_clock_init(&sync->external_clock, NULL);
}

int mini_ffmpeg_av_sync_schedule_frame(MiniFFmpegAVSyncContext *sync,
                                        double pts, int serial) {
    double master_clock, delay;
    if (!sync) return 0;
    master_clock = mini_ffmpeg_av_sync_get_master_clock(sync);
    delay = pts - master_clock;
    mini_ffmpeg_av_sync_update_video_clock(sync, pts, serial);
    if (delay <= 0) return 0;
    if (delay > 0.5) return 0;
    return (int)(delay * 1000.0);
}

void mini_ffmpeg_av_sync_smooth_timestamps(MiniFFmpegAVSyncContext *sync,
                                            double *pts, int count) {
    int i;
    double prev = 0;
    if (!sync || !pts || count <= 0) return;
    prev = pts[0];
    for (i = 1; i < count; i++) {
        double expected = prev + sync->avg_frame_duration;
        pts[i] = pts[i] * (1.0 - sync->smooth_factor) +
                 expected * sync->smooth_factor;
        prev = pts[i];
    }
}
