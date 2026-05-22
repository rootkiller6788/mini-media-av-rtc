# mini-audio-processing Design Document

## Architecture Overview

```
pcm_wav.h/c        — Buffer management, WAV I/O, mixing
fft_core.h/c       — FFT, inverse FFT, windowing, STFT spectrogram
audio_filter.h/c   — FIR/IIR filters, convolution, delay, limiter
audio_codec.h/c    — MDCT, AAC simulator, psychoacoustic model
resample_mix.h/c   — Resampling, channel mapping, multi-track mixing
```

## Data Flow

```
WAV File → [wav_read] → AudioBuffer (float32) → [processing] → AudioBuffer → [wav_write] → WAV File
                                            ↕
                                    various modules:
                                    • FFT → Spectrum
                                    • Filters → Filtered AudioBuffer
                                    • Codec → AacBitstream
                                    • Resample → Resampled AudioBuffer
```

## Internal Sample Format

All processing uses **float32** in the range **[-1.0, 1.0]**. WAV I/O converts to/from int16 transparently.

## Module Details

### pcm_wav — Buffer & File I/O
- `AudioBuffer` is the central data structure: float32, multi-channel
- RIFF/Fmt/Data parsing handles standard 16-bit PCM WAV files
- Mix functions sum samples with configurable weighting

### fft_core — Spectral Analysis
- Radix-2 Cooley-Tukey: bit-reversal reorder + log2(N) butterfly stages
- `next_pow2()` pads signals to power-of-2
- STFT: frames extracted with hop_size step, windowed, FFT → magnitude spectrogram

### audio_filter — Filter Design
- **FIR**: sinc-based design with Hamming window. Delay-line circular buffer for O(N) per-sample
- **Biquad**: Direct Form I (RBJ audio cookbook formulas). 7 filter types
- **Convolution Reverb**: Direct convolution (O(N*M)); suitable for short IRs
- **Echo/Delay**: Circular buffer with feedback
- **Limiter**: Simple brickwall with hard knee

### audio_codec — AAC Simulator
- **MDCT**: Direct O(N^2) implementation (educational, not optimized)
- **Psychoacoustic Model**: Bark scale mapping, spreading function, masking threshold
- **Quantization**: Scale factor band quantization
- **Noise Shaping**: Per-band weighting based on SMR
- **ADTS**: Basic 7-byte header packing/unpacking

### resample_mix — Sample Rate & Mix
- **Linear**: Fast, low quality
- **Sinc**: Windowed sinc with Hann window (32 taps default)
- **Lanczos**: a=3 by default, good quality
- **Channel mapping**: mono↔stereo
- **Multi-track**: In-memory N-track mixer with per-track gain and pan
- **Buffer pool**: Slab allocator for AudioBuffer reuse

## Quality / Limitations

- Educational reference implementation, not production-grade
- MDCT is O(N^2), suitable for small frames only
- Convolution reverb is direct (no FFT overlap-save optimization)
- No fixed-point arithmetic — all float32
- Single-threaded, no SIMD
- AAC: ADTS format simulation, not compliant bitstream

## Dependencies

- **C99 standard library** only: `<stdlib.h>`, `<stdio.h>`, `<string.h>`, `<math.h>`, `<stdint.h>`, `<stddef.h>`
- No external libraries required

## References

- Cooley & Tukey (1965), "An Algorithm for the Machine Calculation of Complex Fourier Series"
- Robert Bristow-Johnson, "Audio EQ Cookbook"
- ISO/IEC 13818-7 (MPEG-2 AAC)
- E. Zwicker, "Subdivision of the Audible Frequency Range into Critical Bands"
- Smith, J.O., "Digital Audio Resampling Home Page"
