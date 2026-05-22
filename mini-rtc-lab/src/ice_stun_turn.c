#include "ice_stun_turn.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static uint32_t internal_random(void) {
    static uint32_t seed = 0;
    if (seed == 0) seed = (uint32_t)time(NULL);
    seed = seed * 1103515245 + 12345;
    return seed;
}

int stunBuildBindingRequest(StunMessage *msg, const uint8_t tid[12]) {
    memset(msg, 0, sizeof(*msg));
    msg->msg_type = STUN_BINDING_REQUEST;
    msg->magic_cookie = STUN_MAGIC_COOKIE;
    memcpy(msg->transaction_id, tid, 12);
    msg->attr_count = 0;
    msg->msg_length = 0;
    return 0;
}

int stunBuildBindingResponse(StunMessage *msg, const uint8_t tid[12], const char *mapped_addr, int port) {
    memset(msg, 0, sizeof(*msg));
    msg->msg_type = STUN_BINDING_RESPONSE_SUCCESS;
    msg->magic_cookie = STUN_MAGIC_COOKIE;
    memcpy(msg->transaction_id, tid, 12);
    msg->attr_count = 0;
    stunAddXorMappedAddress(msg, mapped_addr, port);
    return 0;
}

int stunBuildBindingIndication(StunMessage *msg, const uint8_t tid[12]) {
    memset(msg, 0, sizeof(*msg));
    msg->msg_type = STUN_BINDING_INDICATION;
    msg->magic_cookie = STUN_MAGIC_COOKIE;
    memcpy(msg->transaction_id, tid, 12);
    msg->attr_count = 0;
    return 0;
}

int stunAddAttribute(StunMessage *msg, uint16_t type, const uint8_t *data, uint16_t len) {
    if (msg->attr_count >= STUN_MAX_ATTRIBUTES) return -1;
    uint8_t *base = msg->attr_data + msg->msg_length;
    base[0] = (uint8_t)(type >> 8);
    base[1] = (uint8_t)(type & 0xFF);
    base[2] = (uint8_t)(len >> 8);
    base[3] = (uint8_t)(len & 0xFF);
    if (data && len) memcpy(base + 4, data, len);
    int padded = (len + 3) & ~3;
    msg->msg_length += 4 + padded;
    msg->attr_count++;
    return 0;
}

int stunAddXorMappedAddress(StunMessage *msg, const char *addr, int port) {
    uint8_t buf[16];
    memset(buf, 0, sizeof(buf));
    buf[1] = 0x01;
    uint16_t xport = port ^ (uint16_t)(STUN_MAGIC_COOKIE >> 16);
    buf[2] = (uint8_t)(xport >> 8);
    buf[3] = (uint8_t)(xport & 0xFF);
    uint32_t ip_parts[4] = {0};
    sscanf(addr, "%u.%u.%u.%u", &ip_parts[0], &ip_parts[1], &ip_parts[2], &ip_parts[3]);
    uint32_t ip = (ip_parts[0] << 24) | (ip_parts[1] << 16) | (ip_parts[2] << 8) | ip_parts[3];
    ip ^= STUN_MAGIC_COOKIE;
    buf[4] = (uint8_t)(ip >> 24);
    buf[5] = (uint8_t)(ip >> 16);
    buf[6] = (uint8_t)(ip >> 8);
    buf[7] = (uint8_t)(ip);
    return stunAddAttribute(msg, STUN_ATTR_XOR_MAPPED_ADDRESS, buf, 8);
}

int stunAddMappedAddress(StunMessage *msg, const char *addr, int port) {
    uint8_t buf[8];
    memset(buf, 0, sizeof(buf));
    buf[1] = 0x01;
    buf[2] = (uint8_t)(port >> 8);
    buf[3] = (uint8_t)(port & 0xFF);
    uint32_t ip_parts[4] = {0};
    sscanf(addr, "%u.%u.%u.%u", &ip_parts[0], &ip_parts[1], &ip_parts[2], &ip_parts[3]);
    buf[4] = (uint8_t)ip_parts[0];
    buf[5] = (uint8_t)ip_parts[1];
    buf[6] = (uint8_t)ip_parts[2];
    buf[7] = (uint8_t)ip_parts[3];
    return stunAddAttribute(msg, STUN_ATTR_MAPPED_ADDRESS, buf, 8);
}

int stunAddIceControlling(StunMessage *msg, uint64_t tiebreaker) {
    uint8_t buf[8];
    for (int i = 0; i < 8; i++) buf[i] = (uint8_t)(tiebreaker >> (56 - i * 8));
    return stunAddAttribute(msg, STUN_ATTR_ICE_CONTROLLING, buf, 8);
}

int stunAddIceControlled(StunMessage *msg, uint64_t tiebreaker) {
    uint8_t buf[8];
    for (int i = 0; i < 8; i++) buf[i] = (uint8_t)(tiebreaker >> (56 - i * 8));
    return stunAddAttribute(msg, STUN_ATTR_ICE_CONTROLLED, buf, 8);
}

int stunAddPriority(StunMessage *msg, uint32_t priority) {
    uint8_t buf[4];
    buf[0] = (uint8_t)(priority >> 24);
    buf[1] = (uint8_t)(priority >> 16);
    buf[2] = (uint8_t)(priority >> 8);
    buf[3] = (uint8_t)(priority);
    return stunAddAttribute(msg, STUN_ATTR_PRIORITY, buf, 4);
}

int stunAddUseCandidate(StunMessage *msg) {
    return stunAddAttribute(msg, STUN_ATTR_USE_CANDIDATE, NULL, 0);
}

int stunAddUsername(StunMessage *msg, const char *username) {
    return stunAddAttribute(msg, STUN_ATTR_USERNAME, (const uint8_t *)username, (uint16_t)strlen(username));
}

static void sha1_hmac_sim(const uint8_t *key, size_t keylen, const uint8_t *data, size_t datalen, uint8_t *out) {
    uint32_t h0 = 0x67452301, h1 = 0xEFCDAB89, h2 = 0x98BADCFE, h3 = 0x10325476, h4 = 0xC3D2E1F0;
    size_t total = datalen + 64;
    for (size_t i = 0; i < total; i++) {
        uint8_t b = i < datalen ? data[i] : 0;
        h0 = (h0 << 5 | h0 >> 27) + h1 + b + (uint32_t)i;
        h1 = (h1 << 3 | h1 >> 29) ^ h2 + b;
        h2 = (h2 << 7 | h2 >> 25) + h3 ^ b;
        h3 = (h3 << 11 | h3 >> 21) + h4 + b;
        h4 = (h4 << 13 | h4 >> 19) ^ h0 + b + (uint32_t)keylen;
    }
    out[0] = (uint8_t)(h0 >> 24); out[1] = (uint8_t)(h0 >> 16);
    out[2] = (uint8_t)(h0 >> 8);  out[3] = (uint8_t)(h0);
    out[4] = (uint8_t)(h1 >> 24); out[5] = (uint8_t)(h1 >> 16);
    out[6] = (uint8_t)(h1 >> 8);  out[7] = (uint8_t)(h1);
    out[8] = (uint8_t)(h2 >> 24); out[9] = (uint8_t)(h2 >> 16);
    out[10] = (uint8_t)(h2 >> 8); out[11] = (uint8_t)(h2);
    out[12] = (uint8_t)(h3 >> 24); out[13] = (uint8_t)(h3 >> 16);
    out[14] = (uint8_t)(h3 >> 8); out[15] = (uint8_t)(h3);
    out[16] = (uint8_t)(h4 >> 24); out[17] = (uint8_t)(h4 >> 16);
    out[18] = (uint8_t)(h4 >> 8); out[19] = (uint8_t)(h4);
}

int stunAddMessageIntegrity(StunMessage *msg, const char *password) {
    uint8_t hmac[20];
    sha1_hmac_sim((const uint8_t *)password, strlen(password), msg->attr_data, msg->msg_length, hmac);
    return stunAddAttribute(msg, STUN_ATTR_MESSAGE_INTEGRITY, hmac, 20);
}

int stunAddFingerprint(StunMessage *msg) {
    uint32_t crc = msg->msg_length ^ STUN_MAGIC_COOKIE;
    uint8_t buf[4];
    buf[0] = (uint8_t)(crc >> 24); buf[1] = (uint8_t)(crc >> 16);
    buf[2] = (uint8_t)(crc >> 8);  buf[3] = (uint8_t)(crc);
    return stunAddAttribute(msg, STUN_ATTR_FINGERPRINT, buf, 4);
}

int stunSerializeMessage(const StunMessage *msg, uint8_t *buf, size_t bufsz, size_t *outlen) {
    size_t total = STUN_HEADER_SIZE + msg->msg_length;
    if (bufsz < total) return -1;
    buf[0] = (uint8_t)(msg->msg_type >> 8);
    buf[1] = (uint8_t)(msg->msg_type & 0xFF);
    buf[2] = (uint8_t)(msg->msg_length >> 8);
    buf[3] = (uint8_t)(msg->msg_length & 0xFF);
    buf[4] = (uint8_t)(msg->magic_cookie >> 24);
    buf[5] = (uint8_t)(msg->magic_cookie >> 16);
    buf[6] = (uint8_t)(msg->magic_cookie >> 8);
    buf[7] = (uint8_t)(msg->magic_cookie);
    memcpy(buf + 8, msg->transaction_id, 12);
    memcpy(buf + STUN_HEADER_SIZE, msg->attr_data, msg->msg_length);
    *outlen = total;
    return 0;
}

int stunParseMessage(const uint8_t *buf, size_t len, StunMessage *msg) {
    if (len < STUN_HEADER_SIZE) return -1;
    memset(msg, 0, sizeof(*msg));
    msg->msg_type = ((uint16_t)buf[0] << 8) | buf[1];
    msg->msg_length = ((uint16_t)buf[2] << 8) | buf[3];
    msg->magic_cookie = ((uint32_t)buf[4] << 24) | ((uint32_t)buf[5] << 16) | ((uint32_t)buf[6] << 8) | buf[7];
    memcpy(msg->transaction_id, buf + 8, 12);
    if (len >= STUN_HEADER_SIZE + msg->msg_length) {
        memcpy(msg->attr_data, buf + STUN_HEADER_SIZE, msg->msg_length);
        msg->attr_count = 0;
        size_t offset = 0;
        while (offset + 4 <= msg->msg_length) {
            uint16_t alen = ((uint16_t)msg->attr_data[offset + 2] << 8) | msg->attr_data[offset + 3];
            size_t padded = (alen + 3) & ~3;
            offset += 4 + padded;
            msg->attr_count++;
            if (offset > msg->msg_length) break;
        }
    }
    return 0;
}

int stunGetAttribute(const StunMessage *msg, uint16_t type, uint8_t *out, size_t *outlen) {
    size_t offset = 0;
    while (offset + 4 <= msg->msg_length) {
        uint16_t atype = ((uint16_t)msg->attr_data[offset] << 8) | msg->attr_data[offset + 1];
        uint16_t alen = ((uint16_t)msg->attr_data[offset + 2] << 8) | msg->attr_data[offset + 3];
        if (atype == type) {
            if (out && outlen && *outlen >= alen) {
                memcpy(out, msg->attr_data + offset + 4, alen);
                *outlen = alen;
            }
            return 0;
        }
        size_t padded = (alen + 3) & ~3;
        offset += 4 + padded;
    }
    return -1;
}

int stunGetMappedAddress(const StunMessage *msg, char *addr, size_t addrsz, int *port) {
    uint8_t val[16]; size_t vlen = sizeof(val);
    if (stunGetAttribute(msg, STUN_ATTR_MAPPED_ADDRESS, val, &vlen) < 0) return -1;
    if (vlen < 8) return -1;
    *port = ((int)val[2] << 8) | val[3];
    snprintf(addr, addrsz, "%d.%d.%d.%d", val[4], val[5], val[6], val[7]);
    return 0;
}

int stunGetXorMappedAddress(const StunMessage *msg, char *addr, size_t addrsz, int *port) {
    uint8_t val[16]; size_t vlen = sizeof(val);
    if (stunGetAttribute(msg, STUN_ATTR_XOR_MAPPED_ADDRESS, val, &vlen) < 0) return -1;
    if (vlen < 8) return -1;
    uint16_t xport = ((uint16_t)val[2] << 8) | val[3];
    *port = xport ^ (uint16_t)(STUN_MAGIC_COOKIE >> 16);
    uint32_t ip = ((uint32_t)val[4] << 24) | ((uint32_t)val[5] << 16) | ((uint32_t)val[6] << 8) | val[7];
    ip ^= STUN_MAGIC_COOKIE;
    snprintf(addr, addrsz, "%u.%u.%u.%u", (ip >> 24) & 0xFF, (ip >> 16) & 0xFF, (ip >> 8) & 0xFF, ip & 0xFF);
    return 0;
}

uint32_t calcIcePriority(IceCandidateType type, int local_pref, int component_id) {
    int type_pref = type == ICE_CAND_TYPE_HOST ? 126 :
                    type == ICE_CAND_TYPE_SRFLX ? 100 :
                    type == ICE_CAND_TYPE_RELAY ? 0 : 110;
    return computeIceCandidatePriority(type_pref, local_pref, component_id);
}

uint64_t calcPairPriority(uint32_t local_priority, uint32_t remote_priority, bool controlling) {
    uint64_t g = controlling ? (uint64_t)local_priority : (uint64_t)remote_priority;
    uint64_t d = controlling ? (uint64_t)remote_priority : (uint64_t)local_priority;
    return ((g & 0xFFFFFFFF) << 32) | (d & 0xFFFFFFFF);
}

int iceSessionCreate(IceSession *sess, bool controlling) {
    memset(sess, 0, sizeof(*sess));
    sess->controlling_role = controlling;
    sess->state = ICE_STATE_IDLE;
    sess->tiebreaker = ((uint64_t)internal_random() << 32) | internal_random();
    return 0;
}

int iceSessionSetLocalCredentials(IceSession *sess, const char *ufrag, const char *pwd) {
    strncpy(sess->local_ufrag, ufrag, sizeof(sess->local_ufrag) - 1);
    strncpy(sess->local_pwd, pwd, sizeof(sess->local_pwd) - 1);
    return 0;
}

int iceSessionSetRemoteCredentials(IceSession *sess, const char *ufrag, const char *pwd) {
    strncpy(sess->remote_ufrag, ufrag, sizeof(sess->remote_ufrag) - 1);
    strncpy(sess->remote_pwd, pwd, sizeof(sess->remote_pwd) - 1);
    return 0;
}

int iceSessionAddLocalCandidate(IceSession *sess, const IceCandidateLocal *cand) {
    if (sess->local_count >= ICE_MAX_CANDIDATES) return -1;
    sess->local_candidates[sess->local_count++] = *cand;
    return 0;
}

int iceSessionAddRemoteCandidate(IceSession *sess, const IceCandidateLocal *cand) {
    if (sess->remote_count >= ICE_MAX_CANDIDATES) return -1;
    sess->remote_candidates[sess->remote_count++] = *cand;
    return 0;
}

int iceSessionGatherCandidates(IceSession *sess, int components) {
    (void)components;
    sess->state = ICE_STATE_GATHERING;
    iceSessionBuildCheckList(sess);
    return 0;
}

int iceSessionBuildCheckList(IceSession *sess) {
    sess->pair_count = 0;
    for (int li = 0; li < sess->local_count; li++) {
        for (int ri = 0; ri < sess->remote_count; ri++) {
            if (sess->local_candidates[li].component_id != sess->remote_candidates[ri].component_id)
                continue;
            IceCandidatePair *pair = &sess->pairs[sess->pair_count++];
            memset(pair, 0, sizeof(*pair));
            pair->local = sess->local_candidates[li];
            pair->remote = sess->remote_candidates[ri];
            pair->pair_priority = calcPairPriority(pair->local.priority, pair->remote.priority, sess->controlling_role);
        }
    }
    return 0;
}

int iceSessionStartChecks(IceSession *sess) {
    sess->state = ICE_STATE_CHECKING;
    return 0;
}

int iceSessionProcessBindingRequest(IceSession *sess, const StunMessage *req, const char *src_addr, int src_port) {
    (void)sess; (void)req; (void)src_addr; (void)src_port;
    return 0;
}

int iceSessionProcessBindingResponse(IceSession *sess, const StunMessage *resp, const char *src_addr, int src_port) {
    for (int i = 0; i < sess->pair_count; i++) {
        IceCandidatePair *pair = &sess->pairs[i];
        if (strcmp(pair->remote.address, src_addr) == 0 && pair->remote.port == src_port) {
            pair->valid = true;
            if (pair->nominated) {
                sess->selected_pair = pair;
                sess->state = ICE_STATE_COMPLETED;
                return 0;
            }
            if (!sess->selected_pair) {
                sess->selected_pair = pair;
                sess->state = ICE_STATE_CONNECTED;
            }
            return 0;
        }
    }
    return -1;
}

int iceSessionGetNextCheck(IceSession *sess, IceCandidatePair **pair) {
    (void)sess; (void)pair;
    for (int i = 0; i < sess->pair_count; i++) {
        if (!sess->pairs[i].valid) {
            *pair = &sess->pairs[i];
            return 0;
        }
    }
    return -1;
}

int iceSessionNominatePair(IceSession *sess, IceCandidatePair *pair) {
    pair->nominated = true;
    sess->selected_pair = pair;
    sess->state = ICE_STATE_COMPLETED;
    return 0;
}

int iceSessionRestart(IceSession *sess) {
    sess->state = ICE_STATE_RESTART;
    for (int i = 0; i < sess->pair_count; i++) {
        sess->pairs[i].valid = false;
        sess->pairs[i].nominated = false;
    }
    sess->selected_pair = NULL;
    return 0;
}

int iceSessionDestroy(IceSession *sess) {
    sess->state = ICE_STATE_CLOSED;
    return 0;
}

int turnAllocateRequest(StunMessage *msg, const uint8_t tid[12], int lifetime) {
    memset(msg, 0, sizeof(*msg));
    msg->msg_type = (uint16_t)((STUN_METHOD_ALLOCATE & 0x0FFF) | STUN_MSG_REQUEST);
    msg->magic_cookie = STUN_MAGIC_COOKIE;
    memcpy(msg->transaction_id, tid, 12);
    uint8_t lt_buf[4] = {(uint8_t)(lifetime >> 24), (uint8_t)(lifetime >> 16),
                         (uint8_t)(lifetime >> 8), (uint8_t)(lifetime)};
    stunAddAttribute(msg, 0x000D, lt_buf, 4);
    return 0;
}

int turnRefreshRequest(StunMessage *msg, const uint8_t tid[12], int lifetime) {
    return turnAllocateRequest(msg, tid, lifetime);
}

int turnCreatePermissionRequest(StunMessage *msg, const uint8_t tid[12], const char *peer_addr, int peer_port) {
    memset(msg, 0, sizeof(*msg));
    msg->msg_type = (uint16_t)((STUN_METHOD_CREATE_PERMISSION & 0x0FFF) | STUN_MSG_REQUEST);
    msg->magic_cookie = STUN_MAGIC_COOKIE;
    memcpy(msg->transaction_id, tid, 12);
    stunAddXorMappedAddress(msg, peer_addr, peer_port);
    return 0;
}

int turnSendIndication(StunMessage *msg, const uint8_t tid[12], const char *peer_addr, int peer_port, const uint8_t *data, size_t len) {
    memset(msg, 0, sizeof(*msg));
    msg->msg_type = (uint16_t)((STUN_METHOD_SEND & 0x0FFF) | STUN_MSG_INDICATION);
    msg->magic_cookie = STUN_MAGIC_COOKIE;
    memcpy(msg->transaction_id, tid, 12);
    stunAddXorMappedAddress(msg, peer_addr, peer_port);
    stunAddAttribute(msg, 0x0013, data, (uint16_t)len);
    return 0;
}

int turnChannelBindRequest(StunMessage *msg, const uint8_t tid[12], uint16_t channel_num, const char *peer_addr, int peer_port) {
    memset(msg, 0, sizeof(*msg));
    msg->msg_type = (uint16_t)((STUN_METHOD_CHANNEL_BIND & 0x0FFF) | STUN_MSG_REQUEST);
    msg->magic_cookie = STUN_MAGIC_COOKIE;
    memcpy(msg->transaction_id, tid, 12);
    uint8_t cn[4] = {(uint8_t)(channel_num >> 8), (uint8_t)(channel_num), 0, 0};
    stunAddAttribute(msg, 0x000C, cn, 4);
    stunAddXorMappedAddress(msg, peer_addr, peer_port);
    return 0;
}

int turnWrapChannelData(uint16_t channel_num, const uint8_t *data, size_t len, uint8_t *out, size_t outsz, size_t *written) {
    if (outsz < len + 4) return -1;
    out[0] = (uint8_t)(channel_num >> 8);
    out[1] = (uint8_t)(channel_num);
    out[2] = (uint8_t)(len >> 8);
    out[3] = (uint8_t)(len);
    memcpy(out + 4, data, len);
    *written = len + 4;
    return 0;
}

int turnUnwrapChannelData(const uint8_t *buf, size_t len, uint16_t *channel_num, uint8_t *data, size_t *datalen) {
    if (len < 4) return -1;
    *channel_num = ((uint16_t)buf[0] << 8) | buf[1];
    size_t payload_len = ((size_t)buf[2] << 8) | buf[3];
    if (len < 4 + payload_len) return -1;
    memcpy(data, buf + 4, payload_len);
    *datalen = payload_len;
    return 0;
}
