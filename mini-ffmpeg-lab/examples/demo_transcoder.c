#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "demuxer.h"
#include "decoder_engine.h"
#include "encoder_engine.h"
#include "filter_graph.h"
#include "av_sync.h"

typedef struct MiniFFmpegTranscodeState {
    MiniFFmpegDemuxContext   *demux;
    MiniFFmpegDecoderEngine  *video_dec;
    MiniFFmpegDecoderEngine  *audio_dec;
    MiniFFmpegEncoderEngine  *video_enc;
    MiniFFmpegEncoderEngine  *audio_enc;
    MiniFFmpegAVFilterGraph  *video_filter_graph;
    MiniFFmpegAVFilterGraph  *audio_filter_graph;
    MiniFFmpegAVFilterContext *vsrc, *vsink;
    MiniFFmpegAVFilterContext *asrc, *asink;
    MiniFFmpegAVSyncContext  *sync;
    int     vstream_idx;
    int     astream_idx;
    int     total_packets;
    int     vframes_dec;
    int     aframes_dec;
    int     vpkts_enc;
    int     apkts_enc;
    int     running;
    int     use_video_filters;
    int     use_audio_filters;
    int64_t input_bitrate;
    int64_t output_bitrate;
    double  elapsed_sec;
} MiniFFmpegTranscodeState;

static MiniFFmpegTranscodeState g_state;

static int transcode_init(const char *input, const char *output,
                           int target_w, int target_h,
                           int target_bitrate_v, int target_bitrate_a) {
    MiniFFmpegAVStream *st;
    MiniFFmpegAVCodecContext vdec_ctx, adec_ctx;
    MiniFFmpegAVCodecContext venc_ctx, aenc_ctx;
    MiniFFmpegEncParams venc_params, aenc_params;
    MiniFFmpegAVCodec *codec;
    int nb_streams, i;

    memset(&g_state, 0, sizeof(g_state));
    g_state.vstream_idx = -1;
    g_state.astream_idx = -1;
    g_state.running = 1;
    g_state.use_video_filters = 1;
    g_state.use_audio_filters = 1;

    printf("[Transcoder] Input:  %s\n", input);
    printf("[Transcoder] Output: %s\n", output);
    printf("[Transcoder] Target: %dx%d @ %d kbps video, %d kbps audio\n",
           target_w, target_h, target_bitrate_v / 1000,
           target_bitrate_a / 1000);

    g_state.demux = mini_ffmpeg_demux_alloc();
    g_state.sync = mini_ffmpeg_av_sync_alloc();

    {
        MiniFFmpegIOContext io;
        memset(&io, 0, sizeof(io));
        io.buffer = (uint8_t *)malloc(MINI_FFMPEG_IO_BUFFER_SIZE);
        io.buffer_size = MINI_FFMPEG_IO_BUFFER_SIZE;
        io.buf_ptr = io.buffer;
        io.buf_end = io.buffer;
        io.buffer[4] = 'f'; io.buffer[5] = 't';
        io.buffer[6] = 'y'; io.buffer[7] = 'p';
        mini_ffmpeg_demux_open(g_state.demux, &io);
        free(io.buffer);
    }

    mini_ffmpeg_codec_register_all();
    mini_ffmpeg_filter_register_all();
    mini_ffmpeg_demux_find_stream_info(g_state.demux);

    nb_streams = mini_ffmpeg_demux_get_nb_streams(g_state.demux);
    printf("[Transcoder] Input has %d streams\n", nb_streams);

    for (i = 0; i < nb_streams; i++) {
        st = mini_ffmpeg_demux_get_stream(g_state.demux, i);
        if (!st) continue;
        g_state.input_bitrate += st->bit_rate;
        if (st->codec_type == MINI_FFMPEG_MEDIA_TYPE_VIDEO &&
            g_state.vstream_idx < 0) {
            g_state.vstream_idx = i;
            codec = mini_ffmpeg_codec_find_decoder(st->codec_id);
            if (codec) {
                memset(&vdec_ctx, 0, sizeof(vdec_ctx));
                vdec_ctx.codec = codec;
                vdec_ctx.codec_type = st->codec_type;
                vdec_ctx.codec_id = st->codec_id;
                vdec_ctx.width = st->width;
                vdec_ctx.height = st->height;
                vdec_ctx.pix_fmt = MINI_FFMPEG_PIX_FMT_YUV420P;
                vdec_ctx.time_base = st->time_base;
                vdec_ctx.has_b_frames = 2;
                g_state.video_dec = mini_ffmpeg_decoder_alloc();
                mini_ffmpeg_decoder_open(g_state.video_dec, &vdec_ctx);
            }
            mini_ffmpeg_enc_params_default(&venc_params,
                MINI_FFMPEG_CODEC_ID_H264);
            venc_params.width = target_w > 0 ? target_w : st->width;
            venc_params.height = target_h > 0 ? target_h : st->height;
            venc_params.bit_rate = target_bitrate_v;
            venc_params.gop_size = 60;
            venc_params.max_b_frames = 2;
            venc_params.frame_rate_num = 30;
            venc_params.frame_rate_den = 1;
            venc_params.preset = MINI_FFMPEG_ENC_PRESET_MEDIUM;
            venc_params.rate_control = MINI_FFMPEG_RC_VBR;
            venc_params.scene_change_detection = 1;
            memset(&venc_ctx, 0, sizeof(venc_ctx));
            venc_ctx.codec_id = MINI_FFMPEG_CODEC_ID_H264;
            venc_ctx.codec_type = MINI_FFMPEG_MEDIA_TYPE_VIDEO;
            g_state.video_enc = mini_ffmpeg_encoder_alloc();
            mini_ffmpeg_encoder_open(g_state.video_enc, &venc_ctx,
                                      &venc_params);
            printf("[Transcoder] Video: %s -> H.264 %dx%d\n",
                   codec->name, venc_params.width, venc_params.height);
        } else if (st->codec_type == MINI_FFMPEG_MEDIA_TYPE_AUDIO &&
                   g_state.astream_idx < 0) {
            g_state.astream_idx = i;
            codec = mini_ffmpeg_codec_find_decoder(st->codec_id);
            if (codec) {
                memset(&adec_ctx, 0, sizeof(adec_ctx));
                adec_ctx.codec = codec;
                adec_ctx.codec_type = st->codec_type;
                adec_ctx.codec_id = st->codec_id;
                adec_ctx.sample_rate = st->sample_rate;
                adec_ctx.channels = st->channels;
                adec_ctx.sample_fmt = MINI_FFMPEG_SAMPLE_FMT_S16;
                adec_ctx.time_base = st->time_base;
                g_state.audio_dec = mini_ffmpeg_decoder_alloc();
                mini_ffmpeg_decoder_open(g_state.audio_dec, &adec_ctx);
            }
            mini_ffmpeg_enc_params_default(&aenc_params,
                MINI_FFMPEG_CODEC_ID_AAC);
            aenc_params.sample_rate = st->sample_rate > 0 ?
                                      st->sample_rate : 48000;
            aenc_params.channels = st->channels > 0 ? st->channels : 2;
            aenc_params.bit_rate = target_bitrate_a;
            memset(&aenc_ctx, 0, sizeof(aenc_ctx));
            aenc_ctx.codec_id = MINI_FFMPEG_CODEC_ID_AAC;
            aenc_ctx.codec_type = MINI_FFMPEG_MEDIA_TYPE_AUDIO;
            g_state.audio_enc = mini_ffmpeg_encoder_alloc();
            mini_ffmpeg_encoder_open(g_state.audio_enc, &aenc_ctx,
                                      &aenc_params);
            printf("[Transcoder] Audio: %s -> AAC %dHz %dch\n",
                   codec->name, aenc_params.sample_rate,
                   aenc_params.channels);
        }
    }

    g_state.video_filter_graph = mini_ffmpeg_filter_graph_alloc();
    mini_ffmpeg_filter_graph_create_src(g_state.video_filter_graph,
                                         "vsrc", &g_state.vsrc);
    mini_ffmpeg_filter_graph_create_sink(g_state.video_filter_graph,
                                          "vsink", &g_state.vsink);
    printf("[Transcoder] Video filter graph created\n");

    g_state.audio_filter_graph = mini_ffmpeg_filter_graph_alloc();
    mini_ffmpeg_filter_graph_create_src(g_state.audio_filter_graph,
                                         "asrc", &g_state.asrc);
    mini_ffmpeg_filter_graph_create_sink(g_state.audio_filter_graph,
                                          "asink", &g_state.asink);
    printf("[Transcoder] Audio filter graph created\n");

    mini_ffmpeg_av_sync_set_master(g_state.sync,
        MINI_FFMPEG_SYNC_AUDIO_MASTER);
    printf("[Transcoder] Initialization complete.\n\n");

    return 0;
}

static int transcode_process_packet(void) {
    MiniFFmpegAVPacket pkt;
    MiniFFmpegAVFrame *frame;
    int ret;
    int processed = 0;

    if (!g_state.running) return -1;

    mini_ffmpeg_packet_init(&pkt);
    frame = mini_ffmpeg_frame_alloc();

    while (g_state.total_packets < 250 && processed < 10) {
        ret = mini_ffmpeg_demux_read_packet(g_state.demux, &pkt);
        if (ret < 0) break;

        g_state.total_packets++;
        processed++;

        if (pkt.stream_index == g_state.vstream_idx &&
            g_state.video_dec && g_state.video_enc) {
            ret = mini_ffmpeg_decoder_send_packet(g_state.video_dec, &pkt);
            if (ret >= 0) {
                while (mini_ffmpeg_decoder_receive_frame(
                           g_state.video_dec, frame) >= 0) {
                    g_state.vframes_dec++;

                    if (g_state.use_video_filters) {
                        mini_ffmpeg_filter_graph_send_frame(
                            g_state.video_filter_graph,
                            g_state.vsink, frame);
                        mini_ffmpeg_filter_graph_receive_frame(
                            g_state.video_filter_graph,
                            g_state.vsink, frame);
                    }

                    mini_ffmpeg_encoder_send_frame(g_state.video_enc,
                                                    frame);
                    MiniFFmpegAVPacket enc_pkt;
                    mini_ffmpeg_packet_init(&enc_pkt);
                    while (mini_ffmpeg_encoder_receive_packet(
                               g_state.video_enc, &enc_pkt) >= 0) {
                        g_state.vpkts_enc++;
                        g_state.output_bitrate += enc_pkt.size * 8;
                        mini_ffmpeg_packet_unref(&enc_pkt);
                    }
                }
            }
        } else if (pkt.stream_index == g_state.astream_idx &&
                   g_state.audio_dec && g_state.audio_enc) {
            ret = mini_ffmpeg_decoder_send_packet(g_state.audio_dec, &pkt);
            if (ret >= 0) {
                while (mini_ffmpeg_decoder_receive_frame(
                           g_state.audio_dec, frame) >= 0) {
                    g_state.aframes_dec++;

                    if (g_state.use_audio_filters) {
                        int adj = mini_ffmpeg_av_sync_adjust_audio(
                            g_state.sync, frame->nb_samples);
                        frame->nb_samples = adj;
                    }

                    mini_ffmpeg_encoder_send_frame(g_state.audio_enc,
                                                    frame);
                    MiniFFmpegAVPacket enc_pkt;
                    mini_ffmpeg_packet_init(&enc_pkt);
                    while (mini_ffmpeg_encoder_receive_packet(
                               g_state.audio_enc, &enc_pkt) >= 0) {
                        g_state.apkts_enc++;
                        g_state.output_bitrate += enc_pkt.size * 8;
                        mini_ffmpeg_packet_unref(&enc_pkt);
                    }
                }
            }
        }
        mini_ffmpeg_packet_unref(&pkt);
    }

    mini_ffmpeg_frame_free(frame);

    if (processed == 0 && g_state.total_packets >= 250)
        g_state.running = 0;

    return 0;
}

static void transcode_finish(void) {
    g_state.running = 0;
    printf("\n[Transcoder] Flushing encoders...\n");

    if (g_state.video_enc) {
        mini_ffmpeg_encoder_send_frame(g_state.video_enc, NULL);
        MiniFFmpegAVPacket pkt;
        mini_ffmpeg_packet_init(&pkt);
        while (mini_ffmpeg_encoder_receive_packet(
                   g_state.video_enc, &pkt) >= 0) {
            g_state.vpkts_enc++;
            mini_ffmpeg_packet_unref(&pkt);
        }
    }
    if (g_state.audio_enc) {
        mini_ffmpeg_encoder_send_frame(g_state.audio_enc, NULL);
        MiniFFmpegAVPacket pkt;
        mini_ffmpeg_packet_init(&pkt);
        while (mini_ffmpeg_encoder_receive_packet(
                   g_state.audio_enc, &pkt) >= 0) {
            g_state.apkts_enc++;
            mini_ffmpeg_packet_unref(&pkt);
        }
    }
}

static void transcode_shutdown(void) {
    printf("[Transcoder] Shutting down...\n");

    if (g_state.video_dec) mini_ffmpeg_decoder_free(g_state.video_dec);
    if (g_state.audio_dec) mini_ffmpeg_decoder_free(g_state.audio_dec);
    if (g_state.video_enc) mini_ffmpeg_encoder_free(g_state.video_enc);
    if (g_state.audio_enc) mini_ffmpeg_encoder_free(g_state.audio_enc);
    if (g_state.video_filter_graph)
        mini_ffmpeg_filter_graph_free(g_state.video_filter_graph);
    if (g_state.audio_filter_graph)
        mini_ffmpeg_filter_graph_free(g_state.audio_filter_graph);
    if (g_state.sync) mini_ffmpeg_av_sync_free(g_state.sync);
    if (g_state.demux) mini_ffmpeg_demux_free(g_state.demux);

    printf("[Transcoder] Cleanup complete.\n");
}

int main(int argc, char *argv[]) {
    const char *input = "input_4k.mkv";
    const char *output = "output_1080p.mp4";
    int i;
    (void)argc; (void)argv;

    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    printf("=== mini-ffmpeg-lab: Demo Transcoder ===\n\n");
    printf("Full transcoding pipeline simulation:\n");
    printf("  Demux -> Decode -> Filters -> Encode -> AV Sync\n\n");

    srand((unsigned int)time(NULL));

    if (transcode_init(input, output, 1920, 1080, 4000000, 128000) < 0) {
        fprintf(stderr, "Transcoder initialization failed.\n");
        return 1;
    }

    printf("--- Starting Transcode ---\n");
    for (i = 0; i < 8 && g_state.running; i++) {
        if (i == 10) {
            printf("\n--- Adjusting bitrate: 2 Mbps -> 6 Mbps ---\n");
            if (g_state.video_enc)
                mini_ffmpeg_encoder_set_bitrate(g_state.video_enc,
                                                 6000000);
        }
        if (i == 18) {
            printf("\n--- Switching to CBR rate control ---\n");
            if (g_state.video_enc)
                mini_ffmpeg_encoder_set_rate_control(
                    g_state.video_enc, MINI_FFMPEG_RC_CBR);
        }
        if (i == 22) {
            printf("\n--- Forcing keyframe ---\n");
            if (g_state.video_enc)
                mini_ffmpeg_encoder_force_keyframe(g_state.video_enc);
        }

        transcode_process_packet();

        if (i % 5 == 4) {
            int v_enc_frames;
            float v_qp;
            int64_t v_bits;
            if (g_state.video_enc) {
                mini_ffmpeg_encoder_get_stats(g_state.video_enc,
                    &v_enc_frames, &v_qp, &v_bits);
                printf("\n[Progress #%2d] Video: %d dec / %d enc, "
                       "Audio: %d dec / %d enc\n",
                       i + 1, g_state.vframes_dec, v_enc_frames,
                       g_state.aframes_dec, g_state.apkts_enc);
                printf("              QP=%.1f, bits=%lld, drift=%.4f\n",
                       v_qp, (long long)v_bits,
                       g_state.sync->drift);
            }
        }
    }

    transcode_finish();

    printf("\n--- Transcoding Statistics ---\n");
    printf("  Input packets:        %d\n", g_state.total_packets);
    printf("  Video frames decoded:  %d\n", g_state.vframes_dec);
    printf("  Audio frames decoded:  %d\n", g_state.aframes_dec);
    printf("  Video packets encoded: %d\n", g_state.vpkts_enc);
    printf("  Audio packets encoded: %d\n", g_state.apkts_enc);
    {
        int fc; float qp; int64_t tb;
        if (g_state.video_enc &&
            mini_ffmpeg_encoder_get_stats(g_state.video_enc, &fc, &qp,
                                           &tb) >= 0) {
            printf("  Encoder QP avg:        %.1f\n", qp);
            printf("  Output video bits:     %lld\n", (long long)tb);
        }
    }
    printf("  Input bitrate est:   %lld bps\n",
           (long long)g_state.input_bitrate);
    printf("  Compression ratio:    %.2f:1\n",
           g_state.output_bitrate > 0 ?
           (double)g_state.input_bitrate /
           (double)g_state.output_bitrate : 0.0);
    printf("  AV drift final:       %.4f sec\n",
           g_state.sync->drift);

    transcode_shutdown();

    printf("\n=== Demo Complete ===\n");
    return 0;
}
