# mini-rtc-lab — 实时通信实验室 (C 语言实现)

纯 C99 实现的 WebRTC 协议栈教学/仿真库。零外部依赖，仅需 libc。

## 模块

| 模块 | 头文件 | 源码 | 说明 |
|------|--------|------|------|
| SDP 信令 | `sdp_signaling.h` | `sdp_signaling.c` | Offer/Answer、ICE candidate、fingerprint 解析与生成 |
| ICE/STUN/TURN | `ice_stun_turn.h` | `ice_stun_turn.c` | 候选收集、连通性检查、STUN 绑定、TURN 中继 |
| DTLS-SRTP | `dtls_srtp.h` | `dtls_srtp.c` | DTLS 握手仿真、SRTP 密钥派生、加解密 |
| 媒体轨道 | `media_track.h` | `media_track.c` | RTP 头、Opus/H.264/VP8、RTCP 报告 (SR/RR/REMB/PLI/FIR/TWCC) |
| SFU/MCU | `sfu_mcu.h` | `sfu_mcu.c` | 选择性转发、Simulcast 层选择、MCU 合成、带宽估计 |

## 快速开始

```bash
cd mini-rtc-lab
make all

./example_webrtc_call
./example_sfu_call
./example_ice_turn

./demo_webrtc_endpoint
./demo_sfu_server
```

## 项目结构

```
mini-rtc-lab/
├── sdp_signaling.h     # SDP 解析/生成
├── ice_stun_turn.h     # ICE/STUN/TURN 定义
├── dtls_srtp.h         # DTLS-SRTP 定义
├── media_track.h       # RTP/RTCP/Codec 定义
├── sfu_mcu.h           # SFU/MCU/BWE 定义
├── sdp_signaling.c     # SDP 实现
├── ice_stun_turn.c     # ICE/STUN/TURN 实现
├── dtls_srtp.c         # DTLS-SRTP 实现
├── media_track.c       # 媒体实现
├── sfu_mcu.c           # SFU/MCU 实现
├── example_webrtc_call.c   # WebRTC 通话示例
├── example_sfu_call.c      # SFU 会议示例
├── example_ice_turn.c      # ICE/STUN/TURN 示例
├── demo_webrtc_endpoint.c  # 端点到端点演示
├── demo_sfu_server.c       # SFU 服务器演示
├── API_REFERENCE.md        # API 参考
├── DESIGN_NOTES.md         # 设计笔记
├── Makefile
└── README.md               # 本文件
```

## 示例: SDP Offer/Answer

```c
#include "sdp_signaling.h"

SessionDescription offer;
memset(&offer, 0, sizeof(offer));
offer.session_id = 1234567890;

MediaDescription *audio = &offer.media[offer.media_count++];
audio->type = SDP_MEDIA_AUDIO;
audio->port = 9;
strncpy(audio->proto, "UDP/TLS/RTP/SAVPF", sizeof(audio->proto) - 1);
sdpAddCodec(audio, SDP_CODEC_OPUS, 111, "opus", 48000);

char buf[4096];
generateSessionDescription(&offer, buf, sizeof(buf));
printf("%s\n", buf);

SessionDescription answer;
generateAnswer(&offer, &answer);
```

## 示例: ICE 连通性检查

```c
#include "ice_stun_turn.h"

IceSession ice;
iceSessionCreate(&ice, true);  // controlling role

IceCandidateLocal host = {
    .foundation = "1",
    .component_id = 1,
    .type = ICE_CAND_TYPE_HOST,
    .address = "192.168.1.100",
    .port = 50000,
    .priority = calcIcePriority(ICE_CAND_TYPE_HOST, 65535, 1),
};
iceSessionAddLocalCandidate(&ice, &host);
iceSessionBuildCheckList(&ice);
iceSessionStartChecks(&ice);
```

## 示例: RTP 包构建

```c
#include "media_track.h"

RtpPacket rtp;
rtpBuildHeader(&rtp, VP8_PAYLOAD_TYPE, 100, 48000, 2001);
rtpSetMarker(&rtp, true);
memcpy(rtp.payload, vp8_data, len);
rtp.payload_len = len;

uint8_t wire[2048]; size_t wire_len;
rtpSerializeHeader(&rtp, wire, sizeof(wire), &wire_len);
```

## 许可证

MIT
