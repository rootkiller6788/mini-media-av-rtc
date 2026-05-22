#include "sdp_signaling.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static void trim(char *s) {
    char *p = s;
    while (*p) p++;
    p--;
    while (p >= s && (*p == '\r' || *p == '\n' || *p == ' ' || *p == '\t')) {
        *p = '\0';
        p--;
    }
    while (*s == ' ' || *s == '\t') s++;
    if (s != s) { /* noop */ }
}

static int parseLine(const char *line, char *type, size_t typesz, char *value, size_t valuesz) {
    const char *eq = strchr(line, '=');
    if (!eq) {
        if (strlen(line) < 2) return -1;
        strncpy(type, line, 1);
        type[1] = '\0';
        strncpy(value, line + 2, valuesz - 1);
        value[valuesz - 1] = '\0';
        return 0;
    }
    size_t tlen = eq - line;
    if (tlen >= typesz) tlen = typesz - 1;
    memcpy(type, line, tlen);
    type[tlen] = '\0';
    strncpy(value, eq + 1, valuesz - 1);
    value[valuesz - 1] = '\0';
    return 0;
}

int parseSessionDescription(const char *sdp_text, SessionDescription *out) {
    memset(out, 0, sizeof(*out));
    char buf[SDP_MAX_LINE];
    const char *p = sdp_text;
    int current_media = -1;

    while (*p) {
        const char *nl = strchr(p, '\n');
        if (!nl) break;
        size_t len = nl - p;
        if (len > 0 && *(nl - 1) == '\r') len--;
        if (len >= SDP_MAX_LINE) len = SDP_MAX_LINE - 1;
        memcpy(buf, p, len);
        buf[len] = '\0';
        p = nl + 1;
        if (len == 0) continue;

        char type[8] = {0}, value[SDP_MAX_LINE] = {0};
        parseLine(buf, type, sizeof(type), value, sizeof(value));

        if (type[0] == 'v') continue;
        else if (type[0] == 'o') {
            char user[64]; uint32_t sid, sver;
            sscanf(value, "%63s %u %u", user, &sid, &sver);
            out->session_id = sid;
            out->session_version = sver;
        }
        else if (type[0] == 's') strncpy(out->session_name, value, sizeof(out->session_name) - 1);
        else if (type[0] == 'c') strncpy(out->connection_addr, value + 7, sizeof(out->connection_addr) - 1);
        else if (type[0] == 'a' && strncmp(value, "group:BUNDLE", 12) == 0) {
            out->bundle_enabled = true;
            strncpy(out->group_bundle, value + 13, sizeof(out->group_bundle) - 1);
        }
        else if (type[0] == 'm') {
            current_media++;
            if (current_media >= SDP_MAX_MEDIA) break;
            MediaDescription *md = &out->media[current_media];
            parseMediaDescription(buf, md);
            out->media_count = current_media + 1;
        }
        else if (current_media >= 0 && type[0] == 'a') {
            MediaDescription *md = &out->media[current_media];
            if (strncmp(value, "rtpmap:", 7) == 0) {
                int pt, clock; char enc[32] = {0};
                if (sscanf(value + 7, "%d %31[^/]/%d", &pt, enc, &clock) >= 2) {
                    SdpCodec c = {0};
                    c.payload_type = pt;
                    strncpy(c.encoding, enc, sizeof(c.encoding) - 1);
                    c.clock_rate = clock;
                    if (strcmp(enc, "opus") == 0) c.id = SDP_CODEC_OPUS;
                    else if (strcmp(enc, "H264") == 0) c.id = SDP_CODEC_H264;
                    else if (strcmp(enc, "VP8") == 0) c.id = SDP_CODEC_VP8;
                    else if (strcmp(enc, "VP9") == 0) c.id = SDP_CODEC_VP9;
                    else if (strcmp(enc, "PCMU") == 0) c.id = SDP_CODEC_PCMU;
                    else if (strcmp(enc, "PCMA") == 0) c.id = SDP_CODEC_PCMA;
                    if (md->codec_count < SDP_MAX_CODECS)
                        md->codecs[md->codec_count++] = c;
                }
            }
            else if (strncmp(value, "fmtp:", 5) == 0) {
                int pt; char fmtp[256] = {0};
                if (sscanf(value + 5, "%d %255[^\n]", &pt, fmtp) >= 2) {
                    for (int i = 0; i < md->codec_count; i++) {
                        if (md->codecs[i].payload_type == pt)
                            strncpy(md->codecs[i].fmtp, fmtp, sizeof(md->codecs[i].fmtp) - 1);
                    }
                }
            }
            else if (strncmp(value, "candidate:", 10) == 0) {
                IceCandidate cand;
                if (parseIceCandidate(value + 10, &cand) == 0)
                    sdpAddCandidate(md, &cand);
            }
            else if (strncmp(value, "fingerprint:", 12) == 0) {
                parseDtlsFingerprint(value + 12, &md->dtls);
            }
            else if (strncmp(value, "extmap:", 7) == 0) {
                int id; char uri[256];
                if (sscanf(value + 7, "%d %255s", &id, uri) >= 2)
                    sdpAddExtmap(md, id, uri);
            }
            else if (strncmp(value, "ice-ufrag:", 10) == 0) {
                strncpy(md->ice_ufrag, value + 10, sizeof(md->ice_ufrag) - 1);
            }
            else if (strncmp(value, "ice-pwd:", 8) == 0) {
                strncpy(md->ice_pwd, value + 8, sizeof(md->ice_pwd) - 1);
            }
            else if (strncmp(value, "rtcp-mux", 8) == 0) {
                md->rtcp_mux = true;
            }
            else if (strncmp(value, "mid:", 4) == 0) {
                strncpy(md->mid, value + 4, sizeof(md->mid) - 1);
            }
            else if (strncmp(value, "ssrc:", 5) == 0) {
                uint32_t ss; char attr[64] = {0}, attrv[128] = {0};
                if (sscanf(value + 5, "%u %63[^:]:%127s", &ss, attr, attrv) >= 3) {
                    md->ssrc = ss;
                    if (strcmp(attr, "cname") == 0)
                        strncpy(md->cname, attrv, sizeof(md->cname) - 1);
                    else if (strcmp(attr, "msid") == 0)
                        strncpy(md->msid, attrv, sizeof(md->msid) - 1);
                    else if (strcmp(attr, "mslabel") == 0 && strlen(md->track_id) == 0)
                        strncpy(md->track_id, attrv, sizeof(md->track_id) - 1);
                }
            }
            else if (strncmp(value, "setup:", 6) == 0) {
                strncpy(md->dtls.setup, value + 6, sizeof(md->dtls.setup) - 1);
            }
        }
    }
    out->rtcp_mux_enabled = out->bundle_enabled;
    return 0;
}

int parseMediaDescription(const char *m_line, MediaDescription *out) {
    memset(out, 0, sizeof(*out));
    char proto[16]; char fmtp_list[256] = {0};
    int port;
    char media_str[16];
    int n = sscanf(m_line, "m=%15s %d %15s %255[^\n]", media_str, &port, proto, fmtp_list);
    if (n < 3) return -1;

    if (strcmp(media_str, "audio") == 0) out->type = SDP_MEDIA_AUDIO;
    else if (strcmp(media_str, "video") == 0) out->type = SDP_MEDIA_VIDEO;
    else out->type = SDP_MEDIA_APPLICATION;

    out->port = port;
    strncpy(out->proto, proto, sizeof(out->proto) - 1);
    return 0;
}

int parseIceCandidate(const char *candidate_line, IceCandidate *out) {
    memset(out, 0, sizeof(*out));
    char transport[8], type[16], addr[64], raddr[64] = {0};
    int comp, port, rport = 0;
    uint32_t pri;

    int n = sscanf(candidate_line, "%15s %d %7s %u %63s %d typ %15s",
                   out->foundation, &comp, transport, &pri, addr, &port, type);
    if (n < 7) return -1;

    out->component_id = comp;
    strncpy(out->transport, transport, sizeof(out->transport) - 1);
    out->priority = pri;
    strncpy(out->address, addr, sizeof(out->address) - 1);
    out->port = port;
    strncpy(out->type, type, sizeof(out->type) - 1);

    const char *raddr_pos = strstr(candidate_line, " raddr ");
    if (raddr_pos) {
        sscanf(raddr_pos, " raddr %63s rport %d", raddr, &rport);
        strncpy(out->related_address, raddr, sizeof(out->related_address) - 1);
        out->related_port = rport;
    }
    out->tcp_active = strstr(candidate_line, "tcptype active") != NULL;
    return 0;
}

int parseDtlsFingerprint(const char *fp_line, DtlsFingerprint *out) {
    memset(out, 0, sizeof(*out));
    char algo[16];
    int n = sscanf(fp_line, "%15s %127s", algo, out->fingerprint);
    if (n < 2) return -1;
    if (algo[strlen(algo) - 1] == ':') algo[strlen(algo) - 1] = '\0';
    strncpy(out->hash_algo, algo, sizeof(out->hash_algo) - 1);
    return 0;
}

int parseRtpExtmap(const char *ext_line, RtpExtmap *out) {
    memset(out, 0, sizeof(*out));
    int n = sscanf(ext_line, "%d %255s", &out->id, out->uri);
    return n >= 2 ? 0 : -1;
}

int generateSessionDescription(const SessionDescription *sdp, char *buf, size_t bufsz) {
    int off = 0;
    off += snprintf(buf + off, bufsz - off, "v=0\r\n");
    off += snprintf(buf + off, bufsz - off, "o=- %u 1 IN IP4 0.0.0.0\r\n", sdp->session_id);
    off += snprintf(buf + off, bufsz - off, "s=%s\r\n", sdp->session_name[0] ? sdp->session_name : "-");
    off += snprintf(buf + off, bufsz - off, "t=0 0\r\n");
    if (sdp->bundle_enabled && sdp->group_bundle[0])
        off += snprintf(buf + off, bufsz - off, "a=group:BUNDLE %s\r\n", sdp->group_bundle);
    for (int i = 0; i < sdp->media_count; i++) {
        off += generateMediaDescription(&sdp->media[i], buf + off, bufsz - off);
    }
    return off;
}

int generateMediaDescription(const MediaDescription *md, char *buf, size_t bufsz) {
    int off = 0;
    const char *type_str = md->type == SDP_MEDIA_AUDIO ? "audio" :
                           md->type == SDP_MEDIA_VIDEO ? "video" : "application";
    off += snprintf(buf + off, bufsz - off, "m=%s %d %s", type_str, md->port, md->proto);
    for (int i = 0; i < md->codec_count; i++)
        off += snprintf(buf + off, bufsz - off, " %d", md->codecs[i].payload_type);
    off += snprintf(buf + off, bufsz - off, "\r\n");
    off += snprintf(buf + off, bufsz - off, "c=IN IP4 0.0.0.0\r\n");

    if (md->bundle && md->mid[0])
        off += snprintf(buf + off, bufsz - off, "a=mid:%s\r\n", md->mid);
    if (md->rtcp_mux)
        off += snprintf(buf + off, bufsz - off, "a=rtcp-mux\r\n");
    if (md->ice_ufrag[0])
        off += snprintf(buf + off, bufsz - off, "a=ice-ufrag:%s\r\n", md->ice_ufrag);
    if (md->ice_pwd[0])
        off += snprintf(buf + off, bufsz - off, "a=ice-pwd:%s\r\n", md->ice_pwd);
    if (md->dtls.fingerprint[0]) {
        off += snprintf(buf + off, bufsz - off, "a=fingerprint:%s %s\r\n",
                        md->dtls.hash_algo, md->dtls.fingerprint);
        if (md->dtls.setup[0])
            off += snprintf(buf + off, bufsz - off, "a=setup:%s\r\n", md->dtls.setup);
    }
    for (int i = 0; i < md->codec_count; i++) {
        SdpCodec *c = &md->codecs[i];
        if (c->encoding[0])
            off += snprintf(buf + off, bufsz - off, "a=rtpmap:%d %s/%d\r\n",
                            c->payload_type, c->encoding, c->clock_rate);
        if (c->fmtp[0])
            off += snprintf(buf + off, bufsz - off, "a=fmtp:%d %s\r\n",
                            c->payload_type, c->fmtp);
    }
    for (int i = 0; i < md->extmap_count; i++)
        off += snprintf(buf + off, bufsz - off, "a=extmap:%d %s\r\n",
                        md->extmaps[i].id, md->extmaps[i].uri);
    for (int i = 0; i < md->candidate_count; i++) {
        off += generateIceCandidate(&md->candidates[i], i, buf + off, bufsz - off);
    }
    if (md->ssrc) {
        off += snprintf(buf + off, bufsz - off, "a=ssrc:%u cname:%s\r\n", md->ssrc, md->cname);
        off += snprintf(buf + off, bufsz - off, "a=ssrc:%u msid:%s %s\r\n",
                        md->ssrc, md->msid, md->track_id);
    }
    return off;
}

int generateIceCandidate(const IceCandidate *cand, int mline, char *buf, size_t bufsz) {
    int off = snprintf(buf, bufsz, "a=candidate:%s %d %s %u %s %d typ %s",
                       cand->foundation, cand->component_id, cand->transport,
                       cand->priority, cand->address, cand->port, cand->type);
    if (cand->related_address[0])
        off += snprintf(buf + off, bufsz - off, " raddr %s rport %d",
                        cand->related_address, cand->related_port);
    if (cand->tcp_active)
        off += snprintf(buf + off, bufsz - off, " tcptype active");
    off += snprintf(buf + off, bufsz - off, " generation 0\r\n");
    return off;
}

int generateAnswer(const SessionDescription *offer, SessionDescription *answer) {
    memset(answer, 0, sizeof(*answer));
    answer->session_id = offer->session_id + 1;
    answer->session_version = 2;
    strncpy(answer->session_name, offer->session_name[0] ? offer->session_name : "answer",
            sizeof(answer->session_name) - 1);
    answer->bundle_enabled = offer->bundle_enabled;
    strncpy(answer->group_bundle, offer->group_bundle, sizeof(answer->group_bundle) - 1);

    for (int i = 0; i < offer->media_count; i++) {
        MediaDescription *om = &offer->media[i];
        MediaDescription *am = &answer->media[answer->media_count++];

        am->type = om->type;
        am->port = 9;
        strncpy(am->proto, om->proto, sizeof(am->proto) - 1);
        am->bundle = om->bundle;
        if (om->mid[0]) strncpy(am->mid, om->mid, sizeof(am->mid) - 1);
        am->rtcp_mux = om->rtcp_mux;

        int pref_codecs[] = {SDP_CODEC_OPUS, SDP_CODEC_VP8, SDP_CODEC_H264};
        int npref = om->type == SDP_MEDIA_AUDIO ? 1 : 3;
        for (int j = 0; j < npref; j++) {
            for (int k = 0; k < om->codec_count; k++) {
                if (om->codecs[k].id == pref_codecs[j]) {
                    am->codecs[am->codec_count++] = om->codecs[k];
                    break;
                }
            }
        }
    }
    return 0;
}

uint32_t computeIceCandidatePriority(int type_pref, int local_pref, int component_id) {
    return ((uint32_t)type_pref << 24) | ((uint32_t)local_pref << 8) | (256 - (uint32_t)component_id);
}

void sdpSetBundle(SessionDescription *sdp, bool enabled) { sdp->bundle_enabled = enabled; }

void sdpSetRtcpMux(MediaDescription *md, bool enabled) { md->rtcp_mux = enabled; }

int sdpAddCodec(MediaDescription *md, SdpCodecId id, int pt, const char *name, int clock) {
    if (md->codec_count >= SDP_MAX_CODECS) return -1;
    SdpCodec *c = &md->codecs[md->codec_count++];
    c->id = id;
    c->payload_type = pt;
    strncpy(c->encoding, name, sizeof(c->encoding) - 1);
    c->clock_rate = clock;
    c->channels = 1;
    return 0;
}

int sdpAddCandidate(MediaDescription *md, const IceCandidate *cand) {
    if (md->candidate_count >= SDP_MAX_CANDIDATES) return -1;
    md->candidates[md->candidate_count++] = *cand;
    return 0;
}

int sdpAddExtmap(MediaDescription *md, int id, const char *uri) {
    if (md->extmap_count >= SDP_MAX_EXTMAPS) return -1;
    md->extmaps[md->extmap_count].id = id;
    strncpy(md->extmaps[md->extmap_count].uri, uri, sizeof(md->extmaps[0].uri) - 1);
    md->extmap_count++;
    return 0;
}

int sdpSetSsrc(MediaDescription *md, uint32_t ssrc, const char *cname, const char *msid) {
    md->ssrc = ssrc;
    if (cname) strncpy(md->cname, cname, sizeof(md->cname) - 1);
    if (msid) strncpy(md->msid, msid, sizeof(md->msid) - 1);
    return 0;
}

int signalingChannelInit(SignalingChannel *ch, const char *url) {
    memset(ch, 0, sizeof(*ch));
    strncpy(ch->url, url, sizeof(ch->url) - 1);
    ch->state = 0;
    return 0;
}

int signalingChannelConnect(SignalingChannel *ch) {
    ch->state = 1;
    return 0;
}

int signalingChannelSendSdp(SignalingChannel *ch, const SessionDescription *sdp) {
    char buf[8192];
    generateSessionDescription(sdp, buf, sizeof(buf));
    return 0;
}

int signalingChannelSendCandidate(SignalingChannel *ch, const char *mid, int mline, const IceCandidate *cand) {
    char buf[1024];
    generateIceCandidate(cand, mline, buf, sizeof(buf));
    return 0;
}

int signalingChannelPoll(SignalingChannel *ch, int timeout_ms) {
    (void)timeout_ms;
    return 0;
}

int signalingChannelClose(SignalingChannel *ch) {
    ch->state = 0;
    return 0;
}
