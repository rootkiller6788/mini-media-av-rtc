#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "sdp_signaling.h"
#include "ice_stun_turn.h"
#include "dtls_srtp.h"
#include "media_track.h"
#include "sfu_mcu.h"

typedef struct {
    char id[64];
    IceSession ice;
    DtlsSrtpContext dtls;
    TrackContext audio_track;
    TrackContext video_track;
    SessionDescription local_sdp;
    SessionDescription remote_sdp;
    uint32_t audio_ssrc;
    uint32_t video_ssrc;
    int audio_seq;
    int video_seq;
    uint32_t audio_ts;
    uint32_t video_ts;
    bool dtls_done;
    bool ice_connected;
} WebRtcEndpoint;

static uint32_t rand_ssrc(void) {
    return (uint32_t)(((uint64_t)rand() << 32) | (uint64_t)rand()) & 0xFFFFFFFF;
}

static void endpoint_init(WebRtcEndpoint *ep, const char *id, bool is_offer) {
    memset(ep, 0, sizeof(*ep));
    strncpy(ep->id, id, sizeof(ep->id) - 1);
    ep->audio_ssrc = rand_ssrc();
    ep->video_ssrc = rand_ssrc();
    ep->audio_seq = rand() & 0xFFFF;
    ep->video_seq = rand() & 0xFFFF;

    iceSessionCreate(&ep->ice, is_offer);
    iceSessionSetLocalCredentials(&ep->ice, id, "password12345");
    dtlsSrtpInit(&ep->dtls, !is_offer);
    dtlsSrtpCreateCertificate(&ep->dtls);
}

static void endpoint_create_offer(WebRtcEndpoint *ep) {
    SessionDescription *sdp = &ep->local_sdp;
    sdp->session_id = (uint32_t)time(NULL);
    sdp->session_version = 1;
    strncpy(sdp->session_name, ep->id, sizeof(sdp->session_name) - 1);
    sdp->bundle_enabled = true;
    strncpy(sdp->group_bundle, "0 1", sizeof(sdp->group_bundle) - 1);

    MediaDescription *audio = &sdp->media[sdp->media_count++];
    audio->type = SDP_MEDIA_AUDIO;
    audio->port = 9;
    strncpy(audio->proto, "UDP/TLS/RTP/SAVPF", sizeof(audio->proto) - 1);
    strncpy(audio->mid, "0", sizeof(audio->mid) - 1);
    audio->rtcp_mux = true;
    audio->bundle = true;
    strncpy(audio->ice_ufrag, ep->ice.local_ufrag, sizeof(audio->ice_ufrag) - 1);
    strncpy(audio->ice_pwd, ep->ice.local_pwd, sizeof(audio->ice_pwd) - 1);
    sdpAddCodec(audio, SDP_CODEC_OPUS, 111, "opus", 48000);
    sdpSetSsrc(audio, ep->audio_ssrc, ep->id, "audio-sender");
    audio->track_id[0] = '\0';
    snprintf(audio->track_id, sizeof(audio->track_id), "audio-%s", ep->id);

    MediaDescription *video = &sdp->media[sdp->media_count++];
    video->type = SDP_MEDIA_VIDEO;
    video->port = 9;
    strncpy(video->proto, "UDP/TLS/RTP/SAVPF", sizeof(video->proto) - 1);
    strncpy(video->mid, "1", sizeof(video->mid) - 1);
    video->rtcp_mux = true;
    video->bundle = true;
    strncpy(video->ice_ufrag, ep->ice.local_ufrag, sizeof(video->ice_ufrag) - 1);
    strncpy(video->ice_pwd, ep->ice.local_pwd, sizeof(video->ice_pwd) - 1);
    sdpAddCodec(video, SDP_CODEC_VP8, 98, "VP8", 90000);
    sdpAddCodec(video, SDP_CODEC_H264, 96, "H264", 90000);
    sdpSetSsrc(video, ep->video_ssrc, ep->id, "video-sender");
    snprintf(video->track_id, sizeof(video->track_id), "video-%s", ep->id);

    char fp[256];
    dtlsSrtpGetLocalFingerprint(&ep->dtls, fp, sizeof(fp));
    strncpy(audio->dtls.fingerprint, fp, sizeof(audio->dtls.fingerprint) - 1);
    strncpy(audio->dtls.hash_algo, "sha-256", sizeof(audio->dtls.hash_algo) - 1);
    strncpy(audio->dtls.setup, "actpass", sizeof(audio->dtls.setup) - 1);
    video->dtls = audio->dtls;
}

static void endpoint_handle_answer(WebRtcEndpoint *ep, const SessionDescription *answer) {
    ep->remote_sdp = *answer;
    for (int i = 0; i < answer->media_count; i++) {
        const MediaDescription *md = &answer->media[i];
        iceSessionSetRemoteCredentials(&ep->ice, md->ice_ufrag, md->ice_pwd);
    }
}

static void endpoint_set_remote(WebRtcEndpoint *ep, const SessionDescription *remote_sdp) {
    ep->remote_sdp = *remote_sdp;
    for (int i = 0; i < remote_sdp->media_count; i++) {
        const MediaDescription *md = &remote_sdp->media[i];
        iceSessionSetRemoteCredentials(&ep->ice, md->ice_ufrag, md->ice_pwd);
        if (md->dtls.fingerprint[0])
            dtlsSrtpSetRemoteFingerprint(&ep->dtls, md->dtls.hash_algo, md->dtls.fingerprint);
    }
}

static void endpoint_gather_ice(WebRtcEndpoint *ep) {
    IceCandidateLocal host;
    memset(&host, 0, sizeof(host));
    snprintf(host.foundation, sizeof(host.foundation), "h-%s", ep->id);
    host.component_id = 1;
    host.type = ICE_CAND_TYPE_HOST;
    strncpy(host.address, "192.168.1.100", sizeof(host.address) - 1);
    host.port = 50000 + (rand() % 10000);
    host.priority = calcIcePriority(ICE_CAND_TYPE_HOST, 65535, 1);
    iceSessionAddLocalCandidate(&ep->ice, &host);

    IceCandidateLocal srflx;
    memset(&srflx, 0, sizeof(srflx));
    snprintf(srflx.foundation, sizeof(srflx.foundation), "s-%s", ep->id);
    srflx.component_id = 1;
    srflx.type = ICE_CAND_TYPE_SRFLX;
    strncpy(srflx.address, "203.0.113.45", sizeof(srflx.address) - 1);
    srflx.port = host.port;
    srflx.priority = calcIcePriority(ICE_CAND_TYPE_SRFLX, 65535, 1);
    strncpy(srflx.related_address, host.address, sizeof(srflx.related_address) - 1);
    srflx.related_port = host.port;
    iceSessionAddLocalCandidate(&ep->ice, &srflx);
}

static void endpoint_add_remote_candidate(WebRtcEndpoint *ep, const IceCandidate *cand) {
    IceCandidateLocal remote;
    memset(&remote, 0, sizeof(remote));
    snprintf(remote.foundation, sizeof(remote.foundation), "%s", cand->foundation);
    remote.component_id = cand->component_id;
    remote.type = strcmp(cand->type, "host") == 0 ? ICE_CAND_TYPE_HOST :
                  strcmp(cand->type, "srflx") == 0 ? ICE_CAND_TYPE_SRFLX :
                  ICE_CAND_TYPE_RELAY;
    strncpy(remote.address, cand->address, sizeof(remote.address) - 1);
    remote.port = cand->port;
    remote.priority = cand->priority;
    iceSessionAddRemoteCandidate(&ep->ice, &remote);
}

static void endpoint_run_ice(WebRtcEndpoint *ep, WebRtcEndpoint *peer) {
    endpoint_gather_ice(ep);
    endpoint_gather_ice(peer);

    IceCandidate peer_host;
    memset(&peer_host, 0, sizeof(peer_host));
    snprintf(peer_host.foundation, sizeof(peer_host.foundation), "h-%s", peer->id);
    peer_host.component_id = 1;
    peer_host.type[0] = 'h';
    strncpy(peer_host.address, peer->ice.local_candidates[0].address, sizeof(peer_host.address) - 1);
    peer_host.port = peer->ice.local_candidates[0].port;
    peer_host.priority = peer->ice.local_candidates[0].priority;
    endpoint_add_remote_candidate(ep, &peer_host);

    IceCandidate ep_host;
    memset(&ep_host, 0, sizeof(ep_host));
    snprintf(ep_host.foundation, sizeof(ep_host.foundation), "h-%s", ep->id);
    ep_host.component_id = 1;
    ep_host.type[0] = 'h';
    strncpy(ep_host.address, ep->ice.local_candidates[0].address, sizeof(ep_host.address) - 1);
    ep_host.port = ep->ice.local_candidates[0].port;
    ep_host.priority = ep->ice.local_candidates[0].priority;
    endpoint_add_remote_candidate(peer, &ep_host);

    iceSessionBuildCheckList(&ep->ice);
    iceSessionBuildCheckList(&peer->ice);
    iceSessionStartChecks(&ep->ice);
    iceSessionStartChecks(&peer->ice);

    uint8_t tid[12] = {0};
    for (int i = 0; i < 12; i++) tid[i] = (uint8_t)(i + (rand() & 0xFF));

    for (int i = 0; i < ep->ice.pair_count; i++) {
        IceCandidatePair *pair = &ep->ice.pairs[i];
        StunMessage resp;
        stunBuildBindingResponse(&resp, tid, pair->remote.address, pair->remote.port);
        stunAddXorMappedAddress(&resp, pair->remote.address, pair->remote.port);
        iceSessionProcessBindingResponse(&ep->ice, &resp, pair->remote.address, pair->remote.port);

        for (int j = 0; j < peer->ice.pair_count; j++) {
            IceCandidatePair *peer_pair = &peer->ice.pairs[j];
            if (strcmp(peer_pair->remote.address, pair->local.address) == 0 &&
                peer_pair->remote.port == pair->local.port) {
                StunMessage peer_resp;
                stunBuildBindingResponse(&peer_resp, tid, peer_pair->remote.address, peer_pair->remote.port);
                stunAddXorMappedAddress(&peer_resp, peer_pair->remote.address, peer_pair->remote.port);
                iceSessionProcessBindingResponse(&peer->ice, &peer_resp, peer_pair->remote.address, peer_pair->remote.port);
            }
        }
    }

    if (ep->ice.state == ICE_STATE_CONNECTED && peer->ice.state == ICE_STATE_CONNECTED) {
        for (int i = 0; i < ep->ice.pair_count; i++) {
            if (ep->ice.pairs[i].valid) {
                iceSessionNominatePair(&ep->ice, &ep->ice.pairs[i]);
                break;
            }
        }
        for (int i = 0; i < peer->ice.pair_count; i++) {
            if (peer->ice.pairs[i].valid) {
                iceSessionNominatePair(&peer->ice, &peer->ice.pairs[i]);
                break;
            }
        }
        ep->ice_connected = true;
        peer->ice_connected = true;
    }
}

static void endpoint_do_dtls(WebRtcEndpoint *ep, WebRtcEndpoint *peer) {
    uint8_t out_buf[4096], in_buf[4096];
    size_t out_len = 0, in_len = 0;

    dtlsDoHandshake(&ep->dtls, NULL, 0, out_buf, sizeof(out_buf), &out_len);
    memcpy(in_buf, out_buf, out_len);
    in_len = out_len;
    dtlsDoHandshake(&peer->dtls, in_buf, in_len, out_buf, sizeof(out_buf), &out_len);
    if (out_len > 0) {
        dtlsDoHandshake(&ep->dtls, out_buf, out_len, out_buf, sizeof(out_buf), &out_len);
    }
    if (ep->dtls.state == DTLS_STATE_READY || ep->dtls.state == DTLS_STATE_CONNECTING) {
        dtlsSrtpDeriveKeys(&ep->dtls);
        ep->dtls_done = true;
    }
    if (peer->dtls.state == DTLS_STATE_READY || peer->dtls.state == DTLS_STATE_CONNECTING) {
        dtlsSrtpDeriveKeys(&peer->dtls);
        peer->dtls_done = true;
    }
}

static void endpoint_send_audio(WebRtcEndpoint *ep, WebRtcEndpoint *target, int frame_ms) {
    if (!ep->ice_connected || !ep->dtls_done) return;

    RtpPacket rtp;
    memset(&rtp, 0, sizeof(rtp));
    rtpBuildHeader(&rtp, OPUS_PAYLOAD_TYPE, (uint16_t)(ep->audio_seq++ & 0xFFFF), ep->audio_ts, ep->audio_ssrc);
    ep->audio_ts += (uint32_t)(OPUS_SAMPLE_RATE * frame_ms / 1000);

    uint8_t opus_frame[64];
    for (int i = 0; i < 40; i++) opus_frame[i] = (uint8_t)((ep->audio_seq + i) & 0xFF);
    rtp.payload_len = 40;
    memcpy(rtp.payload, opus_frame, 40);

    SrtpPacket srtp_pkt = {
        .seq = rtp.sequence_number,
        .ssrc = rtp.ssrc,
        .payload_len = rtp.payload_len,
        .roc = 0
    };
    memcpy(srtp_pkt.payload, rtp.payload, rtp.payload_len);

    size_t prot_len;
    srtpProtect(&srtp_pkt, ep->dtls.keys.client_write_key, ep->dtls.keys.client_write_salt, &prot_len);
}

static void endpoint_send_video(WebRtcEndpoint *ep, WebRtcEndpoint *target, int width, int height) {
    if (!ep->ice_connected || !ep->dtls_done) return;

    RtpPacket rtp;
    memset(&rtp, 0, sizeof(rtp));
    rtpBuildHeader(&rtp, VP8_PAYLOAD_TYPE, (uint16_t)(ep->video_seq++ & 0xFFFF), ep->video_ts, ep->video_ssrc);
    ep->video_ts += 3000;

    uint8_t vp8_desc[4] = {0x10, 0x80, (uint8_t)(ep->video_seq >> 8), (uint8_t)(ep->video_seq)};
    uint8_t fake_payload[1024];
    memset(fake_payload, 0xA5, 500);
    memcpy(rtp.payload, vp8_desc, 4);
    memcpy(rtp.payload + 4, fake_payload, 500);
    rtp.payload_len = 504;
    rtpSetMarker(&rtp, (ep->video_seq % 30 == 0));

    SrtpPacket srtp_pkt = {
        .seq = rtp.sequence_number,
        .ssrc = rtp.ssrc,
        .payload_len = rtp.payload_len,
        .roc = 0
    };
    memcpy(srtp_pkt.payload, rtp.payload, rtp.payload_len);
    size_t prot_len;
    srtpProtect(&srtp_pkt, ep->dtls.keys.client_write_key, ep->dtls.keys.client_write_salt, &prot_len);
}

static void endpoint_send_rtcp_sr(WebRtcEndpoint *ep) {
    if (!ep->ice_connected || !ep->dtls_done) return;

    RtcpSenderReport sr = {
        .ssrc = ep->video_ssrc,
        .ntp_timestamp = trackNtpNow(),
        .rtp_timestamp = ep->video_ts,
        .packet_count = (uint32_t)(ep->video_seq & 0xFFFF),
        .octet_count = (uint32_t)(ep->video_seq * 1200)
    };

    uint8_t buf[256]; size_t len;
    rtcpBuildSenderReport(buf, sizeof(buf), &sr, &len);

    uint8_t srtcp_buf[256]; size_t srtcp_len;
    srtcpProtect(ep->dtls.keys.client_write_key, ep->dtls.keys.client_write_salt,
                 buf, len, srtcp_buf, &srtcp_len);
}

static void endpoint_cleanup(WebRtcEndpoint *ep) {
    iceSessionDestroy(&ep->ice);
    dtlsSrtpClose(&ep->dtls);
}

int main(void) {
    srand((unsigned)time(NULL));
    printf("=== WebRTC Endpoint-to-Endpoint Demo ===\n");
    printf("Simulates a full PeerConnection between two endpoints\n\n");

    printf("[1] Initializing endpoints\n");
    WebRtcEndpoint caller, callee;
    endpoint_init(&caller, "caller", true);
    endpoint_init(&callee, "callee", false);
    printf("    caller: audio_ssrc=%u, video_ssrc=%u\n", caller.audio_ssrc, caller.video_ssrc);
    printf("    callee: audio_ssrc=%u, video_ssrc=%u\n", callee.audio_ssrc, callee.video_ssrc);

    printf("\n[2] Creating SDP offer\n");
    endpoint_create_offer(&caller);
    char offer_buf[4096];
    generateSessionDescription(&caller.local_sdp, offer_buf, sizeof(offer_buf));
    printf("%s\n", offer_buf);

    printf("[3] Generating SDP answer\n");
    SessionDescription answer;
    generateAnswer(&caller.local_sdp, &answer);
    char answer_buf[4096];
    generateSessionDescription(&answer, answer_buf, sizeof(answer_buf));

    endpoint_handle_answer(&callee, &answer);
    endpoint_set_remote(&caller, &answer);
    endpoint_set_remote(&callee, &caller.local_sdp);
    printf("%s\n", answer_buf);

    printf("[4] ICE connectivity check\n");
    endpoint_run_ice(&caller, &callee);
    printf("    caller ICE: %s\n", caller.ice_connected ? "connected" : "failed");
    printf("    callee ICE: %s\n", callee.ice_connected ? "connected" : "failed");

    if (caller.ice_connected && callee.ice_connected) {
        printf("\n[5] DTLS-SRTP handshake\n");
        endpoint_do_dtls(&caller, &callee);
        printf("    caller DTLS: %s\n", caller.dtls_done ? "ready" : "pending");
        printf("    callee DTLS: %s\n", callee.dtls_done ? "ready" : "pending");

        printf("\n[6] Media transmission simulation\n");
        printf("    Sending 10 audio frames (20ms each)...\n");
        for (int i = 0; i < 10; i++) {
            endpoint_send_audio(&caller, &callee, 20);
            endpoint_send_audio(&callee, &caller, 20);
        }

        printf("    Sending 5 video frames (33ms each, 30fps)...\n");
        for (int i = 0; i < 5; i++) {
            endpoint_send_video(&caller, &callee, 1280, 720);
            endpoint_send_video(&callee, &caller, 1280, 720);
        }

        printf("    Sending RTCP Sender Reports...\n");
        endpoint_send_rtcp_sr(&caller);
        endpoint_send_rtcp_sr(&callee);

        printf("\n[7] RTP statistics\n");
        printf("    caller: audio_seq=%d, video_seq=%d, audio_ts=%u, video_ts=%u\n",
               caller.audio_seq, caller.video_seq, caller.audio_ts, caller.video_ts);
        printf("    callee: audio_seq=%d, video_seq=%d, audio_ts=%u, video_ts=%u\n",
               callee.audio_seq, callee.video_seq, callee.audio_ts, callee.video_ts);

        printf("\n[8] BW estimation during call\n");
        BweState bwe;
        bweInit(&bwe, 300000, 5000000);

        for (int i = 0; i < 20; i++) {
            BwePacketInfo info = {
                .timestamp_us = (uint64_t)(i * 33000),
                .size_bytes = 1200,
                .received = true,
            };
            bweUpdateOnPacket(&bwe, &info);
        }

        uint32_t acked_br;
        bweUpdateOnFeedback(&bwe, 1000000, &acked_br, 0.02f);
        printf("    Target bitrate: %u kbps (loss=2%%)\n", bweGetTargetBitrate(&bwe) / 1000);

        bweUpdateOnRemb(&bwe, 1500000);
        printf("    After REMB 1.5Mbps: %u kbps\n", bweGetTargetBitrate(&bwe) / 1000);
    }

    printf("\n[9] Call teardown\n");
    endpoint_cleanup(&caller);
    endpoint_cleanup(&callee);
    printf("    Endpoints cleaned up\n");

    printf("\n=== Demo Complete ===\n");
    return 0;
}
