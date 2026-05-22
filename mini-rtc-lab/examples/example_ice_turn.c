#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ice_stun_turn.h"
#include "dtls_srtp.h"
#include "media_track.h"

static void print_ice_state(int state) {
    const char *names[] = {"idle","gathering","checking","connected","completed","failed","disconnected","restart","closed"};
    printf("%s", (state >= 0 && state <= 8) ? names[state] : "unknown");
}

int main(void) {
    printf("=== ICE/STUN/TURN Connectivity Demo ===\n\n");

    /* --- 1. STUN Binding Request/Response --- */
    printf("[1] STUN Binding Request/Response\n");

    uint8_t tid_req[12] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B};

    StunMessage req;
    stunBuildBindingRequest(&req, tid_req);
    stunAddUsername(&req, "testuser:remotepwd");
    stunAddIceControlling(&req, 0x1234567890ABCDEFULL);
    stunAddPriority(&req, 0x7E0001FF);

    uint8_t stun_buf[512]; size_t stun_len;
    stunSerializeMessage(&req, stun_buf, sizeof(stun_buf), &stun_len);
    printf("    Binding request serialized: %zu bytes\n", stun_len);

    StunMessage parsed;
    if (stunParseMessage(stun_buf, stun_len, &parsed) == 0) {
        printf("    Parsed: type=0x%04X, magic=0x%08X\n",
               parsed.msg_type, parsed.magic_cookie);
        uint8_t uname[128]; size_t ulen = sizeof(uname);
        if (stunGetAttribute(&parsed, STUN_ATTR_USERNAME, uname, &ulen) == 0)
            printf("    Username: %s\n", uname);
    }

    StunMessage resp;
    stunBuildBindingResponse(&resp, tid_req, "203.0.113.45", 3478);
    stunAddXorMappedAddress(&resp, "203.0.113.45", 3478);
    stunAddMessageIntegrity(&resp, "remotepwd");
    stunAddFingerprint(&resp);

    uint8_t resp_buf[512]; size_t resp_len;
    stunSerializeMessage(&resp, resp_buf, sizeof(resp_buf), &resp_len);
    printf("    Binding response serialized: %zu bytes\n", resp_len);

    char mapped_addr[64]; int mapped_port;
    if (stunGetXorMappedAddress(&resp, mapped_addr, sizeof(mapped_addr), &mapped_port) == 0)
        printf("    XorMappedAddress: %s:%d\n", mapped_addr, mapped_port);

    /* --- 2. ICE Candidate Priority Calculation --- */
    printf("[2] ICE Candidate Priority Calculation\n");

    uint32_t host_pri = calcIcePriority(ICE_CAND_TYPE_HOST, 65535, 1);
    uint32_t srflx_pri = calcIcePriority(ICE_CAND_TYPE_SRFLX, 65535, 1);
    uint32_t relay_pri = calcIcePriority(ICE_CAND_TYPE_RELAY, 65535, 1);

    printf("    Host:  %u (0x%08X)\n", host_pri, host_pri);
    printf("    Srflx: %u (0x%08X)\n", srflx_pri, srflx_pri);
    printf("    Relay: %u (0x%08X)\n", relay_pri, relay_pri);

    uint64_t pair_pri = calcPairPriority(host_pri, srflx_pri, true);
    printf("    Pair priority (controlling): %llu\n", (unsigned long long)pair_pri);

    /* --- 3. Full ICE Session --- */
    printf("[3] Full ICE Session Lifecycle\n");

    IceSession sess;
    iceSessionCreate(&sess, true);
    printf("    Created (%s role)\n", sess.controlling_role ? "controlling" : "controlled");

    iceSessionSetLocalCredentials(&sess, "ABC123", "pwd12345678");
    iceSessionSetRemoteCredentials(&sess, "XYZ789", "pwd87654321");

    IceCandidateLocal loc[4];
    snprintf(loc[0].foundation, sizeof(loc[0].foundation), "1");
    loc[0].component_id = 1;
    loc[0].type = ICE_CAND_TYPE_HOST;
    strncpy(loc[0].address, "192.168.1.10", sizeof(loc[0].address) - 1);
    loc[0].port = 50000;
    loc[0].priority = calcIcePriority(ICE_CAND_TYPE_HOST, 65535, 1);

    snprintf(loc[1].foundation, sizeof(loc[1].foundation), "2");
    loc[1].component_id = 1;
    loc[1].type = ICE_CAND_TYPE_SRFLX;
    strncpy(loc[1].address, "203.0.113.10", sizeof(loc[1].address) - 1);
    loc[1].port = 50000;
    loc[1].priority = calcIcePriority(ICE_CAND_TYPE_SRFLX, 65535, 1);
    strncpy(loc[1].related_address, "192.168.1.10", sizeof(loc[1].related_address) - 1);
    loc[1].related_port = 50000;

    for (int i = 0; i < 2; i++) iceSessionAddLocalCandidate(&sess, &loc[i]);
    printf("    Local candidates: %d\n", sess.local_count);

    IceCandidateLocal rem[2];
    snprintf(rem[0].foundation, sizeof(rem[0].foundation), "1");
    rem[0].component_id = 1;
    rem[0].type = ICE_CAND_TYPE_HOST;
    strncpy(rem[0].address, "10.0.0.20", sizeof(rem[0].address) - 1);
    rem[0].port = 49000;
    rem[0].priority = calcIcePriority(ICE_CAND_TYPE_HOST, 65535, 1);

    snprintf(rem[1].foundation, sizeof(rem[1].foundation), "2");
    rem[1].component_id = 1;
    rem[1].type = ICE_CAND_TYPE_SRFLX;
    strncpy(rem[1].address, "203.0.113.20", sizeof(rem[1].address) - 1);
    rem[1].port = 49000;
    rem[1].priority = calcIcePriority(ICE_CAND_TYPE_SRFLX, 65535, 1);
    strncpy(rem[1].related_address, "10.0.0.20", sizeof(rem[1].related_address) - 1);
    rem[1].related_port = 49000;

    for (int i = 0; i < 2; i++) iceSessionAddRemoteCandidate(&sess, &rem[i]);
    printf("    Remote candidates: %d\n", sess.remote_count);

    iceSessionBuildCheckList(&sess);
    printf("    Candidate pairs: %d\n", sess.pair_count);

    for (int i = 0; i < sess.pair_count; i++) {
        printf("      Pair %d: %s:%d -> %s:%d (priority=%llu)\n",
               i, sess.pairs[i].local.address, sess.pairs[i].local.port,
               sess.pairs[i].remote.address, sess.pairs[i].remote.port,
               (unsigned long long)sess.pairs[i].pair_priority);
    }

    iceSessionStartChecks(&sess);
    printf("    ICE state: "); print_ice_state(sess.state); printf("\n");

    uint8_t tid_ice[12] = {1,2,3,4,5,6,7,8,9,10,11,12};
    StunMessage bind_resp;
    stunBuildBindingResponse(&bind_resp, tid_ice, "10.0.0.20", 49000);
    stunAddXorMappedAddress(&bind_resp, "10.0.0.20", 49000);

    iceSessionProcessBindingResponse(&sess, &bind_resp, "10.0.0.20", 49000);
    printf("    After response: "); print_ice_state(sess.state); printf("\n");

    iceSessionNominatePair(&sess, &sess.pairs[0]);
    printf("    After nomination: "); print_ice_state(sess.state); printf("\n");

    /* --- 4. ICE Restart --- */
    printf("[4] ICE Restart\n");
    iceSessionRestart(&sess);
    printf("    State after restart: "); print_ice_state(sess.state); printf("\n");
    int still_valid = 0;
    for (int i = 0; i < sess.pair_count; i++) if (sess.pairs[i].valid) still_valid++;
    printf("    Valid pairs: %d\n", still_valid);

    /* --- 5. TURN Allocate --- */
    printf("[5] TURN Allocate Request\n");

    uint8_t tid_turn[12] = {0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1A,0x1B};
    StunMessage alloc_req;
    turnAllocateRequest(&alloc_req, tid_turn, 600);
    stunAddUsername(&alloc_req, "turn-user");
    stunAddMessageIntegrity(&alloc_req, "turn-password");

    uint8_t turn_buf[512]; size_t turn_len;
    stunSerializeMessage(&alloc_req, turn_buf, sizeof(turn_buf), &turn_len);
    printf("    TURN Allocate serialized: %zu bytes (lifetime=600s)\n", turn_len);

    /* --- 6. TURN CreatePermission --- */
    printf("[6] TURN CreatePermission\n");

    uint8_t tid_perm[12] = {0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28,0x29,0x2A,0x2B};
    StunMessage perm_req;
    turnCreatePermissionRequest(&perm_req, tid_perm, "10.0.0.50", 50000);
    stunSerializeMessage(&perm_req, turn_buf, sizeof(turn_buf), &turn_len);
    printf("    Permission request serialized: %zu bytes (peer=10.0.0.50:50000)\n", turn_len);

    /* --- 7. TURN Send Indication --- */
    printf("[7] TURN Send Indication\n");

    uint8_t tid_send[12] = {0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,0x3A,0x3B};
    StunMessage send_ind;
    uint8_t app_data[] = "Hello via TURN relay!";
    turnSendIndication(&send_ind, tid_send, "10.0.0.50", 50000, app_data, sizeof(app_data));
    stunSerializeMessage(&send_ind, turn_buf, sizeof(turn_buf), &turn_len);
    printf("    Send indication serialized: %zu bytes\n", turn_len);

    /* --- 8. TURN Channel Bind --- */
    printf("[8] TURN Channel Bind\n");

    uint8_t tid_ch[12] = {0x40,0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4A,0x4B};
    StunMessage chbind_req;
    turnChannelBindRequest(&chbind_req, tid_ch, 0x4000, "10.0.0.50", 50000);
    stunSerializeMessage(&chbind_req, turn_buf, sizeof(turn_buf), &turn_len);
    printf("    Channel bind serialized: %zu bytes (channel=0x4000)\n", turn_len);

    /* --- 9. TURN Channel Data --- */
    printf("[9] TURN ChannelData Wrap/Unwrap\n");

    uint8_t data[] = "RTP payload via channel";
    uint8_t ch_buf[256]; size_t ch_len;
    turnWrapChannelData(0x4000, data, sizeof(data), ch_buf, sizeof(ch_buf), &ch_len);
    printf("    Wrapped: %zu bytes (header + payload)\n", ch_len);

    uint16_t ch_num;
    uint8_t unwrapped[256]; size_t unwrapped_len;
    if (turnUnwrapChannelData(ch_buf, ch_len, &ch_num, unwrapped, &unwrapped_len) == 0)
        printf("    Unwrapped: channel=0x%04X, data=\"%s\" (%zu bytes)\n",
               ch_num, unwrapped, unwrapped_len);

    /* --- 10. DTLS-SRTP Key Derivation --- */
    printf("[10] DTLS-SRTP Key Derivation\n");

    DtlsSrtpContext dtls;
    dtlsSrtpInit(&dtls, false);
    dtlsSrtpCreateCertificate(&dtls);

    char fp[256];
    dtlsSrtpGetLocalFingerprint(&dtls, fp, sizeof(fp));
    printf("    TLS fingerprint: %s\n", fp);

    for (int i = 0; i < 48; i++) dtls.master_secret[i] = (uint8_t)(i * 3 + 41);
    dtlsSrtpDeriveKeys(&dtls);
    printf("    SRTP keys derived (profile=SRTP_AES128_CM_HMAC_SHA1_80)\n");

    /* --- 11. SRTP/SRTCP Round-Trip --- */
    printf("[11] SRTP/SRTCP Protection\n");

    uint8_t test_payload[64];
    for (int i = 0; i < 64; i++) test_payload[i] = (uint8_t)(i + 0x30);

    size_t enc_len;
    uint8_t encrypted[128];
    srtpEncrypt(dtls.keys.client_write_key, dtls.keys.client_write_salt,
                42, 12345, test_payload, 64, encrypted, &enc_len);
    printf("    RTP encrypted: %zu bytes\n", enc_len);

    uint8_t decrypted[128]; size_t dec_len;
    srtpDecrypt(dtls.keys.server_write_key, dtls.keys.server_write_salt,
                42, 12345, encrypted, enc_len, decrypted, &dec_len);
    printf("    RTP decrypted: %zu bytes, match=%d\n",
           dec_len, memcmp(test_payload, decrypted, 64) == 0);

    uint8_t srtcp_data[32];
    for (int i = 0; i < 32; i++) srtcp_data[i] = (uint8_t)(i);
    uint8_t srtcp_enc[128]; size_t srtcp_enc_len;
    srtcpProtect(dtls.keys.client_write_key, dtls.keys.client_write_salt,
                 srtcp_data, 32, srtcp_enc, &srtcp_enc_len);
    printf("    SRTCP protected: %zu bytes (payload=%d + auth=%d)\n",
           srtcp_enc_len, 32, (int)(srtcp_enc_len - 32));

    uint8_t srtcp_dec[128]; size_t srtcp_dec_len;
    srtcpUnprotect(dtls.keys.server_write_key, dtls.keys.server_write_salt,
                   srtcp_enc, srtcp_enc_len, srtcp_dec, &srtcp_dec_len);
    printf("    SRTCP unprotected: %zu bytes, match=%d\n",
           srtcp_dec_len, memcmp(srtcp_data, srtcp_dec, 32) == 0);

    /* --- Cleanup --- */
    printf("\n=== ICE/STUN/TURN Demo Complete ===\n");
    iceSessionDestroy(&sess);
    dtlsSrtpClose(&dtls);
    return 0;
}
