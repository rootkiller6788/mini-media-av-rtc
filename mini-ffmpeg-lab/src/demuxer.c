#include "demuxer.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define MINI_FFMPEG_MP4_FTYP_TAG  0x70797466
#define MINI_FFMPEG_MP4_MOOV_TAG  0x766F6F6D
#define MINI_FFMPEG_MP4_MDAT_TAG  0x7461646D
#define MINI_FFMPEG_MKV_EBML_ID   0x1A45DFA3
#define MINI_FFMPEG_MKV_SEGMENT_ID 0x18538067
#define MINI_FFMPEG_MKV_CLUSTER_ID 0x1F43B675

typedef struct MiniFFmpegDemuxPriv {
    int          format;
    int64_t      file_size;
    int64_t      data_offset;
    int64_t      moov_offset;
    int64_t      mdat_offset;
    int64_t      current_cluster_pos;
    int          nb_streams;
    MiniFFmpegAVStream streams[MINI_FFMPEG_MAX_STREAMS];
    int64_t      duration;
    int64_t      bit_rate;
    uint8_t      probe_data[MINI_FFMPEG_MAX_PROBE_SIZE];
    int          probe_size;
    MiniFFmpegIOContext *io;
    int          seekable;
    int64_t      last_seek_pos;
    int64_t      *keyframe_index;
    int          keyframe_count;
    int          keyframe_capacity;
    int          ts_pmt_pid;
    int          ts_pcr_pid;
    int64_t      first_dts;
    void        *container_priv;
} MiniFFmpegDemuxPriv;

typedef struct MiniFFmpegDemuxContext {
    MiniFFmpegDemuxPriv priv;
} MiniFFmpegDemuxContext;

static int mini_ffmpeg_probe_mp4(const uint8_t *buf, int size) {
    if (size < 12) return 0;
    if (buf[4] == 'f' && buf[5] == 't' && buf[6] == 'y' && buf[7] == 'p')
        return 100;
    return 0;
}

static int mini_ffmpeg_probe_mkv(const uint8_t *buf, int size) {
    if (size < 8) return 0;
    if (buf[0] == 0x1A && buf[1] == 0x45 && buf[2] == 0xDF && buf[3] == 0xA3)
        return 90;
    return 0;
}

static int mini_ffmpeg_probe_ts(const uint8_t *buf, int size) {
    int i;
    if (size < MINI_FFMPEG_TS_PACKET_SIZE) return 0;
    for (i = 0; i < size; i += MINI_FFMPEG_TS_PACKET_SIZE) {
        if (buf[i] != 0x47) return 0;
        if (i + MINI_FFMPEG_TS_PACKET_SIZE > size) break;
    }
    return 80;
}

static int mini_ffmpeg_probe_avi(const uint8_t *buf, int size) {
    if (size < 12) return 0;
    if (memcmp(buf, "RIFF", 4) == 0 &&
        memcmp(buf + 8, "AVI ", 4) == 0)
        return 70;
    return 0;
}

static int mini_ffmpeg_read_mp4_header(MiniFFmpegDemuxPriv *priv) {
    MiniFFmpegAVStream *st;
    priv->nb_streams = 2;
    st = &priv->streams[0];
    st->index = 0;
    st->codec_type = MINI_FFMPEG_MEDIA_TYPE_VIDEO;
    st->codec_id = MINI_FFMPEG_CODEC_ID_H264;
    st->time_base.num = 1; st->time_base.den = 90000;
    st->duration = 900000;
    st->width = 1920; st->height = 1080;
    st->bit_rate = 2000000;
    st->priv_data = NULL;
    st = &priv->streams[1];
    st->index = 1;
    st->codec_type = MINI_FFMPEG_MEDIA_TYPE_AUDIO;
    st->codec_id = MINI_FFMPEG_CODEC_ID_AAC;
    st->time_base.num = 1; st->time_base.den = 48000;
    st->duration = 480000;
    st->sample_rate = 48000; st->channels = 2;
    st->bit_rate = 128000;
    st->priv_data = NULL;
    priv->duration = 10000000;
    priv->bit_rate = 2128000;
    return 0;
}

static int mini_ffmpeg_read_mkv_header(MiniFFmpegDemuxPriv *priv) {
    MiniFFmpegAVStream *st;
    priv->nb_streams = 3;
    st = &priv->streams[0];
    st->index = 0;
    st->codec_type = MINI_FFMPEG_MEDIA_TYPE_VIDEO;
    st->codec_id = MINI_FFMPEG_CODEC_ID_VP9;
    st->time_base.num = 1; st->time_base.den = 1000;
    st->duration = 10000;
    st->width = 3840; st->height = 2160;
    st->bit_rate = 8000000;
    st = &priv->streams[1];
    st->index = 1;
    st->codec_type = MINI_FFMPEG_MEDIA_TYPE_AUDIO;
    st->codec_id = MINI_FFMPEG_CODEC_ID_OPUS;
    st->time_base.num = 1; st->time_base.den = 48000;
    st->duration = 480000;
    st->sample_rate = 48000; st->channels = 6;
    st->bit_rate = 384000;
    st = &priv->streams[2];
    st->index = 2;
    st->codec_type = MINI_FFMPEG_MEDIA_TYPE_SUBTITLE;
    st->codec_id = MINI_FFMPEG_CODEC_ID_NONE;
    st->time_base.num = 1; st->time_base.den = 1000;
    st->duration = 10000;
    priv->duration = 10000000;
    priv->bit_rate = 8384000;
    return 0;
}

static int mini_ffmpeg_read_ts_header(MiniFFmpegDemuxPriv *priv) {
    MiniFFmpegAVStream *st;
    priv->nb_streams = 2;
    priv->ts_pmt_pid = 0x100;
    priv->ts_pcr_pid = 0x200;
    st = &priv->streams[0];
    st->index = 0;
    st->codec_type = MINI_FFMPEG_MEDIA_TYPE_VIDEO;
    st->codec_id = MINI_FFMPEG_CODEC_ID_H265;
    st->time_base.num = 1; st->time_base.den = 90000;
    st->duration = 0;
    st->width = 1920; st->height = 1080;
    st->bit_rate = 4000000;
    st = &priv->streams[1];
    st->index = 1;
    st->codec_type = MINI_FFMPEG_MEDIA_TYPE_AUDIO;
    st->codec_id = MINI_FFMPEG_CODEC_ID_MP3;
    st->time_base.num = 1; st->time_base.den = 90000;
    st->duration = 0;
    st->sample_rate = 48000; st->channels = 2;
    st->bit_rate = 192000;
    priv->duration = 0;
    priv->bit_rate = 4192000;
    return 0;
}

static int mini_ffmpeg_probe_format(MiniFFmpegDemuxPriv *priv) {
    int score_mp4, score_mkv, score_ts, score_avi;
    int best_score = 0;
    int best_fmt = MINI_FFMPEG_FORMAT_UNKNOWN;
    score_mp4 = mini_ffmpeg_probe_mp4(priv->probe_data, priv->probe_size);
    score_mkv = mini_ffmpeg_probe_mkv(priv->probe_data, priv->probe_size);
    score_ts  = mini_ffmpeg_probe_ts(priv->probe_data, priv->probe_size);
    score_avi = mini_ffmpeg_probe_avi(priv->probe_data, priv->probe_size);
    if (score_mp4 > best_score) { best_score = score_mp4; best_fmt = MINI_FFMPEG_FORMAT_MP4; }
    if (score_mkv > best_score) { best_score = score_mkv; best_fmt = MINI_FFMPEG_FORMAT_MKV; }
    if (score_ts  > best_score) { best_score = score_ts;  best_fmt = MINI_FFMPEG_FORMAT_TS;  }
    if (score_avi > best_score) { best_score = score_avi; best_fmt = MINI_FFMPEG_FORMAT_AVI; }
    return best_fmt;
}

MiniFFmpegDemuxContext *mini_ffmpeg_demux_alloc(void) {
    MiniFFmpegDemuxContext *ctx = (MiniFFmpegDemuxContext *)
        calloc(1, sizeof(MiniFFmpegDemuxContext));
    if (ctx) {
        ctx->priv.keyframe_capacity = 1024;
        ctx->priv.keyframe_index = (int64_t *)
            malloc(sizeof(int64_t) * ctx->priv.keyframe_capacity);
        ctx->priv.keyframe_count = 0;
        ctx->priv.first_dts = MINI_FFMPEG_NO_PTS_VALUE;
    }
    return ctx;
}

void mini_ffmpeg_demux_free(MiniFFmpegDemuxContext *ctx) {
    if (!ctx) return;
    if (ctx->priv.keyframe_index) free(ctx->priv.keyframe_index);
    if (ctx->priv.container_priv) free(ctx->priv.container_priv);
    free(ctx);
}

int mini_ffmpeg_demux_open(MiniFFmpegDemuxContext *ctx,
                            MiniFFmpegIOContext *io) {
    int fmt;
    if (!ctx || !io) return -1;
    ctx->priv.io = io;
    if (io->read_packet) {
        ctx->priv.probe_size = io->read_packet(io->opaque,
            ctx->priv.probe_data, MINI_FFMPEG_MAX_PROBE_SIZE);
    } else {
        int copy = io->buffer_size < MINI_FFMPEG_MAX_PROBE_SIZE
                   ? io->buffer_size : MINI_FFMPEG_MAX_PROBE_SIZE;
        memcpy(ctx->priv.probe_data, io->buffer, copy);
        ctx->priv.probe_size = copy;
    }
    if (ctx->priv.probe_size < 0) return -1;
    if (ctx->priv.probe_size > MINI_FFMPEG_MAX_PROBE_SIZE)
        ctx->priv.probe_size = MINI_FFMPEG_MAX_PROBE_SIZE;
    fmt = mini_ffmpeg_probe_format(&ctx->priv);
    if (fmt == MINI_FFMPEG_FORMAT_UNKNOWN) return -2;
    ctx->priv.format = fmt;
    switch (fmt) {
    case MINI_FFMPEG_FORMAT_MP4:
        return mini_ffmpeg_read_mp4_header(&ctx->priv);
    case MINI_FFMPEG_FORMAT_MKV:
        return mini_ffmpeg_read_mkv_header(&ctx->priv);
    case MINI_FFMPEG_FORMAT_TS:
        return mini_ffmpeg_read_ts_header(&ctx->priv);
    default:
        return -3;
    }
}

int mini_ffmpeg_demux_find_stream_info(MiniFFmpegDemuxContext *ctx) {
    int i;
    (void)ctx;
    for (i = 0; i < ctx->priv.nb_streams; i++) {
        MiniFFmpegAVStream *st = &ctx->priv.streams[i];
        if (st->codec_type == MINI_FFMPEG_MEDIA_TYPE_VIDEO) {
            if (st->width == 0) st->width = 1920;
            if (st->height == 0) st->height = 1080;
        }
        if (st->codec_type == MINI_FFMPEG_MEDIA_TYPE_AUDIO) {
            if (st->sample_rate == 0) st->sample_rate = 44100;
            if (st->channels == 0) st->channels = 2;
        }
    }
    return 0;
}

int mini_ffmpeg_demux_read_packet(MiniFFmpegDemuxContext *ctx,
                                   MiniFFmpegAVPacket *pkt) {
    static int64_t video_pts_counter = 0;
    static int64_t audio_pts_counter = 0;
    static int call_count = 0;
    int stream_idx;
    MiniFFmpegAVStream *st;
    if (!ctx || !pkt) return -1;
    if (call_count >= 100) return -2;
    stream_idx = call_count % ctx->priv.nb_streams;
    st = &ctx->priv.streams[stream_idx];
    mini_ffmpeg_packet_alloc(pkt, 4096);
    if (st->codec_type == MINI_FFMPEG_MEDIA_TYPE_VIDEO) {
        pkt->stream_index = stream_idx;
        pkt->pts = video_pts_counter;
        pkt->dts = video_pts_counter;
        pkt->duration = st->time_base.den / 25;
        pkt->flags = (video_pts_counter % (30 * pkt->duration) == 0)
                     ? MINI_FFMPEG_PKT_FLAG_KEY : 0;
        video_pts_counter += pkt->duration;
        pkt->size = 8192 + (rand() % 16384);
    } else if (st->codec_type == MINI_FFMPEG_MEDIA_TYPE_AUDIO) {
        pkt->stream_index = stream_idx;
        pkt->pts = audio_pts_counter;
        pkt->dts = audio_pts_counter;
        pkt->duration = st->time_base.den / 50;
        pkt->flags = MINI_FFMPEG_PKT_FLAG_KEY;
        audio_pts_counter += pkt->duration;
        pkt->size = 512 + (rand() % 1024);
    } else {
        pkt->stream_index = stream_idx;
        pkt->pts = call_count * 1000;
        pkt->dts = pkt->pts;
        pkt->duration = 1000;
        pkt->flags = MINI_FFMPEG_PKT_FLAG_KEY;
        pkt->size = 128;
    }
    if (ctx->priv.first_dts == MINI_FFMPEG_NO_PTS_VALUE)
        ctx->priv.first_dts = pkt->dts;
    call_count++;
    return 0;
}

int mini_ffmpeg_demux_seek(MiniFFmpegDemuxContext *ctx,
                            int stream_index, int64_t timestamp, int flags) {
    int i;
    (void)stream_index;
    (void)flags;
    if (!ctx) return -1;
    if (ctx->priv.keyframe_count > 0) {
        for (i = ctx->priv.keyframe_count - 1; i >= 0; i--) {
            if (ctx->priv.keyframe_index[i] <= timestamp) {
                ctx->priv.last_seek_pos = ctx->priv.keyframe_index[i];
                return 0;
            }
        }
    }
    ctx->priv.last_seek_pos = timestamp;
    return 0;
}

int mini_ffmpeg_demux_get_nb_streams(MiniFFmpegDemuxContext *ctx) {
    return ctx ? ctx->priv.nb_streams : 0;
}

MiniFFmpegAVStream *mini_ffmpeg_demux_get_stream(MiniFFmpegDemuxContext *ctx,
                                                   int index) {
    if (!ctx || index < 0 || index >= ctx->priv.nb_streams) return NULL;
    return &ctx->priv.streams[index];
}

int mini_ffmpeg_demux_get_format(MiniFFmpegDemuxContext *ctx) {
    return ctx ? ctx->priv.format : MINI_FFMPEG_FORMAT_UNKNOWN;
}

int64_t mini_ffmpeg_demux_get_duration(MiniFFmpegDemuxContext *ctx) {
    return ctx ? ctx->priv.duration : 0;
}

void mini_ffmpeg_packet_init(MiniFFmpegAVPacket *pkt) {
    if (!pkt) return;
    memset(pkt, 0, sizeof(*pkt));
    pkt->pts = MINI_FFMPEG_NO_PTS_VALUE;
    pkt->dts = MINI_FFMPEG_NO_PTS_VALUE;
}

void mini_ffmpeg_packet_unref(MiniFFmpegAVPacket *pkt) {
    if (!pkt) return;
    if (pkt->data) { free(pkt->data); pkt->data = NULL; }
    pkt->size = 0;
}

int mini_ffmpeg_packet_alloc(MiniFFmpegAVPacket *pkt, int size) {
    if (!pkt) return -1;
    mini_ffmpeg_packet_unref(pkt);
    if (size <= 0) return -1;
    pkt->data = (uint8_t *)malloc(size);
    if (!pkt->data) return -1;
    pkt->size = size;
    memset(pkt->data, 0, size);
    return 0;
}

static int mini_ffmpeg_file_read(void *opaque, uint8_t *buf, int buf_size) {
    FILE *fp = (FILE *)opaque;
    size_t n;
    if (!fp || feof(fp)) return -1;
    n = fread(buf, 1, buf_size, fp);
    return (int)n;
}

static int64_t mini_ffmpeg_file_seek(void *opaque, int64_t offset, int whence) {
    FILE *fp = (FILE *)opaque;
    if (!fp) return -1;
    fseek(fp, (long)offset, whence);
    return ftell(fp);
}

int mini_ffmpeg_io_open(MiniFFmpegIOContext *io, const char *filename) {
    FILE *fp;
    if (!io || !filename) return -1;
    fp = fopen(filename, "rb");
    if (!fp) {
        memset(io, 0, sizeof(*io));
        return -1;
    }
    memset(io, 0, sizeof(*io));
    io->buffer = (uint8_t *)malloc(MINI_FFMPEG_IO_BUFFER_SIZE);
    io->buffer_size = MINI_FFMPEG_IO_BUFFER_SIZE;
    io->buf_ptr = io->buffer;
    io->buf_end = io->buffer;
    io->opaque = fp;
    io->read_packet = mini_ffmpeg_file_read;
    io->seek = mini_ffmpeg_file_seek;
    io->pos = 0;
    io->eof_reached = 0;
    io->error = 0;
    return 0;
}

void mini_ffmpeg_io_close(MiniFFmpegIOContext *io) {
    if (!io) return;
    if (io->opaque) {
        fclose((FILE *)io->opaque);
        io->opaque = NULL;
    }
    if (io->buffer) free(io->buffer);
    memset(io, 0, sizeof(*io));
}
