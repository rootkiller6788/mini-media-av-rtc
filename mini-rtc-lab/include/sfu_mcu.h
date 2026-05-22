#ifndef SFU_MCU_H
#define SFU_MCU_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "media_track.h"

#define SFU_MAX_PARTICIPANTS 32
#define SFU_MAX_LAYERS 4
#define SFU_MAX_TRACKS 128
#define MCU_MAX_INPUTS 16
#define MCU_COMPOSITE_WIDTH 1920
#define MCU_COMPOSITE_HEIGHT 1080

typedef enum {
    SFU_LAYER_HIGH = 0,
    SFU_LAYER_MEDIUM = 1,
    SFU_LAYER_LOW = 2,
    SFU_LAYER_BASE = 3
} SfuLayerId;

typedef enum {
    SFU_SCALABILITY_TEMPORAL = 0,
    SFU_SCALABILITY_SPATIAL = 1,
    SFU_SCALABILITY_QUALITY = 2
} SfuScalabilityType;

typedef struct {
    SfuLayerId id;
    char rid[16];
    char mid[32];
    int width;
    int height;
    int frame_rate;
    uint32_t target_bitrate_bps;
    uint32_t ssrc;
    int temporal_layers;
    bool active;
    uint32_t frame_count;
    uint64_t octet_count;
    uint32_t last_seq;
    uint32_t last_ts;
} SfuSimulcastLayer;

typedef struct {
    char track_id[64];
    char mid[32];
    int media_type;
    SfuSimulcastLayer layers[SFU_MAX_LAYERS];
    int layer_count;
    SfuLayerId current_layer;
    uint32_t primary_ssrc;
    short lost;
    uint32_t total_lost;
    uint32_t jitter;
    float fraction_lost;
    uint64_t bitrate_bps;
    bool paused;
    bool is_active_speaker;
} SfuTrack;

typedef struct {
    char participant_id[64];
    SfuTrack tracks[SFU_MAX_TRACKS];
    int track_count;
    uint32_t audio_ssrc;
    uint32_t video_ssrc;
    int audio_level;
    bool muted;
    bool video_off;
    bool is_active_speaker;
    uint64_t last_activity;
} SfuParticipant;

typedef struct {
    SfuParticipant participants[SFU_MAX_PARTICIPANTS];
    int participant_count;
    uint32_t bandwidth_limit_bps;
    int active_speaker_count;
    int tile_width;
    int tile_height;
    uint32_t global_ssrc_base;
} SfuContext;

typedef struct {
    int input_count;
    int output_width;
    int output_height;
    uint32_t output_ssrc;
    int payload_type;
    int (*decode)(int input_id, const uint8_t *data, size_t len, uint8_t *yuv, size_t *yuv_len);
    int (*compose)(uint8_t **inputs_yuv, int count, int *positions, int *sizes, uint8_t *out, size_t *outlen);
    int (*encode)(const uint8_t *yuv, size_t yuv_len, uint8_t *encoded, size_t *enc_len, bool keyframe);
} McuContext;

typedef struct {
    uint64_t timestamp_us;
    uint32_t ssrc;
    int seq;
    uint16_t size_bytes;
    bool received;
    int64_t arrival_us;
    int64_t delta_us;
    int64_t inter_arrival_us;
} BwePacketInfo;

typedef struct {
    uint32_t target_bitrate_bps;
    uint32_t min_bitrate_bps;
    uint32_t max_bitrate_bps;
    float loss_fraction;
    int64_t rtt_us;
    uint8_t state;
    BwePacketInfo packets[256];
    int packet_count;
    int packet_index;
    uint64_t last_update_us;
    uint64_t last_feedback_us;
    float kalman_gain;
    float estimate_noise;
} BweState;

int sfuInit(SfuContext *sfu, uint32_t bandwidth_limit);
int sfuAddParticipant(SfuContext *sfu, const char *id);
int sfuRemoveParticipant(SfuContext *sfu, const char *id);
SfuParticipant *sfuFindParticipant(SfuContext *sfu, const char *id);

int sfuAddTrack(SfuContext *sfu, const char *participant_id, const char *track_id, int media_type);
int sfuAddSimulcastLayer(SfuContext *sfu, const char *participant_id, const char *track_id, SfuLayerId layer_id, const char *rid, uint32_t ssrc, int width, int height, uint32_t bitrate);

int sfuSelectActiveSpeaker(SfuContext *sfu);
int sfuSelectLayer(SfuContext *sfu, const char *participant_id, const char *track_id, SfuLayerId layer);
SfuLayerId sfuGetCurrentLayer(SfuContext *sfu, const char *participant_id, const char *track_id);

int sfuForwardPacket(SfuContext *sfu, const RtpPacket *pkt, const char *from_participant, const char *to_participant, RtpPacket *out);
int sfuRewriteSsrc(RtpPacket *pkt, uint32_t new_ssrc);
int sfuRewriteSequence(RtpPacket *pkt, uint16_t new_seq);
int sfuRewriteTimestamp(RtpPacket *pkt, uint32_t new_ts);
int sfuDropPaddingOnly(RtpPacket *pkt);

int sfuHandleRemb(SfuContext *sfu, const RtcpRemb *remb);
int sfuDistributeRemb(SfuContext *sfu, const char *target_participant, uint64_t bitrate_bps);
int sfuApplyBandwidthLimit(SfuContext *sfu, uint64_t bitrate_bps);

int sfuSetActiveSpeaker(SfuContext *sfu, const char *participant_id, bool active);
int sfuUpdateAudioLevel(SfuContext *sfu, const char *participant_id, int level);
int sfuGetStats(SfuContext *sfu, const char *participant_id, const char *track_id, uint32_t *bitrate, float *loss);

int mcuInit(McuContext *mcu, int width, int height, uint32_t ssrc, int pt);
int mcuAddInput(McuContext *mcu, int width, int height);
int mcuSetLayout(McuContext *mcu, int *positions, int count);
int mcuProcessFrame(McuContext *mcu, int input_id, const uint8_t *encoded, size_t len, uint8_t *output, size_t *outlen, bool *keyframe);
int mcuRequestKeyFrame(McuContext *mcu, int input_id);
int mcuSwitchLayout(McuContext *mcu, int layout_type);
int mcuClose(McuContext *mcu);

int bweInit(BweState *bwe, uint32_t min_rate, uint32_t max_rate);
int bweUpdateOnPacket(BweState *bwe, const BwePacketInfo *info);
int bweUpdateOnFeedback(BweState *bwe, uint64_t now_us, uint32_t *acked_bitrate, float loss);
int bweUpdateOnRemb(BweState *bwe, uint64_t bitrate_bps);
int bweUpdateOnTwcc(BweState *bwe, const RtcpTwcc *twcc, uint64_t now_us);
uint32_t bweGetTargetBitrate(BweState *bwe);
int bweGetState(BweState *bwe, uint32_t *target, uint32_t *min, uint32_t *max, float *loss);
void bweReset(BweState *bwe);

#endif
