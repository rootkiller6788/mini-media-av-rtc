#include "dtls_srtp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static uint32_t dtls_rand(void) {
    static uint32_t s = 0;
    if (!s) s = (uint32_t)time(NULL);
    s = s * 1103515245 + 12345;
    return s;
}

static void dtls_rand_bytes(uint8_t *buf, size_t len) {
    for (size_t i = 0; i < len; i++) buf[i] = (uint8_t)(dtls_rand() & 0xFF);
}

int dtlsSrtpInit(DtlsSrtpContext *ctx, bool is_server) {
    memset(ctx, 0, sizeof(*ctx));
    ctx->is_server = is_server;
    ctx->state = DTLS_STATE_NEW;
    ctx->epoch = 0;
    dtls_rand_bytes(ctx->local_random, 32);
    ctx->cipher_suite = 0xC02F;
    return 0;
}

int dtlsSrtpCreateCertificate(DtlsSrtpContext *ctx) {
    ctx->local_cert_len = 256;
    dtls_rand_bytes(ctx->local_cert_der, ctx->local_cert_len);
    snprintf(ctx->keys.tls_id, sizeof(ctx->keys.tls_id), "mini-rtc-%08x", dtls_rand());
    return 0;
}

int dtlsSrtpSetRemoteFingerprint(DtlsSrtpContext *ctx, const char *hash, const char *fingerprint) {
    strncpy(ctx->keys.tls_fingerprint, fingerprint, sizeof(ctx->keys.tls_fingerprint) - 1);
    (void)hash;
    return 0;
}

int dtlsSrtpGetLocalFingerprint(DtlsSrtpContext *ctx, char *fingerprint, size_t len) {
    if (ctx->local_cert_len == 0) return -1;
    uint32_t hash = 0;
    for (size_t i = 0; i < ctx->local_cert_len; i++)
        hash = (hash * 31) + ctx->local_cert_der[i];
    snprintf(fingerprint, len,
             "AA:BB:CC:DD:EE:FF:%02X:%02X:%02X:%02X:%02X:%02X"
             ":11:22:33:44:55:66:%02X:%02X:%02X:%02X:%02X:%02X"
             ":%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X",
             (hash >> 24) & 0xFF, (hash >> 16) & 0xFF, (hash >> 8) & 0xFF, hash & 0xFF,
             dtls_rand() & 0xFF, dtls_rand() & 0xFF, dtls_rand() & 0xFF,
             dtls_rand() & 0xFF, dtls_rand() & 0xFF, dtls_rand() & 0xFF,
             dtls_rand() & 0xFF, dtls_rand() & 0xFF, dtls_rand() & 0xFF,
             dtls_rand() & 0xFF, dtls_rand() & 0xFF, dtls_rand() & 0xFF,
             dtls_rand() & 0xFF, dtls_rand() & 0xFF);
    return 0;
}

static int buildRecord(DtlsContentType ct, uint16_t epoch, const uint8_t *seq, const uint8_t *frag, uint16_t len, uint8_t *buf, size_t bufsz, size_t *written) {
    size_t total = 13 + len;
    if (bufsz < total) return -1;
    buf[0] = (uint8_t)ct;
    buf[1] = 0xFE; buf[2] = 0xFD;
    buf[3] = (uint8_t)(epoch >> 8); buf[4] = (uint8_t)(epoch);
    memcpy(buf + 5, seq, 6);
    buf[11] = (uint8_t)(len >> 8); buf[12] = (uint8_t)(len);
    memcpy(buf + 13, frag, len);
    *written = total;
    return 0;
}

static uint8_t dtls_seq[6] = {0};

int dtlsBuildClientHello(DtlsSrtpContext *ctx, uint8_t *buf, size_t bufsz, size_t *written) {
    uint8_t hello[DTLS_MAX_HANDSHAKE_SIZE];
    int off = 0;
    hello[off++] = DTLS_HANDSHAKE_CLIENT_HELLO;
    hello[off++] = 0; hello[off++] = 0; hello[off++] = 0;
    hello[off++] = (uint8_t)(ctx->handshake_seq >> 8); hello[off++] = (uint8_t)(ctx->handshake_seq++);
    hello[off++] = 0; hello[off++] = 0; hello[off++] = 0;
    hello[off++] = 0xFE; hello[off++] = 0xFD;
    memcpy(hello + off, ctx->local_random, 32); off += 32;
    hello[off++] = 0;
    hello[off++] = 0; hello[off++] = 0x02;
    hello[off++] = 0xC0; hello[off++] = 0x2F;
    hello[off++] = 1; hello[off++] = 0;
    hello[off++] = 0; hello[off++] = 0;
    int body_len = off;
    hello[1] = (uint8_t)((body_len - 4) >> 16);
    hello[2] = (uint8_t)((body_len - 4) >> 8);
    hello[3] = (uint8_t)((body_len - 4));
    hello[6] = (uint8_t)((body_len - 4) >> 16);
    hello[7] = (uint8_t)((body_len - 4) >> 8);
    hello[8] = (uint8_t)((body_len - 4));
    ctx->state = DTLS_STATE_CONNECTING;
    return buildRecord(DTLS_CONTENT_HANDSHAKE, ctx->epoch, dtls_seq, hello, off, buf, bufsz, written);
}

int dtlsBuildServerHello(DtlsSrtpContext *ctx, uint8_t *buf, size_t bufsz, size_t *written) {
    uint8_t hello[256];
    int off = 0;
    hello[off++] = DTLS_HANDSHAKE_SERVER_HELLO;
    hello[off++] = 0; hello[off++] = 0; hello[off++] = 0;
    hello[off++] = (uint8_t)(ctx->handshake_seq >> 8); hello[off++] = (uint8_t)(ctx->handshake_seq++);
    hello[off++] = 0; hello[off++] = 0; hello[off++] = 0;
    hello[off++] = 0xFE; hello[off++] = 0xFD;
    memcpy(hello + off, ctx->local_random, 32); off += 32;
    hello[off++] = 0;
    hello[off++] = 0xC0; hello[off++] = 0x2F;
    hello[off++] = 0;
    hello[off++] = 0; hello[off++] = 0;
    int body_len = off;
    hello[1] = (uint8_t)((body_len - 4) >> 16);
    hello[2] = (uint8_t)((body_len - 4) >> 8);
    hello[3] = (uint8_t)((body_len - 4));
    hello[6] = (uint8_t)((body_len - 4) >> 16);
    hello[7] = (uint8_t)((body_len - 4) >> 8);
    hello[8] = (uint8_t)((body_len - 4));
    return buildRecord(DTLS_CONTENT_HANDSHAKE, ctx->epoch, dtls_seq, hello, off, buf, bufsz, written);
}

int dtlsBuildCertificate(DtlsSrtpContext *ctx, uint8_t *buf, size_t bufsz, size_t *written) {
    size_t cert_len = ctx->local_cert_len;
    size_t hdr = 12;
    size_t total = hdr + 3 + cert_len;
    uint8_t *cert = malloc(total);
    if (!cert) return -1;
    cert[0] = DTLS_HANDSHAKE_CERTIFICATE;
    cert[1] = (uint8_t)((total - 4) >> 16);
    cert[2] = (uint8_t)((total - 4) >> 8);
    cert[3] = (uint8_t)((total - 4));
    cert[4] = (uint8_t)(ctx->handshake_seq >> 8);
    cert[5] = (uint8_t)(ctx->handshake_seq++);
    cert[6] = (uint8_t)((total - 4) >> 16);
    cert[7] = (uint8_t)((total - 4) >> 8);
    cert[8] = (uint8_t)((total - 4));
    cert[9]  = (uint8_t)((cert_len + 3) >> 16);
    cert[10] = (uint8_t)((cert_len + 3) >> 8);
    cert[11] = (uint8_t)((cert_len + 3));
    cert[12] = (uint8_t)(cert_len >> 16);
    cert[13] = (uint8_t)(cert_len >> 8);
    cert[14] = (uint8_t)(cert_len);
    memcpy(cert + 15, ctx->local_cert_der, cert_len);
    int ret = buildRecord(DTLS_CONTENT_HANDSHAKE, ctx->epoch, dtls_seq, cert, (uint16_t)total, buf, bufsz, written);
    free(cert);
    return ret;
}

int dtlsBuildClientKeyExchange(DtlsSrtpContext *ctx, uint8_t *buf, size_t bufsz, size_t *written) {
    uint8_t cke[80];
    memset(cke, 0, sizeof(cke));
    cke[0] = DTLS_HANDSHAKE_CLIENT_KEY_EXCHANGE;
    cke[4] = (uint8_t)(ctx->handshake_seq >> 8);
    cke[5] = (uint8_t)(ctx->handshake_seq++);
    cke[9] = 0; cke[10] = 48;
    dtls_rand_bytes(ctx->premaster_secret, 48);
    memcpy(cke + 11, ctx->premaster_secret, 48);
    int total = 59;
    cke[1] = (uint8_t)((total - 4) >> 16);
    cke[2] = (uint8_t)((total - 4) >> 8);
    cke[3] = (uint8_t)((total - 4));
    cke[6] = (uint8_t)((total - 4) >> 16);
    cke[7] = (uint8_t)((total - 4) >> 8);
    cke[8] = (uint8_t)((total - 4));
    return buildRecord(DTLS_CONTENT_HANDSHAKE, ctx->epoch, dtls_seq, cke, total, buf, bufsz, written);
}

int dtlsBuildFinished(DtlsSrtpContext *ctx, uint8_t *buf, size_t bufsz, size_t *written) {
    uint8_t fin[64];
    memset(fin, 0, sizeof(fin));
    fin[0] = DTLS_HANDSHAKE_FINISHED;
    fin[6] = (uint8_t)(ctx->handshake_seq >> 8);
    fin[7] = (uint8_t)(ctx->handshake_seq++);
    int body = 20;
    fin[1] = (uint8_t)((body) >> 16);
    fin[2] = (uint8_t)((body) >> 8);
    fin[3] = (uint8_t)(body);
    fin[8] = (uint8_t)((body) >> 16);
    fin[9] = (uint8_t)((body) >> 8);
    fin[10] = (uint8_t)(body);
    dtls_rand_bytes(fin + 11, 12);
    int total = 11 + 12;
    return buildRecord(DTLS_CONTENT_HANDSHAKE, ctx->epoch, dtls_seq, fin, total, buf, bufsz, written);
}

int dtlsBuildChangeCipherSpec(DtlsSrtpContext *ctx, uint8_t *buf, size_t bufsz, size_t *written) {
    uint8_t ccs[1] = {1};
    return buildRecord(DTLS_CONTENT_CHANGE_CIPHER_SPEC, ctx->epoch, dtls_seq, ccs, 1, buf, bufsz, written);
}

int dtlsParseHandshake(DtlsSrtpContext *ctx, const uint8_t *buf, size_t len, DtlsHandshake *out) {
    (void)ctx;
    if (len < 12) return -1;
    memset(out, 0, sizeof(*out));
    out->msg_type = (DtlsHandshakeType)buf[0];
    out->length = ((uint32_t)buf[1] << 16) | ((uint32_t)buf[2] << 8) | buf[3];
    out->message_seq = ((uint16_t)buf[4] << 8) | buf[5];
    out->fragment_offset = ((uint32_t)buf[6] << 16) | ((uint32_t)buf[7] << 8) | buf[8];
    out->fragment_length = ((uint32_t)buf[9] << 16) | ((uint32_t)buf[10] << 8) | buf[11];
    return 0;
}

int dtlsDoHandshake(DtlsSrtpContext *ctx, const uint8_t *input, size_t input_len, uint8_t *output, size_t outsz, size_t *written) {
    *written = 0;
    if (ctx->state == DTLS_STATE_READY) return 0;
    if (input && input_len >= 13) {
        DtlsContentType ct = (DtlsContentType)input[0];
        if (ct == DTLS_CONTENT_HANDSHAKE) {
            if (!ctx->is_server) {
                dtlsBuildClientKeyExchange(ctx, output, outsz, written);
                ctx->state = DTLS_STATE_READY;
                return 0;
            }
        }
    }
    if (ctx->is_server && ctx->state == DTLS_STATE_NEW) {
        dtlsBuildServerHello(ctx, output, outsz, written);
        ctx->state = DTLS_STATE_CONNECTING;
        return 0;
    }
    if (!ctx->is_server && ctx->state == DTLS_STATE_NEW) {
        dtlsBuildClientHello(ctx, output, outsz, written);
        return 0;
    }
    return 0;
}

int dtlsSrtpDeriveKeys(DtlsSrtpContext *ctx) {
    for (int i = 0; i < DTLS_SRTP_KEY_LEN; i++)
        ctx->keys.client_write_key[i] = (uint8_t)(ctx->master_secret[i % 48] ^ 0x36);
    for (int i = 0; i < DTLS_SRTP_KEY_LEN; i++)
        ctx->keys.server_write_key[i] = (uint8_t)(ctx->master_secret[i % 48] ^ 0x5C);
    for (int i = 0; i < DTLS_SRTP_SALT_LEN; i++) {
        ctx->keys.client_write_salt[i] = (uint8_t)(ctx->master_secret[(i + 16) % 48] ^ 0xAA);
        ctx->keys.server_write_salt[i] = (uint8_t)(ctx->master_secret[(i + 16) % 48] ^ 0x55);
    }
    return 0;
}

int srtpEncrypt(const uint8_t *key, const uint8_t *salt, uint16_t seq, uint32_t ssrc, const uint8_t *payload, size_t payload_len, uint8_t *out, size_t *outlen) {
    uint8_t iv[16];
    memset(iv, 0, sizeof(iv));
    memcpy(iv, salt, DTLS_SRTP_SALT_LEN);
    iv[14] = (uint8_t)(seq >> 8); iv[15] = (uint8_t)(seq);
    size_t i;
    for (i = 0; i < payload_len; i++) {
        uint8_t ks = key[i % DTLS_SRTP_KEY_LEN] ^ iv[i % 14];
        out[i] = payload[i] ^ ks;
    }
    *outlen = payload_len;
    (void)ssrc;
    return 0;
}

int srtpDecrypt(const uint8_t *key, const uint8_t *salt, uint16_t seq, uint32_t ssrc, const uint8_t *data, size_t datalen, uint8_t *out, size_t *outlen) {
    return srtpEncrypt(key, salt, seq, ssrc, data, datalen, out, outlen);
}

int srtpComputeAuthTag(const uint8_t *key, const uint8_t *data, size_t len, uint8_t roc, uint8_t *tag) {
    memset(tag, 0, SRTP_AUTH_TAG_LEN);
    for (size_t i = 0; i < len && i < (size_t)SRTP_AUTH_TAG_LEN; i++)
        tag[i % SRTP_AUTH_TAG_LEN] ^= data[i] ^ key[i % DTLS_SRTP_KEY_LEN];
    for (int i = 0; i < SRTP_AUTH_TAG_LEN; i++)
        tag[i] ^= roc;
    return 0;
}

int srtpVerifyAuthTag(const uint8_t *key, const uint8_t *data, size_t len, uint8_t roc, const uint8_t *tag) {
    uint8_t computed[SRTP_AUTH_TAG_LEN];
    srtpComputeAuthTag(key, data, len, roc, computed);
    return memcmp(computed, tag, SRTP_AUTH_TAG_LEN) == 0 ? 0 : -1;
}

int srtpProtect(SrtpPacket *pkt, const uint8_t *key, const uint8_t *salt, size_t *outlen) {
    size_t enc_len;
    srtpEncrypt(key, salt, pkt->seq, pkt->ssrc, pkt->payload, pkt->payload_len, pkt->payload, &enc_len);
    srtpComputeAuthTag(key, pkt->payload, enc_len, pkt->roc, pkt->auth_tag);
    pkt->encrypted = true;
    pkt->authenticated = true;
    *outlen = enc_len + SRTP_AUTH_TAG_LEN;
    return 0;
}

int srtpUnprotect(SrtpPacket *pkt, const uint8_t *key, const uint8_t *salt) {
    if (srtpVerifyAuthTag(key, pkt->payload, pkt->payload_len, pkt->roc, pkt->auth_tag) != 0)
        return -1;
    return srtpDecrypt(key, salt, pkt->seq, pkt->ssrc, pkt->payload, pkt->payload_len, pkt->payload, &pkt->payload_len);
}

int srtcpProtect(const uint8_t *key, const uint8_t *salt, const uint8_t *data, size_t len, uint8_t *out, size_t *outlen) {
    memcpy(out, data, len);
    uint8_t roc = 0;
    srtpComputeAuthTag(key, data, len, roc, out + len);
    *outlen = len + SRTCP_AUTH_TAG_LEN;
    (void)salt;
    return 0;
}

int srtcpUnprotect(const uint8_t *key, const uint8_t *salt, const uint8_t *data, size_t len, uint8_t *out, size_t *outlen) {
    if (len < SRTCP_AUTH_TAG_LEN) return -1;
    size_t payload_len = len - SRTCP_AUTH_TAG_LEN;
    uint8_t roc = 0;
    if (srtpVerifyAuthTag(key, data, payload_len, roc, data + payload_len) != 0) return -1;
    memcpy(out, data, payload_len);
    *outlen = payload_len;
    (void)salt;
    return 0;
}

void dtlsSrtpClose(DtlsSrtpContext *ctx) {
    ctx->state = DTLS_STATE_CLOSED;
}
