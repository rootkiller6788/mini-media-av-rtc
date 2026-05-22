# mini-audio-processing API Reference

## pcm_wav.h — PCM & WAV File I/O

| Function | Description |
|---|---|
| `audiobuf_alloc(buf, n_samp, n_ch, sr, fmt)` | Allocate audio buffer (float32) |
| `audiobuf_free(buf)` | Free audio buffer memory |
| `audiobuf_copy(dst, src)` | Deep copy buffer contents |
| `audiobuf_silence(buf)` | Zero all samples |
| `wav_read(filename, buf)` | Read 16-bit PCM WAV into float buffer |
| `wav_write(filename, buf)` | Write float buffer as 16-bit PCM WAV |
| `audiobuf_mix(result, a, b)` | Mix two buffers (equal weights) |
| `audiobuf_mix_weighted(result, a, wa, b, wb)` | Mix with per-buffer weights |
| `pcm_int16_to_float(in, out, n)` | Convert int16 PCM to float [-1,1] |
| `pcm_float_to_int16(in, out, n)` | Convert float [-1,1] to int16 PCM |

### Data Types

```c
AudioBuffer { float *data; int num_samples, num_channels, sample_rate; PcmFormat format; }
PcmFormat   { PCM_FMT_INT16, PCM_FMT_FLOAT32 }
ChannelLayout { CHANNEL_MONO=1, CHANNEL_STEREO=2 }
SampleRate  { SAMPLE_RATE_8000 .. SAMPLE_RATE_48000 }
RiffHeader, FmtChunk, DataChunk — WAV file structures (packed)
```

---

## fft_core.h — FFT & Spectrum Analysis

| Function | Description |
|---|---|
| `next_pow2(n)` | Next power of 2 >= n |
| `fft(x, n)` | In-place radix-2 Cooley-Tukey FFT |
| `ifft(x, n)` | Inverse FFT (normalized) |
| `fft_real(real_in, cplx_out, n)` | Real input to complex FFT |
| `fft_magnitude(cplx, mag, n)` | Magnitude spectrum (nyquist bins) |
| `fft_phase(cplx, phase, n)` | Phase spectrum in radians |
| `spectrum_compute(spec, cplx, n)` | Compute full spectrum (mag + phase) |
| `spectrum_free(spec)` | Free spectrum memory |
| `window_generate(win, size, type)` | Generate window coefficients |
| `window_apply(signal, size, type)` | Apply window in-place |
| `stft_init(sg, ...)` | Allocate STFT spectrogram |
| `stft_compute(signal, len, sg)` | Compute STFT magnitude frames |
| `stft_free(sg)` | Free spectrogram |

### Window Types

- `WINDOW_RECTANGLE` — No window (unity)
- `WINDOW_HANN` — Hann window (cosine)
- `WINDOW_HAMMING` — Hamming window (0.54 - 0.46 cos)
- `WINDOW_BLACKMAN` — Blackman window (3-term)

---

## audio_filter.h — Filters & Effects

| Function | Description |
|---|---|
| `fir_design(fir, taps, cutoff, sr, type)` | Design FIR filter (window method) |
| `fir_design_bp(fir, taps, lo, hi, sr)` | Design FIR bandpass |
| `fir_free(fir)` | Free FIR filter |
| `fir_process(fir, in, out, n)` | Apply FIR filter |
| `biquad_design(bq, type, freq, Q, gain, sr)` | Design biquad filter (RBJ cookbook) |
| `biquad_process(bq, in, out, n)` | Apply biquad filter |
| `biquad_reset(bq)` | Reset biquad state |
| `conv_reverb_init(rev, ir, ir_len)` | Initialize convolution reverb |
| `conv_reverb_free(rev)` | Free reverb |
| `conv_reverb_process(rev, in, out, n)` | Apply reverb |
| `echo_delay_init(echo, ms, sr, fb, wet)` | Initialize delay line |
| `echo_delay_free(echo)` | Free delay |
| `echo_delay_process(echo, in, out, n)` | Apply delay/echo |
| `apply_gain(in, out, n, gain_db)` | Apply gain (dB) |
| `apply_limiter(samples, n, thresh, ceil)` | Brickwall limiter |

### Filter Types

**FIR:** `FIR_LOWPASS`, `FIR_HIGHPASS`, `FIR_BANDPASS`

**Biquad:** `BIQUAD_LOWPASS`, `BIQUAD_HIGHPASS`, `BIQUAD_BANDPASS`, `BIQUAD_NOTCH`, `BIQUAD_PEAKING`, `BIQUAD_LOWSHELF`, `BIQUAD_HIGHSHELF`

---

## audio_codec.h — AAC Simulation

| Function | Description |
|---|---|
| `mdct_forward(mdct, in, out)` | Forward Modified DCT |
| `mdct_inverse(mdct, in, out)` | Inverse MDCT |
| `bark_scale(freq_hz, bark_val)` | Hz to Bark scale |
| `bark_freq(bark_val, freq_hz)` | Bark to Hz |
| `psychoacoustic_init(pam, sr)` | Init psychoacoustic model |
| `psychoacoustic_compute(pam, spec, len)` | Compute masking thresholds |
| `psychoacoustic_get_smr(pam, band, smr)` | Get SMR for a band |
| `aac_quantize_scf(coeffs, n, scf, bands, out)` | Quantize with scalefactors |
| `aac_noise_shaping(coeffs, n, pam, out)` | Perceptual noise shaping |
| `adts_header_write(buf, ...)` | Write ADTS header (7 bytes) |
| `adts_header_parse(buf, ...)` | Parse ADTS header |
| `aac_encode_frame(pcm, n, sr, ch, frame)` | Encode AAC frame |
| `aac_decode_frame(frame, pcm, n)` | Decode AAC frame |

---

## resample_mix.h — Resampling & Mixing

| Function | Description |
|---|---|
| `resample_linear(in, ilen, out, olen)` | Linear interpolation |
| `resample_sinc(in, ilen, out, olen, k)` | Sinc interpolation (windowed) |
| `resample_lanczos(in, ilen, out, olen, a)` | Lanczos (a-tap) interpolation |
| `src_convert(in, ilen, ir, out, olen, or, m)` | Full sample rate conversion |
| `channel_stereo_to_mono(stereo, n, mono)` | Stereo downmix |
| `channel_mono_to_stereo(mono, n, stereo)` | Mono to stereo (duplicate) |
| `multitrack_init(mt, n_tracks, len)` | Allocate multi-track |
| `multitrack_set_gain(mt, idx, db)` | Set per-track gain |
| `multitrack_set_track(mt, idx, data)` | Set track audio data |
| `multitrack_mixdown(mt, out, len)` | Render mixdown |
| `audiobuf_pool_init(pool, cap)` | Initialize buffer pool |
| `audiobuf_pool_alloc(pool, ..., &idx)` | Allocate from pool |
| `audiobuf_pool_free_idx(pool, idx)` | Free a pool slot |
| `audiobuf_pool_copy_to(pool, src, dst)` | Copy between pool slots |
