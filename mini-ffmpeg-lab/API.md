# mini-ffmpeg-lab API Reference

## Demuxer (`demuxer.h`)

### 生命周期

```c
MiniFFmpegDemuxContext *mini_ffmpeg_demux_alloc(void);
void mini_ffmpeg_demux_free(MiniFFmpegDemuxContext *ctx);
```

### 格式探测与打开

```c
int mini_ffmpeg_io_open(MiniFFmpegIOContext *io, const char *filename);
void mini_ffmpeg_io_close(MiniFFmpegIOContext *io);
int mini_ffmpeg_demux_open(MiniFFmpegDemuxContext *ctx, MiniFFmpegIOContext *io);
int mini_ffmpeg_demux_find_stream_info(MiniFFmpegDemuxContext *ctx);
```

`mini_ffmpeg_demux_open` 读取文件头部字节，通过魔术字匹配识别容器格式:
- MP4: `ftyp` atom (偏移 4 字节)
- MKV: EBML header `0x1A45DFA3`
- TS: 同步字节 `0x47` 每 188B
- AVI: RIFF/AIV 签名

返回 0 表示成功，负值表示失败。

### 读取数据包

```c
int mini_ffmpeg_demux_read_packet(MiniFFmpegDemuxContext *ctx,
                                   MiniFFmpegAVPacket *pkt);
```

从容器中读取下一个 AVPacket，返回填充的 PTS/DTS/stream_index/flags。

### 流信息

```c
int mini_ffmpeg_demux_get_nb_streams(MiniFFmpegDemuxContext *ctx);
MiniFFmpegAVStream *mini_ffmpeg_demux_get_stream(MiniFFmpegDemuxContext *ctx,
                                                  int index);
int mini_ffmpeg_demux_get_format(MiniFFmpegDemuxContext *ctx);
int64_t mini_ffmpeg_demux_get_duration(MiniFFmpegDemuxContext *ctx);
```

### Seek

```c
int mini_ffmpeg_demux_seek(MiniFFmpegDemuxContext *ctx,
                           int stream_index, int64_t timestamp, int flags);
```

### Packet 管理

```c
void mini_ffmpeg_packet_init(MiniFFmpegAVPacket *pkt);
void mini_ffmpeg_packet_unref(MiniFFmpegAVPacket *pkt);
int mini_ffmpeg_packet_alloc(MiniFFmpegAVPacket *pkt, int size);
```

### 关键结构体

**MiniFFmpegAVStream**

| 字段 | 类型 | 说明 |
|------|------|------|
| index | int | 流索引号 |
| codec_type | int | VIDEO/AUDIO/SUBTITLE |
| codec_id | int | 编解码器 ID |
| time_base | MiniFFmpegRational | 时间基 |
| duration | int64_t | 流时长 |
| width/height | int | 视频分辨率 |
| sample_rate | int | 音频采样率 |
| channels | int | 音频声道数 |

**MiniFFmpegAVPacket**

| 字段 | 类型 | 说明 |
|------|------|------|
| data | uint8_t* | 压缩数据指针 |
| size | int | 数据大小 |
| pts | int64_t | 显示时间戳 |
| dts | int64_t | 解码时间戳 |
| stream_index | int | 所属流 |
| flags | int | KEY/CORRUPT 标志 |

---

## Decoder Engine (`decoder_engine.h`)

### Codec 注册与查找

```c
void mini_ffmpeg_codec_register_all(void);
MiniFFmpegAVCodec *mini_ffmpeg_codec_find_decoder(int codec_id);
MiniFFmpegAVCodec *mini_ffmpeg_codec_find_encoder(int codec_id);
MiniFFmpegAVCodec *mini_ffmpeg_codec_find_by_name(const char *name);
MiniFFmpegAVCodec *mini_ffmpeg_codec_iterate(void **opaque);
```

内置 codecs: h264, h265, vp8, vp9, av1, aac, mp3, opus, pcm_s16le.

### 解码模型

```c
MiniFFmpegDecoderEngine *mini_ffmpeg_decoder_alloc(void);
void mini_ffmpeg_decoder_free(MiniFFmpegDecoderEngine *engine);
int mini_ffmpeg_decoder_open(MiniFFmpegDecoderEngine *engine,
                             MiniFFmpegAVCodecContext *codec_ctx);
int mini_ffmpeg_decoder_send_packet(MiniFFmpegDecoderEngine *engine,
                                    MiniFFmpegAVPacket *pkt);
int mini_ffmpeg_decoder_receive_frame(MiniFFmpegDecoderEngine *engine,
                                      MiniFFmpegAVFrame *frame);
int mini_ffmpeg_decoder_flush(MiniFFmpegDecoderEngine *engine);
```

采用 send_packet / receive_frame 模型:
1. `send_packet(pkt)` — 输入压缩包
2. `receive_frame(frame)` — 循环取出解码帧，返回 < 0 时停止
3. `send_packet(NULL)` — 进入 drain 模式
4. `flush()` — 强制排空解码器缓冲

### Frame 管理

```c
void mini_ffmpeg_frame_init(MiniFFmpegAVFrame *frame);
void mini_ffmpeg_frame_unref(MiniFFmpegAVFrame *frame);
int mini_ffmpeg_frame_alloc_buffer(MiniFFmpegAVFrame *frame, int align);
MiniFFmpegAVFrame *mini_ffmpeg_frame_alloc(void);
void mini_ffmpeg_frame_free(MiniFFmpegAVFrame *frame);
```

### H.264 解码模拟

输入包含 NAL 单元的 AVPacket:
- NAL type 7: SPS (解析宽高)
- NAL type 8: PPS
- NAL type 1/5: slice (IDR/keyframe 判断)

输出 YUV420P AVFrame，填充合成像素数据。

### AAC 解码模拟

从压缩数据产生 PCM S16 音频帧 (1024 samples)。

---

## Encoder Engine (`encoder_engine.h`)

### 编码模型

```c
MiniFFmpegEncoderEngine *mini_ffmpeg_encoder_alloc(void);
void mini_ffmpeg_encoder_free(MiniFFmpegEncoderEngine *engine);
int mini_ffmpeg_encoder_open(MiniFFmpegEncoderEngine *engine,
                             MiniFFmpegAVCodecContext *codec_ctx,
                             const MiniFFmpegEncParams *params);
int mini_ffmpeg_encoder_send_frame(MiniFFmpegEncoderEngine *engine,
                                   const MiniFFmpegAVFrame *frame);
int mini_ffmpeg_encoder_receive_packet(MiniFFmpegEncoderEngine *engine,
                                       MiniFFmpegAVPacket *pkt);
int mini_ffmpeg_encoder_flush(MiniFFmpegEncoderEngine *engine);
```

send_frame(NULL) 进入 flush 模式，排出残留帧。

### 编码参数

**MiniFFmpegEncParams** 关键字段:

| 字段 | 说明 |
|------|------|
| width/height | 视频分辨率 |
| bit_rate | 目标码率 (bps) |
| gop_size | GOP 间隔 |
| max_b_frames | 最大 B 帧数 |
| preset | 速度预设 (0-6) |
| rate_control | CQP/CBR/VBR/ABR/CRF |
| crf_value | CRF 质量值 (0-51) |
| qp_constant | 固定 QP |
| frame_rate_num/den | 帧率 |

### 动态参数调整

```c
int mini_ffmpeg_encoder_set_bitrate(MiniFFmpegEncoderEngine *engine, int bit_rate);
int mini_ffmpeg_encoder_set_gop_size(MiniFFmpegEncoderEngine *engine, int gop_size);
int mini_ffmpeg_encoder_set_preset(MiniFFmpegEncoderEngine *engine, int preset);
int mini_ffmpeg_encoder_set_rate_control(MiniFFmpegEncoderEngine *engine, int mode);
int mini_ffmpeg_encoder_force_keyframe(MiniFFmpegEncoderEngine *engine);
```

### 码率控制

```c
void mini_ffmpeg_rate_control_init(MiniFFmpegRateControlContext *rc,
                                    const MiniFFmpegEncParams *params);
void mini_ffmpeg_rate_control_update(MiniFFmpegRateControlContext *rc,
                                      int frame_type, int frame_bits);
float mini_ffmpeg_rate_control_get_qp(MiniFFmpegRateControlContext *rc,
                                       int frame_type);
```

支持 CBR (buffer feedback), VBR (average QP), CRF (constant rate factor).

### 统计信息

```c
int mini_ffmpeg_encoder_get_stats(MiniFFmpegEncoderEngine *engine,
                                   int *frame_count, float *avg_qp,
                                   int64_t *total_bits);
```

---

## Filter Graph (`filter_graph.h`)

### 图管理

```c
void mini_ffmpeg_filter_register_all(void);
MiniFFmpegAVFilterGraph *mini_ffmpeg_filter_graph_alloc(void);
void mini_ffmpeg_filter_graph_free(MiniFFmpegAVFilterGraph *graph);
int mini_ffmpeg_filter_graph_parse(MiniFFmpegAVFilterGraph *graph,
                                    const char *filters_desc,
                                    MiniFFmpegAVFilterContext **inputs,
                                    MiniFFmpegAVFilterContext **outputs);
int mini_ffmpeg_filter_graph_config(MiniFFmpegAVFilterGraph *graph);
```

Buffer source/sink:
```c
int mini_ffmpeg_filter_graph_create_src(MiniFFmpegAVFilterGraph *graph,
                                         const char *name,
                                         MiniFFmpegAVFilterContext **src_ctx);
int mini_ffmpeg_filter_graph_create_sink(MiniFFmpegAVFilterGraph *graph,
                                          const char *name,
                                          MiniFFmpegAVFilterContext **sink_ctx);
```

### 帧进出

```c
int mini_ffmpeg_filter_graph_send_frame(MiniFFmpegAVFilterGraph *graph,
                                         MiniFFmpegAVFilterContext *sink,
                                         MiniFFmpegAVFrame *frame);
int mini_ffmpeg_filter_graph_receive_frame(MiniFFmpegAVFilterGraph *graph,
                                            MiniFFmpegAVFilterContext *sink,
                                            MiniFFmpegAVFrame *frame);
```

### 内置滤镜及初始化

| 滤镜 | 函数 | 参数 |
|------|------|------|
| scale | `mini_ffmpeg_filter_scale_init` | src/dst w,h,fmt |
| crop | `mini_ffmpeg_filter_crop_init` | x, y, w, h |
| transpose | `mini_ffmpeg_filter_transpose_init` | dir (0-3) |
| fps | — | frame_rate |
| overlay | `mini_ffmpeg_filter_overlay_init` | x, y |
| concat | — | — |
| aresample | `mini_ffmpeg_filter_aresample_init` | rate, ch, fmt |
| volume | `mini_ffmpeg_filter_volume_init` | volume factor |
| equalizer | — | 18-band |
| amix | `mini_ffmpeg_filter_amix_init` | nb_inputs |

---

## AV Sync (`av_sync.h`)

### 同步上下文

```c
MiniFFmpegAVSyncContext *mini_ffmpeg_av_sync_alloc(void);
void mini_ffmpeg_av_sync_free(MiniFFmpegAVSyncContext *sync);
void mini_ffmpeg_av_sync_init(MiniFFmpegAVSyncContext *sync);
```

### 时钟系统

```c
void mini_ffmpeg_clock_init(MiniFFmpegClock *clock, int *queue_serial);
void mini_ffmpeg_clock_set(MiniFFmpegClock *clock, double pts, int serial);
double mini_ffmpeg_clock_get(MiniFFmpegClock *clock);
void mini_ffmpeg_clock_set_speed(MiniFFmpegClock *clock, double speed);
```

三种时钟: audio, video, external.

### 同步控制

```c
int mini_ffmpeg_av_sync_set_master(MiniFFmpegAVSyncContext *sync, int type);
double mini_ffmpeg_av_sync_get_master_clock(MiniFFmpegAVSyncContext *sync);
int mini_ffmpeg_av_sync_compute_video_delay(MiniFFmpegAVSyncContext *sync,
                                             double pts);
int mini_ffmpeg_av_sync_should_drop(MiniFFmpegAVSyncContext *sync, double pts);
int mini_ffmpeg_av_sync_should_repeat(MiniFFmpegAVSyncContext *sync, double pts);
```

同步策略:
- `REPEAT_LAST`: 重复上一帧
- `DROP_FRAME`: 丢弃延迟帧
- `INTERPOLATE`: 帧插值
- `AUDIO_RESAMPLE`: 音频重采样

### 时间基转换

```c
int64_t mini_ffmpeg_av_sync_pts_to_output(MiniFFmpegAVSyncContext *sync,
                                           int64_t pts,
                                           MiniFFmpegRational *src_tb);
```

### 音频调整

```c
int mini_ffmpeg_av_sync_adjust_audio(MiniFFmpegAVSyncContext *sync,
                                      int nb_samples);
```

### 质量检测

```c
double mini_ffmpeg_av_sync_get_lip_sync_quality(MiniFFmpegAVSyncContext *sync);
void mini_ffmpeg_av_sync_smooth_timestamps(MiniFFmpegAVSyncContext *sync,
                                            double *pts, int count);
```

返回 0.0-1.0 之间的唇音同步质量评分。

### 调度与重置

```c
int mini_ffmpeg_av_sync_schedule_frame(MiniFFmpegAVSyncContext *sync,
                                        double pts, int serial);
void mini_ffmpeg_av_sync_reset(MiniFFmpegAVSyncContext *sync);
```
