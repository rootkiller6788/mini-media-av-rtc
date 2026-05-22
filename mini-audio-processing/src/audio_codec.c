#include "audio_codec.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void mdct_init(MDCT *mdct, int n)
{
    mdct->n = n;
    mdct->in = (float *)calloc((size_t)(2 * n), sizeof(float));
}

void mdct_free(MDCT *mdct)
{
    if (mdct) {
        free(mdct->in);
        mdct->in = NULL;
        mdct->n = 0;
    }
}

int mdct_forward(MDCT *mdct, const float *input, float *output)
{
    if (!mdct || !mdct->in || !input || !output) return -1;

    int N = mdct->n;
    for (int i = 0; i < 2 * N; i++) {
        mdct->in[i] = input[i];
    }

    for (int k = 0; k < N; k++) {
        float sum = 0.0f;
        float n0 = (float)N;
        for (int n = 0; n < 2 * N; n++) {
            float arg = (float)M_PI / n0 * ((float)n + 0.5f + n0 / 2.0f) * ((float)k + 0.5f);
            sum += mdct->in[n] * cosf(arg);
        }
        output[k] = sum;
    }
    return 0;
}

int mdct_inverse(MDCT *mdct, const float *input, float *output)
{
    if (!mdct || !input || !output) return -1;

    int N = mdct->n;
    for (int n = 0; n < 2 * N; n++) {
        float sum = 0.0f;
        float n0 = (float)N;
        for (int k = 0; k < N; k++) {
            float arg = (float)M_PI / n0 * ((float)n + 0.5f + n0 / 2.0f) * ((float)k + 0.5f);
            sum += input[k] * cosf(arg);
        }
        output[n] = sum / n0;
    }
    return 0;
}

int bark_scale(float freq_hz, float *bark_val)
{
    if (!bark_val || freq_hz < 0.0f) return -1;
    float f = freq_hz / 1000.0f;
    *bark_val = 13.0f * atanf(0.76f * f) + 3.5f * atanf(f * f / 56.25f);
    return 0;
}

int bark_freq(float bark_val, float *freq_hz)
{
    if (!freq_hz || bark_val < 0.0f) return -1;
    *freq_hz = 1960.0f * (bark_val + 1.53f) / (26.28f - bark_val);
    return 0;
}

int psychoacoustic_init(PsychoacousticModel *pam, int sample_rate)
{
    if (!pam || sample_rate <= 0) return -1;

    int bands = BARK_BANDS;
    pam->num_bands = bands;
    float max_freq = sample_rate / 2.0f;

    for (int i = 0; i < bands; i++) {
        float bark = (float)i * 24.0f / (bands - 1);
        float freq;
        bark_freq(bark, &freq);
        if (freq > max_freq) freq = max_freq;
        pam->bark_val = 0.0f;
        pam->masking_threshold[i] = 0.0f;
        pam->signal_energy[i] = 0.0f;
        pam->smr[i] = 0.0f;
    }
    return 0;
}

int psychoacoustic_compute(PsychoacousticModel *pam, const float *spectrum, int spectrum_len)
{
    if (!pam || !spectrum || spectrum_len <= 0) return -1;

    int bins_per_band = spectrum_len / pam->num_bands;
    if (bins_per_band < 1) bins_per_band = 1;

    float absolute_threshold = 1e-6f;

    for (int b = 0; b < pam->num_bands; b++) {
        float energy = 0.0f;
        int start = b * bins_per_band;
        int end   = start + bins_per_band;
        if (end > spectrum_len) end = spectrum_len;

        for (int i = start; i < end; i++) {
            energy += spectrum[i] * spectrum[i];
        }
        pam->signal_energy[b] = energy / (end - start);

        float spread = 0.0f;
        for (int s = 0; s < pam->num_bands; s++) {
            float s_energy = pam->signal_energy[s];
            float db = s_energy > 0.0f ? 10.0f * log10f(s_energy) : -100.0f;
            float dist = (float)(b - s) * 2.5f;
            float spread_db = db - dist - 10.0f;
            float spread_lin = powf(10.0f, spread_db / 10.0f);
            spread += spread_lin;
        }

        pam->masking_threshold[b] = spread > absolute_threshold ? spread : absolute_threshold;

        if (pam->masking_threshold[b] > 0.0f && pam->signal_energy[b] > 0.0f) {
            pam->smr[b] = 10.0f * log10f(pam->masking_threshold[b] / absolute_threshold);
        } else {
            pam->smr[b] = 0.0f;
        }
    }
    return 0;
}

int psychoacoustic_get_smr(PsychoacousticModel *pam, int band, float *smr_out)
{
    if (!pam || band < 0 || band >= pam->num_bands || !smr_out) return -1;
    *smr_out = pam->smr[band];
    return 0;
}

void psychoacoustic_free(PsychoacousticModel *pam)
{
    if (pam) {
        pam->num_bands = 0;
    }
}

int aac_quantize_scf(const float *mdct_coeffs, int num_coeffs, const float *scf, int scf_bands, float *quantized)
{
    if (!mdct_coeffs || num_coeffs <= 0 || !scf || !quantized) return -1;

    int coeffs_per_band = num_coeffs / scf_bands;
    if (coeffs_per_band < 1) coeffs_per_band = 1;

    for (int b = 0; b < scf_bands; b++) {
        float scale = scf[b];
        if (scale < 1e-9f) scale = 1e-9f;
        int start = b * coeffs_per_band;
        int end   = start + coeffs_per_band;
        if (end > num_coeffs) end = num_coeffs;

        for (int i = start; i < end; i++) {
            quantized[i] = roundf(mdct_coeffs[i] / scale);
        }
    }
    return 0;
}

int aac_dequantize_scf(const float *quantized, int num_coeffs, const float *scf, int scf_bands, float *mdct_coeffs)
{
    if (!quantized || num_coeffs <= 0 || !scf || !mdct_coeffs) return -1;

    int coeffs_per_band = num_coeffs / scf_bands;
    if (coeffs_per_band < 1) coeffs_per_band = 1;

    for (int b = 0; b < scf_bands; b++) {
        float scale = scf[b];
        int start = b * coeffs_per_band;
        int end   = start + coeffs_per_band;
        if (end > num_coeffs) end = num_coeffs;

        for (int i = start; i < end; i++) {
            mdct_coeffs[i] = quantized[i] * scale;
        }
    }
    return 0;
}

int aac_noise_shaping(const float *mdct_coeffs, int num_coeffs, const PsychoacousticModel *pam, float *shaped)
{
    if (!mdct_coeffs || !pam || !shaped) return -1;

    int coeffs_per_band = num_coeffs / pam->num_bands;
    if (coeffs_per_band < 1) coeffs_per_band = 1;

    for (int b = 0; b < pam->num_bands; b++) {
        float smr = pam->smr[b];
        float noise_weight = powf(10.0f, -smr / 20.0f);
        int start = b * coeffs_per_band;
        int end   = start + coeffs_per_band;
        if (end > num_coeffs) end = num_coeffs;

        for (int i = start; i < end; i++) {
            shaped[i] = mdct_coeffs[i] * noise_weight;
        }
    }
    return 0;
}

static const uint32_t adts_sr_table[16] = {
    96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050,
    16000, 12000, 11025, 8000, 7350, 0, 0, 0
};

static int find_sr_idx(int sample_rate)
{
    for (int i = 0; i < 16; i++) {
        if (adts_sr_table[i] == (uint32_t)sample_rate) return i;
    }
    return 8;
}

int adts_header_write(uint8_t *buf, int profile, int sample_rate_idx, int num_channels, int frame_length)
{
    if (!buf) return -1;

    if (sample_rate_idx < 0) sample_rate_idx = 8;

    uint32_t header = 0;
    header |= 0xFFF00000;
    header |= ((uint32_t)profile & 0x3) << 17;
    header |= ((uint32_t)sample_rate_idx & 0xF) << 13;
    header |= ((uint32_t)(num_channels - 1) & 0x7) << 10;
    header |= ((uint32_t)frame_length & 0x1FFF) << 0;

    buf[0] = (uint8_t)((header >> 24) & 0xFF);
    buf[1] = (uint8_t)((header >> 16) & 0xFF);
    buf[2] = (uint8_t)((header >> 8) & 0xFF);
    buf[3] = (uint8_t)((header >> 0) & 0xFF);
    return 7;
}

int adts_header_parse(const uint8_t *buf, int *profile, int *sample_rate_idx, int *num_channels, int *frame_length)
{
    if (!buf) return -1;

    uint32_t header = ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16)
                    | ((uint32_t)buf[2] << 8) | (uint32_t)buf[3];

    if ((header & 0xFFF00000) != 0xFFF00000) return -2;

    if (profile)         *profile          = (int)((header >> 17) & 0x3);
    if (sample_rate_idx) *sample_rate_idx  = (int)((header >> 13) & 0xF);
    if (num_channels)    *num_channels     = (int)((header >> 10) & 0x7) + 1;
    if (frame_length)    *frame_length     = (int)(header & 0x1FFF);
    return 0;
}

int aac_bitstream_init(AacBitstream *bs, int capacity)
{
    if (!bs || capacity <= 0) return -1;
    bs->data = (uint8_t *)calloc((size_t)capacity, 1);
    if (!bs->data) return -1;
    bs->size         = 0;
    bs->sample_rate  = 44100;
    bs->num_channels = 2;
    bs->frame_length = capacity;
    return 0;
}

void aac_bitstream_free(AacBitstream *bs)
{
    if (bs) {
        free(bs->data);
        bs->data  = NULL;
        bs->size  = 0;
        bs->frame_length = 0;
    }
}

int aac_encode_frame(const float *pcm, int num_samples, int sample_rate, int num_channels, AacFrame *frame)
{
    if (!pcm || num_samples <= 0 || !frame) return -1;

    int mdct_n = num_samples / 2;
    MDCT mdct;
    mdct_init(&mdct, mdct_n);

    frame->num_coeffs = mdct_n;
    frame->mdct_coeffs = (float *)malloc((size_t)mdct_n * sizeof(float));
    if (!frame->mdct_coeffs) { mdct_free(&mdct); return -1; }

    float *mdct_in = (float *)malloc((size_t)(2 * mdct_n) * sizeof(float));
    if (!mdct_in) { mdct_free(&mdct); free(frame->mdct_coeffs); return -1; }
    memcpy(mdct_in, pcm, (size_t)(2 * mdct_n) * sizeof(float));

    mdct_forward(&mdct, mdct_in, frame->mdct_coeffs);

    frame->scf_bands = SCF_BANDS;
    frame->quantized = (float *)malloc((size_t)frame->num_coeffs * sizeof(float));
    if (!frame->quantized) { mdct_free(&mdct); free(mdct_in); free(frame->mdct_coeffs); return -1; }

    PsychoacousticModel pam;
    psychoacoustic_init(&pam, sample_rate);
    psychoacoustic_compute(&pam, frame->mdct_coeffs, frame->num_coeffs);

    for (int b = 0; b < frame->scf_bands; b++) {
        frame->scf[b] = 1.0f + (float)b * 0.1f;
    }

    aac_quantize_scf(frame->mdct_coeffs, frame->num_coeffs, frame->scf, frame->scf_bands, frame->quantized);

    psychoacoustic_free(&pam);
    free(mdct_in);
    mdct_free(&mdct);
    return 0;
}

int aac_decode_frame(const AacFrame *frame, float *pcm, int num_samples)
{
    if (!frame || !pcm || !frame->mdct_coeffs || !frame->quantized) return -1;

    float *dequant = (float *)malloc((size_t)frame->num_coeffs * sizeof(float));
    if (!dequant) return -1;

    aac_dequantize_scf(frame->quantized, frame->num_coeffs, frame->scf, frame->scf_bands, dequant);

    MDCT mdct;
    mdct_init(&mdct, frame->num_coeffs);
    mdct_inverse(&mdct, dequant, pcm);

    mdct_free(&mdct);
    free(dequant);
    return 0;
}

int aac_bitstream_pack(const AacFrame *frame, AacBitstream *bs)
{
    if (!frame || !bs || !bs->data) return -1;

    int sr_idx = find_sr_idx(bs->sample_rate);
    adts_header_write(bs->data, 0, sr_idx, bs->num_channels, bs->frame_length);

    int header_size = 7;
    bs->size = header_size;

    int coeffs = frame->num_coeffs;
    if (bs->size + coeffs * 4 > bs->frame_length) return -2;

    for (int i = 0; i < coeffs && bs->size < bs->frame_length; i++) {
        int32_t q = (int32_t)frame->quantized[i];
        bs->data[bs->size++] = (uint8_t)(q & 0xFF);
        if (bs->size < bs->frame_length) bs->data[bs->size++] = (uint8_t)((q >> 8) & 0xFF);
    }
    return 0;
}

int aac_bitstream_unpack(const AacBitstream *bs, AacFrame *frame)
{
    if (!bs || !bs->data || !frame || bs->size < 7) return -1;

    int profile, sr_idx, num_ch, fl;
    if (adts_header_parse(bs->data, &profile, &sr_idx, &num_ch, &fl) != 0) return -2;

    int header_size = 7;
    int data_size = bs->size - header_size;
    int num_coeffs = data_size / 2;

    frame->num_coeffs = num_coeffs;
    frame->quantized = (float *)malloc((size_t)num_coeffs * sizeof(float));
    if (!frame->quantized) return -1;

    for (int i = 0; i < num_coeffs; i++) {
        int idx = header_size + i * 2;
        int16_t val = (int16_t)(bs->data[idx] | ((int16_t)bs->data[idx + 1] << 8));
        frame->quantized[i] = (float)val;
    }
    return 0;
}
