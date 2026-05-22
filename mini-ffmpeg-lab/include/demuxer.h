#ifndef MINI_FFMPEG_DEMUXER_H
#define MINI_FFMPEG_DEMUXER_H

#include <stdint.h>
#include <stddef.h>
#include <limits.h>

#ifndef MINI_FFMPEG_NO_PTS_VALUE
#define MINI_FFMPEG_NO_PTS_VALUE        INT64_MIN
#define MINI_FFMPEG_NOPTS_VALUE         INT64_MIN
#endif

#define MINI_FFMPEG_MAX_STREAMS      32
#define MINI_FFMPEG_MAX_PROBE_SIZE   4096
#define MINI_FFMPEG_TS_PACKET_SIZE   188
#define MINI_FFMPEG_IO_BUFFER_SIZE   65536

enum {
    MINI_FFMPEG_MEDIA_TYPE_UNKNOWN = -1,
    MINI_FFMPEG_MEDIA_TYPE_VIDEO,
    MINI_FFMPEG_MEDIA_TYPE_AUDIO,
    MINI_FFMPEG_MEDIA_TYPE_SUBTITLE,
};

enum {
    MINI_FFMPEG_CODEC_ID_NONE = 0,
    MINI_FFMPEG_CODEC_ID_H264,
    MINI_FFMPEG_CODEC_ID_H265,
    MINI_FFMPEG_CODEC_ID_AAC,
    MINI_FFMPEG_CODEC_ID_MP3,
    MINI_FFMPEG_CODEC_ID_OPUS,
    MINI_FFMPEG_CODEC_ID_VP8,
    MINI_FFMPEG_CODEC_ID_VP9,
    MINI_FFMPEG_CODEC_ID_AV1,
    MINI_FFMPEG_CODEC_ID_PCM_S16LE,
};

enum AVPacketFlag {
    MINI_FFMPEG_PKT_FLAG_KEY   = 0x0001,
    MINI_FFMPEG_PKT_FLAG_CORRUPT = 0x0002,
};

enum {
    MINI_FFMPEG_FORMAT_UNKNOWN = 0,
    MINI_FFMPEG_FORMAT_MP4,
    MINI_FFMPEG_FORMAT_MKV,
    MINI_FFMPEG_FORMAT_TS,
    MINI_FFMPEG_FORMAT_FLV,
    MINI_FFMPEG_FORMAT_AVI,
};

typedef struct MiniFFmpegRational {
    int num;
    int den;
} MiniFFmpegRational;

typedef struct MiniFFmpegAVStream {
    int index;
    int codec_type;
    int codec_id;
    MiniFFmpegRational time_base;
    int64_t duration;
    int64_t start_time;
    int64_t nb_frames;
    int width;
    int height;
    int sample_rate;
    int channels;
    int bit_rate;
    void *priv_data;
} MiniFFmpegAVStream;

typedef struct MiniFFmpegAVPacket {
    uint8_t *data;
    int      size;
    int64_t  pts;
    int64_t  dts;
    int      stream_index;
    int      flags;
    int64_t  duration;
    int64_t  pos;
} MiniFFmpegAVPacket;

typedef struct MiniFFmpegIOContext {
    uint8_t *buffer;
    int      buffer_size;
    uint8_t *buf_ptr;
    uint8_t *buf_end;
    void    *opaque;
    int    (*read_packet)(void *opaque, uint8_t *buf, int buf_size);
    int64_t(*seek)(void *opaque, int64_t offset, int whence);
    int64_t pos;
    int     eof_reached;
    int     error;
} MiniFFmpegIOContext;

typedef struct MiniFFmpegDemuxContext MiniFFmpegDemuxContext;

MiniFFmpegDemuxContext *mini_ffmpeg_demux_alloc(void);
void mini_ffmpeg_demux_free(MiniFFmpegDemuxContext *ctx);

int mini_ffmpeg_demux_open(MiniFFmpegDemuxContext *ctx,
                           MiniFFmpegIOContext *io);

int mini_ffmpeg_demux_read_packet(MiniFFmpegDemuxContext *ctx,
                                  MiniFFmpegAVPacket *pkt);

int mini_ffmpeg_demux_seek(MiniFFmpegDemuxContext *ctx,
                           int stream_index, int64_t timestamp, int flags);

int mini_ffmpeg_demux_find_stream_info(MiniFFmpegDemuxContext *ctx);

int mini_ffmpeg_demux_get_nb_streams(MiniFFmpegDemuxContext *ctx);

MiniFFmpegAVStream *mini_ffmpeg_demux_get_stream(MiniFFmpegDemuxContext *ctx,
                                                  int index);

int mini_ffmpeg_demux_get_format(MiniFFmpegDemuxContext *ctx);

int64_t mini_ffmpeg_demux_get_duration(MiniFFmpegDemuxContext *ctx);

void mini_ffmpeg_packet_init(MiniFFmpegAVPacket *pkt);

void mini_ffmpeg_packet_unref(MiniFFmpegAVPacket *pkt);

int mini_ffmpeg_packet_alloc(MiniFFmpegAVPacket *pkt, int size);

int mini_ffmpeg_io_open(MiniFFmpegIOContext *io, const char *filename);

void mini_ffmpeg_io_close(MiniFFmpegIOContext *io);

#endif /* MINI_FFMPEG_DEMUXER_H */
