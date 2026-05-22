#include "media_track.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int rtpBuildHeader(RtpPacket *pkt, int payload_type, uint16_t seq, uint32_t ts, uint32_t ssrc) {
    memset(pkt, 0, sizeof(*pkt));
    pkt->version = RTP_VERSION;
    pkt->payload_type = payload_type;
    pkt->sequence_number = seq;
    pkt->timestamp = ts;
    pkt->ssrc = ssrc;
    return 0;
}

int rtpSerializeHeader(RtpPacket *pkt, uint8_t *buf, size_t bufsz, size_t *written) {
    size_t hdr_size = RTP_HEADER_MIN_SIZE + pkt->csrc_count * 4;
    if (bufsz < hdr_size) return -1;

    buf[0] = (uint8_t)((pkt->version << 6) | (pkt->padding << 5) | (pkt->extension << 4) | pkt->csrc_count);
    buf[1] = (uint8_t)((pkt->marker << 7) | pkt->payload_type);
    buf[2] = (uint8_t)(pkt->sequence_number >> 8);
    buf[3] = (uint8_t)(pkt->sequence_number & 0xFF);
    buf[4] = (uint8_t)(pkt->timestamp >> 24);
    buf[5] = (uint8_t)(pkt->timestamp >> 16);
    buf[6] = (uint8_t)(pkt->timestamp >> 8);
    buf[7] = (uint8_t)(pkt->timestamp);
    buf[8] = (uint8_t)(pkt->ssrc >> 24);
    buf[9] = (uint8_t)(pkt->ssrc >> 16);
    buf[10] = (uint8_t)(pkt->ssrc >> 8);
    buf[11] = (uint8_t)(pkt->ssrc);

    for (int i = 0; i < pkt->csrc_count; i++) {
        int off = RTP_HEADER_MIN_SIZE + i * 4;
        buf[off]     = (uint8_t)(pkt->csrc[i] >> 24);
        buf[off + 1] = (uint8_t)(pkt->csrc[i] >> 16);
        buf[off + 2] = (uint8_t)(pkt->csrc[i] >> 8);
        buf[off + 3] = (uint8_t)(pkt->csrc[i]);
    }
    *written = hdr_size;
    pkt->total_len = hdr_size + pkt->payload_len;
    return 0;
}

int rtpParseHeader(RtpPacket *pkt, const uint8_t *buf, size_t len) {
    if (len < RTP_HEADER_MIN_SIZE) return -1;
    memset(pkt, 0, sizeof(*pkt));

    pkt->version = (buf[0] >> 6) & 0x03;
    pkt->padding = (buf[0] >> 5) & 0x01;
    pkt->extension = (buf[0] >> 4) & 0x01;
    pkt->csrc_count = buf[0] & 0x0F;
    pkt->marker = (buf[1] >> 7) & 0x01;
    pkt->payload_type = buf[1] & 0x7F;
    pkt->sequence_number = ((uint16_t)buf[2] << 8) | buf[3];
    pkt->timestamp = ((uint32_t)buf[4] << 24) | ((uint32_t)buf[5] << 16) | ((uint32_t)buf[6] << 8) | buf[7];
    pkt->ssrc = ((uint32_t)buf[8] << 24) | ((uint32_t)buf[9] << 16) | ((uint32_t)buf[10] << 8) | buf[11];

    size_t hdr_size = RTP_HEADER_MIN_SIZE + pkt->csrc_count * 4;
    for (int i = 0; i < pkt->csrc_count; i++) {
        int off = RTP_HEADER_MIN_SIZE + i * 4;
        pkt->csrc[i] = ((uint32_t)buf[off] << 24) | ((uint32_t)buf[off + 1] << 16) |
                       ((uint32_t)buf[off + 2] << 8) | buf[off + 3];
    }
    if (len > hdr_size) {
        pkt->payload_len = len - hdr_size;
        if (pkt->payload_len > RTP_MAX_PAYLOAD) pkt->payload_len = RTP_MAX_PAYLOAD;
        memcpy(pkt->payload, buf + hdr_size, pkt->payload_len);
    }
    pkt->total_len = len;
    return 0;
}

int rtpSetMarker(RtpPacket *pkt, bool marker) {
    pkt->marker = marker ? 1 : 0;
    return 0;
}

int rtpSetExtension(RtpPacket *pkt, uint16_t profile, const uint8_t *data, size_t len) {
    pkt->extension = 1;
    pkt->ext_profile = profile;
    pkt->ext_length = (uint16_t)len;
    if (len <= sizeof(pkt->ext_data)) memcpy(pkt->ext_data, data, len);
    return 0;
}

int opusParseToc(uint8_t toc, OpusFrame *frame) {
    memset(frame, 0, sizeof(*frame));
    frame->toc = toc;
    int config = (toc >> 3) & 0x1F;
    frame->stereo = (toc & 0x04) != 0;
    frame->vad = frame->fec = false;

    if (config < 12) { frame->bandwidth = OPUS_FRAME_NARROWBAND; frame->duration_ms = 10; }
    else if (config < 16) { frame->bandwidth = OPUS_FRAME_WIDEBAND; frame->duration_ms = 20; }
    else if (config < 20) { frame->bandwidth = OPUS_FRAME_SUPERWIDEBAND; frame->duration_ms = 20; }
    else { frame->bandwidth = OPUS_FRAME_FULLBAND; frame->duration_ms = 20; }

    int code_count = toc & 0x03;
    frame->frame_count = code_count == 0 ? 1 : code_count == 1 ? 2 :
                         code_count == 2 ? 2 : code_count == 3 ? 255 : 1;
    frame->sample_rate = 48000;
    frame->channels = frame->stereo ? 2 : 1;
    return 0;
}

int opusPackFrame(const OpusFrame *frame, uint8_t *buf, size_t bufsz, size_t *written) {
    if (bufsz < frame->data_len + 1) return -1;
    buf[0] = frame->toc;
    memcpy(buf + 1, frame->data, frame->data_len);
    *written = frame->data_len + 1;
    return 0;
}

int opusUnpackFrame(const uint8_t *buf, size_t len, OpusFrame *frame) {
    if (len < 2) return -1;
    memset(frame, 0, sizeof(*frame));
    opusParseToc(buf[0], frame);
    frame->data_len = len - 1;
    if (frame->data_len > OPUS_MAX_FRAME_SIZE) frame->data_len = OPUS_MAX_FRAME_SIZE;
    memcpy(frame->data, buf + 1, frame->data_len);
    return 0;
}

int h264BuildFuA(const uint8_t *nalu, size_t nalu_len, int mtu, H264FuA *fus, int *fu_count) {
    if (nalu_len < 1) return -1;
    H264NalHeader nah;
    nah.forbidden = (nalu[0] >> 7) & 0x01;
    nah.nri = (nalu[0] >> 5) & 0x03;
    nah.nal_type = nalu[0] & 0x1F;

    int max_payload = mtu - RTP_HEADER_MIN_SIZE - H264_FU_HEADER_SIZE;
    int total = (int)(nalu_len - 1);
    int n_fus = (total + max_payload - 1) / max_payload;
    if (n_fus <= 0) n_fus = 1;
    *fu_count = n_fus;

    for (int i = 0; i < n_fus; i++) {
        fus[i].start = (i == 0);
        fus[i].end = (i == n_fus - 1);
        fus[i].nal_type = nah.nal_type;
        fus[i].nri = nah.nri;
        int off = i * max_payload;
        int chunk = total - off;
        if (chunk > max_payload) chunk = max_payload;
        fus[i].data_len = (size_t)chunk;
        memcpy(fus[i].data, nalu + 1 + off, chunk);
    }
    return 0;
}

int h264CombineFuA(const H264FuA *fus, int fu_count, uint8_t *nalu, size_t *nalu_len) {
    if (fu_count <= 0) return -1;
    size_t total = 1;
    for (int i = 0; i < fu_count; i++) total += fus[i].data_len;
    nalu[0] = (uint8_t)((fus[0].nri << 5) | fus[0].nal_type);
    size_t off = 1;
    for (int i = 0; i < fu_count; i++) {
        memcpy(nalu + off, fus[i].data, fus[i].data_len);
        off += fus[i].data_len;
    }
    *nalu_len = off;
    return 0;
}

int h264GetNalType(const uint8_t *nalu, size_t len, uint8_t *nal_type) {
    if (len < 1) return -1;
    *nal_type = nalu[0] & 0x1F;
    return 0;
}

int h264IsKeyFrame(const uint8_t *nalu, size_t len) {
    uint8_t nt;
    if (h264GetNalType(nalu, len, &nt) < 0) return 0;
    return nt == H264_NAL_TYPE_IDR || nt == H264_NAL_TYPE_SPS;
}

int h264ParseStapA(const uint8_t *buf, size_t len, uint8_t **nalus, size_t *nalus_len, int *nalus_count) {
    (void)nalus; (void)nalus_len;
    *nalus_count = 0;
    if (len < 3) return -1;
    size_t off = 1;
    while (off + 2 <= len) {
        size_t nalu_sz = ((size_t)buf[off] << 8) | buf[off + 1];
        off += 2;
        if (off + nalu_sz > len) break;
        off += nalu_sz;
        (*nalus_count)++;
    }
    return 0;
}

int vp8ParsePayloadDescriptor(const uint8_t *buf, size_t len, Vp8Payload *vp8) {
    if (len < VP8_PAYLOAD_DESC_SIZE) return -1;
    memset(vp8, 0, sizeof(*vp8));
    vp8->key_frame = (buf[0] & 0x01) != 0;
    int off = 1;
    if (buf[0] & 0x80) {
        if (len < (size_t)(off + 1)) return -1;
        vp8->picture_id = (buf[off] & 0x7F);
        if (buf[off] & 0x80) {
            off++;
            if (len < (size_t)(off + 1)) return -1;
            vp8->picture_id = (vp8->picture_id << 8) | buf[off];
        }
        off++;
    }
    if (buf[0] & 0x40) {
        if (len < (size_t)(off + 1)) return -1;
        vp8->tl0_pic_idx = buf[off];
        vp8->tid_present = (buf[off] & 0x80) != 0;
        if (vp8->tid_present) {
            vp8->tid = (buf[off] >> 6) & 0x03;
            vp8->key_idx = (buf[off] >> 1) & 0x1F;
        }
        off++;
    }
    vp8->is_golden = vp8->is_altref = vp8->layer_sync = 0;
    if (len > (size_t)off) {
        vp8->data_len = len - off;
        if (vp8->data_len > RTP_MAX_PAYLOAD) vp8->data_len = RTP_MAX_PAYLOAD;
        memcpy(vp8->data, buf + off, vp8->data_len);
    }
    return 0;
}

int vp8BuildPayloadDescriptor(const Vp8Payload *vp8, uint8_t *buf, size_t bufsz, size_t *written) {
    buf[0] = (uint8_t)(vp8->key_frame ? 0x01 : 0x00);
    int off = 1;
    if (vp8->picture_id >= 128 && bufsz > (size_t)(off + 1)) {
        buf[0] |= 0x80;
        buf[off++] = (uint8_t)((vp8->picture_id >> 8) | 0x80);
        buf[off++] = (uint8_t)(vp8->picture_id);
    }
    *written = (size_t)off;
    return 0;
}

int vp8IsKeyFrame(const uint8_t *buf, size_t len) {
    if (len < 1) return 0;
    return (buf[0] & 0x01) != 0;
}

int rtcpBuildSenderReport(uint8_t *buf, size_t bufsz, const RtcpSenderReport *sr, size_t *written) {
    if (bufsz < 28) return -1;
    memset(buf, 0, 28);
    buf[0] = (uint8_t)((RTP_VERSION << 6) | (0 << 5) | 0);
    buf[1] = RTCP_TYPE_SR;
    buf[2] = 0; buf[3] = 6;
    buf[4] = (uint8_t)(sr->ssrc >> 24); buf[5] = (uint8_t)(sr->ssrc >> 16);
    buf[6] = (uint8_t)(sr->ssrc >> 8);  buf[7] = (uint8_t)(sr->ssrc);
    for (int i = 0; i < 8; i++) buf[8 + i] = (uint8_t)(sr->ntp_timestamp >> (56 - i * 8));
    buf[16] = (uint8_t)(sr->rtp_timestamp >> 24); buf[17] = (uint8_t)(sr->rtp_timestamp >> 16);
    buf[18] = (uint8_t)(sr->rtp_timestamp >> 8);  buf[19] = (uint8_t)(sr->rtp_timestamp);
    buf[20] = (uint8_t)(sr->packet_count >> 24); buf[21] = (uint8_t)(sr->packet_count >> 16);
    buf[22] = (uint8_t)(sr->packet_count >> 8);  buf[23] = (uint8_t)(sr->packet_count);
    buf[24] = (uint8_t)(sr->octet_count >> 24); buf[25] = (uint8_t)(sr->octet_count >> 16);
    buf[26] = (uint8_t)(sr->octet_count >> 8);  buf[27] = (uint8_t)(sr->octet_count);
    *written = 28;
    return 0;
}

int rtcpBuildReceiverReport(uint8_t *buf, size_t bufsz, const RtcpReceiverReport *rr, size_t *written) {
    int nblocks = rr->block_count;
    size_t total = 8 + (size_t)nblocks * 24;
    if (bufsz < total) return -1;
    memset(buf, 0, total);
    buf[0] = (uint8_t)((RTP_VERSION << 6) | (nblocks & 0x1F));
    buf[1] = RTCP_TYPE_RR;
    uint16_t len16 = (uint16_t)((total / 4) - 1);
    buf[2] = (uint8_t)(len16 >> 8); buf[3] = (uint8_t)(len16);
    buf[4] = (uint8_t)(rr->ssrc >> 24); buf[5] = (uint8_t)(rr->ssrc >> 16);
    buf[6] = (uint8_t)(rr->ssrc >> 8);  buf[7] = (uint8_t)(rr->ssrc);
    for (int i = 0; i < nblocks; i++) {
        int base = 8 + i * 24;
        RtcpReportBlock *rb = &rr->blocks[i];
        buf[base]     = (uint8_t)(rb->ssrc >> 24);
        buf[base+1]   = (uint8_t)(rb->ssrc >> 16);
        buf[base+2]   = (uint8_t)(rb->ssrc >> 8);
        buf[base+3]   = (uint8_t)(rb->ssrc);
        buf[base+4]   = rb->fraction_lost;
        buf[base+5]   = (uint8_t)((rb->cumulative_lost >> 16) & 0xFF);
        buf[base+6]   = (uint8_t)((rb->cumulative_lost >> 8) & 0xFF);
        buf[base+7]   = (uint8_t)(rb->cumulative_lost & 0xFF);
        buf[base+8]   = (uint8_t)(rb->extended_highest_seq >> 24);
        buf[base+9]   = (uint8_t)(rb->extended_highest_seq >> 16);
        buf[base+10]  = (uint8_t)(rb->extended_highest_seq >> 8);
        buf[base+11]  = (uint8_t)(rb->extended_highest_seq);
        buf[base+12]  = (uint8_t)(rb->jitter >> 24);
        buf[base+13]  = (uint8_t)(rb->jitter >> 16);
        buf[base+14]  = (uint8_t)(rb->jitter >> 8);
        buf[base+15]  = (uint8_t)(rb->jitter);
        buf[base+16]  = (uint8_t)(rb->last_sr_timestamp >> 24);
        buf[base+17]  = (uint8_t)(rb->last_sr_timestamp >> 16);
        buf[base+18]  = (uint8_t)(rb->last_sr_timestamp >> 8);
        buf[base+19]  = (uint8_t)(rb->last_sr_timestamp);
        buf[base+20]  = (uint8_t)(rb->delay_since_last_sr >> 24);
        buf[base+21]  = (uint8_t)(rb->delay_since_last_sr >> 16);
        buf[base+22]  = (uint8_t)(rb->delay_since_last_sr >> 8);
        buf[base+23]  = (uint8_t)(rb->delay_since_last_sr);
    }
    *written = total;
    return 0;
}

int rtcpParseSenderReport(const uint8_t *buf, size_t len, RtcpSenderReport *sr) {
    if (len < 28) return -1;
    memset(sr, 0, sizeof(*sr));
    sr->ssrc = ((uint32_t)buf[4] << 24) | ((uint32_t)buf[5] << 16) | ((uint32_t)buf[6] << 8) | buf[7];
    sr->ntp_timestamp = 0;
    for (int i = 0; i < 8; i++) sr->ntp_timestamp = (sr->ntp_timestamp << 8) | buf[8 + i];
    sr->rtp_timestamp = ((uint32_t)buf[16] << 24) | ((uint32_t)buf[17] << 16) | ((uint32_t)buf[18] << 8) | buf[19];
    sr->packet_count = ((uint32_t)buf[20] << 24) | ((uint32_t)buf[21] << 16) | ((uint32_t)buf[22] << 8) | buf[23];
    sr->octet_count = ((uint32_t)buf[24] << 24) | ((uint32_t)buf[25] << 16) | ((uint32_t)buf[26] << 8) | buf[27];
    return 0;
}

int rtcpParseReceiverReport(const uint8_t *buf, size_t len, RtcpReceiverReport *rr) {
    if (len < 8) return -1;
    memset(rr, 0, sizeof(*rr));
    rr->ssrc = ((uint32_t)buf[4] << 24) | ((uint32_t)buf[5] << 16) | ((uint32_t)buf[6] << 8) | buf[7];
    int nblocks = buf[0] & 0x1F;
    if (nblocks > 16) nblocks = 16;
    rr->block_count = nblocks;
    for (int i = 0; i < nblocks; i++) {
        int base = 8 + i * 24;
        if (base + 24 > (int)len) break;
        RtcpReportBlock *rb = &rr->blocks[i];
        rb->ssrc = ((uint32_t)buf[base] << 24) | ((uint32_t)buf[base+1] << 16) |
                   ((uint32_t)buf[base+2] << 8) | buf[base+3];
        rb->fraction_lost = buf[base+4];
        rb->cumulative_lost = ((int32_t)buf[base+5] << 16) | ((int32_t)buf[base+6] << 8) | buf[base+7];
        rb->extended_highest_seq = ((uint32_t)buf[base+8] << 24) | ((uint32_t)buf[base+9] << 16) |
                                    ((uint32_t)buf[base+10] << 8) | buf[base+11];
        rb->jitter = ((uint32_t)buf[base+12] << 24) | ((uint32_t)buf[base+13] << 16) |
                     ((uint32_t)buf[base+14] << 8) | buf[base+15];
        rb->last_sr_timestamp = ((uint32_t)buf[base+16] << 24) | ((uint32_t)buf[base+17] << 16) |
                                 ((uint32_t)buf[base+18] << 8) | buf[base+19];
        rb->delay_since_last_sr = ((uint32_t)buf[base+20] << 24) | ((uint32_t)buf[base+21] << 16) |
                                   ((uint32_t)buf[base+22] << 8) | buf[base+23];
    }
    return 0;
}

int rtcpBuildSdesCname(uint8_t *buf, size_t bufsz, uint32_t ssrc, const char *cname, size_t *written) {
    size_t cname_len = strlen(cname);
    size_t total = 12 + cname_len;
    if (bufsz < total) return -1;
    memset(buf, 0, total);
    buf[0] = (uint8_t)((RTP_VERSION << 6) | (1 << 5) | 1);
    buf[1] = RTCP_TYPE_SDES;
    uint16_t len16 = (uint16_t)((total / 4) - 1);
    buf[2] = (uint8_t)(len16 >> 8); buf[3] = (uint8_t)(len16);
    buf[4] = (uint8_t)(ssrc >> 24); buf[5] = (uint8_t)(ssrc >> 16);
    buf[6] = (uint8_t)(ssrc >> 8);  buf[7] = (uint8_t)(ssrc);
    buf[8] = 0x01;
    buf[9] = (uint8_t)cname_len;
    memcpy(buf + 10, cname, cname_len);
    *written = total;
    return 0;
}

int rtcpBuildBye(uint8_t *buf, size_t bufsz, uint32_t *ssrcs, int count, const char *reason, size_t *written) {
    size_t reason_len = reason ? strlen(reason) : 0;
    size_t total = 8 + (size_t)count * 4 + reason_len;
    if (bufsz < total) return -1;
    memset(buf, 0, total);
    buf[0] = (uint8_t)((RTP_VERSION << 6) | (count & 0x1F));
    buf[1] = RTCP_TYPE_BYE;
    uint16_t len16 = (uint16_t)((total / 4) - 1);
    buf[2] = (uint8_t)(len16 >> 8); buf[3] = (uint8_t)(len16);
    for (int i = 0; i < count; i++) {
        int base = 4 + i * 4;
        buf[base]   = (uint8_t)(ssrcs[i] >> 24);
        buf[base+1] = (uint8_t)(ssrcs[i] >> 16);
        buf[base+2] = (uint8_t)(ssrcs[i] >> 8);
        buf[base+3] = (uint8_t)(ssrcs[i]);
    }
    if (reason_len) {
        buf[4 + count * 4] = (uint8_t)reason_len;
        memcpy(buf + 4 + count * 4 + 1, reason, reason_len);
    }
    *written = total;
    return 0;
}

int rtcpBuildRemb(uint8_t *buf, size_t bufsz, const RtcpRemb *remb, size_t *written) {
    size_t total = 20 + (size_t)remb->ssrc_count * 4;
    if (bufsz < total) return -1;
    memset(buf, 0, total);
    buf[0] = (uint8_t)((RTP_VERSION << 6) | (0 << 5) | 0);
    buf[1] = RTCP_TYPE_REMB;
    uint16_t len16 = (uint16_t)((total / 4) - 1);
    buf[2] = (uint8_t)(len16 >> 8); buf[3] = (uint8_t)(len16);
    buf[4] = (uint8_t)(remb->ssrc >> 24); buf[5] = (uint8_t)(remb->ssrc >> 16);
    buf[6] = (uint8_t)(remb->ssrc >> 8);  buf[7] = (uint8_t)(remb->ssrc);
    buf[8] = 'R'; buf[9] = 'E'; buf[10] = 'M'; buf[11] = 'B';
    buf[12] = (uint8_t)(remb->ssrc_count);
    uint32_t br = (uint32_t)(remb->bitrate_bps & 0x3FFFFF);
    buf[13] = (uint8_t)(br >> 18); buf[14] = (uint8_t)(br >> 10);
    buf[15] = (uint8_t)(br >> 2);  buf[16] = (uint8_t)((br & 0x03) << 6);
    for (int i = 0; i < remb->ssrc_count; i++) {
        int base = 20 + i * 4;
        buf[base]   = (uint8_t)(remb->ssrcs[i] >> 24);
        buf[base+1] = (uint8_t)(remb->ssrcs[i] >> 16);
        buf[base+2] = (uint8_t)(remb->ssrcs[i] >> 8);
        buf[base+3] = (uint8_t)(remb->ssrcs[i]);
    }
    *written = total;
    return 0;
}

int rtcpParseRemb(const uint8_t *buf, size_t len, RtcpRemb *remb) {
    if (len < 24) return -1;
    memset(remb, 0, sizeof(*remb));
    remb->ssrc = ((uint32_t)buf[4] << 24) | ((uint32_t)buf[5] << 16) | ((uint32_t)buf[6] << 8) | buf[7];
    remb->ssrc_count = buf[12];
    uint32_t br = ((uint32_t)(buf[13] & 0x3F) << 18) | ((uint32_t)buf[14] << 10) |
                  ((uint32_t)buf[15] << 2) | (buf[16] >> 6);
    remb->bitrate_bps = br;
    for (int i = 0; i < remb->ssrc_count && i < 64; i++) {
        int base = 20 + i * 4;
        if (base + 4 > (int)len) break;
        remb->ssrcs[i] = ((uint32_t)buf[base] << 24) | ((uint32_t)buf[base+1] << 16) |
                         ((uint32_t)buf[base+2] << 8) | buf[base+3];
    }
    return 0;
}

int rtcpBuildPli(uint8_t *buf, size_t bufsz, uint32_t ssrc, uint32_t media_ssrc, size_t *written) {
    if (bufsz < 12) return -1;
    memset(buf, 0, 12);
    buf[0] = (uint8_t)((RTP_VERSION << 6) | (1 << 5) | 1);
    buf[1] = RTCP_TYPE_PLI;
    buf[2] = 0; buf[3] = 2;
    buf[4] = (uint8_t)(ssrc >> 24); buf[5] = (uint8_t)(ssrc >> 16);
    buf[6] = (uint8_t)(ssrc >> 8);  buf[7] = (uint8_t)(ssrc);
    buf[8] = (uint8_t)(media_ssrc >> 24); buf[9] = (uint8_t)(media_ssrc >> 16);
    buf[10] = (uint8_t)(media_ssrc >> 8); buf[11] = (uint8_t)(media_ssrc);
    *written = 12;
    return 0;
}

int rtcpBuildFir(uint8_t *buf, size_t bufsz, const RtcpFir *fir, size_t *written) {
    if (bufsz < 20) return -1;
    memset(buf, 0, 20);
    buf[0] = (uint8_t)((RTP_VERSION << 6) | (1 << 5) | 1);
    buf[1] = RTCP_TYPE_FIR;
    buf[2] = 0; buf[3] = 4;
    buf[4] = (uint8_t)(fir->ssrc >> 24); buf[5] = (uint8_t)(fir->ssrc >> 16);
    buf[6] = (uint8_t)(fir->ssrc >> 8);  buf[7] = (uint8_t)(fir->ssrc);
    buf[8] = (uint8_t)(fir->media_ssrc >> 24); buf[9] = (uint8_t)(fir->media_ssrc >> 16);
    buf[10] = (uint8_t)(fir->media_ssrc >> 8); buf[11] = (uint8_t)(fir->media_ssrc);
    buf[12] = fir->seq;
    buf[13] = buf[14] = buf[15] = 0;
    *written = 20;
    return 0;
}

int rtcpParseFir(const uint8_t *buf, size_t len, RtcpFir *fir) {
    if (len < 20) return -1;
    memset(fir, 0, sizeof(*fir));
    fir->ssrc = ((uint32_t)buf[4] << 24) | ((uint32_t)buf[5] << 16) | ((uint32_t)buf[6] << 8) | buf[7];
    fir->media_ssrc = ((uint32_t)buf[8] << 24) | ((uint32_t)buf[9] << 16) | ((uint32_t)buf[10] << 8) | buf[11];
    fir->seq = buf[12];
    return 0;
}

int rtcpBuildTwccFeedback(uint8_t *buf, size_t bufsz, const RtcpTwcc *twcc, size_t *written) {
    size_t total = 20 + (size_t)twcc->chunk_count * 2 + (size_t)twcc->delta_count;
    if (bufsz < total) return -1;
    memset(buf, 0, total);
    buf[0] = (uint8_t)((RTP_VERSION << 6) | (0 << 5) | 0);
    buf[1] = RTCP_TYPE_TWCC;
    uint16_t len16 = (uint16_t)((total / 4) - 1);
    buf[2] = (uint8_t)(len16 >> 8); buf[3] = (uint8_t)(len16);
    buf[8] = (uint8_t)(twcc->base_seq >> 8); buf[9] = (uint8_t)(twcc->base_seq);
    buf[10] = (uint8_t)(twcc->packet_status_count >> 8); buf[11] = (uint8_t)(twcc->packet_status_count);
    buf[12] = (uint8_t)(twcc->ref_time >> 16); buf[13] = (uint8_t)(twcc->ref_time >> 8);
    buf[14] = (uint8_t)(twcc->ref_time);
    buf[15] = (uint8_t)(twcc->feedback_seq);
    for (int i = 0; i < twcc->chunk_count; i++) {
        int base = 20 + i * 2;
        buf[base] = (uint8_t)(twcc->packet_chunks[i] >> 8);
        buf[base+1] = (uint8_t)(twcc->packet_chunks[i]);
    }
    if (twcc->delta_count > 0) {
        int delta_base = 20 + twcc->chunk_count * 2;
        memcpy(buf + delta_base, twcc->recv_deltas, (size_t)twcc->delta_count);
    }
    *written = total;
    return 0;
}

int rtcpParseTwccFeedback(const uint8_t *buf, size_t len, RtcpTwcc *twcc) {
    if (len < 20) return -1;
    memset(twcc, 0, sizeof(*twcc));
    twcc->base_seq = ((uint16_t)buf[8] << 8) | buf[9];
    twcc->packet_status_count = ((uint16_t)buf[10] << 8) | buf[11];
    twcc->ref_time = ((uint32_t)buf[12] << 16) | ((uint32_t)buf[13] << 8) | buf[14];
    twcc->feedback_seq = buf[15];
    return 0;
}

void trackInit(TrackContext *ctx, uint32_t ssrc, int pt, int clock_rate) {
    memset(ctx, 0, sizeof(*ctx));
    ctx->ssrc = ssrc;
    ctx->payload_type = pt;
    ctx->clock_rate = clock_rate;
    ctx->sequence_number = (int)((uint32_t)time(NULL) & 0xFFFF);
    ctx->timestamp = (uint32_t)((uint64_t)time(NULL) * (uint64_t)clock_rate);
}

int trackUpdateSeq(TrackContext *ctx) {
    ctx->sequence_number = (ctx->sequence_number + 1) & 0xFFFF;
    return ctx->sequence_number;
}

uint32_t trackGetTimestamp(TrackContext *ctx, int sample_count) {
    ctx->timestamp += (uint32_t)sample_count;
    return ctx->timestamp;
}

uint64_t trackNtpNow(void) {
    uint64_t us = (uint64_t)time(NULL) * 1000000ULL;
    return (us / 1000000) * (1ULL << 32) + (us % 1000000) * (1ULL << 32) / 1000000;
}
