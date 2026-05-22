#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sdp_signaling.h"
#include "ice_stun_turn.h"
#include "dtls_srtp.h"
#include "media_track.h"

int main(void) {
    printf("=== WebRTC Video Call Demo ===\n\n");

    /* --- 1. SDP Offer/Answer --- */
    printf("[1] SDP Offer/Answer Exchange\n");

    SessionDescription offer;
    memset(&offer, 0, sizeof(offer));
    offer.session_id = 1234567890;
    offer.session_version = 1;
    strncpy(offer.session_name, "mini-rtc", sizeof(offer.session_name) - 1);
    offer.bundle_enabled = true;
    strncpy(offer.group_bundle, "0 1", sizeof(offer.group_bundle) - 1);

    MediaDescription *audio = &offer.media[offer.media_count++];
    audio->type = SDP_MEDIA_AUDIO;
    audio->port = 9;
    strncpy(audio->proto, "UDP/TLS/RTP/SAVPF", sizeof(audio->proto) - 1);
    strncpy(audio->mid, "0", sizeof(audio->mid) - 1);
    audio->rtcp_mux = true;
    strncpy(audio->ice_ufrag, "aZq1", sizeof(audio->ice_ufrag) - 1);
    strncpy(audio->ice_pwd, "XrP3kL9mN2sQ5vW8", sizeof(audio->ice_pwd) - 1);
    sdpAddCodec(audio, SDP_CODEC_OPUS, 111, "opus", 48000);
    sdpAddCodec(audio, SDP_CODEC_PCMU, 0, "PCMU", 8000);
    sdpAddExtmap(audio, 1, "urn:ietf:params:rtp-hdrext:ssrc-audio-level");
    audio->ssrc = 1001;
    strncpy(audio->cname, "caller@mini-rtc", sizeof(audio->cname) - 1);
    strncpy(audio->msid, "audio-sender", sizeof(audio->msid) - 1);
    strncpy(audio->track_id, "audiotrack-1", sizeof(audio->track_id) - 1);

    MediaDescription *video = &offer.media[offer.media_count++];
    video->type = SDP_MEDIA_VIDEO;
    video->port = 9;
    strncpy(video->proto, "UDP/TLS/RTP/SAVPF", sizeof(video->proto) - 1);
    strncpy(video->mid, "1", sizeof(video->mid) - 1);
    video->rtcp_mux = true;
    strncpy(video->ice_ufrag, "aZq1", sizeof(video->ice_ufrag) - 1);
    strncpy(video->ice_pwd, "XrP3kL9mN2sQ5vW8", sizeof(video->ice_pwd) - 1);
    sdpAddCodec(video, SDP_CODEC_VP8, 98, "VP8", 90000);
    sdpAddCodec(video, SDP_CODEC_H264, 96, "H264", 90000);
    sdpAddExtmap(video, 2, "urn:ietf:params:rtp-hdrext:toffset");
    sdpAddExtmap(video, 3, "urn:3gpp:video-orientation");
    video->ssrc = 2001;
    strncpy(video->cname, "caller@mini-rtc", sizeof(video->cname) - 1);
    strncpy(video->msid, "video-sender", sizeof(video->msid) - 1);
    strncpy(video->track_id, "videotrack-1", sizeof(video->track_id) - 1);

    char offer_buf[4096];
    generateSessionDescription(&offer, offer_buf, sizeof(offer_buf));
    printf("Offer SDP:\n%s\n", offer_buf);

    SessionDescription answer;
    if (generateAnswer(&offer, &answer) == 0) {
        char answer_buf[4096];
        generateSessionDescription(&answer, answer_buf, sizeof(answer_buf));
        printf("Answer SDP:\n%s\n", answer_buf);
    }

    /* --- 2. ICE Connectivity Check --- */
    printf("[2] ICE Candidate Gathering and Connectivity Check\n");

    IceSession ice;
    iceSessionCreate(&ice, true);
    iceSessionSetLocalCredentials(&ice, "aZq1", "XrP3kL9mN2sQ5vW8");
    iceSessionSetRemoteCredentials(&ice, "bYr2", "YsQ4kL0nN3tR6wX9");

    IceCandidateLocal host_cand = {
        .foundation = "1",
        .component_id = 1,
        .type = ICE_CAND_TYPE_HOST,
        .address = "192.168.1.100",
        .port = 52000,
        .priority = calcIcePriority(ICE_CAND_TYPE_HOST, 65535, 1)
    };
    iceSessionAddLocalCandidate(&ice, &host_cand);

    IceCandidateLocal remote_host = {
        .foundation = "1",
        .component_id = 1,
        .type = ICE_CAND_TYPE_HOST,
        .address = "10.0.0.50",
        .port = 49000,
        .priority = calcIcePriority(ICE_CAND_TYPE_HOST, 65535, 1)
    };
    iceSessionAddRemoteCandidate(&ice, &remote_host);

    iceSessionBuildCheckList(&ice);
    iceSessionStartChecks(&ice);
    printf("ICE state: checking, pairs: %d\n", ice.pair_count);

    /* Simulate a successful check */
    uint8_t tid[12] = {1,2,3,4,5,6,7,8,9,10,11,12};
    StunMessage req, resp;
    stunBuildBindingRequest(&req, tid);
    stunAddIceControlling(&req, ice.tiebreaker);
    stunAddPriority(&req, host_cand.priority);
    stunAddUsername(&req, "aZq1:bYr2");

    stunBuildBindingResponse(&resp, tid, "192.168.1.100", 52000);

    iceSessionProcessBindingResponse(&ice, &resp, "10.0.0.50", 49000);
    printf("ICE state after check: %s\n",
           ice.state == ICE_STATE_CONNECTED ? "connected" : "not connected");

    /* --- 3. DTLS-SRTP Handshake --- */
    printf("[3] DTLS-SRTP Handshake\n");

    DtlsSrtpContext dtls_client, dtls_server;
    dtlsSrtpInit(&dtls_client, false);
    dtlsSrtpInit(&dtls_server, true);
    dtlsSrtpCreateCertificate(&dtls_client);
    dtlsSrtpCreateCertificate(&dtls_server);

    char fp[160];
    dtlsSrtpGetLocalFingerprint(&dtls_client, fp, sizeof(fp));
    printf("Client fingerprint: %s\n", fp);

    uint8_t dtls_out[4096], dtls_in[4096];
    size_t dtls_out_len = 0, dtls_in_len = 0;

    dtlsDoHandshake(&dtls_client, NULL, 0, dtls_out, sizeof(dtls_out), &dtls_out_len);
    printf("ClientHello sent: %zu bytes\n", dtls_out_len);

    memcpy(dtls_in, dtls_out, dtls_out_len);
    dtls_in_len = dtls_out_len;
    dtlsDoHandshake(&dtls_server, dtls_in, dtls_in_len, dtls_out, sizeof(dtls_out), &dtls_out_len);
    printf("Server response: %zu bytes\n", dtls_out_len);

    dtlsSrtpDeriveKeys(&dtls_client);
    printf("DTLS-SRTP keys derived (state: %d)\n", dtls_client.state);

    /* --- 4. RTP/RTCP Media --- */
    printf("[4] RTP Media Packet Construction\n");

    RtpPacket rtp;
    rtpBuildHeader(&rtp, VP8_PAYLOAD_TYPE, 100, 48000, 2001);
    rtpSetMarker(&rtp, true);

    uint8_t vp8_data[] = {0x10, 0x80, 0x01, 0xDE, 0xAD, 0xBE, 0xEF};
    rtp.payload_len = sizeof(vp8_data);
    memcpy(rtp.payload, vp8_data, rtp.payload_len);

    uint8_t rtp_buf[2048]; size_t rtp_len;
    rtpSerializeHeader(&rtp, rtp_buf, sizeof(rtp_buf), &rtp_len);
    printf("RTP header serialized: %zu bytes (marker=%d, pt=%d, seq=%d, ts=%u, ssrc=%u)\n",
           rtp_len, rtp.marker, rtp.payload_type,
           rtp.sequence_number, rtp.timestamp, rtp.ssrc);

    /* Parse back */
    RtpPacket parsed;
    rtpParseHeader(&parsed, rtp_buf, rtp_len + rtp.payload_len);
    printf("RTP parsed: pt=%d, seq=%d, ts=%u, ssrc=%u, payload=%zu bytes\n",
           parsed.payload_type, parsed.sequence_number, parsed.timestamp,
           parsed.ssrc, parsed.payload_len);

    /* --- 5. Opus Audio Frame --- */
    printf("[5] Opus Audio Frame\n");

    OpusFrame opus;
    opusParseToc(0x78, &opus);
    printf("Opus TOC parsed: bandwidth=%d, frame_count=%d, stereo=%d\n",
           opus.bandwidth, opus.frame_count, opus.stereo);

    uint8_t opus_data[] = {0xF8, 0xFF, 0xFE};
    opus.data_len = 3;
    memcpy(opus.data, opus_data, 3);

    uint8_t opus_buf[256]; size_t opus_len;
    opusPackFrame(&opus, opus_buf, sizeof(opus_buf), &opus_len);
    printf("Opus frame packed: %zu bytes\n", opus_len);

    /* --- 6. RTCP Sender Report --- */
    printf("[6] RTCP Sender Report\n");

    RtcpSenderReport sr = {
        .ssrc = 2001,
        .ntp_timestamp = trackNtpNow(),
        .rtp_timestamp = 48000,
        .packet_count = 150,
        .octet_count = 150000
    };

    uint8_t rtcp_buf[256]; size_t rtcp_len;
    rtcpBuildSenderReport(rtcp_buf, sizeof(rtcp_buf), &sr, &rtcp_len);
    printf("RTCP SR built: %zu bytes (NTP=%llu, RTP=%u, packets=%u)\n",
           rtcp_len, (unsigned long long)sr.ntp_timestamp,
           sr.rtp_timestamp, sr.packet_count);

    RtcpSenderReport sr_parsed;
    if (rtcpParseSenderReport(rtcp_buf, rtcp_len, &sr_parsed) == 0)
        printf("RTCP SR parsed: ssrc=%u, packets=%u, octets=%u\n",
               sr_parsed.ssrc, sr_parsed.packet_count, sr_parsed.octet_count);

    /* --- 7. RTCP REMB --- */
    printf("[7] RTCP REMB\n");

    RtcpRemb remb = {
        .ssrc = 2001,
        .bitrate_bps = 1500000,
        .ssrc_count = 1,
        .ssrcs = {1001}
    };

    uint8_t remb_buf[256]; size_t remb_len;
    rtcpBuildRemb(remb_buf, sizeof(remb_buf), &remb, &remb_len);
    printf("REMB built: %zu bytes, bitrate=%llu bps\n",
           remb_len, (unsigned long long)remb.bitrate_bps);

    RtcpRemb remb_parsed;
    if (rtcpParseRemb(remb_buf, remb_len, &remb_parsed) == 0)
        printf("REMB parsed: bitrate=%llu bps\n",
               (unsigned long long)remb_parsed.bitrate_bps);

    /* --- 8. SRTP Encryption --- */
    printf("[8] SRTP Encrypt/Decrypt\n");

    uint8_t srtp_key[16] = {0};
    uint8_t srtp_salt[14] = {0};
    for (int i = 0; i < 16; i++) srtp_key[i] = (uint8_t)(i * 17 + 3);
    for (int i = 0; i < 14; i++) srtp_salt[i] = (uint8_t)(i * 11 + 7);

    SrtpPacket srtp_pkt = {
        .seq = 100,
        .ssrc = 2001,
        .payload_len = 32,
        .roc = 0
    };
    for (size_t i = 0; i < srtp_pkt.payload_len; i++)
        srtp_pkt.payload[i] = (uint8_t)i;

    size_t prot_len;
    srtpProtect(&srtp_pkt, srtp_key, srtp_salt, &prot_len);
    printf("SRTP protected: payload=%zu, encrypted=%d, auth=%d\n",
           srtp_pkt.payload_len, srtp_pkt.encrypted, srtp_pkt.authenticated);

    if (srtpUnprotect(&srtp_pkt, srtp_key, srtp_salt) == 0)
        printf("SRTP unprotected OK: payload=%zu\n", srtp_pkt.payload_len);

    /* --- 9. H264 FU-A Fragmentation --- */
    printf("[9] H264 FU-A Fragmentation\n");

    uint8_t idr_nalu[128];
    idr_nalu[0] = (uint8_t)((2 << 5) | H264_NAL_TYPE_IDR);
    for (int i = 1; i < 128; i++) idr_nalu[i] = (uint8_t)(i * 7);

    H264FuA fus[8];
    int fu_count = 0;
    if (h264BuildFuA(idr_nalu, sizeof(idr_nalu), 1200, fus, &fu_count) == 0) {
        printf("IDR NAL split into %d FU-A fragments\n", fu_count);
        uint8_t rebuilt[256]; size_t rebuilt_len;
        if (h264CombineFuA(fus, fu_count, rebuilt, &rebuilt_len) == 0)
            printf("FU-A recombined: %zu bytes, keyframe=%d\n",
                   rebuilt_len, h264IsKeyFrame(rebuilt, rebuilt_len));
    }

    /* --- Cleanup --- */
    printf("\n=== Demo Complete ===\n");

    iceSessionDestroy(&ice);
    dtlsSrtpClose(&dtls_client);
    dtlsSrtpClose(&dtls_server);

    return 0;
}
