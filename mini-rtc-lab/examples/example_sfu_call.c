#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sfu_mcu.h"
#include "media_track.h"
#include "sdp_signaling.h"

static void print_bwe_state(const BweState *bwe, const char *label) {
    uint32_t target, min_r, max_r;
    float loss;
    bweGetState((BweState *)bwe, &target, &min_r, &max_r, &loss);
    printf("  %s: target=%u kbps, loss=%.2f%%\n", label, target / 1000, loss * 100.0f);
}

int main(void) {
    printf("=== SFU Simulcast Demo ===\n\n");

    /* --- 1. Initialize SFU --- */
    SfuContext sfu;
    sfuInit(&sfu, 2500000);
    printf("[1] SFU initialized (bandwidth limit: %u kbps)\n", sfu.bandwidth_limit_bps / 1000);

    /* --- 2. Add participants --- */
    const char *participants[] = {"alice", "bob", "charlie", "diana"};
    for (int i = 0; i < 4; i++) {
        sfuAddParticipant(&sfu, participants[i]);
        printf("    Added participant: %s\n", participants[i]);
    }

    /* --- 3. Add tracks with simulcast layers --- */
    for (int i = 0; i < 4; i++) {
        SfuParticipant *p = sfuFindParticipant(&sfu, participants[i]);

        sfuAddTrack(&sfu, participants[i], "audio", 0);
        p->audio_ssrc = (uint32_t)(1000 + i * 100 + 1);
        sfuAddTrack(&sfu, participants[i], "video", 1);
        p->video_ssrc = (uint32_t)(1000 + i * 100 + 2);

        sfuAddSimulcastLayer(&sfu, participants[i], "video",
            SFU_LAYER_HIGH, "h", (uint32_t)(2000 + i * 100), 1920, 1080, 2500000);
        sfuAddSimulcastLayer(&sfu, participants[i], "video",
            SFU_LAYER_MEDIUM, "m", (uint32_t)(3000 + i * 100), 640, 480, 800000);
        sfuAddSimulcastLayer(&sfu, participants[i], "video",
            SFU_LAYER_LOW, "l", (uint32_t)(4000 + i * 100), 320, 180, 200000);
    }
    printf("[2] Tracks with simulcast layers added for %d participants\n", 4);

    /* --- 4. Simulate audio levels and active speaker --- */
    sfuUpdateAudioLevel(&sfu, "alice", -20);
    sfuUpdateAudioLevel(&sfu, "bob", -45);
    sfuUpdateAudioLevel(&sfu, "charlie", -30);
    sfuUpdateAudioLevel(&sfu, "diana", -50);

    sfuSelectActiveSpeaker(&sfu);
    printf("[3] Active speaker selection:\n");
    for (int i = 0; i < sfu.participant_count; i++) {
        SfuParticipant *p = &sfu.participants[i];
        printf("    %s: level=%d dB, active=%s\n",
               p->participant_id, p->audio_level,
               p->is_active_speaker ? "YES" : "no");
    }

    /* --- 5. Select layers based on active speaker --- */
    printf("[4] Layer selection:\n");
    SfuParticipant *active = sfuFindParticipant(&sfu, "alice");
    if (active && active->is_active_speaker) {
        sfuSelectLayer(&sfu, "alice", "video", SFU_LAYER_HIGH);
        printf("    alice (active speaker): HIGH (1080p)\n");
    }
    for (int i = 1; i < 4; i++) {
        sfuSelectLayer(&sfu, participants[i], "video", SFU_LAYER_LOW);
        printf("    %s: LOW (180p)\n", participants[i]);
    }

    /* --- 6. Simulate RTP packet forwarding --- */
    printf("[5] Packet forwarding simulation:\n");

    RtpPacket in_pkt;
    rtpBuildHeader(&in_pkt, VP8_PAYLOAD_TYPE, 500, 90000, 2000);
    in_pkt.payload_len = 1200;
    memset(in_pkt.payload, 0xAB, 1200);

    for (int i = 0; i < 4; i++) {
        SfuParticipant *receiver = &sfu.participants[i];
        if (strcmp(receiver->participant_id, "alice") == 0) continue;
        RtpPacket out_pkt;
        if (sfuForwardPacket(&sfu, &in_pkt, "alice", receiver->participant_id, &out_pkt) == 0) {
            printf("    Forwarded alice->%s: ssrc=%u, seq=%d\n",
                   receiver->participant_id, out_pkt.ssrc, out_pkt.sequence_number);
        }
    }

    /* --- 7. Bandwidth Estimation --- */
    printf("[6] Bandwidth Estimation (BWE):\n");

    BweState bwe;
    bweInit(&bwe, 300000, 5000000);
    printf("    Initial: %u kbps\n", bweGetTargetBitrate(&bwe) / 1000);

    BwePacketInfo pkts[10];
    for (int i = 0; i < 10; i++) {
        pkts[i].timestamp_us = (uint64_t)(i * 33000);
        pkts[i].size_bytes = 1200;
        pkts[i].received = true;
        bweUpdateOnPacket(&bwe, &pkts[i]);
    }
    print_bwe_state(&bwe, "After 10 packets");

    uint32_t acked;
    bweUpdateOnFeedback(&bwe, 500000, &acked, 0.05f);
    print_bwe_state(&bwe, "After 5%% loss feedback");

    bweUpdateOnFeedback(&bwe, 600000, &acked, 0.01f);
    print_bwe_state(&bwe, "After 1%% loss (increase)");

    bweUpdateOnRemb(&bwe, 2000000);
    print_bwe_state(&bwe, "After REMB 2000 kbps");

    bweUpdateOnFeedback(&bwe, 700000, &acked, 0.15f);
    print_bwe_state(&bwe, "After 15%% loss (decrease)");

    /* --- 8. REMB Distribution --- */
    printf("[7] REMB-based bandwidth distribution:\n");

    RtcpRemb remb = {
        .ssrc = 2000,
        .bitrate_bps = 2500000,
        .ssrc_count = 1,
    };

    sfuHandleRemb(&sfu, &remb);
    for (int i = 0; i < sfu.participant_count; i++) {
        SfuParticipant *p = &sfu.participants[i];
        uint32_t video_br = 0;
        for (int j = 0; j < p->track_count; j++) {
            if (p->tracks[j].media_type == 1)
                video_br = (uint32_t)p->tracks[j].bitrate_bps;
        }
        printf("    %s: video bitrate=%u kbps\n",
               p->participant_id, video_br / 1000);
    }

    /* --- 9. SFU Stats --- */
    printf("[8] SFU Per-Track Statistics:\n");
    for (int i = 0; i < sfu.participant_count; i++) {
        const char *pid = sfu.participants[i].participant_id;
        uint32_t br;
        float loss;
        if (sfuGetStats(&sfu, pid, "video", &br, &loss) == 0)
            printf("    %s/video: bitrate=%u kbps, loss=%.2f%%\n",
                   pid, br / 1000, loss * 100.0f);
    }

    /* --- 10. MCU Simulation --- */
    printf("[9] MCU Composite Simulation:\n");

    McuContext mcu;
    mcuInit(&mcu, MCU_COMPOSITE_WIDTH, MCU_COMPOSITE_HEIGHT, 9999, VP8_PAYLOAD_TYPE);

    mcuAddInput(&mcu, 640, 480);
    mcuAddInput(&mcu, 640, 480);
    mcuAddInput(&mcu, 640, 480);
    mcuAddInput(&mcu, 640, 480);
    printf("    MCU initialized with %d inputs (%dx%d -> %dx%d)\n",
           mcu.input_count, 1920 / 2, 1080 / 2,
           mcu.output_width, mcu.output_height);

    uint8_t input_frame[] = {0x10, 0xFF, 0xAB, 0xCD};
    uint8_t output_frame[4096];
    size_t out_len;
    bool is_keyframe;

    mcuProcessFrame(&mcu, 0, input_frame, sizeof(input_frame), output_frame, &out_len, &is_keyframe);
    printf("    Processed frame: output=%zu bytes, keyframe=%s\n",
           out_len, is_keyframe ? "yes" : "no");

    mcuRequestKeyFrame(&mcu, 0);
    printf("    Key frame requested for input 0\n");

    mcuSwitchLayout(&mcu, 1);
    printf("    Layout switched to grid\n");

    mcuClose(&mcu);

    /* --- 11. Simulcast RID/MID Identification --- */
    printf("[10] Simulcast RID/MID Layer Mapping:\n");
    SfuParticipant *alice = sfuFindParticipant(&sfu, "alice");
    if (alice) {
        for (int j = 0; j < alice->track_count; j++) {
            SfuTrack *t = &alice->tracks[j];
            if (t->media_type == 1) {
                printf("    Track: %s, current_layer=%d\n", t->track_id, t->current_layer);
                for (int k = 0; k < t->layer_count; k++) {
                    printf("      Layer %s: %dx%d @ %u kbps (ssrc=%u, active=%d)\n",
                           t->layers[k].rid, t->layers[k].width, t->layers[k].height,
                           t->layers[k].target_bitrate_bps / 1000,
                           t->layers[k].ssrc, t->layers[k].active);
                }
            }
        }
    }

    /* --- 12. Bandwidth limit enforcement --- */
    printf("[11] Bandwidth Limit Enforcement:\n");
    sfuApplyBandwidthLimit(&sfu, 1000000);
    printf("    Applied 1 Mbps limit\n");
    for (int i = 0; i < sfu.participant_count; i++) {
        uint32_t br; float loss;
        sfuGetStats(&sfu, sfu.participants[i].participant_id, "video", &br, &loss);
        printf("    %s: video bitrate=%u kbps\n",
               sfu.participants[i].participant_id, br / 1000);
    }

    /* --- 13. Participant leave --- */
    printf("[12] Participant Leave:\n");
    sfuRemoveParticipant(&sfu, "diana");
    printf("    diana removed. Remaining: %d participants\n", sfu.participant_count);

    printf("\n=== SFU Demo Complete ===\n");
    return 0;
}
