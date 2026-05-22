#ifndef ICE_STUN_TURN_H
#define ICE_STUN_TURN_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "sdp_signaling.h"

#define ICE_MAX_CANDIDATES 32

#define STUN_MAGIC_COOKIE 0x2112A442
#define STUN_HEADER_SIZE 20
#define STUN_MAX_ATTRIBUTES 32
#define STUN_ATTR_HEADER_SIZE 4
#define STUN_BINDING_REQUEST 0x0001
#define STUN_BINDING_RESPONSE_SUCCESS 0x0101
#define STUN_BINDING_RESPONSE_ERROR 0x0111
#define STUN_BINDING_INDICATION 0x0011
#define STUN_MAX_ADDRESS_SIZE 256

#define STUN_ATTR_MAPPED_ADDRESS 0x0001
#define STUN_ATTR_XOR_MAPPED_ADDRESS 0x0020
#define STUN_ATTR_USERNAME 0x0006
#define STUN_ATTR_MESSAGE_INTEGRITY 0x0008
#define STUN_ATTR_FINGERPRINT 0x8028
#define STUN_ATTR_ERROR_CODE 0x0009
#define STUN_ATTR_UNKNOWN_ATTRIBUTES 0x000A
#define STUN_ATTR_REALM 0x0014
#define STUN_ATTR_NONCE 0x0015
#define STUN_ATTR_ICE_CONTROLLING 0x802A
#define STUN_ATTR_ICE_CONTROLLED 0x8029
#define STUN_ATTR_PRIORITY 0x0024
#define STUN_ATTR_USE_CANDIDATE 0x0025
#define STUN_ATTR_SOFTWARE 0x8022

#define TURN_DEFAULT_PORT 3478
#define TURN_CHANNEL_MIN 0x4000
#define TURN_CHANNEL_MAX 0x7FFF
#define TURN_CHANNEL_DATA_HEADER_SIZE 4
#define TURN_DEFAULT_LIFETIME 600

typedef enum {
    STUN_MSG_REQUEST = 0,
    STUN_MSG_INDICATION = 1,
    STUN_MSG_SUCCESS_RESPONSE = 2,
    STUN_MSG_ERROR_RESPONSE = 3
} StunMessageClass;

typedef enum {
    STUN_METHOD_BINDING = 0x001,
    STUN_METHOD_ALLOCATE = 0x003,
    STUN_METHOD_REFRESH = 0x004,
    STUN_METHOD_SEND = 0x006,
    STUN_METHOD_DATA = 0x007,
    STUN_METHOD_CREATE_PERMISSION = 0x008,
    STUN_METHOD_CHANNEL_BIND = 0x009
} StunMethod;

typedef struct {
    uint16_t type;
    uint16_t length;
    uint8_t value[];
} StunAttribute;

typedef struct {
    uint16_t msg_type;
    uint16_t msg_length;
    uint32_t magic_cookie;
    uint8_t transaction_id[12];
    uint8_t attr_data[STUN_MAX_ATTRIBUTES * 256];
    int attr_count;
} StunMessage;

typedef enum {
    ICE_CAND_TYPE_HOST = 0,
    ICE_CAND_TYPE_SRFLX = 1,
    ICE_CAND_TYPE_RELAY = 2,
    ICE_CAND_TYPE_PRFLX = 3
} IceCandidateType;

typedef enum {
    ICE_STATE_IDLE = 0,
    ICE_STATE_GATHERING = 1,
    ICE_STATE_CHECKING = 2,
    ICE_STATE_CONNECTED = 3,
    ICE_STATE_COMPLETED = 4,
    ICE_STATE_FAILED = 5,
    ICE_STATE_DISCONNECTED = 6,
    ICE_STATE_RESTART = 7,
    ICE_STATE_CLOSED = 8
} IceConnectionState;

typedef struct {
    char foundation[16];
    int component_id;
    IceCandidateType type;
    char address[64];
    int port;
    int transport;
    uint32_t priority;
    char related_address[64];
    int related_port;
    char local_ufrag[SDP_ICE_UFRAG_LEN];
    char local_pwd[SDP_ICE_PWD_LEN];
    char remote_ufrag[SDP_ICE_UFRAG_LEN];
    char remote_pwd[SDP_ICE_PWD_LEN];
    uint64_t tiebreaker;
    bool controlling;
    bool nominated;
} IceCandidateLocal;

typedef struct {
    IceCandidateLocal local;
    IceCandidateLocal remote;
    uint64_t pair_priority;
    bool valid;
    bool nominated;
    int64_t last_rtt_us;
    int64_t last_check_us;
    int check_count;
} IceCandidatePair;

typedef struct {
    IceCandidateLocal local_candidates[ICE_MAX_CANDIDATES];
    int local_count;
    IceCandidateLocal remote_candidates[ICE_MAX_CANDIDATES];
    int remote_count;
    IceCandidatePair pairs[ICE_MAX_CANDIDATES * ICE_MAX_CANDIDATES];
    int pair_count;
    IceConnectionState state;
    char local_ufrag[SDP_ICE_UFRAG_LEN];
    char local_pwd[SDP_ICE_PWD_LEN];
    char remote_ufrag[SDP_ICE_UFRAG_LEN];
    char remote_pwd[SDP_ICE_PWD_LEN];
    IceCandidatePair *selected_pair;
    bool controlling_role;
    uint64_t tiebreaker;
    void *stun_ctx;
    void *turn_ctx;
} IceSession;

typedef struct {
    char relay_address[64];
    int relay_port;
    int lifetime;
    char username[128];
    char password[128];
    char realm[64];
    char nonce[64];
    bool allocated;
    uint16_t channel_bindings[64];
    int channel_count;
} TurnAllocation;

int stunBuildBindingRequest(StunMessage *msg, const uint8_t tid[12]);
int stunBuildBindingResponse(StunMessage *msg, const uint8_t tid[12], const char *mapped_addr, int port);
int stunBuildBindingIndication(StunMessage *msg, const uint8_t tid[12]);
int stunAddAttribute(StunMessage *msg, uint16_t type, const uint8_t *data, uint16_t len);
int stunAddXorMappedAddress(StunMessage *msg, const char *addr, int port);
int stunAddMappedAddress(StunMessage *msg, const char *addr, int port);
int stunAddIceControlling(StunMessage *msg, uint64_t tiebreaker);
int stunAddIceControlled(StunMessage *msg, uint64_t tiebreaker);
int stunAddPriority(StunMessage *msg, uint32_t priority);
int stunAddUseCandidate(StunMessage *msg);
int stunAddUsername(StunMessage *msg, const char *username);
int stunAddMessageIntegrity(StunMessage *msg, const char *password);
int stunAddFingerprint(StunMessage *msg);
int stunSerializeMessage(const StunMessage *msg, uint8_t *buf, size_t bufsz, size_t *outlen);
int stunParseMessage(const uint8_t *buf, size_t len, StunMessage *msg);
int stunGetAttribute(const StunMessage *msg, uint16_t type, uint8_t *out, size_t *outlen);
int stunGetMappedAddress(const StunMessage *msg, char *addr, size_t addrsz, int *port);
int stunGetXorMappedAddress(const StunMessage *msg, char *addr, size_t addrsz, int *port);

uint32_t calcIcePriority(IceCandidateType type, int local_pref, int component_id);
uint64_t calcPairPriority(uint32_t local_priority, uint32_t remote_priority, bool controlling);

int iceSessionCreate(IceSession *sess, bool controlling);
int iceSessionSetLocalCredentials(IceSession *sess, const char *ufrag, const char *pwd);
int iceSessionSetRemoteCredentials(IceSession *sess, const char *ufrag, const char *pwd);
int iceSessionAddLocalCandidate(IceSession *sess, const IceCandidateLocal *cand);
int iceSessionAddRemoteCandidate(IceSession *sess, const IceCandidateLocal *cand);
int iceSessionGatherCandidates(IceSession *sess, int components);
int iceSessionBuildCheckList(IceSession *sess);
int iceSessionStartChecks(IceSession *sess);
int iceSessionProcessBindingRequest(IceSession *sess, const StunMessage *req, const char *src_addr, int src_port);
int iceSessionProcessBindingResponse(IceSession *sess, const StunMessage *resp, const char *src_addr, int src_port);
int iceSessionGetNextCheck(IceSession *sess, IceCandidatePair **pair);
int iceSessionNominatePair(IceSession *sess, IceCandidatePair *pair);
int iceSessionRestart(IceSession *sess);
int iceSessionDestroy(IceSession *sess);

int turnAllocateRequest(StunMessage *msg, const uint8_t tid[12], int lifetime);
int turnRefreshRequest(StunMessage *msg, const uint8_t tid[12], int lifetime);
int turnCreatePermissionRequest(StunMessage *msg, const uint8_t tid[12], const char *peer_addr, int peer_port);
int turnSendIndication(StunMessage *msg, const uint8_t tid[12], const char *peer_addr, int peer_port, const uint8_t *data, size_t len);
int turnChannelBindRequest(StunMessage *msg, const uint8_t tid[12], uint16_t channel_num, const char *peer_addr, int peer_port);
int turnWrapChannelData(uint16_t channel_num, const uint8_t *data, size_t len, uint8_t *out, size_t outsz, size_t *written);
int turnUnwrapChannelData(const uint8_t *buf, size_t len, uint16_t *channel_num, uint8_t *data, size_t *datalen);

#endif
