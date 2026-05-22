# mini-ffmpeg-lab Internals

## 架构概述

```
Application
    |
    v
+---+---+   +---------+   +---------+
|Demuxer|   |Decoder  |   |Encoder  |
|(probe,|-->|(NAL->MB |-->|(I/P/B  |
|read   |   | ->PCM)  |   | ->bitstream)
|packets)|  +---------+   +---------+
+---+---+       |               |
    |           v               v
    |      +----------+   +----------+
    +----->|Filter    |-->|AV Sync   |
           |Graph     |   |(PTS/DTS, |
           |(src->sink)|  |clock,    |
           +----------+   |strategy) |
                          +----------+
```

## 模块实现细节

### 1. Demuxer (`demuxer.c`)

**格式探测 (probe):**

每种容器格式有独立的探测函数，返回置信度评分 (0-100):
- `mini_ffmpeg_probe_mp4`: 检测偏移 4 处的 `ftyp` ASCII 标签
- `mini_ffmpeg_probe_mkv`: 检测开头的 EBML 签名 `0x1A45DFA3`
- `mini_ffmpeg_probe_ts`: 验证首个 TS 包同步字节 `0x47` (188B 对齐)
- `mini_ffmpeg_probe_avi`: 检测 `RIFF....AVI ` 四字符码

`probe_format()` 选取最高评分作为容器格式。

**包头解析 (模拟):**

每种容器格式在 `_read_xxx_header()` 中硬编码创建模拟的流信息。例如 MP4 创建 video (H264, 1920x1080) + audio (AAC, 48kHz stereo) 两个流。

**读包 (模拟):**

`demux_read_packet()` 交替产出不同流的包，video 每 30 帧一个 keyframe，audio 全部标记为 keyframe。PTS/DTS 按时间基递增。

**Seek:**

通过 `keyframe_index` 数组二分查找目标时间戳之前最近的 keyframe 位置。

**IO 上下文:**

`MiniFFmpegIOContext` 封装了带缓冲的文件读取，支持自定义 `read_packet` / `seek` 回调。

---

### 2. Decoder Engine (`decoder_engine.c`)

**Codec 注册表:**

使用单向链表 (`MiniFFmpegCodecEntry`) 管理所有已注册 codec。内置 9 个编解码器在首次调用 `codec_register_all()` 时注册。

**解码器引擎:**

`MiniFFmpegDecoderEngine` 维护当前输入包 (`current_pkt`) 和状态标志:
- `packet_consumed`: 当前包是否已被消费
- `draining`: 是否处于 drain 模式
- `frames_decoded`: 已解码帧计数

**send_packet / receive_frame 状态机:**

```
 [IDLE] --send(pkt)--> [HAS_PACKET] --receive()--> [HAS_PACKET|IDLE]
 [IDLE] --send(NULL)--> [DRAINING]  --receive()--> [DRAINING|EOF]
```

**H.264 解码模拟:**

1. NAL 单元解析: 提取 NAL type (SPS/PPS/IDR/slice)
2. SPS 中解析宽高 (从字节 5-6 提取)
3. 帧重建: 创建 YUV420P 三个平面，填充分形图案式测试数据
4. 帧类型: NAL type=5 为 keyframe/I-frame

**AAC 解码模拟:**

从压缩数据生成 1024-sample 的 S16 PCM 帧，填充随机噪声。

**PCM 解码模拟:**

直接拷贝输入数据到输出帧。

---

### 3. Encoder Engine (`encoder_engine.c`)

**H.264 编码模拟:**

1. GOP 管理: `gop_index` 自增，重置时输出 I-frame，B-frame 在 GOP 末尾
2. SPS/PPS 生成: 构造简单的字节序列 (profile=77, level=40)
3. 输出格式化: 起始码 + NAL header (I=0x65, P=0x41, B=0x01)
4. 帧类型决策: `gop_index >= gop_size` → I, else P/B
5. Scene change detection: `scene_change_detected` 标志强制 I-frame

**AAC 编码模拟:**

1. ADTS header 构造 (7 bytes)
2. 固定大小 frame 输出，填充 0xAB 模拟数据
3. PTS 按 `frame_size * 1000000 / sample_rate` 计算

**码率控制 (MiniFFmpegRateControlContext):**

- `CQP`: 恒定 QP
- `CBR`: QP 根据 buffer level 动态调整 (`qp + deviation`)
- `VBR`: 使用指数移动平均 QP
- `CRF`: 保持恒定质量，QP 微调

`rate_control_update()` 计算实际 bits 与目标的偏差，更新 buffer level 和 QP。QP 夹在 0-51 范围。

**Encoder 缓冲:**

`buffered_packets[MAX_ENC_DELAY]` 用于模拟编码延迟/重排序 (B-frame reordering)。

---

### 4. Filter Graph (`filter_graph.c`)

**滤镜注册表:**

与 codec 类似，使用链表管理所有滤镜。内置 12 个滤镜。

**滤镜实现 (逐个说明):**

| 滤镜 | 私有数据 | filter_frame 行为 |
|------|----------|-------------------|
| buffer | BufferSrcPriv | 返回错误 (仅作源) |
| buffersink | BufferSinkPriv | 存储帧到私有 frame |
| scale | ScalePriv | 缩放宽高, 填充测试像素 |
| crop | CropPriv | 修改 frame 宽高 |
| transpose | TransposePriv | 交换宽高 |
| fps | FPSPriv | 缓存 last_frame, 记录计数 |
| overlay | OverlayPriv | 记录主画面尺寸 |
| volume | VolumePriv | 乘以增益因子并钳位 |
| aresample | AResamplePriv | 按比例调整 nb_samples |
| equalizer | EqualizerPriv | (占位, 无实际处理) |
| amix | AMixPriv | 累加 inputs_ready 计数 |

**图执行:**

当前采用简化模型: `send_frame` 直接写入 sink 的私有帧缓冲区, `receive_frame` 从中读取。

---

### 5. AV Sync (`av_sync.c`)

**时钟模型:**

`MiniFFmpegClock` 维护:
- `pts`: 上次更新的 PTS
- `pts_drift`: PTS - 系统时间漂移
- `speed`: 播放速度系数

`clock_get()` 返回 `pts_drift + current_time - (speed * drift_adjustment)`，支持暂停检测。

**主时钟选择:**

`master_sync_type` 决定参考时钟:
- `AUDIO_MASTER` (默认): 以音频时钟为参考
- `VIDEO_MASTER`: 以视频时钟为参考
- `EXTERNAL_MASTER`: 以外部时钟为参考

**video_delay 计算:**

```
delay = current_pts - last_video_pts  (真实帧间隔)
diff  = video_pts - master_clock       (同步差值)
delay += diff * avg_frame_duration     (调整延迟)
```

使用指数平滑 (`smooth_factor = 0.9`) 减少抖动。

**丢帧/重复帧判断:**

- `should_drop()`: `diff < -frame_drop_threshold` → 丢帧 (视频超前)
- `should_repeat()`: `diff > sync_threshold` → 重复 (视频落后)

**唇音同步质量:**

```
quality = 1.0 - total_error / (samples * avg_frame_duration)
```

`total_error` 为历次 PTS 差值的绝对值之和。

**时间基转换:**

```
output_ts = pts * src_tb.num / src_tb.den * output_tb.den / output_tb.num
```

**音频调整:**

通过改变 `nb_samples` 实现音频拉伸/压缩来匹配主时钟。

---

## 数据流示例

### 播放管线

```
demux_read_packet() --> video_dec_send_packet()
                    --> audio_dec_send_packet()
video_dec_receive_frame() --> av_sync_update_video_clock()
audio_dec_receive_frame() --> av_sync_update_audio_clock()
av_sync_compute_video_delay() --> schedule/present
```

### 转码管线

```
demux_read_packet() --> decoder_send_packet()
decoder_receive_frame() --> filter_send_frame()
filter_receive_frame() --> encoder_send_frame()
encoder_receive_packet() --> muxer_write_packet()
```

---

## 内存模型

- 所有分配使用 `malloc/calloc/free`
- AVFrame 的 `data[]` 平面各自独立分配
- `frame_unref()` 释放所有 `data[]` 平面
- `packet_unref()` 释放 `data` 缓冲区
- 上层须确保 `alloc/free` 配对调用

---

## 限制

1. 所有编解码器为模拟实现，不产生符合标准的 bitstream
2. 容器解析不解析完整容器结构
3. 滤镜图不支持多输入/多输出自动协商
4. 无硬件加速抽象
5. 单线程模拟，未实现真正的多线程解码
6. Seek 基于虚构的 keyframe 索引
