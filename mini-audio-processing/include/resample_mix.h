#ifndef RESAMPLE_MIX_H
#define RESAMPLE_MIX_H

#include "pcm_wav.h"
#include <stddef.h>

typedef enum {
    INTERP_LINEAR = 0,
    INTERP_SINC   = 1,
    INTERP_LANCZOS = 2
} InterpMethod;

typedef struct {
    float  *tracks;
    int     num_tracks;
    int     track_length;
    float  *gains;
} MultiTrack;

typedef struct {
    AudioBuffer *pool;
    int          pool_size;
    int          pool_capacity;
} AudioBufPool;

int  resample_linear(const float *input, int in_len, float *output, int out_len);
int  resample_sinc(const float *input, int in_len, float *output, int out_len, int kernel_size);
int  resample_lanczos(const float *input, int in_len, float *output, int out_len, int a);

int  src_convert(const float *input, int in_len, int in_rate,
                 float *output, int *out_len, int out_rate, InterpMethod method);

int  channel_stereo_to_mono(const float *stereo, int num_samples, float *mono);
int  channel_mono_to_stereo(const float *mono, int num_samples, float *stereo);

int  multitrack_init(MultiTrack *mt, int num_tracks, int track_length);
void multitrack_free(MultiTrack *mt);
int  multitrack_set_gain(MultiTrack *mt, int track_index, float gain_db);
int  multitrack_set_track(MultiTrack *mt, int track_index, const float *data);
int  multitrack_mixdown(const MultiTrack *mt, float *output, int out_len);

int  audiobuf_pool_init(AudioBufPool *pool, int capacity);
void audiobuf_pool_free(AudioBufPool *pool);
int  audiobuf_pool_alloc(AudioBufPool *pool, int num_samples, int num_channels,
                         int sample_rate, PcmFormat format, int *index);
int  audiobuf_pool_free_idx(AudioBufPool *pool, int index);
int  audiobuf_pool_copy_to(AudioBufPool *pool, int src_idx, int dst_idx);

#endif
