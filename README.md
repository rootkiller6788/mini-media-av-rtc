# Mini Media AV RTC

**From-scratch, zero-dependency C implementations** of audio/video processing, media codecs, streaming, real-time communication, and digital human/media technologies. Modules cover image/audio/video codec internals, FFmpeg-like pipeline, WebRTC, and digital human rendering.

## Modules

| Module | Topics | Key References |
|--------|--------|----------------|
| [mini-image-processing](mini-image-processing/) | Pixel formats (RGB/YUV), filters (blur/sharpen/edge), transforms (DCT), JPEG codec sim | ITU-T.81, LibJPEG |
| [mini-audio-processing](mini-audio-processing/) | PCM/WAV, FFT, filters (FIR/IIR), audio codec (AAC sim), resampling, mixing | FFmpeg, LibAV |
| [mini-video-processing](mini-video-processing/) | YUV/RGB conversion, H.264 sim (NAL, slice, macroblock, DCT+quant), motion estimation | H.264/AVC, FFmpeg |
| [mini-ffmpeg-lab](mini-ffmpeg-lab/) | Demuxer/Muxer, decoder/encoder graph, filter graph, A/V sync, PTS/DTS model | FFmpeg architecture |
| [mini-media-stream](mini-media-stream/) | HLS/DASH segmenter, ABR streaming, CDN origin/edge, DRM sim (Widevine), live streaming | HLS RFC 8216, MPEG-DASH |
| [mini-rtc-lab](mini-rtc-lab/) | WebRTC: SDP offer/answer, ICE (STUN/TURN), DTLS-SRTP, media track (audio/video), data channel, simulcast, SFU/MCU | WebRTC 1.0, libwebrtc |
| [mini-digital-human-media](mini-digital-human-media/) | Face landmark (MediaPipe sim), body pose, expression blendshape, lip sync, virtual avatar driving | MediaPipe, ARKit |

## Design Philosophy

- **Zero external dependencies** — pure C (C99/C11), only `libc` and `libm`
- **Self-contained modules** — each directory has its own `Makefile`, `include/`, `src/`, `examples/`, `demos/`, `tests/`
- **Media pipeline in user-space** — educational models of codec internals, streaming protocols, and real-time communication
- **Theory-to-code mapping** — every module includes `docs/` with standard-alignment notes
- **Practical demos** — JPEG codec, H.264 simulator, FFmpeg-style pipeline, WebRTC SFU, and more

## Building

Each module is standalone. Navigate to a module directory and run:

```bash
cd mini-image-processing
make all    # build everything
make test   # run tests
```

Requires **GCC** and **GNU Make**.

## Project Structure

```
mini-media-av-rtc/
├── mini-image-processing/       # Image Processing
├── mini-audio-processing/       # Audio Processing
├── mini-video-processing/       # Video Processing
├── mini-ffmpeg-lab/             # FFmpeg Lab
├── mini-media-stream/           # Media Streaming
├── mini-rtc-lab/                # Real-Time Communication Lab
└── mini-digital-human-media/    # Digital Human & Media
```

## License

MIT
