# DASH Segmenter 文档 (DASH 分段器)

## 概述

`dash_segmenter` 模块实现了 MPEG-DASH (Dynamic Adaptive Streaming over HTTP) 分段器，支持生成 MPD manifest 文件与 fMP4 分段。

## API 参考

### 初始化与销毁

```c
int  dash_segmenter_init(dash_segmenter_t *seg, const char *output_dir, const char *prefix);
void dash_segmenter_deinit(dash_segmenter_t *seg);
```

### MPD 配置

```c
int dash_segmenter_mpd_init(dash_segmenter_t *seg, const char *profiles, uint8_t is_dynamic);
int dash_segmenter_set_utc_timing(dash_segmenter_t *seg, dash_utc_timing_scheme_t scheme, const char *source_url);
```

- `profiles`: MPD 标识, 如 `"urn:mpeg:dash:profile:isoff-live:2011"`
- `is_dynamic`: 1 = dynamic (直播), 0 = static (点播)
- `utc_timing`: `DASH_TIMING_NTP`, `DASH_TIMING_HTTP`, `DASH_TIMING_GPS`

### Period / AdaptationSet / Representation

```c
int dash_segmenter_add_period(dash_segmenter_t *seg, const char *period_id, uint64_t start_ms, uint64_t duration_ms);
int dash_segmenter_add_adaptation_set(dash_segmenter_t *seg, uint32_t period_index, const char *as_id, dash_content_type_t type, const char *lang);
int dash_segmenter_add_representation(dash_segmenter_t *seg, uint32_t period_index, uint32_t as_index, const char *rep_id, uint32_t bandwidth, uint32_t width, uint32_t height, const char *codecs, const char *mime);
```

#### 内容类型
| 枚举 | 值 |
|------|----|
| `DASH_CONTENT_VIDEO` | video |
| `DASH_CONTENT_AUDIO` | audio |
| `DASH_CONTENT_SUBTITLE` | subtitle |
| `DASH_CONTENT_TEXT` | text |

### Segment Template

```c
int dash_segmenter_set_template(dash_segmenter_t *seg, uint32_t period_index, uint32_t as_index, uint32_t rep_index, dash_template_type_t type, uint32_t start_number, uint32_t duration);
int dash_segmenter_set_init_segment(dash_segmenter_t *seg, uint32_t period_index, uint32_t as_index, uint32_t rep_index, const char *url);
int dash_segmenter_set_media_segment(dash_segmenter_t *seg, uint32_t period_index, uint32_t as_index, uint32_t rep_index, const char *url);
```

模板类型:
- `DASH_TEMPLATE_NUMBER`: 使用 `$Number$` 变量
- `DASH_TEMPLATE_TIME`: 使用 `$Time$` 变量

URL 模板变量:
- `$RepresentationID$` → 码率标识
- `$Number$` → 分段序号
- `$Time$` → 分段时间戳

### Segment Timeline

```c
int dash_segmenter_add_timeline_entry(dash_segmenter_t *seg, uint32_t period_index, uint32_t as_index, uint32_t rep_index, uint64_t time, uint64_t duration, uint32_t number);
```

每个 timeline entry 表示一个分段的展示时间、时长与序号。

### fMP4 Segment 操作

```c
int dash_segmenter_write_init_segment(dash_segmenter_t *seg, const uint8_t *ftyp, size_t ftyp_len, const uint8_t *moov, size_t moov_len);
int dash_segmenter_start_fmp4(dash_segmenter_t *seg, const char *filename, uint64_t base_decode_time);
int dash_segmenter_finish_fmp4(dash_segmenter_t *seg);
int dash_segmenter_advance_segment(dash_segmenter_t *seg);
```

- `write_init_segment`: 写入 ftyp + moov 初始化段
- `start_fmp4`: 开始一个新的 fMP4 分段 (包含 moof + mdat)
- `finish_fmp4`: 关闭当前 fMP4 分段文件
- `advance_segment`: 递增分段计数与时间

### MPD 生成

```c
int dash_segmenter_generate_mpd(dash_segmenter_t *seg, char *buffer, size_t buf_size);
int dash_segmenter_write_mpd_file(dash_segmenter_t *seg, const char *filename);
```

生成的 MPD 包含完整的 XML 结构，包括 Period, AdaptationSet, Representation, SegmentTemplate/SegmentTimeline。

## 数据结构

### dash_mpd_t (MPD 主结构)
| 字段 | 类型 | 说明 |
|------|------|------|
| `periods[]` | `dash_period_t[32]` | Period 数组 |
| `period_count` | `uint32_t` | Period 数量 |
| `profiles` | `char[128]` | MPD profiles |
| `min_buffer_time_ms` | `uint32_t` | 最小缓冲时间 |
| `is_dynamic` | `uint8_t` | 是否动态 MPD |
| `utc_timing` | `dash_utc_timing_t` | UTC 时间同步 |
| `availability_start_time_ms` | `uint64_t` | 可用起始时间 |

### dash_representation_t
| 字段 | 类型 | 说明 |
|------|------|------|
| `bandwidth` | `uint32_t` | 码率 (bps) |
| `width`, `height` | `uint32_t` | 分辨率 |
| `codecs` | `char[64]` | 编码格式 |
| `mime_type` | `char[64]` | MIME 类型 |
| `init_segment_url` | `char[2048]` | 初始化段 URL |
| `media_segment_url` | `char[2048]` | 媒体段 URL 模板 |
| `template_type` | `enum` | Number/Time |
| `timeline` | `dash_segment_timeline_t` | 时间线条目 |

### dash_moof_t (fMP4 头结构)
| 字段 | 类型 | 说明 |
|------|------|------|
| `mfhd` | `dash_mfhd_t` | Movie Fragment Header (sequence_number) |
| `tfhd` | `dash_tfhd_t` | Track Fragment Header (track_id, default_sample_*) |
| `tfdt` | `dash_tfdt_t` | Track Fragment Decode Time (base_media_decode_time) |
| `trun` | `dash_trun_t` | Track Run (sample durations, sizes, flags) |

## 使用示例

### 静态 VOD

```c
dash_segmenter_t seg;
dash_segmenter_init(&seg, "./output", "dash");

dash_segmenter_mpd_init(&seg, "urn:mpeg:dash:profile:isoff-live:2011", 0);
dash_segmenter_add_period(&seg, "p0", 0, 60000);

dash_segmenter_add_adaptation_set(&seg, 0, "video", DASH_CONTENT_VIDEO, NULL);
dash_segmenter_add_representation(&seg, 0, 0, "1080p", 5000000, 1920, 1080,
                                  "avc1.640028", "video/mp4");
dash_segmenter_set_template(&seg, 0, 0, 0, DASH_TEMPLATE_NUMBER, 1, 2000);
dash_segmenter_set_init_segment(&seg, 0, 0, 0, "init-$RepresentationID$.mp4");
dash_segmenter_set_media_segment(&seg, 0, 0, 0, "$RepresentationID$/$Number$.m4s");

// 写入初始化段
uint8_t ftyp[] = { ... }, moov[] = { ... };
dash_segmenter_write_init_segment(&seg, ftyp, sizeof(ftyp), moov, sizeof(moov));

// 生成分段
dash_segmenter_start_fmp4(&seg, "1080p/1.m4s", 0);
// ... 写入 moof + mdat ...
dash_segmenter_finish_fmp4(&seg);

// 输出 MPD
dash_segmenter_write_mpd_file(&seg, "stream.mpd");
dash_segmenter_deinit(&seg);
```

### 动态直播

```c
dash_segmenter_t seg;
dash_segmenter_init(&seg, "./live", "live");

dash_segmenter_mpd_init(&seg, "urn:mpeg:dash:profile:isoff-live:2011", 1);
dash_segmenter_set_utc_timing(&seg, DASH_TIMING_NTP, "pool.ntp.org");

dash_segmenter_add_period(&seg, "p0", 0, 0);  // duration=0 表示持续

dash_segmenter_add_adaptation_set(&seg, 0, "video", DASH_CONTENT_VIDEO, NULL);
dash_segmenter_add_representation(&seg, 0, 0, "1080p", 5000000, 1920, 1080,
                                  "avc1.640028", "video/mp4");
dash_segmenter_set_template(&seg, 0, 0, 0, DASH_TEMPLATE_NUMBER, 1, 2000);
dash_segmenter_set_init_segment(&seg, 0, 0, 0, "init-$RepresentationID$.mp4");
dash_segmenter_set_media_segment(&seg, 0, 0, 0, "$RepresentationID$/$Number$.m4s");
```

### 多 Period (广告插入)

```c
dash_segmenter_add_period(&seg, "pre-roll",      0, 30000);  // 30s 广告
dash_segmenter_add_period(&seg, "main-content", 30000, 3600000);  // 60min 正片
dash_segmenter_add_period(&seg, "post-roll", 3630000, 15000);  // 15s 广告
```

## 生成的 MPD 结构示例

```xml
<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011"
     profiles="urn:mpeg:dash:profile:isoff-live:2011"
     minBufferTime="PT1.500S"
     type="static">
  <Period id="p0" start="PT0S" duration="PT60S">
    <AdaptationSet id="video" contentType="video" segmentAlignment="true">
      <Representation id="1080p" bandwidth="5000000" width="1920" height="1080"
                      codecs="avc1.640028" mimeType="video/mp4">
        <SegmentTemplate timescale="1000" initialization="init-$RepresentationID$.mp4"
                         media="$RepresentationID$/$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="0" d="2000" r="-1"/>
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
</MPD>
```
