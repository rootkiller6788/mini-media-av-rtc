#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "demuxer.h"
#include "decoder_engine.h"
#include "av_sync.h"

typedef struct MiniFFmpegPlayerState {
    MiniFFmpegDemuxContext   *demux;
    MiniFFmpegDecoderEngine  *video_decoder;
    MiniFFmpegDecoderEngine  *audio_decoder;
    MiniFFmpegAVSyncContext  *av_sync;
    int     video_stream_index;
    int     audio_stream_index;
    int     nb_video_frames;
    int     nb_audio_frames;
    int     total_packets;
    int     running;
    int     paused;
    int     step;
    int     loop;
    double  current_time;
    double  duration_sec;
    int     video_width;
    int     video_height;
    double  video_fps;
    int     audio_sample_rate;
    int     audio_channels;
    MiniFFmpegAVFrame *last_video_frame;
    MiniFFmpegAVFrame *last_audio_frame;
} MiniFFmpegPlayerState;

static MiniFFmpegPlayerState g_player;

static int player_init(const char *filename) {
    MiniFFmpegAVStream *st;
    MiniFFmpegAVCodecContext vctx, actx;
    MiniFFmpegAVCodec *codec;
    int i, nb_streams;

    memset(&g_player, 0, sizeof(g_player));
    g_player.video_stream_index = -1;
    g_player.audio_stream_index = -1;
    g_player.running = 1;
    g_player.paused = 0;
    g_player.step = 0;

    printf("[Player] Initializing with file: %s\n", filename);

    g_player.demux = mini_ffmpeg_demux_alloc();
    if (!g_player.demux) {
        fprintf(stderr, "[Player] Failed to allocate demux\n");
        return -1;
    }
    g_player.av_sync = mini_ffmpeg_av_sync_alloc();
    if (!g_player.av_sync) {
        fprintf(stderr, "[Player] Failed to allocate AV sync\n");
        return -1;
    }

    {
        MiniFFmpegIOContext io;
        memset(&io, 0, sizeof(io));
        io.buffer = (uint8_t *)malloc(MINI_FFMPEG_IO_BUFFER_SIZE);
        io.buffer_size = MINI_FFMPEG_IO_BUFFER_SIZE;
        io.buf_ptr = io.buffer;
        io.buf_end = io.buffer;
        {
            int k;
            io.buffer[4] = 'f'; io.buffer[5] = 't';
            io.buffer[6] = 'y'; io.buffer[7] = 'p';
            for (k = 8; k < MINI_FFMPEG_MAX_PROBE_SIZE; k++)
                io.buffer[k] = (uint8_t)(k & 0xFF);
        }
        if (mini_ffmpeg_demux_open(g_player.demux, &io) < 0) {
            fprintf(stderr, "[Player] Failed to open/detect format\n");
            free(io.buffer);
            return -1;
        }
        free(io.buffer);
    }

    mini_ffmpeg_codec_register_all();
    mini_ffmpeg_demux_find_stream_info(g_player.demux);

    nb_streams = mini_ffmpeg_demux_get_nb_streams(g_player.demux);
    printf("[Player] Found %d streams\n", nb_streams);

    for (i = 0; i < nb_streams; i++) {
        st = mini_ffmpeg_demux_get_stream(g_player.demux, i);
        if (!st) continue;
        if (st->codec_type == MINI_FFMPEG_MEDIA_TYPE_VIDEO &&
            g_player.video_stream_index < 0) {
            g_player.video_stream_index = i;
            g_player.video_width = st->width;
            g_player.video_height = st->height;
            if (st->time_base.den > 0)
                g_player.video_fps = (double)st->time_base.den /
                                     (double)st->time_base.num;
            codec = mini_ffmpeg_codec_find_decoder(st->codec_id);
            if (codec) {
                memset(&vctx, 0, sizeof(vctx));
                vctx.codec = codec;
                vctx.codec_type = st->codec_type;
                vctx.codec_id = st->codec_id;
                vctx.width = 320;
                vctx.height = 240;
                vctx.time_base = st->time_base;
                vctx.pix_fmt = MINI_FFMPEG_PIX_FMT_YUV420P;
                vctx.has_b_frames = 2;
                vctx.delay = 0;
                g_player.video_decoder = mini_ffmpeg_decoder_alloc();
                mini_ffmpeg_decoder_open(g_player.video_decoder, &vctx);
                printf("[Player] Video decoder: %s (%dx%d)\n",
                       codec->name, vctx.width, vctx.height);
            }
        } else if (st->codec_type == MINI_FFMPEG_MEDIA_TYPE_AUDIO &&
                   g_player.audio_stream_index < 0) {
            g_player.audio_stream_index = i;
            g_player.audio_sample_rate = st->sample_rate;
            g_player.audio_channels = st->channels;
            codec = mini_ffmpeg_codec_find_decoder(st->codec_id);
            if (codec) {
                memset(&actx, 0, sizeof(actx));
                actx.codec = codec;
                actx.codec_type = st->codec_type;
                actx.codec_id = st->codec_id;
                actx.sample_rate = st->sample_rate;
                actx.channels = st->channels;
                actx.sample_fmt = MINI_FFMPEG_SAMPLE_FMT_S16;
                actx.time_base = st->time_base;
                actx.has_b_frames = 0;
                actx.delay = 0;
                g_player.audio_decoder = mini_ffmpeg_decoder_alloc();
                mini_ffmpeg_decoder_open(g_player.audio_decoder, &actx);
                printf("[Player] Audio decoder: %s (%dHz %dch)\n",
                       codec->name, st->sample_rate, st->channels);
            }
        }
    }

    g_player.duration_sec = (double)mini_ffmpeg_demux_get_duration(
        g_player.demux) / 1000000.0;
    printf("[Player] Duration: %.2f sec\n", g_player.duration_sec);

    mini_ffmpeg_av_sync_set_master(g_player.av_sync,
        MINI_FFMPEG_SYNC_AUDIO_MASTER);
    printf("[Player] Sync master: AUDIO\n");
    printf("[Player] Initialization complete.\n\n");
    return 0;
}

static int player_step_decoder(MiniFFmpegDecoderEngine *decoder,
                                MiniFFmpegAVPacket *pkt,
                                MiniFFmpegAVFrame *frame,
                                int *frame_count) {
    int ret;
    ret = mini_ffmpeg_decoder_send_packet(decoder, pkt);
    if (ret < 0) return ret;
    ret = mini_ffmpeg_decoder_receive_frame(decoder, frame);
    if (ret >= 0) {
        (*frame_count)++;
        return 1;
    }
    return 0;
}

static int player_process_one_iteration(void) {
    MiniFFmpegAVPacket pkt;
    MiniFFmpegAVFrame *vframe, *aframe;
    int ret, got_video, got_audio;
    double video_pts_sec = 0.0, audio_pts_sec = 0.0, master_clock;
    int packets_this_loop = 0;

    if (!g_player.running) return -1;
    if (g_player.paused && !g_player.step) return 0;
    g_player.step = 0;

    vframe = mini_ffmpeg_frame_alloc();
    aframe = mini_ffmpeg_frame_alloc();
    mini_ffmpeg_packet_init(&pkt);
    got_video = 0;
    got_audio = 0;

    while (g_player.total_packets < 300 && packets_this_loop < 15) {
        ret = mini_ffmpeg_demux_read_packet(g_player.demux, &pkt);
        if (ret < 0) {
            if (g_player.loop) {
                mini_ffmpeg_demux_seek(g_player.demux, 0, 0, 0);
                continue;
            }
            break;
        }
        packets_this_loop++;
        g_player.total_packets++;
        if (pkt.stream_index == g_player.video_stream_index &&
            g_player.video_decoder) {
            ret = player_step_decoder(g_player.video_decoder, &pkt,
                                      vframe, &g_player.nb_video_frames);
            if (ret > 0) {
                got_video = 1;
                if (vframe->pts != MINI_FFMPEG_NO_PTS_VALUE)
                    video_pts_sec = (double)vframe->pts / 90000.0;
                else
                    video_pts_sec = g_player.current_time;
            }
        } else if (pkt.stream_index == g_player.audio_stream_index &&
                   g_player.audio_decoder) {
            ret = player_step_decoder(g_player.audio_decoder, &pkt,
                                      aframe, &g_player.nb_audio_frames);
            if (ret > 0) {
                got_audio = 1;
                if (aframe->pts != MINI_FFMPEG_NO_PTS_VALUE)
                    audio_pts_sec = (double)aframe->pts / 48000.0;
                else
                    audio_pts_sec = g_player.current_time;
            }
        }
        mini_ffmpeg_packet_unref(&pkt);
        if (got_video && got_audio) break;
    }

    if (got_video) {
        mini_ffmpeg_av_sync_update_video_clock(g_player.av_sync,
            video_pts_sec, 0);
        if (g_player.last_video_frame)
            mini_ffmpeg_frame_free(g_player.last_video_frame);
        g_player.last_video_frame = vframe;
        vframe = NULL;
    } else if (got_audio) {
        mini_ffmpeg_av_sync_update_audio_clock(g_player.av_sync,
            audio_pts_sec, 0);
        if (g_player.last_audio_frame)
            mini_ffmpeg_frame_free(g_player.last_audio_frame);
        g_player.last_audio_frame = aframe;
        aframe = NULL;
    }

    master_clock = mini_ffmpeg_av_sync_get_master_clock(g_player.av_sync);
    g_player.current_time = master_clock;

    if (got_video || got_audio) {
        int video_delay = mini_ffmpeg_av_sync_compute_video_delay(
            g_player.av_sync, video_pts_sec);
        int drop = mini_ffmpeg_av_sync_should_drop(g_player.av_sync,
                    video_pts_sec);
        int repeat_val = mini_ffmpeg_av_sync_should_repeat(
                    g_player.av_sync, video_pts_sec);
        printf("[Player] t=%.3f Video#%d Audio#%d delay=%dms drop=%d "
               "repeat=%d drift=%.4f\n",
               master_clock,
               g_player.nb_video_frames,
               g_player.nb_audio_frames,
               video_delay, drop, repeat_val,
               g_player.av_sync->drift);
    }

    if (vframe) mini_ffmpeg_frame_free(vframe);
    if (aframe) mini_ffmpeg_frame_free(aframe);

    if (packets_this_loop == 0 && g_player.total_packets >= 300)
        g_player.running = 0;

    return 0;
}

static void player_shutdown(void) {
    printf("\n[Player] Shutting down...\n");
    printf("[Player] Video frames decoded: %d\n",
           g_player.nb_video_frames);
    printf("[Player] Audio frames decoded: %d\n",
           g_player.nb_audio_frames);
    printf("[Player] Total packets: %d\n", g_player.total_packets);
    printf("[Player] Dropped frames: %d\n",
           g_player.av_sync ? g_player.av_sync->nb_dropped_frames : 0);
    printf("[Player] Repeated frames: %d\n",
           g_player.av_sync ? g_player.av_sync->nb_repeated_frames : 0);
    printf("[Player] Lip-sync quality: %.2f%%\n",
           g_player.av_sync ?
           mini_ffmpeg_av_sync_get_lip_sync_quality(g_player.av_sync) * 100.0
           : 0.0);
    printf("[Player] Max lip-sync error: %.4f sec\n",
           g_player.av_sync ? g_player.av_sync->lip_sync_max_error : 0.0);

    if (g_player.last_video_frame) mini_ffmpeg_frame_free(g_player.last_video_frame);
    if (g_player.last_audio_frame) mini_ffmpeg_frame_free(g_player.last_audio_frame);
    if (g_player.video_decoder) mini_ffmpeg_decoder_free(g_player.video_decoder);
    if (g_player.audio_decoder) mini_ffmpeg_decoder_free(g_player.audio_decoder);
    if (g_player.av_sync) mini_ffmpeg_av_sync_free(g_player.av_sync);
    if (g_player.demux) mini_ffmpeg_demux_free(g_player.demux);
    printf("[Player] Cleanup complete.\n");
}

int main(int argc, char *argv[]) {
    const char *filename = "demo_sample.mp4";
    int i;
    (void)argc; (void)argv;

    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    printf("=== mini-ffmpeg-lab: Demo Player ===\n\n");
    printf("Simulated media player with full pipeline:\n");
    printf("  Demux -> Decode -> AV Sync -> Present\n\n");

    srand((unsigned int)time(NULL));

    if (player_init(filename) < 0) {
        fprintf(stderr, "Player initialization failed.\n");
        return 1;
    }

    printf("--- Starting Playback ---\n");
    for (i = 0; i < 8 && g_player.running; i++) {
        if (i == 15) {
            printf("\n--- Seeking to 5.0 sec ---\n");
            mini_ffmpeg_demux_seek(g_player.demux,
                g_player.video_stream_index >= 0 ?
                g_player.video_stream_index : 0,
                5000000, 0);
            mini_ffmpeg_av_sync_reset(g_player.av_sync);
        }
        if (i == 25) {
            printf("\n--- Pausing for 2 iterations ---\n");
            g_player.paused = 1;
        }
        if (i == 27) {
            printf("--- Resuming ---\n");
            g_player.paused = 0;
        }
        if (i == 32) {
            printf("\n--- Switching to VIDEO master ---\n");
            mini_ffmpeg_av_sync_set_master(g_player.av_sync,
                MINI_FFMPEG_SYNC_VIDEO_MASTER);
        }

        player_process_one_iteration();
    }

    printf("\n--- Playback Statistics ---\n");
    printf("  Current time:   %.3f sec\n", g_player.current_time);
    printf("  Duration:       %.2f sec\n", g_player.duration_sec);
    printf("  Progress:       %.1f%%\n",
           g_player.duration_sec > 0 ?
           (g_player.current_time / g_player.duration_sec) * 100.0 : 0.0);
    printf("  Video frames:   %d\n", g_player.nb_video_frames);
    printf("  Audio frames:   %d\n", g_player.nb_audio_frames);
    printf("  Total packets:  %d\n", g_player.total_packets);
    printf("  Dropped:        %d\n",
           g_player.av_sync->nb_dropped_frames);
    printf("  Repeated:       %d\n",
           g_player.av_sync->nb_repeated_frames);
    printf("  A/V drift:      %.4f sec\n",
           g_player.av_sync->drift);
    printf("  Lip-sync:       %.1f%%\n",
           mini_ffmpeg_av_sync_get_lip_sync_quality(
               g_player.av_sync) * 100.0);

    player_shutdown();

    printf("\n=== Demo Complete ===\n");
    return 0;
}
