#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "decoder_engine.h"

static void print_codec_info(MiniFFmpegAVCodec *codec) {
    if (!codec) return;
    printf("  %-12s %-40s type=%d\n",
           codec->name,
           codec->long_name ? codec->long_name : "",
           codec->type);
}

int main(void) {
    MiniFFmpegDecoderEngine *engine;
    MiniFFmpegAVCodecContext codec_ctx;
    MiniFFmpegAVPacket pkt;
    MiniFFmpegAVFrame *frame;
    void *opaque = NULL;
    MiniFFmpegAVCodec *codec;
    int i, ret, decode_count;
    const int test_codecs[] = {
        MINI_FFMPEG_CODEC_ID_H264,
        MINI_FFMPEG_CODEC_ID_AAC,
        MINI_FFMPEG_CODEC_ID_PCM_S16LE,
        MINI_FFMPEG_CODEC_ID_VP9,
        MINI_FFMPEG_CODEC_ID_OPUS,
    };

    printf("=== mini-ffmpeg-lab: Decode Example ===\n\n");

    printf("--- Codec Registry ---\n");
    mini_ffmpeg_codec_register_all();
    while ((codec = mini_ffmpeg_codec_iterate(&opaque)) != NULL) {
        print_codec_info(codec);
    }
    printf("\n");

    printf("--- Codec Lookup by ID ---\n");
    codec = mini_ffmpeg_codec_find_decoder(MINI_FFMPEG_CODEC_ID_H264);
    printf("find_decoder(H264): %s\n", codec ? codec->name : "NOT FOUND");
    codec = mini_ffmpeg_codec_find_decoder(MINI_FFMPEG_CODEC_ID_AAC);
    printf("find_decoder(AAC):  %s\n", codec ? codec->name : "NOT FOUND");
    codec = mini_ffmpeg_codec_find_decoder(999);
    printf("find_decoder(999):  %s\n\n", codec ? codec->name : "NOT FOUND");

    printf("--- Codec Lookup by Name ---\n");
    codec = mini_ffmpeg_codec_find_by_name("vp9");
    printf("find_by_name(vp9): %s\n", codec ? codec->name : "NOT FOUND");
    codec = mini_ffmpeg_codec_find_by_name("opus");
    printf("find_by_name(opus): %s\n\n", codec ? codec->name : "NOT FOUND");

    for (i = 0; i < (int)(sizeof(test_codecs) / sizeof(test_codecs[0])); i++) {
        int codec_id = test_codecs[i];
        codec = mini_ffmpeg_codec_find_decoder(codec_id);
        if (!codec) {
            printf("--- Codec ID %d: no decoder found ---\n\n", codec_id);
            continue;
        }
        printf("--- Testing Decoder: %s ---\n", codec->name);

        engine = mini_ffmpeg_decoder_alloc();
        memset(&codec_ctx, 0, sizeof(codec_ctx));
        codec_ctx.codec = codec;
        codec_ctx.codec_type = codec->type;
        codec_ctx.codec_id = codec->id;
        codec_ctx.width = 1920;
        codec_ctx.height = 1080;
        codec_ctx.pix_fmt = MINI_FFMPEG_PIX_FMT_YUV420P;
        codec_ctx.sample_rate = 48000;
        codec_ctx.channels = 2;
        codec_ctx.sample_fmt = MINI_FFMPEG_SAMPLE_FMT_S16;
        codec_ctx.bit_rate = 1000000;
        codec_ctx.gop_size = 30;
        codec_ctx.max_b_frames = 2;
        codec_ctx.has_b_frames = 2;
        codec_ctx.refs = 4;
        codec_ctx.delay = 0;

        ret = mini_ffmpeg_decoder_open(engine, &codec_ctx);
        if (ret < 0) {
            fprintf(stderr, "  Failed to open decoder: %d\n", ret);
            mini_ffmpeg_decoder_free(engine);
            continue;
        }
        printf("  Decoder opened OK\n");

        mini_ffmpeg_packet_init(&pkt);
        mini_ffmpeg_packet_alloc(&pkt, 8192);
        if (pkt.data) memset(pkt.data, 0xAB, 4096);
        pkt.pts = 0;
        pkt.dts = 0;
        pkt.stream_index = 0;
        pkt.flags = MINI_FFMPEG_PKT_FLAG_KEY;
        pkt.size = 4096;

        decode_count = 0;
        frame = mini_ffmpeg_frame_alloc();
        ret = mini_ffmpeg_decoder_send_packet(engine, &pkt);
        printf("  send_packet: %d\n", ret);

        while (decode_count < 5) {
            ret = mini_ffmpeg_decoder_receive_frame(engine, frame);
            if (ret < 0) break;
            printf("  receive_frame #%d: width=%d height=%d fmt=%d "
                   "key=%d pict=%d pts=%lld\n",
                   decode_count, frame->width, frame->height, frame->format,
                   frame->key_frame, frame->pict_type,
                   (long long)frame->pts);
            decode_count++;
        }

        if (decode_count < 5) {
            mini_ffmpeg_decoder_flush(engine);
            printf("  flush sent...\n");
            ret = mini_ffmpeg_decoder_receive_frame(engine, frame);
            if (ret >= 0) {
                printf("  flush receive_frame: w=%d h=%d\n",
                       frame->width, frame->height);
                decode_count++;
            }
        }

        printf("  Total frames decoded: %d\n\n", decode_count);

        mini_ffmpeg_packet_unref(&pkt);
        mini_ffmpeg_frame_free(frame);
        mini_ffmpeg_decoder_free(engine);
    }

    printf("--- Test codec_context_copy ---\n");
    {
        MiniFFmpegAVCodecContext src, dst;
        memset(&src, 0, sizeof(src));
        src.width = 1280; src.height = 720;
        src.codec_id = MINI_FFMPEG_CODEC_ID_H264;
        src.extradata = (uint8_t *)malloc(32);
        src.extradata_size = 32;
        memset(src.extradata, 0xCD, 32);
        ret = mini_ffmpeg_codec_context_copy(&dst, &src);
        printf("  copy result: %d, width=%d, extradata_size=%d\n",
               ret, dst.width, dst.extradata_size);
        free(src.extradata);
        free(dst.extradata);
    }

    printf("\n=== Example Complete ===\n");
    return 0;
}
