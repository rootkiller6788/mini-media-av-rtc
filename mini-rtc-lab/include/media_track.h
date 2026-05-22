#ifndef MEDIA_TRACK_H
#define MEDIA_TRACK_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define RTP_VERSION 2
#define RTP_HEADER_MIN_SIZE 12
#define RTP_MAX_PAYLOAD 1460
#define RTP_MTU 1500
#define RTP_MAX_SEQ 65535

#define RTCP_PACKET_MIN_SIZE 8
#define RTCP_SR_PAYLOAD_SIZE 24
#define RTCP_RR_PAYLOAD_SIZE 24
#define RTCP_SDES_MIN_SIZE 4
#define RTCP_BYE_MIN_SIZE 4
#define RTCP_APP_MIN_SIZE 8

#define OPUS_SAMPLE_RATE 48000
#define OPUS_MAX_FRAME_SIZE 5760
#define OPUS_PAYLOAD_TYPE 111
#define OPUS_TOC_SILK 0
#define OPUS_TOC_CELT 1
#define OPUS_TOC_HYBRID 2

#define H264_NAL_TYPE_FU_A 28
#define H264_NAL_TYPE_STAP_A 24
#define H264_NAL_TYPE_SPS 7
#define H264_NAL_TYPE_PPS 8
#define H264_NAL_TYPE_IDR 5
#define H264_NAL_TYPE_SEI 6
#define H264_PAYLOAD_TYPE 96
#define H264_FU_HEADER_SIZE 2

#define VP8_PAYLOAD_TYPE 98
#define VP8_PAYLOAD_DESC_SIZE 1
#define VP8_KEY_FRAME 0
#define VP8_INTER_FRAME 1
#define VP8_SDESC_SIZE 2

#define RTCP_TYPE_SR 200
#define RTCP_TYPE_RR 201
#define RTCP_TYPE_SDES 202
#define RTCP_TYPE_BYE 203
#define RTCP_TYPE_APP 204
#define RTCP_TYPE_REMB 206
#define RTCP_TYPE_FIR 192
#define RTCP_TYPE_PLI 206
#define RTCP_TYPE_NACK 193
#define RTCP_TYPE_TWCC 205

typedef struct {
    unsigned version : 2;
    unsigned padding : 1;
    unsigned extension : 1;
    unsigned csrc_count : 4;
    unsigned marker : 1;
    unsigned payload_type : 7;
    uint16_t sequence_number;
    uint32_t timestamp;
    uint32_t ssrc;
    uint32_t csrc[15];
    uint16_t ext_profile;
    uint16_t ext_length;
    uint8_t ext_data[1024];
    uint8_t payload[RTP_MAX_PAYLOAD];
    size_t payload_len;
    size_t total_len;
} RtpPacket;

typedef struct {
    uint8_t version : 2;
    uint8_t padding : 1;
    uint8_t count : 5;
    uint8_t packet_type;
    uint16_t length;
    uint8_t data[1024];
    size_t data_len;
} RtcpPacket;

typedef struct {
    uint32_t ssrc;
    uint64_t ntp_timestamp;
    uint32_t rtp_timestamp;
    uint32_t packet_count;
    uint32_t octet_count;
} RtcpSenderReport;

typedef struct {
    uint32_t ssrc;
    uint8_t fraction_lost;
    int32_t cumulative_lost;
    uint32_t extended_highest_seq;
    uint32_t jitter;
    uint32_t last_sr_timestamp;
    uint32_t delay_since_last_sr;
} RtcpReportBlock;

typedef struct {
    uint32_t ssrc;
    RtcpReportBlock blocks[16];
    int block_count;
} RtcpReceiverReport;

typedef struct {
    uint32_t ssrc;
    uint64_t bitrate_bps;
    uint32_t ssrcs[64];
    int ssrc_count;
    char unique_id[16];
} RtcpRemb;

typedef struct {
    uint32_t ssrc;
    uint32_t media_ssrc;
    uint8_t seq;
} RtcpFir;

typedef struct {
    uint32_t ssrc;
    uint32_t media_ssrc;
} RtcpPli;

typedef struct {
    uint16_t base_seq;
    uint16_t packet_status_count;
    uint32_t ref_time;
    uint16_t feedback_seq;
    uint16_t packet_chunks[128];
    int chunk_count;
    uint8_t recv_deltas[1024];
    int delta_count;
} RtcpTwcc;

typedef enum {
    OPUS_FRAME_NARROWBAND = 0,
    OPUS_FRAME_MEDIUMBAND = 1,
    OPUS_FRAME_WIDEBAND = 2,
    OPUS_FRAME_SUPERWIDEBAND = 3,
    OPUS_FRAME_FULLBAND = 4
} OpusBandwidth;

typedef struct {
    int channels;
    int sample_rate;
    uint8_t toc;
    int frame_count;
    OpusBandwidth bandwidth;
    bool stereo;
    bool vad;
    bool fec;
    int duration_ms;
    uint8_t data[OPUS_MAX_FRAME_SIZE];
    size_t data_len;
} OpusFrame;

typedef struct {
    uint8_t nal_type : 5;
    uint8_t nri : 2;
    uint8_t forbidden : 1;
} H264NalHeader;

typedef struct {
    bool start;
    bool end;
    uint8_t nal_type;
    uint8_t nri;
    uint8_t data[RTP_MAX_PAYLOAD];
    size_t data_len;
} H264FuA;

typedef struct {
    bool key_frame;
    int picture_id;
    int tl0_pic_idx;
    bool tid_present;
    int tid;
    int key_idx;
    int layer_sync;
    bool is_golden;
    bool is_altref;
    uint8_t data[RTP_MAX_PAYLOAD];
    size_t data_len;
} Vp8Payload;

typedef struct {
    uint32_t ssrc;
    int payload_type;
    int clock_rate;
    int sequence_number;
    uint32_t timestamp;
    int packet_count;
    int octet_count;
    uint64_t ntp_base;
    uint32_t rtp_base;
} TrackContext;

int rtpBuildHeader(RtpPacket *pkt, int payload_type, uint16_t seq, uint32_t ts, uint32_t ssrc);
int rtpSerializeHeader(RtpPacket *pkt, uint8_t *buf, size_t bufsz, size_t *written);
int rtpParseHeader(RtpPacket *pkt, const uint8_t *buf, size_t len);
int rtpSetMarker(RtpPacket *pkt, bool marker);
int rtpSetExtension(RtpPacket *pkt, uint16_t profile, const uint8_t *data, size_t len);

int opusParseToc(uint8_t toc, OpusFrame *frame);
int opusPackFrame(const OpusFrame *frame, uint8_t *buf, size_t bufsz, size_t *written);
int opusUnpackFrame(const uint8_t *buf, size_t len, OpusFrame *frame);

int h264BuildFuA(const uint8_t *nalu, size_t nalu_len, int mtu, H264FuA *fus, int *fu_count);
int h264CombineFuA(const H264FuA *fus, int fu_count, uint8_t *nalu, size_t *nalu_len);
int h264GetNalType(const uint8_t *nalu, size_t len, uint8_t *nal_type);
int h264IsKeyFrame(const uint8_t *nalu, size_t len);
int h264ParseStapA(const uint8_t *buf, size_t len, uint8_t **nalus, size_t *nalus_len, int *nalus_count);

int vp8ParsePayloadDescriptor(const uint8_t *buf, size_t len, Vp8Payload *vp8);
int vp8BuildPayloadDescriptor(const Vp8Payload *vp8, uint8_t *buf, size_t bufsz, size_t *written);
int vp8IsKeyFrame(const uint8_t *buf, size_t len);

int rtcpBuildSenderReport(uint8_t *buf, size_t bufsz, const RtcpSenderReport *sr, size_t *written);
int rtcpBuildReceiverReport(uint8_t *buf, size_t bufsz, const RtcpReceiverReport *rr, size_t *written);
int rtcpParseSenderReport(const uint8_t *buf, size_t len, RtcpSenderReport *sr);
int rtcpParseReceiverReport(const uint8_t *buf, size_t len, RtcpReceiverReport *rr);
int rtcpBuildSdesCname(uint8_t *buf, size_t bufsz, uint32_t ssrc, const char *cname, size_t *written);
int rtcpBuildBye(uint8_t *buf, size_t bufsz, uint32_t *ssrcs, int count, const char *reason, size_t *written);

int rtcpBuildRemb(uint8_t *buf, size_t bufsz, const RtcpRemb *remb, size_t *written);
int rtcpParseRemb(const uint8_t *buf, size_t len, RtcpRemb *remb);
int rtcpBuildPli(uint8_t *buf, size_t bufsz, uint32_t ssrc, uint32_t media_ssrc, size_t *written);
int rtcpBuildFir(uint8_t *buf, size_t bufsz, const RtcpFir *fir, size_t *written);
int rtcpParseFir(const uint8_t *buf, size_t len, RtcpFir *fir);
int rtcpBuildTwccFeedback(uint8_t *buf, size_t bufsz, const RtcpTwcc *twcc, size_t *written);
int rtcpParseTwccFeedback(const uint8_t *buf, size_t len, RtcpTwcc *twcc);

void trackInit(TrackContext *ctx, uint32_t ssrc, int pt, int clock_rate);
int trackUpdateSeq(TrackContext *ctx);
uint32_t trackGetTimestamp(TrackContext *ctx, int sample_count);
uint64_t trackNtpNow(void);

#endif
