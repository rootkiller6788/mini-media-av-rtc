#ifndef AUDIO_CODEC_H
#define AUDIO_CODEC_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    float *in;
    int    n;
} MDCT;

#define BARK_BANDS 25
#define SCF_BANDS  49

typedef struct {
    int     bark_idx;
    float   freq_hz;
    float   bark_val;
} BarkMapping;

typedef struct {
    float   masking_threshold[BARK_BANDS];
    float   signal_energy[BARK_BANDS];
    float   smr[BARK_BANDS];
    int     num_bands;
} PsychoacousticModel;

typedef struct {
    uint8_t *data;
    int      size;
    int      sample_rate;
    int      num_channels;
    int      frame_length;
} AacBitstream;

typedef struct {
    float   *mdct_coeffs;
    int      num_coeffs;
    float    scf[SCF_BANDS];
    int      scf_bands;
    float   *quantized;
} AacFrame;

void mdct_init(MDCT *mdct, int n);
void mdct_free(MDCT *mdct);
int  mdct_forward(MDCT *mdct, const float *input, float *output);
int  mdct_inverse(MDCT *mdct, const float *input, float *output);

int  bark_scale(float freq_hz, float *bark_val);
int  bark_freq(float bark_val, float *freq_hz);

int  psychoacoustic_init(PsychoacousticModel *pam, int sample_rate);
int  psychoacoustic_compute(PsychoacousticModel *pam, const float *spectrum, int spectrum_len);
int  psychoacoustic_get_smr(PsychoacousticModel *pam, int band, float *smr_out);
void psychoacoustic_free(PsychoacousticModel *pam);

int  aac_quantize_scf(const float *mdct_coeffs, int num_coeffs, const float *scf, int scf_bands, float *quantized);
int  aac_dequantize_scf(const float *quantized, int num_coeffs, const float *scf, int scf_bands, float *mdct_coeffs);
int  aac_noise_shaping(const float *mdct_coeffs, int num_coeffs, const PsychoacousticModel *pam, float *shaped);

int  adts_header_write(uint8_t *buf, int profile, int sample_rate_idx, int num_channels, int frame_length);
int  adts_header_parse(const uint8_t *buf, int *profile, int *sample_rate_idx, int *num_channels, int *frame_length);

int  aac_bitstream_init(AacBitstream *bs, int capacity);
void aac_bitstream_free(AacBitstream *bs);

int  aac_encode_frame(const float *pcm, int num_samples, int sample_rate, int num_channels, AacFrame *frame);
int  aac_decode_frame(const AacFrame *frame, float *pcm, int num_samples);

int  aac_bitstream_pack(const AacFrame *frame, AacBitstream *bs);
int  aac_bitstream_unpack(const AacBitstream *bs, AacFrame *frame);

#endif
