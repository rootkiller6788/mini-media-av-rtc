mini-rtc-lab Design Notes
===========================

## Architecture

```
+------------------+     +------------------+     +------------------+
|   Signaling      |     |  ICE / STUN /    |     |  DTLS-SRTP       |
|   (SDP Parser)   | --> |  TURN Transport  | --> |  Security Layer  |
+------------------+     +------------------+     +------------------+
        |                         |                         |
        v                         v                         v
+------------------+     +------------------+     +------------------+
|  Media Track     |     |  SFU / MCU       |     |  Bandwidth       |
|  (RTP/RTCP)      | <-> |  (Routing)       | <-> |  Estimation      |
+------------------+     +------------------+     +------------------+
```

## Design Principles

### 1. C99 Compliance
All code targets ISO C99 standard. No platform-specific extensions.
Uses only standard library (libc). No dynamic memory allocation for
core paths (stack-based structs with fixed-size buffers).

### 2. Simulation-First Approach
This library simulates the protocol semantics rather than implementing
actual network I/O. This makes it ideal for:
- Unit testing WebRTC protocol logic
- Understanding the protocol flow without network dependencies
- Embedded environments where a full TLS stack is impractical

### 3. Safe Buffer Handling
All serialization functions take explicit buffer sizes.
Returns -1 on overflow.

## Key Algorithms

### ICE Priority Computation
```
type_pref:  host=126, srflx=100, relay=0
local_pref: 0-65535 (higher = preferred)
component_id: 1-RTP, 2-RTCP

priority = (type_pref << 24) | (local_pref << 8) | (256 - component_id)
```

For pair priority, the controlling agent's formula:
```
G = controlling_agent_candidate_priority
D = controlled_agent_candidate_priority
pair_priority = (G << 32) | D
```

### STUN Fingerprint (CRC-32)
```
fingerprint = crc32(message_up_to_this_point) ^ 0x5354554e
```
Used to detect STUN messages multiplexed on the same port as RTP.

### SRTP Key Derivation (RFC 5764 ext_id 7)
From TLS exporter "EXTRACTOR-dtls_srtp" with master_key + master_salt:
```
client_write_key  = PRF(master_key, "client write key",  random)
server_write_key  = PRF(master_key, "server write key",  random)
client_write_salt = PRF(master_salt, "client write salt", random) [14 bytes]
server_write_salt = PRF(master_salt, "server write salt", random) [14 bytes]
```

### Bandwidth Estimation (GCC-like)
```
if loss_fraction > 0.10:
    target *= 0.85  (multiplicative decrease)
elif loss_fraction < 0.02:
    target *= 1.05  (additive increase)
    target = min(target, max_limit)

REMB arrival:
    target = alpha * target + (1-alpha) * remb_rate
```

### Opus TOC Byte Parsing
```
TOC byte layout:
  [7:3] config (0-31)
  [2]   stereo flag
  [1:0] code count

config 0-11   : Silk NB  10ms
config 12-15  : Hybrid WB 20ms
config 16-19  : CELT SWB 20ms
config 20-31  : CELT FB  20ms
```

### H.264 FU-A Fragmentation
```
NAL Unit Header (1 byte):
  [7]   forbidden_zero_bit (0)
  [6:5] nal_ref_idc (priority)
  [4:0] nal_unit_type

FU-A Header (1 byte):
  [7]   Start bit (first fragment)
  [6]   End bit (last fragment)
  [5]   Reserved (0)
  [4:0] Original NAL unit type

FU-A Payload: NAL unit data without the original header byte
```

### VP8 Payload Descriptor
```
[0]:
  [7] extended control bits present
  [6] non-reference frame
  [5] start of VP8 partition
  [4] TL0PICIDX present
  [3] TID present
  [2] KEYIDX present
  [1] PictureID present (LSB of 7 or 15 bits)
  [0] temporal base layer sync
```

## Simulcast Layer Selection Strategy

1. **Active speaker** (highest audio level) -> SFU_LAYER_HIGH (1080p, full bitrate)
2. **Recent speakers** (top 3 by activity) -> SFU_LAYER_MEDIUM (480p)
3. **Passive listeners** -> SFU_LAYER_LOW (180p)
4. **Muted/video-off** -> No video forwarded

Bandwidth distribution:
- 60% to active speaker
- 30% to recent speakers
- 10% to passive

## Limitations of Simulation

| Component | Simulated | Real Implementation |
|-----------|-----------|---------------------|
| DTLS handshake | Message sequence, key derivation | Requires OpenSSL/mbedTLS |
| SRTP encryption | XOR-stream cipher simulation | Real AES-CTR |
| HMAC | Simplified hash | Real HMAC-SHA1 |
| Certificate | Random bytes, fake fingerprint | Real X.509 certificates |
| ICE connectivity | Logic flow + state machine | Real socket I/O |
| Video encoding | NAL unit parsing only | Requires libvpx/x264 |
| Audio encoding | TOC parsing only | Requires libopus |

## File Sizes Quick Reference

| File | Lines | Purpose |
|------|-------|---------|
| sdp_signaling.h | 160 | SDP parsing and generation |
| ice_stun_turn.h | +200 | ICE, STUN, TURN headers |
| dtls_srtp.h | +160 | DTLS-SRTP handshake, SRTP |
| media_track.h | 180 | RTP, RTCP, codecs |
| sfu_mcu.h | +130 | SFU routing, MCU, BWE |
| sdp_signaling.c | 280 | SDP implementation |
| ice_stun_turn.c | +300 | STUN/TURN/ICE implementation |
| dtls_srtp.c | 280 | DTLS-SRTP implementation |
| media_track.c | 350 | RTP/RTCP/codec implementation |
| sfu_mcu.c | 290 | SFU/MCU/BWE implementation |
