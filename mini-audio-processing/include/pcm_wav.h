#ifndef PCM_WAV_H
#define PCM_WAV_H

#include <stdint.h>
#include <stddef.h>

typedef enum {
    PCM_FMT_INT16 = 0,
    PCM_FMT_FLOAT32 = 1
} PcmFormat;

typedef enum {
    CHANNEL_MONO   = 1,
    CHANNEL_STEREO = 2
} ChannelLayout;

typedef enum {
    SAMPLE_RATE_8000  = 8000,
    SAMPLE_RATE_11025 = 11025,
    SAMPLE_RATE_16000 = 16000,
    SAMPLE_RATE_22050 = 22050,
    SAMPLE_RATE_32000 = 32000,
    SAMPLE_RATE_44100 = 44100,
    SAMPLE_RATE_48000 = 48000
} SampleRate;

typedef struct {
    float   *data;
    int      num_samples;
    int      num_channels;
    int      sample_rate;
    PcmFormat format;
} AudioBuffer;

#pragma pack(push, 1)
typedef struct {
    char     chunk_id[4];
    uint32_t chunk_size;
    char     format[4];
} RiffHeader;

typedef struct {
    char     subchunk1_id[4];
    uint32_t subchunk1_size;
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
} FmtChunk;

typedef struct {
    char     subchunk2_id[4];
    uint32_t subchunk2_size;
} DataChunk;
#pragma pack(pop)

int      audiobuf_alloc(AudioBuffer *buf, int num_samples, int num_channels, int sample_rate, PcmFormat format);
void     audiobuf_free(AudioBuffer *buf);
int      audiobuf_copy(AudioBuffer *dst, const AudioBuffer *src);
int      audiobuf_silence(AudioBuffer *buf);

int      wav_read(const char *filename, AudioBuffer *buf);
int      wav_write(const char *filename, const AudioBuffer *buf);

int      audiobuf_mix(AudioBuffer *result, const AudioBuffer *a, const AudioBuffer *b);
int      audiobuf_mix_weighted(AudioBuffer *result, const AudioBuffer *a, float weight_a,
                               const AudioBuffer *b, float weight_b);

int      pcm_int16_to_float(const int16_t *in, float *out, int num_samples);
int      pcm_float_to_int16(const float *in, int16_t *out, int num_samples);

#endif
