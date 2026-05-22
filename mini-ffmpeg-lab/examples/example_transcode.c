#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "encoder_engine.h"
#include "decoder_engine.h"

int main(void) {
    MiniFFmpegEncoderEngine *encoder;
    MiniFFmpegAVCodecContext codec_ctx;
    MiniFFmpegEncParams params;
    MiniFFmpegAVPacket pkt;
    MiniFFmpegAVFrame *frame;
    int ret, enc_count;
    int64_t total_bits;
    float avg_qp;
    int frame_count;

    printf("=== mini-ffmpeg-lab: Transcode (Encode) Example ===\n\n");

    mini_ffmpeg_codec_register_all();

    printf("--- Encoding H.264 ---\n");
    encoder = mini_ffmpeg_encoder_alloc();
    memset(&codec_ctx, 0, sizeof(codec_ctx));
    codec_ctx.codec_id = MINI_FFMPEG_CODEC_ID_H264;
    codec_ctx.codec_type = MINI_FFMPEG_MEDIA_TYPE_VIDEO;
    mini_ffmpeg_enc_params_default(&params, MINI_FFMPEG_CODEC_ID_H264);
    params.width = 1920;
    params.height = 1080;
    params.bit_rate = 4000000;
    params.gop_size = 60;
    params.max_b_frames = 2;
    params.frame_rate_num = 30;
    params.frame_rate_den = 1;
    params.preset = MINI_FFMPEG_ENC_PRESET_FAST;
    params.rate_control = MINI_FFMPEG_RC_VBR;

    ret = mini_ffmpeg_encoder_open(encoder, &codec_ctx, &params);
    printf("encoder_open: %d\n", ret);

    printf("Sending frames...\n");
    frame = mini_ffmpeg_frame_alloc();
    frame->width = 1920;
    frame->height = 1080;
    frame->format = MINI_FFMPEG_PIX_FMT_YUV420P;
    for (enc_count = 0; enc_count < 120; enc_count++) {
        frame->pts = (int64_t)enc_count * 3000;
        frame->pict_type = 0;
        frame->key_frame = 0;
        if (enc_count % params.gop_size == 0) {
            mini_ffmpeg_encoder_force_keyframe(encoder);
        }
        mini_ffmpeg_encoder_send_frame(encoder, frame);
    }
    mini_ffmpeg_encoder_send_frame(encoder, NULL);

    enc_count = 0;
    mini_ffmpeg_packet_init(&pkt);
    while (enc_count < 130) {
        ret = mini_ffmpeg_encoder_receive_packet(encoder, &pkt);
        if (ret < 0) break;
        if (enc_count < 5 || enc_count % 20 == 0) {
            printf("  Enc Pkt #%3d: pts=%8lld size=%6d %s\n",
                   enc_count, (long long)pkt.pts, pkt.size,
                   (pkt.flags & MINI_FFMPEG_PKT_FLAG_KEY) ? "[I-FRAME]" : "");
        }
        mini_ffmpeg_packet_unref(&pkt);
        enc_count++;
    }
    printf("Total packets: %d\n\n", enc_count);

    mini_ffmpeg_encoder_get_stats(encoder, &frame_count, &avg_qp, &total_bits);
    printf("Stats: frames=%d avg_qp=%.2f total_bits=%lld\n\n",
           frame_count, avg_qp, (long long)total_bits);

    mini_ffmpeg_frame_free(frame);
    mini_ffmpeg_encoder_free(encoder);

    printf("--- Encoding AAC ---\n");
    encoder = mini_ffmpeg_encoder_alloc();
    memset(&codec_ctx, 0, sizeof(codec_ctx));
    codec_ctx.codec_id = MINI_FFMPEG_CODEC_ID_AAC;
    codec_ctx.codec_type = MINI_FFMPEG_MEDIA_TYPE_AUDIO;
    mini_ffmpeg_enc_params_default(&params, MINI_FFMPEG_CODEC_ID_AAC);
    params.sample_rate = 48000;
    params.channels = 2;
    params.bit_rate = 128000;

    ret = mini_ffmpeg_encoder_open(encoder, &codec_ctx, &params);
    printf("encoder_open (AAC): %d\n", ret);

    frame = mini_ffmpeg_frame_alloc();
    frame->sample_rate = 48000;
    frame->channels = 2;
    frame->nb_samples = 1024;
    frame->format = MINI_FFMPEG_SAMPLE_FMT_S16;
    for (enc_count = 0; enc_count < 50; enc_count++) {
        frame->pts = (int64_t)enc_count * 1024 * 1000000 / 48000;
        mini_ffmpeg_encoder_send_frame(encoder, frame);
    }
    mini_ffmpeg_encoder_send_frame(encoder, NULL);

    enc_count = 0;
    mini_ffmpeg_packet_init(&pkt);
    while (enc_count < 60) {
        ret = mini_ffmpeg_encoder_receive_packet(encoder, &pkt);
        if (ret < 0) break;
        if (enc_count < 3) {
            printf("  AAC Pkt #%d: pts=%8lld size=%d\n",
                   enc_count, (long long)pkt.pts, pkt.size);
        }
        mini_ffmpeg_packet_unref(&pkt);
        enc_count++;
    }
    printf("Total AAC packets: %d\n\n", enc_count);

    mini_ffmpeg_frame_free(frame);
    mini_ffmpeg_encoder_free(encoder);

    printf("--- Rate Control Test ---\n");
    {
        MiniFFmpegRateControlContext rc;
        float qp;
        int bits;
        mini_ffmpeg_enc_params_default(&params, MINI_FFMPEG_CODEC_ID_H264);
        params.bit_rate = 2000000;
        params.frame_rate_num = 30;
        mini_ffmpeg_rate_control_init(&rc, &params);
        printf("Initial QP: %.2f\n", mini_ffmpeg_rate_control_get_qp(&rc,
               MINI_FFMPEG_ENC_PIC_TYPE_I));
        bits = 50000;
        mini_ffmpeg_rate_control_update(&rc, MINI_FFMPEG_ENC_PIC_TYPE_I, bits);
        qp = mini_ffmpeg_rate_control_get_qp(&rc, MINI_FFMPEG_ENC_PIC_TYPE_I);
        printf("After I-frame (%d bits): QP=%.2f\n", bits, qp);
        bits = 5000;
        mini_ffmpeg_rate_control_update(&rc, MINI_FFMPEG_ENC_PIC_TYPE_B, bits);
        qp = mini_ffmpeg_rate_control_get_qp(&rc, MINI_FFMPEG_ENC_PIC_TYPE_B);
        printf("After B-frame (%d bits): QP=%.2f\n", bits, qp);
        bits = 15000;
        mini_ffmpeg_rate_control_update(&rc, MINI_FFMPEG_ENC_PIC_TYPE_P, bits);
        qp = mini_ffmpeg_rate_control_get_qp(&rc, MINI_FFMPEG_ENC_PIC_TYPE_P);
        printf("After P-frame (%d bits): QP=%.2f\n", bits, qp);
    }

    printf("\n--- Dynamic Settings ---\n");
    {
        MiniFFmpegEncoderEngine *enc2 = mini_ffmpeg_encoder_alloc();
        MiniFFmpegAVCodecContext ctx2;
        MiniFFmpegEncParams p2;
        memset(&ctx2, 0, sizeof(ctx2));
        ctx2.codec_id = MINI_FFMPEG_CODEC_ID_H264;
        mini_ffmpeg_enc_params_default(&p2, MINI_FFMPEG_CODEC_ID_H264);
        p2.width = 640; p2.height = 480; p2.bit_rate = 1000000;
        mini_ffmpeg_encoder_open(enc2, &ctx2, &p2);
        mini_ffmpeg_encoder_set_bitrate(enc2, 2000000);
        mini_ffmpeg_encoder_set_gop_size(enc2, 120);
        mini_ffmpeg_encoder_set_preset(enc2, MINI_FFMPEG_ENC_PRESET_SLOW);
        mini_ffmpeg_encoder_set_rate_control(enc2, MINI_FFMPEG_RC_CBR);
        printf("  Settings changed: OK\n");
        mini_ffmpeg_encoder_free(enc2);
    }

    printf("\n=== Example Complete ===\n");
    return 0;
}
