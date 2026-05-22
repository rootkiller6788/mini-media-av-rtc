#ifndef DTLS_SRTP_H
#define DTLS_SRTP_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "sdp_signaling.h"

#define DTLS_MTU 1350
#define DTLS_MAX_HANDSHAKE_SIZE (64 * 1024)
#define DTLS_SRTP_KEY_LEN 16
#define DTLS_SRTP_SALT_LEN 14
#define DTLS_SRTP_MASTER_KEY_LEN 16
#define DTLS_SRTP_MASTER_SALT_LEN 14
#define SRTP_AUTH_TAG_LEN 10
#define SRTP_MAX_PAYLOAD 1460
#define SRTCP_AUTH_TAG_LEN 10
#define DTLS_FINGERPRINT_ALGO "sha-256"

typedef enum {
    DTLS_STATE_NEW = 0,
    DTLS_STATE_CONNECTING = 1,
    DTLS_STATE_READY = 2,
    DTLS_STATE_CLOSED = 3,
    DTLS_STATE_FAILED = 4
} DtlsConnectionState;

typedef enum {
    DTLS_HANDSHAKE_HELLO_REQUEST = 0,
    DTLS_HANDSHAKE_CLIENT_HELLO = 1,
    DTLS_HANDSHAKE_SERVER_HELLO = 2,
    DTLS_HANDSHAKE_HELLO_VERIFY_REQUEST = 3,
    DTLS_HANDSHAKE_CERTIFICATE = 11,
    DTLS_HANDSHAKE_SERVER_KEY_EXCHANGE = 12,
    DTLS_HANDSHAKE_CERTIFICATE_REQUEST = 13,
    DTLS_HANDSHAKE_SERVER_HELLO_DONE = 14,
    DTLS_HANDSHAKE_CERTIFICATE_VERIFY = 15,
    DTLS_HANDSHAKE_CLIENT_KEY_EXCHANGE = 16,
    DTLS_HANDSHAKE_FINISHED = 20
} DtlsHandshakeType;

typedef enum {
    DTLS_CONTENT_CHANGE_CIPHER_SPEC = 20,
    DTLS_CONTENT_ALERT = 21,
    DTLS_CONTENT_HANDSHAKE = 22,
    DTLS_CONTENT_APPLICATION_DATA = 23
} DtlsContentType;

typedef struct {
    DtlsContentType content_type;
    uint16_t version;
    uint16_t epoch;
    uint8_t sequence_number[6];
    uint16_t length;
    uint8_t *fragment;
} DtlsRecord;

typedef struct {
    DtlsHandshakeType msg_type;
    uint32_t length;
    uint16_t message_seq;
    uint32_t fragment_offset;
    uint32_t fragment_length;
    union {
        struct {
            uint16_t client_version;
            uint8_t random[32];
            uint8_t session_id[32];
            uint8_t session_id_len;
            uint16_t cipher_suites[32];
            int cipher_suite_count;
            uint8_t compression_methods[8];
            int compression_count;
            uint8_t extensions[2048];
            int extensions_len;
        } client_hello;
        struct {
            uint16_t server_version;
            uint8_t random[32];
            uint8_t session_id[32];
            uint8_t session_id_len;
            uint16_t cipher_suite;
            uint8_t compression_method;
            uint8_t extensions[2048];
            int extensions_len;
        } server_hello;
        struct {
            uint8_t *cert_data;
            uint32_t cert_len;
        } certificate;
        struct {
            uint8_t verify_data[12];
        } finished;
    } body;
} DtlsHandshake;

typedef struct {
    uint8_t client_write_key[DTLS_SRTP_KEY_LEN];
    uint8_t server_write_key[DTLS_SRTP_KEY_LEN];
    uint8_t client_write_salt[DTLS_SRTP_SALT_LEN];
    uint8_t server_write_salt[DTLS_SRTP_SALT_LEN];
    uint8_t master_key[DTLS_SRTP_MASTER_KEY_LEN];
    uint8_t master_salt[DTLS_SRTP_MASTER_SALT_LEN];
    int srtp_profile;
    char tls_fingerprint[SDP_FINGERPRINT_LEN];
    char tls_id[64];
} DtlsSrtpKeys;

typedef struct {
    uint16_t seq;
    uint32_t ssrc;
    uint8_t *payload;
    size_t payload_len;
    bool encrypted;
    bool authenticated;
    uint8_t auth_tag[SRTP_AUTH_TAG_LEN];
    uint8_t roc;
} SrtpPacket;

typedef struct {
    DtlsSrtpKeys keys;
    DtlsConnectionState state;
    bool is_server;
    uint8_t local_random[32];
    uint8_t remote_random[32];
    uint16_t cipher_suite;
    uint16_t epoch;
    uint64_t sequence_number;
    uint8_t local_cert_der[4096];
    size_t local_cert_len;
    uint8_t remote_cert_der[4096];
    size_t remote_cert_len;
    uint8_t premaster_secret[48];
    uint8_t master_secret[48];
    uint16_t handshake_seq;
    void *ssl_ctx;
} DtlsSrtpContext;

int dtlsSrtpInit(DtlsSrtpContext *ctx, bool is_server);
int dtlsSrtpCreateCertificate(DtlsSrtpContext *ctx);
int dtlsSrtpSetRemoteFingerprint(DtlsSrtpContext *ctx, const char *hash, const char *fingerprint);
int dtlsSrtpGetLocalFingerprint(DtlsSrtpContext *ctx, char *fingerprint, size_t len);

int dtlsBuildClientHello(DtlsSrtpContext *ctx, uint8_t *buf, size_t bufsz, size_t *written);
int dtlsBuildServerHello(DtlsSrtpContext *ctx, uint8_t *buf, size_t bufsz, size_t *written);
int dtlsBuildCertificate(DtlsSrtpContext *ctx, uint8_t *buf, size_t bufsz, size_t *written);
int dtlsBuildClientKeyExchange(DtlsSrtpContext *ctx, uint8_t *buf, size_t bufsz, size_t *written);
int dtlsBuildFinished(DtlsSrtpContext *ctx, uint8_t *buf, size_t bufsz, size_t *written);
int dtlsBuildChangeCipherSpec(DtlsSrtpContext *ctx, uint8_t *buf, size_t bufsz, size_t *written);

int dtlsParseHandshake(DtlsSrtpContext *ctx, const uint8_t *buf, size_t len, DtlsHandshake *out);
int dtlsDoHandshake(DtlsSrtpContext *ctx, const uint8_t *input, size_t input_len, uint8_t *output, size_t outsz, size_t *written);

int dtlsSrtpDeriveKeys(DtlsSrtpContext *ctx);
int srtpProtect(SrtpPacket *pkt, const uint8_t *key, const uint8_t *salt, size_t *outlen);
int srtpUnprotect(SrtpPacket *pkt, const uint8_t *key, const uint8_t *salt);
int srtpEncrypt(const uint8_t *key, const uint8_t *salt, uint16_t seq, uint32_t ssrc, const uint8_t *payload, size_t payload_len, uint8_t *out, size_t *outlen);
int srtpDecrypt(const uint8_t *key, const uint8_t *salt, uint16_t seq, uint32_t ssrc, const uint8_t *data, size_t datalen, uint8_t *out, size_t *outlen);
int srtpComputeAuthTag(const uint8_t *key, const uint8_t *data, size_t len, uint8_t roc, uint8_t *tag);
int srtpVerifyAuthTag(const uint8_t *key, const uint8_t *data, size_t len, uint8_t roc, const uint8_t *tag);
int srtcpProtect(const uint8_t *key, const uint8_t *salt, const uint8_t *data, size_t len, uint8_t *out, size_t *outlen);
int srtcpUnprotect(const uint8_t *key, const uint8_t *salt, const uint8_t *data, size_t len, uint8_t *out, size_t *outlen);

void dtlsSrtpClose(DtlsSrtpContext *ctx);

#endif
