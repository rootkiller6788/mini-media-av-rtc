#include "sfu_mcu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int sfuInit(SfuContext *sfu, uint32_t bandwidth_limit) {
    memset(sfu, 0, sizeof(*sfu));
    sfu->bandwidth_limit_bps = bandwidth_limit;
    sfu->global_ssrc_base = 1000;
    sfu->tile_width = 320;
    sfu->tile_height = 240;
    return 0;
}

int sfuAddParticipant(SfuContext *sfu, const char *id) {
    if (sfu->participant_count >= SFU_MAX_PARTICIPANTS) return -1;
    SfuParticipant *p = &sfu->participants[sfu->participant_count++];
    memset(p, 0, sizeof(*p));
    strncpy(p->participant_id, id, sizeof(p->participant_id) - 1);
    return 0;
}

int sfuRemoveParticipant(SfuContext *sfu, const char *id) {
    for (int i = 0; i < sfu->participant_count; i++) {
        if (strcmp(sfu->participants[i].participant_id, id) == 0) {
            if (sfu->participants[i].is_active_speaker) sfu->active_speaker_count--;
            sfu->participants[i] = sfu->participants[sfu->participant_count - 1];
            sfu->participant_count--;
            return 0;
        }
    }
    return -1;
}

SfuParticipant *sfuFindParticipant(SfuContext *sfu, const char *id) {
    for (int i = 0; i < sfu->participant_count; i++)
        if (strcmp(sfu->participants[i].participant_id, id) == 0)
            return &sfu->participants[i];
    return NULL;
}

int sfuAddTrack(SfuContext *sfu, const char *participant_id, const char *track_id, int media_type) {
    SfuParticipant *p = sfuFindParticipant(sfu, participant_id);
    if (!p || p->track_count >= SFU_MAX_TRACKS) return -1;
    SfuTrack *t = &p->tracks[p->track_count++];
    memset(t, 0, sizeof(*t));
    strncpy(t->track_id, track_id, sizeof(t->track_id) - 1);
    t->media_type = media_type;
    t->current_layer = SFU_LAYER_HIGH;
    return 0;
}

int sfuAddSimulcastLayer(SfuContext *sfu, const char *participant_id, const char *track_id, SfuLayerId layer_id, const char *rid, uint32_t ssrc, int width, int height, uint32_t bitrate) {
    SfuParticipant *p = sfuFindParticipant(sfu, participant_id);
    if (!p) return -1;
    for (int i = 0; i < p->track_count; i++) {
        SfuTrack *t = &p->tracks[i];
        if (strcmp(t->track_id, track_id) == 0) {
            if (t->layer_count >= SFU_MAX_LAYERS) return -1;
            SfuSimulcastLayer *l = &t->layers[t->layer_count++];
            memset(l, 0, sizeof(*l));
            l->id = layer_id;
            strncpy(l->rid, rid, sizeof(l->rid) - 1);
            l->ssrc = ssrc;
            l->width = width;
            l->height = height;
            l->target_bitrate_bps = bitrate;
            l->active = true;
            l->temporal_layers = 1;
            if (layer_id == SFU_LAYER_LOW) {
                l->width = width / 4;
                l->height = height / 4;
                l->target_bitrate_bps = bitrate / 4;
            } else if (layer_id == SFU_LAYER_MEDIUM) {
                l->width = width / 2;
                l->height = height / 2;
                l->target_bitrate_bps = bitrate / 2;
            }
            return 0;
        }
    }
    return -1;
}

int sfuSelectActiveSpeaker(SfuContext *sfu) {
    sfu->active_speaker_count = 0;
    int max_level = -127;
    SfuParticipant *best = NULL;
    for (int i = 0; i < sfu->participant_count; i++) {
        SfuParticipant *p = &sfu->participants[i];
        if (p->muted) continue;
        if (p->audio_level > max_level) {
            max_level = p->audio_level;
            best = p;
        }
    }
    for (int i = 0; i < sfu->participant_count; i++)
        sfu->participants[i].is_active_speaker = false;
    if (best) {
        best->is_active_speaker = true;
        sfu->active_speaker_count = 1;
        for (int i = 0; i < best->track_count; i++)
            best->tracks[i].is_active_speaker = true;
    }
    return 0;
}

int sfuSelectLayer(SfuContext *sfu, const char *participant_id, const char *track_id, SfuLayerId layer) {
    SfuParticipant *p = sfuFindParticipant(sfu, participant_id);
    if (!p) return -1;
    for (int i = 0; i < p->track_count; i++) {
        if (strcmp(p->tracks[i].track_id, track_id) == 0) {
            p->tracks[i].current_layer = layer;
            return 0;
        }
    }
    return -1;
}

SfuLayerId sfuGetCurrentLayer(SfuContext *sfu, const char *participant_id, const char *track_id) {
    SfuParticipant *p = sfuFindParticipant(sfu, participant_id);
    if (!p) return SFU_LAYER_HIGH;
    for (int i = 0; i < p->track_count; i++) {
        if (strcmp(p->tracks[i].track_id, track_id) == 0)
            return p->tracks[i].current_layer;
    }
    return SFU_LAYER_HIGH;
}

int sfuForwardPacket(SfuContext *sfu, const RtpPacket *pkt, const char *from_participant, const char *to_participant, RtpPacket *out) {
    *out = *pkt;
    SfuParticipant *fp = sfuFindParticipant(sfu, from_participant);
    SfuParticipant *tp = sfuFindParticipant(sfu, to_participant);
    if (!fp || !tp) return -1;

    for (int i = 0; i < fp->track_count; i++) {
        SfuTrack *t = &fp->tracks[i];
        if (t->current_layer == SFU_LAYER_LOW && t->media_type == 0) {
            out->payload_len = out->payload_len / 2;
        }
    }
    sfuRewriteSsrc(out, tp->video_ssrc ? tp->video_ssrc : tp->audio_ssrc);
    return 0;
}

int sfuRewriteSsrc(RtpPacket *pkt, uint32_t new_ssrc) {
    pkt->ssrc = new_ssrc;
    return 0;
}

int sfuRewriteSequence(RtpPacket *pkt, uint16_t new_seq) {
    pkt->sequence_number = new_seq;
    return 0;
}

int sfuRewriteTimestamp(RtpPacket *pkt, uint32_t new_ts) {
    pkt->timestamp = new_ts;
    return 0;
}

int sfuDropPaddingOnly(RtpPacket *pkt) {
    if (pkt->padding && pkt->payload_len == 0) {
        memset(pkt, 0, sizeof(*pkt));
        return 1;
    }
    return 0;
}

int sfuHandleRemb(SfuContext *sfu, const RtcpRemb *remb) {
    uint64_t total_br = remb->bitrate_bps;
    int n_active = sfu->active_speaker_count;
    if (n_active <= 0) n_active = 1;
    for (int i = 0; i < sfu->participant_count; i++) {
        SfuParticipant *p = &sfu->participants[i];
        if (p->is_active_speaker) {
            uint64_t per_participant = total_br / (uint64_t)n_active;
            if (per_participant > sfu->bandwidth_limit_bps)
                per_participant = sfu->bandwidth_limit_bps;
            for (int j = 0; j < p->track_count; j++) {
                p->tracks[j].bitrate_bps = per_participant;
            }
        } else {
            for (int j = 0; j < p->track_count; j++) {
                uint64_t low_br = p->tracks[j].layers[p->tracks[j].layer_count > 0 ? p->tracks[j].layer_count - 1 : 0].target_bitrate_bps;
                p->tracks[j].bitrate_bps = low_br;
            }
        }
    }
    return 0;
}

int sfuDistributeRemb(SfuContext *sfu, const char *target_participant, uint64_t bitrate_bps) {
    SfuParticipant *p = sfuFindParticipant(sfu, target_participant);
    if (!p) return -1;
    for (int i = 0; i < p->track_count; i++)
        p->tracks[i].bitrate_bps = bitrate_bps;
    return 0;
}

int sfuApplyBandwidthLimit(SfuContext *sfu, uint64_t bitrate_bps) {
    sfu->bandwidth_limit_bps = (uint32_t)bitrate_bps;
    uint64_t per = bitrate_bps / (uint64_t)(sfu->participant_count > 0 ? sfu->participant_count : 1);
    for (int i = 0; i < sfu->participant_count; i++) {
        for (int j = 0; j < sfu->participants[i].track_count; j++) {
            if (sfu->participants[i].tracks[j].bitrate_bps > per)
                sfu->participants[i].tracks[j].bitrate_bps = per;
        }
    }
    return 0;
}

int sfuSetActiveSpeaker(SfuContext *sfu, const char *participant_id, bool active) {
    SfuParticipant *p = sfuFindParticipant(sfu, participant_id);
    if (!p) return -1;
    p->is_active_speaker = active;
    if (active) sfu->active_speaker_count++;
    else if (sfu->active_speaker_count > 0) sfu->active_speaker_count--;
    return 0;
}

int sfuUpdateAudioLevel(SfuContext *sfu, const char *participant_id, int level) {
    SfuParticipant *p = sfuFindParticipant(sfu, participant_id);
    if (!p) return -1;
    p->audio_level = level;
    return 0;
}

int sfuGetStats(SfuContext *sfu, const char *participant_id, const char *track_id, uint32_t *bitrate, float *loss) {
    SfuParticipant *p = sfuFindParticipant(sfu, participant_id);
    if (!p) return -1;
    for (int i = 0; i < p->track_count; i++) {
        SfuTrack *t = &p->tracks[i];
        if (strcmp(t->track_id, track_id) == 0) {
            *bitrate = (uint32_t)t->bitrate_bps;
            *loss = t->fraction_lost;
            return 0;
        }
    }
    return -1;
}

int mcuInit(McuContext *mcu, int width, int height, uint32_t ssrc, int pt) {
    memset(mcu, 0, sizeof(*mcu));
    mcu->output_width = width;
    mcu->output_height = height;
    mcu->output_ssrc = ssrc;
    mcu->payload_type = pt;
    mcu->input_count = 0;
    return 0;
}

int mcuAddInput(McuContext *mcu, int width, int height) {
    if (mcu->input_count >= MCU_MAX_INPUTS) return -1;
    mcu->input_count++;
    (void)width; (void)height;
    return 0;
}

int mcuSetLayout(McuContext *mcu, int *positions, int count) {
    (void)mcu; (void)positions; (void)count;
    return 0;
}

int mcuProcessFrame(McuContext *mcu, int input_id, const uint8_t *encoded, size_t len, uint8_t *output, size_t *outlen, bool *keyframe) {
    (void)mcu; (void)input_id;
    memcpy(output, encoded, len);
    *outlen = len;
    *keyframe = true;
    return 0;
}

int mcuRequestKeyFrame(McuContext *mcu, int input_id) {
    (void)mcu; (void)input_id;
    return 0;
}

int mcuSwitchLayout(McuContext *mcu, int layout_type) {
    (void)mcu; (void)layout_type;
    return 0;
}

int mcuClose(McuContext *mcu) {
    memset(mcu, 0, sizeof(*mcu));
    return 0;
}

int bweInit(BweState *bwe, uint32_t min_rate, uint32_t max_rate) {
    memset(bwe, 0, sizeof(*bwe));
    bwe->min_bitrate_bps = min_rate;
    bwe->max_bitrate_bps = max_rate;
    bwe->target_bitrate_bps = min_rate;
    bwe->estimate_noise = 10000.0f;
    bwe->kalman_gain = 0.5f;
    bwe->packet_index = 0;
    bwe->packet_count = 0;
    return 0;
}

int bweUpdateOnPacket(BweState *bwe, const BwePacketInfo *info) {
    if (bwe->packet_count < 256) bwe->packet_count++;
    bwe->packets[bwe->packet_index] = *info;
    bwe->packet_index = (bwe->packet_index + 1) % 256;

    if (bwe->packet_count >= 2) {
        int prev = (bwe->packet_index - 2 + 256) % 256;
        if (bwe->packet_count >= 2 && bwe->packets[prev].timestamp_us > 0) {
            int64_t delta = (int64_t)info->timestamp_us - (int64_t)bwe->packets[prev].timestamp_us;
            if (delta > 0 && delta < 2000000) {
                uint64_t bitrate = (uint64_t)info->size_bytes * 8 * 1000000 / (uint64_t)delta;
                float alpha = 0.1f;
                bwe->target_bitrate_bps = (uint32_t)((float)bwe->target_bitrate_bps * (1.0f - alpha) + (float)bitrate * alpha);
                if (bwe->target_bitrate_bps < bwe->min_bitrate_bps) bwe->target_bitrate_bps = bwe->min_bitrate_bps;
                if (bwe->target_bitrate_bps > bwe->max_bitrate_bps) bwe->target_bitrate_bps = bwe->max_bitrate_bps;
            }
        }
    }
    return 0;
}

int bweUpdateOnFeedback(BweState *bwe, uint64_t now_us, uint32_t *acked_bitrate, float loss) {
    if (loss > 0.1f) {
        bwe->target_bitrate_bps = (uint32_t)((float)bwe->target_bitrate_bps * 0.85f);
    } else if (loss < 0.02f) {
        bwe->target_bitrate_bps = (uint32_t)((float)bwe->target_bitrate_bps * 1.05f);
        if (bwe->target_bitrate_bps > bwe->max_bitrate_bps)
            bwe->target_bitrate_bps = bwe->max_bitrate_bps;
    }
    if (bwe->target_bitrate_bps < bwe->min_bitrate_bps)
        bwe->target_bitrate_bps = bwe->min_bitrate_bps;

    bwe->loss_fraction = loss;
    bwe->last_feedback_us = now_us;
    *acked_bitrate = bwe->target_bitrate_bps;
    return 0;
}

int bweUpdateOnRemb(BweState *bwe, uint64_t bitrate_bps) {
    if (bitrate_bps > bwe->target_bitrate_bps) {
        float alpha = 0.5f;
        bwe->target_bitrate_bps = (uint32_t)((float)bwe->target_bitrate_bps * (1.0f - alpha) + (float)bitrate_bps * alpha);
    } else {
        bwe->target_bitrate_bps = (uint32_t)bitrate_bps;
    }
    if (bwe->target_bitrate_bps < bwe->min_bitrate_bps) bwe->target_bitrate_bps = bwe->min_bitrate_bps;
    if (bwe->target_bitrate_bps > bwe->max_bitrate_bps) bwe->target_bitrate_bps = bwe->max_bitrate_bps;
    return 0;
}

int bweUpdateOnTwcc(BweState *bwe, const RtcpTwcc *twcc, uint64_t now_us) {
    bwe->last_feedback_us = now_us;
    float total_loss = 0;
    if (twcc->packet_status_count > 0) {
        int lost_count = 0;
        for (int i = 0; i < twcc->chunk_count && i < 128; i++) {
            if ((twcc->packet_chunks[i] & 0x8000) == 0) lost_count++;
        }
        total_loss = (float)lost_count / (float)twcc->packet_status_count;
    }
    return bweUpdateOnFeedback(bwe, now_us, &bwe->target_bitrate_bps, total_loss);
}

uint32_t bweGetTargetBitrate(BweState *bwe) {
    return bwe->target_bitrate_bps;
}

int bweGetState(BweState *bwe, uint32_t *target, uint32_t *min, uint32_t *max, float *loss) {
    *target = bwe->target_bitrate_bps;
    *min = bwe->min_bitrate_bps;
    *max = bwe->max_bitrate_bps;
    *loss = bwe->loss_fraction;
    return 0;
}

void bweReset(BweState *bwe) {
    uint32_t min_r = bwe->min_bitrate_bps;
    uint32_t max_r = bwe->max_bitrate_bps;
    bweInit(bwe, min_r, max_r);
}
