#ifndef CDN_EDGE_H
#define CDN_EDGE_H

#include <stdint.h>
#include <stddef.h>

#define CDN_MAX_EDGES         128
#define CDN_MAX_CACHE_ENTRIES 4096
#define CDN_MAX_URL_LEN       2048
#define CDN_MAX_DNS_RECORDS    16
#define CDN_TTL_DEFAULT        3600u
#define CDN_MAX_PREWARM          16

typedef enum {
    CDN_NODE_ORIGIN,
    CDN_NODE_MID_TIER,
    CDN_NODE_EDGE
} cdn_node_type_t;

typedef enum {
    CDN_STRATEGY_ROUND_ROBIN,
    CDN_STRATEGY_GEO_DNS,
    CDN_STRATEGY_LATENCY_BASED,
    CDN_STRATEGY_WEIGHTED
} cdn_routing_strategy_t;

typedef enum {
    CDN_EVICT_LRU,
    CDN_EVICT_LFU,
    CDN_EVICT_TTL_FIRST
} cdn_eviction_policy_t;

typedef enum {
    CDN_STATE_COLD,
    CDN_STATE_WARM,
    CDN_STATE_HOT
} cdn_cache_state_t;

typedef struct {
    char             key[CDN_MAX_URL_LEN];
    uint8_t         *data;
    size_t           data_size;
    uint64_t         last_access_ms;
    uint64_t         create_time_ms;
    uint64_t         ttl_ms;
    uint64_t         access_count;
    uint32_t         key_hash;
    uint8_t          dirty;
    uint8_t          pinned;
    cdn_cache_state_t state;
} cdn_cache_entry_t;

typedef struct {
    cdn_cache_entry_t  entries[CDN_MAX_CACHE_ENTRIES];
    uint32_t           count;
    uint32_t           hit_count;
    uint32_t           miss_count;
    uint64_t           total_bytes_stored;
    uint64_t           max_bytes;
    cdn_eviction_policy_t eviction;
    uint64_t           default_ttl_ms;
} cdn_cache_store_t;

typedef struct {
    char                   id[64];
    char                   hostname[256];
    char                   base_url[CDN_MAX_URL_LEN];
    uint16_t               port;
    cdn_node_type_t        type;
    uint8_t                available;
    uint8_t                overloaded;
    uint64_t               last_check_ms;
    uint32_t               weight;
    uint32_t               active_connections;
    double                 avg_latency_ms;
    cdn_cache_store_t      cache;
} cdn_edge_node_t;

typedef struct {
    char                    hostname[256];
    char                    ip_address[64];
    uint32_t                ttl;
    uint8_t                 available;
} cdn_dns_record_t;

typedef struct {
    char                    base_url[CDN_MAX_URL_LEN];
    char                    origin_url[CDN_MAX_URL_LEN];
    cdn_edge_node_t         edges[CDN_MAX_EDGES];
    uint32_t                edge_count;
    cdn_routing_strategy_t  routing_strategy;
    cdn_cache_store_t       cache;
    uint32_t                request_id_counter;
    char                    prewarm_list[CDN_MAX_PREWARM][CDN_MAX_URL_LEN];
    uint32_t                prewarm_count;
    cdn_dns_record_t        dns_records[CDN_MAX_DNS_RECORDS];
    uint32_t                dns_count;
} cdn_t;

typedef struct {
    uint64_t  request_time_ms;
    uint64_t  response_time_ms;
    uint64_t  bytes_transferred;
    uint32_t  status_code;
    uint8_t   cache_hit;
    uint8_t   redirected;
    char      edge_node_id[64];
    char      url[CDN_MAX_URL_LEN];
    char      range_header[64];
    uint8_t   has_range;
} cdn_request_log_t;

int     cdn_init(cdn_t *cdn, const char *origin_url);
void    cdn_deinit(cdn_t *cdn);

int     cdn_add_edge(cdn_t *cdn, const char *id, const char *hostname,
                     uint16_t port, cdn_node_type_t type, uint32_t weight);
int     cdn_remove_edge(cdn_t *cdn, const char *id);
int     cdn_set_routing_strategy(cdn_t *cdn, cdn_routing_strategy_t strategy);

int     cdn_cache_init(cdn_cache_store_t *store, uint64_t max_bytes,
                       cdn_eviction_policy_t policy, uint64_t default_ttl_ms);
int     cdn_cache_put(cdn_cache_store_t *store, const char *key,
                      const uint8_t *data, size_t size, uint64_t ttl_ms);
int     cdn_cache_get(cdn_cache_store_t *store, const char *key,
                      uint8_t **data, size_t *size);
int     cdn_cache_contains(cdn_cache_store_t *store, const char *key);
int     cdn_cache_invalidate(cdn_cache_store_t *store, const char *key);
int     cdn_cache_evict(cdn_cache_store_t *store);
int     cdn_cache_evict_lru(cdn_cache_store_t *store);
int     cdn_cache_evict_lfu(cdn_cache_store_t *store);
int     cdn_cache_evict_by_ttl(cdn_cache_store_t *store);
void    cdn_cache_stats(cdn_cache_store_t *store, uint32_t *hits, uint32_t *misses,
                        float *hit_ratio, uint64_t *bytes_stored);
uint32_t cdn_hash_key(const char *key);

int     cdn_prewarm_add(cdn_t *cdn, const char *url);
int     cdn_prewarm_execute(cdn_t *cdn);
int     cdn_prewarm_next_segment(cdn_t *cdn, const char *current_url,
                                 char *next_url, size_t max_len);

int     cdn_add_dns_record(cdn_t *cdn, const char *hostname, const char *ip, uint32_t ttl);
int     cdn_resolve_dns(cdn_t *cdn, const char *hostname, cdn_dns_record_t *records,
                        uint32_t *count);
int     cdn_redirect_302(cdn_t *cdn, const char *request_url,
                         char *redirect_url, size_t max_len);
int     cdn_route_dns(cdn_t *cdn, const char *hostname, char *edge_url, size_t max_len);

int     cdn_select_edge(cdn_t *cdn, cdn_edge_node_t **edge);
int     cdn_edge_is_available(cdn_edge_node_t *edge);
int     cdn_mark_edge_overloaded(cdn_t *cdn, const char *edge_id);
int     cdn_mark_edge_down(cdn_t *cdn, const char *edge_id);

int     cdn_log_request(cdn_request_log_t *log);
int     cdn_simulate_request(cdn_t *cdn, const char *url, cdn_request_log_t *log);

const char *cdn_node_type_string(cdn_node_type_t type);
const char *cdn_eviction_policy_string(cdn_eviction_policy_t policy);
const char *cdn_routing_strategy_string(cdn_routing_strategy_t strategy);

#endif
