#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "demuxer.h"

int main(void) {
    MiniFFmpegDemuxContext *demux;
    MiniFFmpegIOContext io;
    MiniFFmpegAVPacket pkt;
    int nb_streams, i, ret;
    int pkt_count = 0;

    printf("=== mini-ffmpeg-lab: Demux Example ===\n\n");

    demux = mini_ffmpeg_demux_alloc();
    if (!demux) {
        fprintf(stderr, "Failed to allocate demuxer\n");
        return 1;
    }

    memset(&io, 0, sizeof(io));
    io.buffer = (uint8_t *)malloc(MINI_FFMPEG_IO_BUFFER_SIZE);
    io.buffer_size = MINI_FFMPEG_IO_BUFFER_SIZE;
    io.buf_ptr = io.buffer;
    io.buf_end = io.buffer;

    {
        int probe_idx;
        io.buffer[4] = 'f'; io.buffer[5] = 't';
        io.buffer[6] = 'y'; io.buffer[7] = 'p';
        for (probe_idx = 8; probe_idx < MINI_FFMPEG_IO_BUFFER_SIZE; probe_idx++) {
            io.buffer[probe_idx] = (uint8_t)(probe_idx % 256);
        }
    }

    printf("Probing format...\n");
    if (mini_ffmpeg_demux_open(demux, &io) < 0) {
        fprintf(stderr, "Format probe failed\n");
        goto cleanup;
    }

    printf("Format: %d\n", mini_ffmpeg_demux_get_format(demux));

    ret = mini_ffmpeg_demux_find_stream_info(demux);
    if (ret < 0) {
        fprintf(stderr, "find_stream_info failed\n");
        goto cleanup;
    }

    nb_streams = mini_ffmpeg_demux_get_nb_streams(demux);
    printf("Streams: %d\n\n", nb_streams);

    for (i = 0; i < nb_streams; i++) {
        MiniFFmpegAVStream *st = mini_ffmpeg_demux_get_stream(demux, i);
        const char *type_str;
        switch (st->codec_type) {
        case MINI_FFMPEG_MEDIA_TYPE_VIDEO:   type_str = "Video"; break;
        case MINI_FFMPEG_MEDIA_TYPE_AUDIO:   type_str = "Audio"; break;
        case MINI_FFMPEG_MEDIA_TYPE_SUBTITLE:type_str = "Subtitle"; break;
        default:                              type_str = "Unknown"; break;
        }
        printf("  Stream #%d: %s\n", st->index, type_str);
        printf("    Codec ID: %d\n", st->codec_id);
        printf("    Time Base: %d/%d\n", st->time_base.num, st->time_base.den);
        printf("    Duration: %lld\n", (long long)st->duration);
        if (st->codec_type == MINI_FFMPEG_MEDIA_TYPE_VIDEO)
            printf("    Resolution: %dx%d\n", st->width, st->height);
        if (st->codec_type == MINI_FFMPEG_MEDIA_TYPE_AUDIO)
            printf("    Sample Rate: %d, Channels: %d\n",
                   st->sample_rate, st->channels);
        printf("    Bitrate: %d\n\n", st->bit_rate);
    }

    printf("Reading packets...\n");
    mini_ffmpeg_packet_init(&pkt);
    while (pkt_count < 60) {
        ret = mini_ffmpeg_demux_read_packet(demux, &pkt);
        if (ret < 0) break;
        if (pkt_count < 5 || pkt_count % 15 == 0) {
            printf("  Pkt #%3d: stream=%d pts=%8lld dts=%8lld "
                   "size=%6d flags=%s\n",
                   pkt_count, pkt.stream_index,
                   (long long)pkt.pts, (long long)pkt.dts,
                   pkt.size,
                   (pkt.flags & MINI_FFMPEG_PKT_FLAG_KEY) ? "KEY" : "   ");
        }
        mini_ffmpeg_packet_unref(&pkt);
        pkt_count++;
    }
    printf("Total packets read: %d\n\n", pkt_count);

    printf("Testing seek...\n");
    ret = mini_ffmpeg_demux_seek(demux, 0, 5000000, 0);
    printf("Seek to 5000000: %s\n", ret == 0 ? "OK" : "FAILED");

    ret = mini_ffmpeg_demux_seek(demux, 1, 2400000, 0);
    printf("Seek to 2400000: %s\n", ret == 0 ? "OK" : "FAILED");

    printf("\nDuration: %lld us\n\n",
           (long long)mini_ffmpeg_demux_get_duration(demux));

    printf("=== Example Complete ===\n");

cleanup:
    mini_ffmpeg_packet_unref(&pkt);
    mini_ffmpeg_demux_free(demux);
    if (io.buffer) free(io.buffer);
    return 0;
}
