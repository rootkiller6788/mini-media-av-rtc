#include "pcm_wav.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

int audiobuf_alloc(AudioBuffer *buf, int num_samples, int num_channels, int sample_rate, PcmFormat format)
{
    if (!buf || num_samples <= 0 || num_channels <= 0) return -1;

    buf->data = (float *)calloc((size_t)num_samples * num_channels, sizeof(float));
    if (!buf->data) return -1;

    buf->num_samples  = num_samples;
    buf->num_channels = num_channels;
    buf->sample_rate  = sample_rate;
    buf->format       = format;
    return 0;
}

void audiobuf_free(AudioBuffer *buf)
{
    if (buf && buf->data) {
        free(buf->data);
        buf->data = NULL;
    }
    buf->num_samples  = 0;
    buf->num_channels = 0;
    buf->sample_rate  = 0;
}

int audiobuf_copy(AudioBuffer *dst, const AudioBuffer *src)
{
    if (!dst || !src || !src->data) return -1;

    int total = src->num_samples * src->num_channels;
    audiobuf_free(dst);

    dst->data = (float *)malloc((size_t)total * sizeof(float));
    if (!dst->data) return -1;

    memcpy(dst->data, src->data, (size_t)total * sizeof(float));
    dst->num_samples  = src->num_samples;
    dst->num_channels = src->num_channels;
    dst->sample_rate  = src->sample_rate;
    dst->format       = src->format;
    return 0;
}

int audiobuf_silence(AudioBuffer *buf)
{
    if (!buf || !buf->data) return -1;
    int total = buf->num_samples * buf->num_channels;
    memset(buf->data, 0, (size_t)total * sizeof(float));
    return 0;
}

int pcm_int16_to_float(const int16_t *in, float *out, int num_samples)
{
    if (!in || !out || num_samples <= 0) return -1;
    for (int i = 0; i < num_samples; i++) {
        out[i] = (float)in[i] / 32768.0f;
    }
    return 0;
}

int pcm_float_to_int16(const float *in, int16_t *out, int num_samples)
{
    if (!in || !out || num_samples <= 0) return -1;
    for (int i = 0; i < num_samples; i++) {
        float clamped = in[i];
        if (clamped > 1.0f) clamped = 1.0f;
        if (clamped < -1.0f) clamped = -1.0f;
        out[i] = (int16_t)(clamped * 32767.0f);
    }
    return 0;
}

int audiobuf_mix(AudioBuffer *result, const AudioBuffer *a, const AudioBuffer *b)
{
    return audiobuf_mix_weighted(result, a, 1.0f, b, 1.0f);
}

int audiobuf_mix_weighted(AudioBuffer *result, const AudioBuffer *a, float weight_a,
                           const AudioBuffer *b, float weight_b)
{
    if (!result || !a || !b || !a->data || !b->data) return -1;
    if (a->num_channels != b->num_channels || a->sample_rate != b->sample_rate) return -1;

    int max_len = a->num_samples > b->num_samples ? a->num_samples : b->num_samples;
    int ch = a->num_channels;

    if (audiobuf_alloc(result, max_len, ch, a->sample_rate, a->format) != 0) return -1;

    int total = max_len * ch;
    int a_total = a->num_samples * ch;
    int b_total = b->num_samples * ch;

    for (int i = 0; i < total; i++) {
        float va = (i < a_total) ? a->data[i] : 0.0f;
        float vb = (i < b_total) ? b->data[i] : 0.0f;
        result->data[i] = va * weight_a + vb * weight_b;
    }
    return 0;
}

int wav_read(const char *filename, AudioBuffer *buf)
{
    FILE *f = fopen(filename, "rb");
    if (!f) return -1;

    RiffHeader riff;
    FmtChunk   fmt;
    DataChunk  data;

    if (fread(&riff, sizeof(riff), 1, f) != 1) { fclose(f); return -2; }
    if (memcmp(riff.chunk_id, "RIFF", 4) != 0) { fclose(f); return -2; }
    if (memcmp(riff.format, "WAVE", 4) != 0) { fclose(f); return -2; }

    if (fread(&fmt, sizeof(fmt), 1, f) != 1) { fclose(f); return -2; }

    while (memcmp(data.subchunk2_id, "data", 4) != 0) {
        if (fread(&data, sizeof(data), 1, f) != 1) { fclose(f); return -2; }
        if (memcmp(data.subchunk2_id, "data", 4) != 0) {
            fseek(f, data.subchunk2_size, SEEK_CUR);
        }
    }

    int num_samples = data.subchunk2_size / (fmt.bits_per_sample / 8);
    if (audiobuf_alloc(buf, num_samples / fmt.num_channels, fmt.num_channels,
                       fmt.sample_rate, PCM_FMT_FLOAT32) != 0) { fclose(f); return -3; }

    int16_t *raw = (int16_t *)malloc((size_t)num_samples * sizeof(int16_t));
    if (!raw) { fclose(f); return -3; }
    if (fread(raw, sizeof(int16_t), num_samples, f) != (size_t)num_samples) {
        free(raw); fclose(f); return -3;
    }
    pcm_int16_to_float(raw, buf->data, num_samples);
    free(raw);
    fclose(f);
    return 0;
}

int wav_write(const char *filename, const AudioBuffer *buf)
{
    if (!buf || !buf->data) return -1;

    FILE *f = fopen(filename, "wb");
    if (!f) return -1;

    int num_samples = buf->num_samples * buf->num_channels;
    int data_size = num_samples * (int)sizeof(int16_t);

    RiffHeader riff;
    memcpy(riff.chunk_id, "RIFF", 4);
    riff.chunk_size = (uint32_t)(36 + data_size);
    memcpy(riff.format, "WAVE", 4);
    fwrite(&riff, sizeof(riff), 1, f);

    FmtChunk fmt;
    memcpy(fmt.subchunk1_id, "fmt ", 4);
    fmt.subchunk1_size  = 16;
    fmt.audio_format    = 1;
    fmt.num_channels    = (uint16_t)buf->num_channels;
    fmt.sample_rate     = (uint32_t)buf->sample_rate;
    fmt.bits_per_sample = 16;
    fmt.byte_rate       = fmt.sample_rate * fmt.num_channels * (fmt.bits_per_sample / 8);
    fmt.block_align     = (uint16_t)(fmt.num_channels * (fmt.bits_per_sample / 8));
    fwrite(&fmt, sizeof(fmt), 1, f);

    DataChunk data;
    memcpy(data.subchunk2_id, "data", 4);
    data.subchunk2_size = (uint32_t)data_size;
    fwrite(&data, sizeof(data), 1, f);

    int16_t *raw = (int16_t *)malloc((size_t)num_samples * sizeof(int16_t));
    if (!raw) { fclose(f); return -1; }
    pcm_float_to_int16(buf->data, raw, num_samples);
    fwrite(raw, sizeof(int16_t), num_samples, f);
    free(raw);
    fclose(f);
    return 0;
}
