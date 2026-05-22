#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "sfu_mcu.h"
#include "sdp_signaling.h"
#include "media_track.h"
#include "ice_stun_turn.h"
#include "dtls_srtp.h"

typedef struct {
    char id[64];
    uint32_t audio_ssrc;
    uint32_t video_ssrc;
    int audio_seq;
    int video_seq;
    uint32_t audio_ts;
    uint32_t video_ts;
    bool connected;
    int audio_level;
} Client;

static SfuContext sfu;
static Client clients[16];
static int client_count = 0;
static BweState global_bwe;

static uint32_t rand_ssrc(void) {
    return ((uint32_t)rand() << 16) ^ (uint32_t)rand();
}

static void client_add(const char *id) {
    if (client_count >= 16) return;
    Client *c = &clients[client_count++];
    memset(c, 0, sizeof(*c));
    strncpy(c->id, id, sizeof(c->id) - 1);
    c->audio_ssrc = rand_ssrc();
    c->video_ssrc = rand_ssrc();
    c->audio_level = -(rand() % 50);
    c->connected = true;

    sfuAddParticipant(&sfu, id);
    sfuAddTrack(&sfu, id, "audio", 0);
    sfuAddTrack(&sfu, id, "video", 1);

    sfuAddSimulcastLayer(&sfu, id, "video", SFU_LAYER_HIGH, "h",
        c->video_ssrc + 1, 1920, 1080, 2500000);
    sfuAddSimulcastLayer(&sfu, id, "video", SFU_LAYER_MEDIUM, "m",
        c->video_ssrc + 2, 640, 480, 800000);
    sfuAddSimulcastLayer(&sfu, id, "video", SFU_LAYER_LOW, "l",
        c->video_ssrc + 3, 320, 180, 200000);

    SfuParticipant *p = sfuFindParticipant(&sfu, id);
    if (p) {
        p->audio_ssrc = c->audio_ssrc;
        p->video_ssrc = c->video_ssrc;
    }
}

static void client_remove(const char *id) {
    sfuRemoveParticipant(&sfu, id);
    for (int i = 0; i < client_count; i++) {
        if (strcmp(clients[i].id, id) == 0) {
            clients[i] = clients[client_count - 1];
            client_count--;
            return;
        }
    }
}

static void sfu_forward_media(const char *from_id, const char *to_id, int media_type, int seq, uint32_t ts) {
    SfuParticipant *from = sfuFindParticipant(&sfu, from_id);
    SfuParticipant *to = sfuFindParticipant(&sfu, to_id);
    if (!from || !to) return;

    int pt = media_type ? VP8_PAYLOAD_TYPE : OPUS_PAYLOAD_TYPE;
    uint32_t ssrc = media_type ? from->video_ssrc : from->audio_ssrc;

    RtpPacket in_pkt;
    memset(&in_pkt, 0, sizeof(in_pkt));
    rtpBuildHeader(&in_pkt, pt, (uint16_t)seq, ts, ssrc);
    in_pkt.payload_len = media_type ? (size_t)1200 : (size_t)160;
    memset(in_pkt.payload, (uint8_t)(seq & 0xFF), in_pkt.payload_len);

    RtpPacket out_pkt;
    if (sfuForwardPacket(&sfu, &in_pkt, from_id, to_id, &out_pkt) == 0) {
        /* packet forwarded successfully */
    }
}

static void update_audio_levels(void) {
    for (int i = 0; i < client_count; i++) {
        int level = clients[i].audio_level + (rand() % 10) - 5;
        if (level > 0) level = 0;
        if (level < -127) level = -127;
        clients[i].audio_level = level;
        sfuUpdateAudioLevel(&sfu, clients[i].id, level);
    }
}

static void print_conference_status(int tick) {
    printf("\n=== Conference Status (tick %d) ===\n", tick);
    printf("Participants: %d\n", client_count);

    sfuSelectActiveSpeaker(&sfu);

    printf("%-12s %-6s %-10s %-8s %-10s\n",
           "Client", "Audio", "Active?", "V-BR", "V-Layer");
    printf("-----------------------------------------------\n");

    for (int i = 0; i < client_count; i++) {
        Client *c = &clients[i];
        SfuParticipant *p = sfuFindParticipant(&sfu, c->id);
        if (!p) continue;

        uint32_t vbr; float vloss;
        sfuGetStats(&sfu, c->id, "video", &vbr, &vloss);

        const char *layer_name = "?";
        SfuLayerId layer = sfuGetCurrentLayer(&sfu, c->id, "video");
        if (layer == SFU_LAYER_HIGH) layer_name = "HIGH";
        else if (layer == SFU_LAYER_MEDIUM) layer_name = "MED";
        else if (layer == SFU_LAYER_LOW) layer_name = "LOW";

        printf("%-12s %-4d dB  %-6s    %-5u kbps %-10s\n",
               c->id, c->audio_level,
               p->is_active_speaker ? "YES" : "no",
               vbr / 1000, layer_name);
    }

    uint32_t total_bitrate = 0;
    for (int i = 0; i < client_count; i++) {
        uint32_t br; float loss;
        sfuGetStats(&sfu, clients[i].id, "video", &br, &loss);
        total_bitrate += br;
    }
    printf("-----------------------------------------------\n");
    printf("Total video bitrate: %u kbps\n", total_bitrate / 1000);
    printf("BWE target: %u kbps\n", bweGetTargetBitrate(&global_bwe) / 1000);
}

static void simulate_sfu_operation(int total_ticks) {
    printf("\n--- Starting SFU Simulation (%d ticks) ---\n\n", total_ticks);

    for (int tick = 0; tick < total_ticks; tick++) {
        update_audio_levels();
        sfuSelectActiveSpeaker(&sfu);

        /* Layer selection based on active speaker */
        for (int i = 0; i < client_count; i++) {
            SfuParticipant *p = sfuFindParticipant(&sfu, clients[i].id);
            if (!p) continue;
            if (p->is_active_speaker) {
                sfuSelectLayer(&sfu, clients[i].id, "video", SFU_LAYER_HIGH);
            } else {
                sfuSelectLayer(&sfu, clients[i].id, "video", SFU_LAYER_LOW);
            }
        }

        /* Simulate media forwarding */
        for (int sender = 0; sender < client_count; sender++) {
            for (int receiver = 0; receiver < client_count; receiver++) {
                if (sender == receiver) continue;

                Client *sc = &clients[sender];
                sfu_forward_media(sc->id, clients[receiver].id, 0, sc->audio_seq++, sc->audio_ts);
                sc->audio_ts += 960;

                if (tick % 3 == 0) {
                    sfu_forward_media(sc->id, clients[receiver].id, 1, sc->video_seq++, sc->video_ts);
                    sc->video_ts += 3000;
                }
            }
        }

        /* BWE simulation */
        for (int i = 0; i < client_count; i++) {
            BwePacketInfo info = {
                .timestamp_us = (uint64_t)(tick * 33000 + i * 1000),
                .size_bytes = 1200,
                .received = true,
            };
            bweUpdateOnPacket(&global_bwe, &info);
        }

        /* Simulate varying network conditions */
        float simulated_loss = 0.0f;
        if (tick >= 10 && tick < 15) simulated_loss = 0.08f;
        else if (tick >= 20 && tick < 25) simulated_loss = 0.15f;
        else simulated_loss = 0.02f;

        uint32_t acked;
        uint64_t now_us = (uint64_t)(tick + 1) * 33000 * (uint64_t)client_count;
        bweUpdateOnFeedback(&global_bwe, now_us, &acked, simulated_loss);

        /* Apply bandwidth limits to the SFU */
        uint32_t bwe_target = bweGetTargetBitrate(&global_bwe);
        sfuApplyBandwidthLimit(&sfu, bwe_target);

        /* Distribute REMB */
        RtcpRemb remb = {
            .bitrate_bps = (uint64_t)bwe_target,
            .ssrc_count = 1,
        };
        sfuHandleRemb(&sfu, &remb);

        if (tick % 5 == 0)
            print_conference_status(tick);
    }
}

static void demo_mcu_composite(void) {
    printf("\n=== MCU Video Composite Demo ===\n\n");

    McuContext mcu;
    mcuInit(&mcu, MCU_COMPOSITE_WIDTH, MCU_COMPOSITE_HEIGHT, 9999, VP8_PAYLOAD_TYPE);

    printf("[1] MCU initialized: %dx%d output\n", mcu.output_width, mcu.output_height);

    for (int i = 0; i < 4; i++) {
        mcuAddInput(&mcu, 640, 480);
        printf("    Added input %d (640x480)\n", i);
    }

    int positions[] = {0, 0, 640, 0, 0, 480, 640, 480};
    mcuSetLayout(&mcu, positions, 4);

    for (int frame = 0; frame < 5; frame++) {
        for (int input = 0; input < mcu.input_count; input++) {
            uint8_t fake_encoded[256];
            memset(fake_encoded, (uint8_t)(0x10 + input + frame), sizeof(fake_encoded));
            fake_encoded[0] = 0x10;
            fake_encoded[1] = (uint8_t)(input * 50);

            uint8_t output[4096];
            size_t out_len;
            bool keyframe;

            if (mcuProcessFrame(&mcu, input, fake_encoded, sizeof(fake_encoded),
                                output, &out_len, &keyframe) == 0) {
                if (frame == 0)
                    printf("    Processed frame %d/input %d: %zu bytes, keyframe=%s\n",
                           frame, input, out_len, keyframe ? "yes" : "no");
            }
        }
        if (frame == 0) printf("    ...\n");
    }

    printf("[2] Requesting keyframes for all inputs\n");
    for (int i = 0; i < mcu.input_count; i++)
        mcuRequestKeyFrame(&mcu, i);

    printf("[3] Layout switching demonstration\n");
    const char *layouts[] = {"single-speaker", "2x2 grid", "picture-in-picture", "vertical-strip"};
    for (int l = 1; l <= 4; l++) {
        mcuSwitchLayout(&mcu, l);
        printf("    Switched to %s\n", layouts[l - 1]);
    }

    mcuClose(&mcu);
    printf("[4] MCU composite closed\n");
}

static void demo_bandwidth_adaptation(void) {
    printf("\n=== Bandwidth Adaptation Demo ===\n\n");

    printf("[1] Starting bandwidth test at 300 kbps\n");
    BweState test_bwe;
    bweInit(&test_bwe, 300000, 5000000);
    printf("    Target: %u kbps\n", bweGetTargetBitrate(&test_bwe) / 1000);

    /* Simulate gradually increasing bandwidth */
    uint32_t rates[] = {500, 800, 1200, 2000, 3500, 5000};
    for (int i = 0; i < 6; i++) {
        for (int p = 0; p < 10; p++) {
            BwePacketInfo info = {
                .timestamp_us = (uint64_t)(p * 5000 + i * 50000),
                .size_bytes = (uint16_t)(rates[i] / 8 / 30),
                .received = true,
            };
            bweUpdateOnPacket(&test_bwe, &info);
        }

        uint32_t acked;
        float loss = rates[i] > 2000000 ? 0.05f : 0.01f;
        bweUpdateOnFeedback(&test_bwe, (uint64_t)((i + 1) * 100000), &acked, loss);
        printf("    After rate %u kbps (loss=%.1f%%): target=%u kbps\n",
               rates[i], loss * 100.0f, bweGetTargetBitrate(&test_bwe) / 1000);
    }

    /* Simulate congestion collapse */
    printf("\n[2] Simulating network congestion...\n");
    for (int i = 0; i < 5; i++) {
        uint32_t acked;
        bweUpdateOnFeedback(&test_bwe, (uint64_t)((i + 10) * 100000), &acked, 0.25f);
        printf("    tick %d (25%% loss): target=%u kbps\n",
               i, bweGetTargetBitrate(&test_bwe) / 1000);
    }

    /* Recovery */
    printf("\n[3] Network recovery...\n");
    for (int i = 0; i < 5; i++) {
        uint32_t acked;
        bweUpdateOnFeedback(&test_bwe, (uint64_t)((i + 20) * 100000), &acked, 0.01f);
        printf("    tick %d (1%% loss): target=%u kbps\n",
               i, bweGetTargetBitrate(&test_bwe) / 1000);
    }
}

int main(void) {
    srand((unsigned)time(NULL));
    printf("=====================================\n");
    printf("  SFU Server & MCU Composite Demo\n");
    printf("=====================================\n\n");

    /* Initialize SFU */
    sfuInit(&sfu, 5000000);
    bweInit(&global_bwe, 300000, 5000000);

    /* Add participants */
    printf("[Setup] Adding conference participants\n");
    const char *names[] = {"Alice", "Bob", "Charlie", "Diana", "Eve", "Frank"};
    for (int i = 0; i < 6; i++) {
        client_add(names[i]);
        printf("    + %s (audio=%u, video=%u)\n",
               names[i],
               clients[i].audio_ssrc,
               clients[i].video_ssrc);
    }

    /* Simulate 30 ticks of SFU operation */
    simulate_sfu_operation(30);

    /* Demonstrate participant leave/join dynamics */
    printf("\n=== Participant Dynamics ===\n");
    printf("[1] Removing Frank...\n");
    client_remove("Frank");
    printf("    Remaining: %d participants\n", client_count);

    printf("[2] Adding Grace...\n");
    client_add("Grace");
    printf("    Total: %d participants\n", client_count);

    simulate_sfu_operation(10);

    /* MCU composite demo */
    demo_mcu_composite();

    /* Bandwidth adaptation demo */
    demo_bandwidth_adaptation();

    /* Final stats */
    printf("\n=== Final Conference Statistics ===\n");
    uint32_t total_video_br = 0;
    for (int i = 0; i < client_count; i++) {
        SfuParticipant *p = sfuFindParticipant(&sfu, clients[i].id);
        if (!p) continue;

        uint32_t video_br; float video_loss;
        sfuGetStats(&sfu, clients[i].id, "video", &video_br, &video_loss);
        total_video_br += video_br;

        printf("  %s:\n", clients[i].id);
        printf("    Audio: %d dB, seq=%d\n", clients[i].audio_level, clients[i].audio_seq);
        printf("    Video: %u kbps, loss=%.2f%%, seq=%d, active=%s\n",
               video_br / 1000, video_loss * 100.0f,
               clients[i].video_seq,
               p->is_active_speaker ? "yes" : "no");
    }

    printf("\n  Total: %d clients, %u kbps video\n",
           client_count, total_video_br / 1000);
    printf("  SFU bandwidth limit: %u kbps\n", sfu.bandwidth_limit_bps / 1000);

    printf("\n=== SFU Demo Complete ===\n");
    return 0;
}
