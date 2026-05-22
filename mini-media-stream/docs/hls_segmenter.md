# HLS Segmenter 文档 (HLS 分段器)

## 概述

`hls_segmenter` 模块实现了 HTTP Live Streaming (HLS) 分段器，支持生成 M3U8 playlist 文件与 MPEG-TS 分段文件。

## API 参考

### 初始化与销毁

```c
int  hls_segmenter_init(hls_segmenter_t *seg, const char *output_dir, const char *prefix);
void hls_segmenter_deinit(hls_segmenter_t *seg);
```

初始化分段器，指定输出目录与分段文件名前缀。

### Playlist 配置

```c
int hls_segmenter_set_type(hls_segmenter_t *seg, hls_playlist_type_t type);
int hls_segmenter_set_target_duration(hls_segmenter_t *seg, uint32_t duration);
int hls_segmenter_set_window_size(hls_segmenter_t *seg, uint32_t size);
int hls_segmenter_set_version(hls_segmenter_t *seg, uint32_t version);
```

- `type`: `HLS_PLAYLIST_VOD`, `HLS_PLAYLIST_LIVE`, `HLS_PLAYLIST_EVENT`
- `duration`: 目标分段时长 (秒), 对应 `EXT-X-TARGETDURATION`
- `size`: 直播滑动窗口大小, 仅对 LIVE 模式有效

### 分段操作

```c
int hls_segmenter_start_segment(hls_segmenter_t *seg, const char *filename);
int hls_segmenter_write_packet(hls_segmenter_t *seg, const uint8_t *data, size_t len);
int hls_segmenter_finish_segment(hls_segmenter_t *seg, double duration);
```

开始/写入/完成一个 MPEG-TS 分段。`write_packet` 写入 TS packet 数据。

### 高级标签

```c
int hls_segmenter_add_byte_range(hls_segmenter_t *seg, uint64_t offset, uint64_t length);
int hls_segmenter_mark_discontinuity(hls_segmenter_t *seg);
int hls_segmenter_set_program_date_time(hls_segmenter_t *seg, int64_t time_ms);
int hls_segmenter_end_playlist(hls_segmenter_t *seg);
```

- `add_byte_range`: 添加 `EXT-X-BYTERANGE` 标签
- `mark_discontinuity`: 标记 `EXT-X-DISCONTINUITY`
- `set_program_date_time`: 设置 `EXT-X-PROGRAM-DATE-TIME`
- `end_playlist`: 标记 playlist 结束 (VOD/EVENT)

### 加密密钥

```c
int hls_segmenter_add_key(hls_segmenter_t *seg, const hls_key_t *key);
int hls_segmenter_rotate_key(hls_segmenter_t *seg, const hls_key_t *new_key);
```

设置 `EXT-X-KEY` 标签，支持 AES-128 与 SAMPLE-AES。

### Playlist 生成

```c
int hls_segmenter_generate_playlist(hls_segmenter_t *seg, char *buffer, size_t buf_size);
int hls_segmenter_write_playlist_file(hls_segmenter_t *seg, const char *filename);
```

### 多码率 Variant Playlist

```c
int hls_variant_init(hls_variant_playlist_t *vp);
int hls_variant_add_stream(hls_variant_playlist_t *vp, const char *uri,
                           uint32_t bandwidth, uint32_t width, uint32_t height,
                           const char *codecs);
int hls_variant_generate_m3u8(hls_variant_playlist_t *vp, char *buffer, size_t buf_size);
```

## 数据结构

### hls_segment_t
| 字段 | 类型 | 说明 |
|------|------|------|
| `url` | `char[]` | 分段 URL |
| `duration` | `double` | 分段时长 (秒) |
| `byte_offset` | `uint64_t` | BYTERANGE 偏移 |
| `byte_length` | `uint64_t` | BYTERANGE 长度 |
| `sequence` | `uint64_t` | 媒体序号 |
| `discontinuity` | `uint8_t` | 断点标记 |
| `program_date_time_ms` | `int64_t` | 节目时间 (ms) |
| `key` | `hls_key_t` | AES 密钥信息 |

### hls_playlist_t
| 字段 | 类型 | 说明 |
|------|------|------|
| `segments[]` | `hls_segment_t[1024]` | 分段数组 |
| `segment_count` | `uint32_t` | 当前分段数 |
| `media_sequence` | `uint32_t` | 媒体序列号 |
| `target_duration` | `uint32_t` | 目标时长 |
| `version` | `uint32_t` | HLS 协议版本 |
| `type` | `enum` | VOD/LIVE/EVENT |
| `window_size` | `uint32_t` | 滑动窗口大小 |
| `ended` | `uint8_t` | 是否已结束 |

## 使用示例

```c
hls_segmenter_t seg;
hls_segmenter_init(&seg, "./output", "seg");

// VOD 模式
hls_segmenter_set_type(&seg, HLS_PLAYLIST_VOD);
hls_segmenter_set_target_duration(&seg, 6);

// 创建分段
for (int i = 0; i < 5; i++) {
    char fn[32];
    snprintf(fn, sizeof(fn), "seg_%d.ts", i);
    hls_segmenter_start_segment(&seg, fn);
    uint8_t ts_data[188] = { /* TS packet */ };
    hls_segmenter_write_packet(&seg, ts_data, 188);
    hls_segmenter_finish_segment(&seg, 5.0);
}

hls_segmenter_end_playlist(&seg);
hls_segmenter_write_playlist_file(&seg, "output.m3u8");
hls_segmenter_deinit(&seg);
```

## 直播模式

```c
hls_segmenter_t seg;
hls_segmenter_init(&seg, "./live", "live");
hls_segmenter_set_type(&seg, HLS_PLAYLIST_LIVE);
hls_segmenter_set_window_size(&seg, 3);  // 保留最近 3 个分段
hls_segmenter_set_target_duration(&seg, 2);

// 滑动窗口: 旧分段自动从 playlist 中移除, media_sequence 递增
```
