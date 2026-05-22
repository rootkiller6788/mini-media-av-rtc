#include "filter_graph.h"
#include "decoder_engine.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

typedef struct MiniFFmpegBufferSrcPriv {
    MiniFFmpegAVFrame *frame;
    int     eof;
    int     w, h, format;
    int     sample_rate, channels, sample_fmt;
    MiniFFmpegRational time_base;
    MiniFFmpegRational frame_rate;
} MiniFFmpegBufferSrcPriv;

typedef struct MiniFFmpegBufferSinkPriv {
    MiniFFmpegAVFrame *frame;
    int     eof;
    int     frame_count;
} MiniFFmpegBufferSinkPriv;

typedef struct MiniFFmpegScalePriv {
    int     src_w, src_h, src_fmt;
    int     dst_w, dst_h, dst_fmt;
    int     flags;
    int     initialized;
} MiniFFmpegScalePriv;

typedef struct MiniFFmpegCropPriv {
    int     x, y, w, h;
    int     initialized;
} MiniFFmpegCropPriv;

typedef struct MiniFFmpegTransposePriv {
    int     dir;
    int     initialized;
} MiniFFmpegTransposePriv;

typedef struct MiniFFmpegFPSPriv {
    MiniFFmpegRational frame_rate;
    MiniFFmpegAVFrame *last_frame;
    int     initialized;
    int     frame_count;
    int64_t next_pts;
} MiniFFmpegFPSPriv;

typedef struct MiniFFmpegOverlayPriv {
    int     x, y;
    int     main_w, main_h;
    int     over_w, over_h;
    int     initialized;
} MiniFFmpegOverlayPriv;

typedef struct MiniFFmpegVolumePriv {
    float   volume;
    int     initialized;
} MiniFFmpegVolumePriv;

typedef struct MiniFFmpegAResamplePriv {
    int     out_rate;
    int     out_channels;
    int     out_sample_fmt;
    int     in_rate;
    int     in_channels;
    int     in_sample_fmt;
    int     initialized;
} MiniFFmpegAResamplePriv;

typedef struct MiniFFmpegEqualizerPriv {
    float   bands[18];
    int     initialized;
} MiniFFmpegEqualizerPriv;

typedef struct MiniFFmpegAMixPriv {
    int     nb_inputs;
    int     inputs_ready;
    int     initialized;
} MiniFFmpegAMixPriv;

typedef struct MiniFFmpegFilterEntry {
    MiniFFmpegAVFilter filter;
    struct MiniFFmpegFilterEntry *next;
} MiniFFmpegFilterEntry;

static MiniFFmpegFilterEntry *g_filter_list = NULL;

static int mini_ffmpeg_buffer_src_filter_frame(MiniFFmpegAVFilterLink *link,
                                                MiniFFmpegAVFrame *frame) {
    (void)link; (void)frame;
    return -1;
}

static int mini_ffmpeg_buffer_sink_filter_frame(MiniFFmpegAVFilterLink *link,
                                                  MiniFFmpegAVFrame *frame) {
    MiniFFmpegBufferSinkPriv *priv = (MiniFFmpegBufferSinkPriv *)
        link->dst->priv;
    if (priv->frame) mini_ffmpeg_frame_unref(priv->frame);
    if (!priv->frame) priv->frame = mini_ffmpeg_frame_alloc();
    if (!priv->frame) return -1;
    priv->frame->width = frame->width;
    priv->frame->height = frame->height;
    priv->frame->format = frame->format;
    priv->frame->pts = frame->pts;
    priv->frame->sample_rate = frame->sample_rate;
    priv->frame->channels = frame->channels;
    priv->frame->nb_samples = frame->nb_samples;
    priv->frame_count++;
    return 0;
}

static int mini_ffmpeg_scale_filter_frame(MiniFFmpegAVFilterLink *link,
                                           MiniFFmpegAVFrame *frame) {
    MiniFFmpegScalePriv *priv = (MiniFFmpegScalePriv *)link->dst->priv;
    MiniFFmpegAVFrame *out;
    int out_size, i;
    (void)link;
    if (!priv->initialized) {
        priv->src_w = frame->width;
        priv->src_h = frame->height;
        if (priv->dst_w <= 0) priv->dst_w = priv->src_w;
        if (priv->dst_h <= 0) priv->dst_h = priv->src_h;
        priv->initialized = 1;
    }
    out = mini_ffmpeg_frame_alloc();
    out->width = priv->dst_w;
    out->height = priv->dst_h;
    out->format = priv->dst_fmt >= 0 ? priv->dst_fmt : frame->format;
    out->pts = frame->pts;
    out_size = priv->dst_w * priv->dst_h;
    out->data[0] = (uint8_t *)malloc(out_size);
    out->linesize[0] = priv->dst_w;
    for (i = 0; i < out_size; i++)
        out->data[0][i] = (uint8_t)((i * 7 + 128) % 256);
    mini_ffmpeg_frame_unref(frame);
    memcpy(frame, out, sizeof(*frame));
    free(out);
    return 0;
}

static int mini_ffmpeg_crop_filter_frame(MiniFFmpegAVFilterLink *link,
                                          MiniFFmpegAVFrame *frame) {
    MiniFFmpegCropPriv *priv = (MiniFFmpegCropPriv *)link->dst->priv;
    int crop_w, crop_h;
    if (!frame) return -1;
    crop_w = priv->w > 0 ? priv->w : frame->width - priv->x;
    crop_h = priv->h > 0 ? priv->h : frame->height - priv->y;
    if (crop_w > frame->width - priv->x) crop_w = frame->width - priv->x;
    if (crop_h > frame->height - priv->y) crop_h = frame->height - priv->y;
    frame->width = crop_w;
    frame->height = crop_h;
    return 0;
}

static int mini_ffmpeg_transpose_filter_frame(MiniFFmpegAVFilterLink *link,
                                               MiniFFmpegAVFrame *frame) {
    MiniFFmpegTransposePriv *priv = (MiniFFmpegTransposePriv *)link->dst->priv;
    (void)priv;
    if (frame->width > 0 && frame->height > 0) {
        int tmp = frame->width;
        frame->width = frame->height;
        frame->height = tmp;
    }
    return 0;
}

static int mini_ffmpeg_fps_filter_frame(MiniFFmpegAVFilterLink *link,
                                         MiniFFmpegAVFrame *frame) {
    MiniFFmpegFPSPriv *priv = (MiniFFmpegFPSPriv *)link->dst->priv;
    (void)frame;
    if (priv->last_frame) mini_ffmpeg_frame_unref(priv->last_frame);
    if (!priv->last_frame) priv->last_frame = mini_ffmpeg_frame_alloc();
    memcpy(priv->last_frame, frame, sizeof(*frame));
    priv->frame_count++;
    return 0;
}

static int mini_ffmpeg_overlay_filter_frame(MiniFFmpegAVFilterLink *link,
                                             MiniFFmpegAVFrame *frame) {
    MiniFFmpegOverlayPriv *priv = (MiniFFmpegOverlayPriv *)link->dst->priv;
    (void)link; (void)frame;
    priv->main_w = frame->width;
    priv->main_h = frame->height;
    return 0;
}

static int mini_ffmpeg_volume_filter_frame(MiniFFmpegAVFilterLink *link,
                                            MiniFFmpegAVFrame *frame) {
    MiniFFmpegVolumePriv *priv = (MiniFFmpegVolumePriv *)link->dst->priv;
    int i, total_samples;
    int16_t *samples;
    if (!frame || !frame->data[0]) return -1;
    total_samples = frame->nb_samples * frame->channels;
    samples = (int16_t *)frame->data[0];
    for (i = 0; i < total_samples; i++) {
        int val = (int)((float)samples[i] * priv->volume);
        if (val > 32767) val = 32767;
        if (val < -32768) val = -32768;
        samples[i] = (int16_t)val;
    }
    return 0;
}

static int mini_ffmpeg_aresample_filter_frame(MiniFFmpegAVFilterLink *link,
                                                MiniFFmpegAVFrame *frame) {
    MiniFFmpegAResamplePriv *priv = (MiniFFmpegAResamplePriv *)link->dst->priv;
    int new_samples;
    (void)frame;
    if (!priv->initialized) {
        priv->in_rate = frame->sample_rate;
        priv->in_channels = frame->channels;
        priv->initialized = 1;
    }
    if (priv->out_rate > 0)
        new_samples = (int)((int64_t)frame->nb_samples * priv->out_rate / priv->in_rate);
    else
        new_samples = frame->nb_samples;
    frame->nb_samples = new_samples;
    frame->sample_rate = priv->out_rate > 0 ? priv->out_rate : frame->sample_rate;
    frame->channels = priv->out_channels > 0 ? priv->out_channels : frame->channels;
    return 0;
}

static int mini_ffmpeg_equalizer_filter_frame(MiniFFmpegAVFilterLink *link,
                                               MiniFFmpegAVFrame *frame) {
    MiniFFmpegEqualizerPriv *priv = (MiniFFmpegEqualizerPriv *)link->dst->priv;
    (void)link; (void)frame; (void)priv;
    return 0;
}

static int mini_ffmpeg_amix_filter_frame(MiniFFmpegAVFilterLink *link,
                                          MiniFFmpegAVFrame *frame) {
    MiniFFmpegAMixPriv *priv = (MiniFFmpegAMixPriv *)link->dst->priv;
    (void)link; (void)frame;
    priv->inputs_ready++;
    return 0;
}

static MiniFFmpegAVFilterPad g_buffer_src_inputs[] = {
    { "default", MINI_FFMPEG_FILTER_PAD_INPUT, NULL, NULL, NULL },
};

static MiniFFmpegAVFilterPad g_buffer_src_outputs[] = {
    { "default", MINI_FFMPEG_FILTER_PAD_OUTPUT,
      mini_ffmpeg_buffer_src_filter_frame, NULL, NULL },
};

static MiniFFmpegAVFilterPad g_buffer_sink_inputs[] = {
    { "default", MINI_FFMPEG_FILTER_PAD_INPUT,
      mini_ffmpeg_buffer_sink_filter_frame, NULL, NULL },
};

static MiniFFmpegAVFilterPad g_buffer_sink_outputs[] = {
    { "default", MINI_FFMPEG_FILTER_PAD_OUTPUT, NULL, NULL, NULL },
};

static MiniFFmpegAVFilterPad g_scale_inputs[] = {
    { "default", MINI_FFMPEG_FILTER_PAD_INPUT,
      mini_ffmpeg_scale_filter_frame, NULL, NULL },
};

static MiniFFmpegAVFilterPad g_scale_outputs[] = {
    { "default", MINI_FFMPEG_FILTER_PAD_OUTPUT, NULL, NULL, NULL },
};

static MiniFFmpegAVFilterPad g_crop_inputs[] = {
    { "default", MINI_FFMPEG_FILTER_PAD_INPUT,
      mini_ffmpeg_crop_filter_frame, NULL, NULL },
};

static MiniFFmpegAVFilterPad g_crop_outputs[] = {
    { "default", MINI_FFMPEG_FILTER_PAD_OUTPUT, NULL, NULL, NULL },
};

static MiniFFmpegAVFilterPad g_transpose_inputs[] = {
    { "default", MINI_FFMPEG_FILTER_PAD_INPUT,
      mini_ffmpeg_transpose_filter_frame, NULL, NULL },
};

static MiniFFmpegAVFilterPad g_transpose_outputs[] = {
    { "default", MINI_FFMPEG_FILTER_PAD_OUTPUT, NULL, NULL, NULL },
};

static MiniFFmpegAVFilterPad g_fps_inputs[] = {
    { "default", MINI_FFMPEG_FILTER_PAD_INPUT,
      mini_ffmpeg_fps_filter_frame, NULL, NULL },
};

static MiniFFmpegAVFilterPad g_fps_outputs[] = {
    { "default", MINI_FFMPEG_FILTER_PAD_OUTPUT, NULL, NULL, NULL },
};

static MiniFFmpegAVFilterPad g_overlay_inputs[] = {
    { "main",   MINI_FFMPEG_FILTER_PAD_INPUT, NULL, NULL, NULL },
    { "overlay",MINI_FFMPEG_FILTER_PAD_INPUT,
      mini_ffmpeg_overlay_filter_frame, NULL, NULL },
};

static MiniFFmpegAVFilterPad g_overlay_outputs[] = {
    { "default", MINI_FFMPEG_FILTER_PAD_OUTPUT, NULL, NULL, NULL },
};

static MiniFFmpegAVFilterPad g_volume_inputs[] = {
    { "default", MINI_FFMPEG_FILTER_PAD_INPUT,
      mini_ffmpeg_volume_filter_frame, NULL, NULL },
};

static MiniFFmpegAVFilterPad g_volume_outputs[] = {
    { "default", MINI_FFMPEG_FILTER_PAD_OUTPUT, NULL, NULL, NULL },
};

static MiniFFmpegAVFilterPad g_aresample_inputs[] = {
    { "default", MINI_FFMPEG_FILTER_PAD_INPUT,
      mini_ffmpeg_aresample_filter_frame, NULL, NULL },
};

static MiniFFmpegAVFilterPad g_aresample_outputs[] = {
    { "default", MINI_FFMPEG_FILTER_PAD_OUTPUT, NULL, NULL, NULL },
};

static MiniFFmpegAVFilterPad g_equalizer_inputs[] = {
    { "default", MINI_FFMPEG_FILTER_PAD_INPUT,
      mini_ffmpeg_equalizer_filter_frame, NULL, NULL },
};

static MiniFFmpegAVFilterPad g_equalizer_outputs[] = {
    { "default", MINI_FFMPEG_FILTER_PAD_OUTPUT, NULL, NULL, NULL },
};

static MiniFFmpegAVFilterPad g_amix_inputs[] = {
    { "default", MINI_FFMPEG_FILTER_PAD_INPUT,
      mini_ffmpeg_amix_filter_frame, NULL, NULL },
};

static MiniFFmpegAVFilterPad g_amix_outputs[] = {
    { "default", MINI_FFMPEG_FILTER_PAD_OUTPUT, NULL, NULL, NULL },
};

static void mini_ffmpeg_register_builtin_filters(void) {
    struct {
        const char *name, *desc;
        int type, priv_size;
        const MiniFFmpegAVFilterPad *inputs;
        int nb_inputs;
        const MiniFFmpegAVFilterPad *outputs;
        int nb_outputs;
    } builtins[] = {
        {"buffer",   "Buffer source",MINI_FFMPEG_FILTER_TYPE_BUFFER_SRC,sizeof(MiniFFmpegBufferSrcPriv), g_buffer_src_inputs,1,g_buffer_src_outputs,1},
        {"buffersink","Buffer sink", MINI_FFMPEG_FILTER_TYPE_BUFFER_SINK,sizeof(MiniFFmpegBufferSinkPriv),g_buffer_sink_inputs,1,g_buffer_sink_outputs,1},
        {"scale",    "Scale",       MINI_FFMPEG_FILTER_TYPE_VIDEO,sizeof(MiniFFmpegScalePriv),     g_scale_inputs,1,g_scale_outputs,1},
        {"crop",     "Crop",        MINI_FFMPEG_FILTER_TYPE_VIDEO,sizeof(MiniFFmpegCropPriv),      g_crop_inputs,1,g_crop_outputs,1},
        {"transpose","Transpose",   MINI_FFMPEG_FILTER_TYPE_VIDEO,sizeof(MiniFFmpegTransposePriv), g_transpose_inputs,1,g_transpose_outputs,1},
        {"fps",      "FPS",         MINI_FFMPEG_FILTER_TYPE_VIDEO,sizeof(MiniFFmpegFPSPriv),       g_fps_inputs,1,g_fps_outputs,1},
        {"overlay",  "Overlay",     MINI_FFMPEG_FILTER_TYPE_VIDEO,sizeof(MiniFFmpegOverlayPriv),   g_overlay_inputs,2,g_overlay_outputs,1},
        {"volume",   "Volume",      MINI_FFMPEG_FILTER_TYPE_AUDIO,sizeof(MiniFFmpegVolumePriv),    g_volume_inputs,1,g_volume_outputs,1},
        {"aresample","Audio resample",MINI_FFMPEG_FILTER_TYPE_AUDIO,sizeof(MiniFFmpegAResamplePriv),g_aresample_inputs,1,g_aresample_outputs,1},
        {"equalizer","Equalizer",   MINI_FFMPEG_FILTER_TYPE_AUDIO,sizeof(MiniFFmpegEqualizerPriv), g_equalizer_inputs,1,g_equalizer_outputs,1},
        {"amix",     "Audio mix",   MINI_FFMPEG_FILTER_TYPE_AUDIO,sizeof(MiniFFmpegAMixPriv),      g_amix_inputs,1,g_amix_outputs,1},
        {"concat",   "Concat",      MINI_FFMPEG_FILTER_TYPE_VIDEO,64,                              g_scale_inputs,1,g_scale_outputs,1},
        {NULL, NULL, 0, 0, NULL, 0, NULL, 0}
    };
    int i;
    for (i = 0; builtins[i].name; i++) {
        MiniFFmpegFilterEntry *entry = (MiniFFmpegFilterEntry *)
            calloc(1, sizeof(*entry));
        entry->filter.name = builtins[i].name;
        entry->filter.description = builtins[i].desc;
        entry->filter.type = builtins[i].type;
        entry->filter.priv_size = builtins[i].priv_size;
        entry->filter.inputs = builtins[i].inputs;
        entry->filter.nb_inputs = builtins[i].nb_inputs;
        entry->filter.outputs = builtins[i].outputs;
        entry->filter.nb_outputs = builtins[i].nb_outputs;
        entry->next = g_filter_list;
        g_filter_list = entry;
    }
}

void mini_ffmpeg_filter_register_all(void) {
    static int registered = 0;
    if (!registered) {
        mini_ffmpeg_register_builtin_filters();
        registered = 1;
    }
}

MiniFFmpegAVFilter *mini_ffmpeg_filter_get_by_name(const char *name) {
    MiniFFmpegFilterEntry *entry;
    mini_ffmpeg_filter_register_all();
    for (entry = g_filter_list; entry; entry = entry->next) {
        if (strcmp(entry->filter.name, name) == 0)
            return &entry->filter;
    }
    return NULL;
}

MiniFFmpegAVFilterGraph *mini_ffmpeg_filter_graph_alloc(void) {
    MiniFFmpegAVFilterGraph *graph = (MiniFFmpegAVFilterGraph *)
        calloc(1, sizeof(MiniFFmpegAVFilterGraph));
    if (graph) {
        graph->filters = (MiniFFmpegAVFilterContext **)
            calloc(MINI_FFMPEG_MAX_FILTERS, sizeof(*graph->filters));
    }
    return graph;
}

void mini_ffmpeg_filter_graph_free(MiniFFmpegAVFilterGraph *graph) {
    int i;
    if (!graph) return;
    for (i = 0; i < graph->nb_filters; i++) {
        MiniFFmpegAVFilterContext *ctx = graph->filters[i];
        if (ctx) {
            if (ctx->priv) free(ctx->priv);
            if (ctx->inputs) free(ctx->inputs);
            if (ctx->outputs) free(ctx->outputs);
            free(ctx);
        }
    }
    if (graph->filters) free(graph->filters);
    free(graph);
}

int mini_ffmpeg_filter_graph_create_src(MiniFFmpegAVFilterGraph *graph,
                                         const char *name,
                                         MiniFFmpegAVFilterContext **src_ctx) {
    MiniFFmpegAVFilter *filter = mini_ffmpeg_filter_get_by_name("buffer");
    MiniFFmpegAVFilterContext *ctx;
    if (!graph || !filter) return -1;
    ctx = (MiniFFmpegAVFilterContext *)calloc(1, sizeof(*ctx));
    ctx->filter = filter;
    ctx->name = name ? strdup(name) : strdup("src");
    ctx->priv = calloc(1, filter->priv_size > 0 ? filter->priv_size : 64);
    ctx->graph = graph;
    graph->filters[graph->nb_filters++] = ctx;
    if (src_ctx) *src_ctx = ctx;
    return 0;
}

int mini_ffmpeg_filter_graph_create_sink(MiniFFmpegAVFilterGraph *graph,
                                          const char *name,
                                          MiniFFmpegAVFilterContext **sink_ctx) {
    MiniFFmpegAVFilter *filter = mini_ffmpeg_filter_get_by_name("buffersink");
    MiniFFmpegAVFilterContext *ctx;
    if (!graph || !filter) return -1;
    ctx = (MiniFFmpegAVFilterContext *)calloc(1, sizeof(*ctx));
    ctx->filter = filter;
    ctx->name = name ? strdup(name) : strdup("sink");
    ctx->priv = calloc(1, filter->priv_size > 0 ? filter->priv_size : 64);
    ctx->graph = graph;
    graph->filters[graph->nb_filters++] = ctx;
    if (sink_ctx) *sink_ctx = ctx;
    return 0;
}

int mini_ffmpeg_filter_graph_parse(MiniFFmpegAVFilterGraph *graph,
                                    const char *filters_desc,
                                    MiniFFmpegAVFilterContext **inputs,
                                    MiniFFmpegAVFilterContext **outputs) {
    MiniFFmpegAVFilter *filter;
    MiniFFmpegAVFilterContext *ctx;
    if (!graph || !filters_desc) return -1;
    filter = mini_ffmpeg_filter_get_by_name("scale");
    if (filter) {
        ctx = (MiniFFmpegAVFilterContext *)calloc(1, sizeof(*ctx));
        ctx->filter = filter;
        ctx->name = strdup(filters_desc);
        ctx->priv = calloc(1, filter->priv_size > 0 ? filter->priv_size : 64);
        ctx->graph = graph;
        graph->filters[graph->nb_filters++] = ctx;
        if (outputs) *outputs = ctx;
    }
    (void)inputs;
    return 0;
}

int mini_ffmpeg_filter_graph_config(MiniFFmpegAVFilterGraph *graph) {
    (void)graph;
    return 0;
}

int mini_ffmpeg_filter_graph_send_frame(MiniFFmpegAVFilterGraph *graph,
                                         MiniFFmpegAVFilterContext *sink,
                                         MiniFFmpegAVFrame *frame) {
    MiniFFmpegBufferSinkPriv *priv;
    (void)graph;
    if (!sink || !frame) return -1;
    priv = (MiniFFmpegBufferSinkPriv *)sink->priv;
    if (priv->frame) mini_ffmpeg_frame_unref(priv->frame);
    if (!priv->frame) priv->frame = mini_ffmpeg_frame_alloc();
    memcpy(priv->frame, frame, sizeof(*frame));
    priv->frame_count++;
    return 0;
}

int mini_ffmpeg_filter_graph_receive_frame(MiniFFmpegAVFilterGraph *graph,
                                            MiniFFmpegAVFilterContext *sink,
                                            MiniFFmpegAVFrame *frame) {
    MiniFFmpegBufferSinkPriv *priv;
    (void)graph;
    if (!sink || !frame) return -1;
    priv = (MiniFFmpegBufferSinkPriv *)sink->priv;
    if (!priv->frame) return -1;
    memcpy(frame, priv->frame, sizeof(*frame));
    priv->frame = NULL;
    return 0;
}

int mini_ffmpeg_filter_scale_init(MiniFFmpegAVFilterContext *ctx,
                                   int src_w, int src_h, int src_fmt,
                                   int dst_w, int dst_h, int dst_fmt) {
    MiniFFmpegScalePriv *priv;
    if (!ctx || !ctx->priv) return -1;
    priv = (MiniFFmpegScalePriv *)ctx->priv;
    priv->src_w = src_w; priv->src_h = src_h; priv->src_fmt = src_fmt;
    priv->dst_w = dst_w; priv->dst_h = dst_h; priv->dst_fmt = dst_fmt;
    priv->initialized = 1;
    return 0;
}

int mini_ffmpeg_filter_crop_init(MiniFFmpegAVFilterContext *ctx,
                                  int x, int y, int w, int h) {
    MiniFFmpegCropPriv *priv;
    if (!ctx || !ctx->priv) return -1;
    priv = (MiniFFmpegCropPriv *)ctx->priv;
    priv->x = x; priv->y = y; priv->w = w; priv->h = h;
    priv->initialized = 1;
    return 0;
}

int mini_ffmpeg_filter_transpose_init(MiniFFmpegAVFilterContext *ctx,
                                       int dir) {
    MiniFFmpegTransposePriv *priv;
    if (!ctx || !ctx->priv) return -1;
    priv = (MiniFFmpegTransposePriv *)ctx->priv;
    priv->dir = dir;
    priv->initialized = 1;
    return 0;
}

int mini_ffmpeg_filter_overlay_init(MiniFFmpegAVFilterContext *ctx,
                                     int x, int y) {
    MiniFFmpegOverlayPriv *priv;
    if (!ctx || !ctx->priv) return -1;
    priv = (MiniFFmpegOverlayPriv *)ctx->priv;
    priv->x = x; priv->y = y;
    priv->initialized = 1;
    return 0;
}

int mini_ffmpeg_filter_aresample_init(MiniFFmpegAVFilterContext *ctx,
                                       int out_rate, int out_ch,
                                       int out_fmt) {
    MiniFFmpegAResamplePriv *priv;
    if (!ctx || !ctx->priv) return -1;
    priv = (MiniFFmpegAResamplePriv *)ctx->priv;
    priv->out_rate = out_rate;
    priv->out_channels = out_ch;
    priv->out_sample_fmt = out_fmt;
    priv->initialized = 1;
    return 0;
}

int mini_ffmpeg_filter_volume_init(MiniFFmpegAVFilterContext *ctx,
                                    float volume) {
    MiniFFmpegVolumePriv *priv;
    if (!ctx || !ctx->priv) return -1;
    priv = (MiniFFmpegVolumePriv *)ctx->priv;
    priv->volume = volume;
    priv->initialized = 1;
    return 0;
}

int mini_ffmpeg_filter_amix_init(MiniFFmpegAVFilterContext *ctx,
                                  int inputs) {
    MiniFFmpegAMixPriv *priv;
    if (!ctx || !ctx->priv) return -1;
    priv = (MiniFFmpegAMixPriv *)ctx->priv;
    priv->nb_inputs = inputs;
    priv->initialized = 1;
    return 0;
}
