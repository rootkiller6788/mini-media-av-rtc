#ifndef SDP_SIGNALING_H
#define SDP_SIGNALING_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define SDP_MAX_LINE 2048
#define SDP_MAX_CODECS 32
#define SDP_MAX_CANDIDATES 16
#define SDP_MAX_EXTMAPS 16
#define SDP_MAX_MEDIA 8
#define SDP_FINGERPRINT_LEN 128
#define SDP_ICE_UFRAG_LEN 64
#define SDP_ICE_PWD_LEN 128

typedef enum {
    SDP_MEDIA_AUDIO = 0,
    SDP_MEDIA_VIDEO = 1,
    SDP_MEDIA_APPLICATION = 2
} SdpMediaType;

typedef enum {
    SDP_CODEC_OPUS = 0,
    SDP_CODEC_H264 = 1,
    SDP_CODEC_VP8 = 2,
    SDP_CODEC_VP9 = 3,
    SDP_CODEC_H265 = 4,
    SDP_CODEC_AV1 = 5,
    SDP_CODEC_PCMU = 6,
    SDP_CODEC_PCMA = 7,
    SDP_CODEC_G722 = 8,
    SDP_CODEC_RED = 9,
    SDP_CODEC_ULPFEC = 10,
    SDP_CODEC_RTX = 11
} SdpCodecId;

typedef struct {
    SdpCodecId id;
    int payload_type;
    char encoding[32];
    int clock_rate;
    int channels;
    char fmtp[256];
} SdpCodec;

typedef struct {
    char foundation[16];
    int component_id;
    char transport[8];
    uint32_t priority;
    char address[64];
    int port;
    char type[16];
    char related_address[64];
    int related_port;
    bool tcp_active;
} IceCandidate;

typedef struct {
    char fingerprint[SDP_FINGERPRINT_LEN];
    char hash_algo[16];
    char setup[16];
    char tls_id[64];
} DtlsFingerprint;

typedef struct {
    int id;
    char uri[256];
    SdpMediaType direction;
} RtpExtmap;

typedef struct {
    SdpMediaType type;
    int port;
    char proto[16];
    SdpCodec codecs[SDP_MAX_CODECS];
    int codec_count;
    IceCandidate candidates[SDP_MAX_CANDIDATES];
    int candidate_count;
    RtpExtmap extmaps[SDP_MAX_EXTMAPS];
    int extmap_count;
    bool bundle;
    bool rtcp_mux;
    char mid[32];
    char ice_ufrag[SDP_ICE_UFRAG_LEN];
    char ice_pwd[SDP_ICE_PWD_LEN];
    DtlsFingerprint dtls;
    uint32_t ssrc;
    char cname[64];
    char msid[128];
    char track_id[64];
} MediaDescription;

typedef struct {
    uint32_t session_id;
    uint32_t session_version;
    char origin_address[64];
    char session_name[128];
    MediaDescription media[SDP_MAX_MEDIA];
    int media_count;
    char group_bundle[512];
    bool bundle_enabled;
    bool rtcp_mux_enabled;
    char connection_addr[64];
} SessionDescription;

typedef struct {
    int (*on_sdp)(void *ctx, const SessionDescription *sdp);
    int (*on_ice_candidate)(void *ctx, const char *mid, int mline, const IceCandidate *cand);
    int (*on_dtls_fingerprint)(void *ctx, const char *mid, const DtlsFingerprint *fp);
    int (*on_connected)(void *ctx);
    int (*on_disconnected)(void *ctx);
    int (*on_error)(void *ctx, int code, const char *msg);
} SignalingCallbacks;

typedef struct {
    char url[256];
    int state;
    void *internal;
    SignalingCallbacks cb;
    void *cb_ctx;
} SignalingChannel;

int parseSessionDescription(const char *sdp_text, SessionDescription *out);
int parseMediaDescription(const char *m_line, MediaDescription *out);
int parseIceCandidate(const char *candidate_line, IceCandidate *out);
int parseDtlsFingerprint(const char *fp_line, DtlsFingerprint *out);
int parseRtpExtmap(const char *ext_line, RtpExtmap *out);

int generateSessionDescription(const SessionDescription *sdp, char *buf, size_t bufsz);
int generateMediaDescription(const MediaDescription *md, char *buf, size_t bufsz);
int generateIceCandidate(const IceCandidate *cand, int mline, char *buf, size_t bufsz);

int generateAnswer(const SessionDescription *offer, SessionDescription *answer);

uint32_t computeIceCandidatePriority(int type_pref, int local_pref, int component_id);

void sdpSetBundle(SessionDescription *sdp, bool enabled);
void sdpSetRtcpMux(MediaDescription *md, bool enabled);
int sdpAddCodec(MediaDescription *md, SdpCodecId id, int pt, const char *name, int clock);
int sdpAddCandidate(MediaDescription *md, const IceCandidate *cand);
int sdpAddExtmap(MediaDescription *md, int id, const char *uri);
int sdpSetSsrc(MediaDescription *md, uint32_t ssrc, const char *cname, const char *msid);

int signalingChannelInit(SignalingChannel *ch, const char *url);
int signalingChannelConnect(SignalingChannel *ch);
int signalingChannelSendSdp(SignalingChannel *ch, const SessionDescription *sdp);
int signalingChannelSendCandidate(SignalingChannel *ch, const char *mid, int mline, const IceCandidate *cand);
int signalingChannelPoll(SignalingChannel *ch, int timeout_ms);
int signalingChannelClose(SignalingChannel *ch);

#endif
