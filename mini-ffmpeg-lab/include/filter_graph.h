#ifndef MINI_FFMPEG_FILTER_GRAPH_H
#define MINI_FFMPEG_FILTER_GRAPH_H

#include <stdint.h>
#include <stddef.h>
#include "decoder_engine.h"

#define MINI_FFMPEG_MAX_FILTERS       64
#define MINI_FFMPEG_MAX_FILTER_PADS   16
#define MINI_FFMPEG_MAX_AUDIO_CHANNELS 8
#define MINI_FFMPEG_FILTER_NAME_MAX   64

enum AVFilterPadType {
    MINI_FFMPEG_FILTER_PAD_INPUT,
    MINI_FFMPEG_FILTER_PAD_OUTPUT,
};

enum AVFilterType {
    MINI_FFMPEG_FILTER_TYPE_BUFFER_SRC,
    MINI_FFMPEG_FILTER_TYPE_BUFFER_SINK,
    MINI_FFMPEG_FILTER_TYPE_VIDEO,
    MINI_FFMPEG_FILTER_TYPE_AUDIO,
};

typedef struct MiniFFmpegAVFilterLink {
    struct MiniFFmpegAVFilterContext *src;
    int   srcpad;
    struct MiniFFmpegAVFilterContext *dst;
    int   dstpad;
    int   format;
    int   w;
    int   h;
    int   sample_rate;
    int   channels;
    int   channel_layout;
    MiniFFmpegRational time_base;
    MiniFFmpegRational frame_rate;
    MiniFFmpegRational sample_aspect_ratio;
    int   eof;
} MiniFFmpegAVFilterLink;

typedef struct MiniFFmpegAVFilterPad {
    const char *name;
    int         type;
    int         (*filter_frame)(MiniFFmpegAVFilterLink *link,
                                MiniFFmpegAVFrame *frame);
    int         (*request_frame)(MiniFFmpegAVFilterLink *link);
    int         (*config_props)(MiniFFmpegAVFilterLink *link);
} MiniFFmpegAVFilterPad;

typedef struct MiniFFmpegAVFilter {
    const char *name;
    const char *description;
    int         type;
    const MiniFFmpegAVFilterPad *inputs;
    int         nb_inputs;
    const MiniFFmpegAVFilterPad *outputs;
    int         nb_outputs;
    int         priv_size;
    int         (*init)(struct MiniFFmpegAVFilterContext *ctx);
    void        (*uninit)(struct MiniFFmpegAVFilterContext *ctx);
    int         (*process_command)(struct MiniFFmpegAVFilterContext *ctx,
                                   const char *cmd, const char *arg,
                                   char *res, int res_len, int flags);
} MiniFFmpegAVFilter;

typedef struct MiniFFmpegAVFilterContext {
    const MiniFFmpegAVFilter  *filter;
    char   *name;
    int     nb_inputs;
    MiniFFmpegAVFilterPad    *input_pads;
    MiniFFmpegAVFilterLink  **inputs;
    int     nb_outputs;
    MiniFFmpegAVFilterPad    *output_pads;
    MiniFFmpegAVFilterLink  **outputs;
    void   *priv;
    struct MiniFFmpegAVFilterGraph *graph;
    int     ready;
    int     extra_hw_frames;
    int     format;
    char   *args;
    int     enabled;
} MiniFFmpegAVFilterContext;

typedef struct MiniFFmpegAVFilterGraph {
    MiniFFmpegAVFilterContext **filters;
    int      nb_filters;
    char    *scale_sws_opts;
    int      index;
    int      disable_auto_convert;
    int      nb_threads;
    void    *opaque;
    int    (*execute)(struct MiniFFmpegAVFilterGraph *g,
                      int (*func)(MiniFFmpegAVFilterContext *f, void *arg,
                                  int *ret, int nb_jobs),
                      void *arg, int *ret, int nb_jobs);
    void   *internal;
    char   *aresample_swr_opts;
} MiniFFmpegAVFilterGraph;

typedef struct MiniFFmpegFilterGraph Module0;

MiniFFmpegAVFilterGraph *mini_ffmpeg_filter_graph_alloc(void);

void mini_ffmpeg_filter_graph_free(MiniFFmpegAVFilterGraph *graph);

int mini_ffmpeg_filter_graph_parse(MiniFFmpegAVFilterGraph *graph,
                                    const char *filters_desc,
                                    MiniFFmpegAVFilterContext **inputs,
                                    MiniFFmpegAVFilterContext **outputs);

int mini_ffmpeg_filter_graph_create_src(MiniFFmpegAVFilterGraph *graph,
                                         const char *name,
                                         MiniFFmpegAVFilterContext **src_ctx);

int mini_ffmpeg_filter_graph_create_sink(MiniFFmpegAVFilterGraph *graph,
                                          const char *name,
                                          MiniFFmpegAVFilterContext **sink_ctx);

int mini_ffmpeg_filter_graph_config(MiniFFmpegAVFilterGraph *graph);

int mini_ffmpeg_filter_graph_send_frame(MiniFFmpegAVFilterGraph *graph,
                                         MiniFFmpegAVFilterContext *sink,
                                         MiniFFmpegAVFrame *frame);

int mini_ffmpeg_filter_graph_receive_frame(MiniFFmpegAVFilterGraph *graph,
                                            MiniFFmpegAVFilterContext *sink,
                                            MiniFFmpegAVFrame *frame);

void mini_ffmpeg_filter_register_all(void);

MiniFFmpegAVFilter *mini_ffmpeg_filter_get_by_name(const char *name);

int mini_ffmpeg_filter_scale_init(MiniFFmpegAVFilterContext *ctx,
                                   int src_w, int src_h, int src_fmt,
                                   int dst_w, int dst_h, int dst_fmt);

int mini_ffmpeg_filter_crop_init(MiniFFmpegAVFilterContext *ctx,
                                  int x, int y, int w, int h);

int mini_ffmpeg_filter_transpose_init(MiniFFmpegAVFilterContext *ctx,
                                       int dir);

int mini_ffmpeg_filter_overlay_init(MiniFFmpegAVFilterContext *ctx,
                                     int x, int y);

int mini_ffmpeg_filter_aresample_init(MiniFFmpegAVFilterContext *ctx,
                                       int out_rate, int out_ch,
                                       int out_fmt);

int mini_ffmpeg_filter_volume_init(MiniFFmpegAVFilterContext *ctx,
                                    float volume);

int mini_ffmpeg_filter_amix_init(MiniFFmpegAVFilterContext *ctx,
                                  int inputs);

#endif /* MINI_FFMPEG_FILTER_GRAPH_H */
