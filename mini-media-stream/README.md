# mini-media-stream — 流媒体 (C 语言实现)

mini-media-stream 是一个用纯 C99 编写的流媒体库，提供 HLS/DASH 分段器、自适应码率 (ABR) 引擎、CDN 边缘节点模型与 DRM 数字版权管理系统。

## 目录结构

```
mini-media-stream/
├── README.md
├── Makefile
├── include/
│   ├── hls_segmenter.h    — HLS 分段器: M3U8 playlist, TS segment, variant playlist
│   ├── dash_segmenter.h   — DASH 分段器: MPD manifest, fMP4 segment, SegmentTemplate
│   ├── abr_engine.h       — ABR 引擎: BOLA, 吞吐量估算, 规则切换, CDN 选择
│   ├── cdn_edge.h         — CDN 模型: 源站/边缘节点, 缓存策略, DNS 路由
│   └── drm_system.h       — DRM 系统: AES-128, CENC, Widevine/PlayReady/FairPlay
├── src/
│   ├── hls_segmenter.c    — HLS 分段器实现 (190+ 行)
│   ├── dash_segmenter.c   — DASH 分段器实现 (310+ 行)
│   ├── abr_engine.c       — ABR 引擎实现 (290+ 行)
│   ├── cdn_edge.c         — CDN 边缘节点实现 (340+ 行)
│   └── drm_system.c       — DRM 系统实现 (360+ 行)
├── examples/
│   ├── example_hls.c      — HLS 示例: VOD playlist, variant playlist, key rotation
│   ├── example_dash.c     — DASH 示例: MPD generation, fMP4, multi-representation
│   └── example_abr.c      — ABR 示例: BOLA/Throughput/Rule-based/Hybrid, CDN switch
├── demos/
│   ├── demo_media_stream.c — 完整演示: HLS live + DASH+ABR + CDN (310+ 行)
│   └── demo_drm_stream.c   — DRM 演示: 密钥管理, AES-128/CENC 加解密, PSSH, license (310+ 行)
└── docs/
    ├── hls_segmenter.md    — HLS 分段器文档
    └── dash_segmenter.md   — DASH 分段器文档
```

## 功能特性

### HLS 分段器 (HLS Segmenter)
- M3U8 playlist 生成: `EXTM3U`, `EXT-X-VERSION`, `EXT-X-TARGETDURATION`, `EXTINF`, `EXT-X-BYTERANGE`, `EXT-X-KEY`, `EXT-X-ENDLIST`
- MPEG-TS 分段 (`.ts` 文件写入)
- 直播滑动窗口 (sliding window)
- Event playlist 与 VOD playlist
- 多码率 variant playlist (`#EXT-X-STREAM-INF`)
- 断点标记 `#EXT-X-DISCONTINUITY`
- 节目时间戳 `#EXT-X-PROGRAM-DATE-TIME`
- AES-128 密钥管理

### DASH 分段器 (DASH Segmenter)
- MPD manifest 生成: `Period`, `AdaptationSet`, `Representation`
- fMP4 分段 (`moof` + `mdat`)
- 初始化段 (init segment)
- `SegmentTemplate`: `$Number$` 与 `$Time$` 模板
- `SegmentTimeline` 时间线
- 多码率带宽切换
- `UTCTiming` (NTP/HTTP/GPS)
- 多 Period 支持 (广告插入)

### ABR 引擎 (ABR Engine)
- **BOLA** (Buffer Occupancy based Lyapunov Algorithm)
- **吞吐量估算** (Throughput-based) — 基于分段下载时间
- **规则切换** (Rule-based) — buffer < X 降码率, buffer > Y 升码率
- **混合模式** (Hybrid) — BOLA + Throughput 综合判定
- 质量振荡防护 (oscillation prevention)
- 分段下载调度
- 多 CDN 选择与故障切换

### CDN 边缘节点 (CDN Edge)
- 源站 (Origin) → 中间层 (Mid-Tier) → 边缘节点 (Edge) 三级拓扑
- 缓存策略: TTL 过期, LRU / LFU 淘汰
- 缓存命中/未命中统计
- 预取预热 (pre-warming)
- 基于延迟/权重/轮询的边缘节点选择
- DNS 解析与 302 重定向路由

### DRM 数字版权 (DRM System)
- AES-128 CBC (HLS)
- CENC / CTR 模式 (DASH — Common Encryption)
- Widevine / PlayReady / FairPlay PSSH box 生成
- PlayReady PRO header
- 密钥系统: License Server, Key Request/Response
- 密钥轮换 (key rotation)
- 密钥层级: Content Key → License
- 子样本加密 (subsample encryption, 明文+密文混合)
- IV 推导 (constant IV + sample index)
- HTTPS License 交付

## 构建与运行

### 构建

```bash
make
```

### 运行示例

```bash
make examples
./build/example_hls
./build/example_dash
./build/example_abr
```

### 运行演示

```bash
make demos
./build/demo_media_stream
./build/demo_drm_stream
```

### 清理

```bash
make clean
```

## 头文件 API 概览

| 头文件 | 核心 API |
|--------|----------|
| `hls_segmenter.h` | `hls_segmenter_init()`, `hls_segmenter_generate_playlist()`, `hls_variant_add_stream()` |
| `dash_segmenter.h` | `dash_segmenter_mpd_init()`, `dash_segmenter_generate_mpd()`, `dash_segmenter_add_timeline_entry()` |
| `abr_engine.h` | `abr_engine_decide()`, `abr_engine_decide_bola()`, `abr_engine_select_cdn()` |
| `cdn_edge.h` | `cdn_add_edge()`, `cdn_cache_get()`, `cdn_select_edge()`, `cdn_simulate_request()` |
| `drm_system.h` | `drm_add_key()`, `drm_encrypt_sample_cenc()`, `drm_build_pssh_box()`, `drm_request_license()` |

## 编译要求

- C99 编译器 (GCC, Clang, MSVC)
- 标准 C 库
- 无外部依赖

## 许可证

MIT License
