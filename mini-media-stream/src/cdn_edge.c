#include "cdn_edge.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>

int cdn_init(cdn_t *cdn, const char *origin_url)
{
    if (!cdn || !origin_url) return -1;
    memset(cdn, 0, sizeof(*cdn));
    strncpy(cdn->origin_url, origin_url, CDN_MAX_URL_LEN - 1);
    cdn->routing_strategy = CDN_STRATEGY_ROUND_ROBIN;
    cdn->request_id_counter = 0;
    return cdn_cache_init(&cdn->cache, 256ULL * 1024 * 1024,
                          CDN_EVICT_LRU, CDN_TTL_DEFAULT * 1000);
}

void cdn_deinit(cdn_t *cdn)
{
    uint32_t i;
    if (!cdn) return;
    for (i = 0; i < cdn->edge_count; i++) {
        uint32_t j;
        for (j = 0; j < cdn->edges[i].cache.count; j++) {
            free(cdn->edges[i].cache.entries[j].data);
        }
    }
    for (i = 0; i < cdn->cache.count; i++) {
        free(cdn->cache.entries[i].data);
    }
}

int cdn_add_edge(cdn_t *cdn, const char *id, const char *hostname,
                 uint16_t port, cdn_node_type_t type, uint32_t weight)
{
    cdn_edge_node_t *edge;
    if (!cdn || !id || !hostname || cdn->edge_count >= CDN_MAX_EDGES) return -1;

    edge = &cdn->edges[cdn->edge_count];
    memset(edge, 0, sizeof(*edge));
    strncpy(edge->id, id, 63);
    strncpy(edge->hostname, hostname, 255);
    edge->port = port;
    edge->type = type;
    edge->weight = weight;
    edge->available = 1;
    edge->last_check_ms = (uint64_t)time(NULL) * 1000;
    edge->avg_latency_ms = 50.0;

    snprintf(edge->base_url, sizeof(edge->base_url), "http://%s:%u", hostname, port);

    cdn->edge_count++;
    return 0;
}

int cdn_remove_edge(cdn_t *cdn, const char *id)
{
    uint32_t i;
    if (!cdn || !id) return -1;

    for (i = 0; i < cdn->edge_count; i++) {
        if (strcmp(cdn->edges[i].id, id) == 0) {
            if (i < cdn->edge_count - 1) {
                memmove(&cdn->edges[i], &cdn->edges[i + 1],
                        (cdn->edge_count - i - 1) * sizeof(cdn_edge_node_t));
            }
            cdn->edge_count--;
            return 0;
        }
    }
    return -1;
}

int cdn_set_routing_strategy(cdn_t *cdn, cdn_routing_strategy_t strategy)
{
    if (!cdn) return -1;
    cdn->routing_strategy = strategy;
    return 0;
}

int cdn_cache_init(cdn_cache_store_t *store, uint64_t max_bytes,
                   cdn_eviction_policy_t policy, uint64_t default_ttl_ms)
{
    if (!store) return -1;
    memset(store, 0, sizeof(*store));
    store->max_bytes = max_bytes;
    store->eviction = policy;
    store->default_ttl_ms = default_ttl_ms;
    return 0;
}

int cdn_cache_put(cdn_cache_store_t *store, const char *key,
                  const uint8_t *data, size_t size, uint64_t ttl_ms)
{
    cdn_cache_entry_t *entry;
    if (!store || !key || !data || size == 0) return -1;

    if (store->count >= CDN_MAX_CACHE_ENTRIES) {
        cdn_cache_evict(store);
        if (store->count >= CDN_MAX_CACHE_ENTRIES) {
            return -1;
        }
    }

    while (store->total_bytes_stored + (uint64_t)size > store->max_bytes &&
           store->count > 0) {
        cdn_cache_evict(store);
    }

    entry = &store->entries[store->count];
    memset(entry, 0, sizeof(*entry));
    strncpy(entry->key, key, CDN_MAX_URL_LEN - 1);
    entry->data = (uint8_t *)malloc(size);
    if (!entry->data) return -1;
    memcpy(entry->data, data, size);
    entry->data_size = size;
    entry->ttl_ms = (ttl_ms == 0) ? store->default_ttl_ms : ttl_ms;
    entry->create_time_ms = (uint64_t)time(NULL) * 1000;
    entry->last_access_ms = entry->create_time_ms;
    entry->key_hash = cdn_hash_key(key);
    entry->state = CDN_STATE_WARM;

    store->total_bytes_stored += (uint64_t)size;
    store->count++;
    return 0;
}

int cdn_cache_get(cdn_cache_store_t *store, const char *key,
                  uint8_t **data, size_t *size)
{
    uint32_t i;
    uint64_t now_ms;
    if (!store || !key || !data || !size) return -1;

    now_ms = (uint64_t)time(NULL) * 1000;

    for (i = 0; i < store->count; i++) {
        cdn_cache_entry_t *entry = &store->entries[i];
        if (strcmp(entry->key, key) == 0) {
            if (now_ms - entry->create_time_ms > entry->ttl_ms) {
                cdn_cache_invalidate(store, key);
                store->miss_count++;
                return -1;
            }
            entry->last_access_ms = now_ms;
            entry->access_count++;
            *data = entry->data;
            *size = entry->data_size;
            store->hit_count++;
            return 0;
        }
    }

    store->miss_count++;
    return -1;
}

int cdn_cache_contains(cdn_cache_store_t *store, const char *key)
{
    uint8_t *dummy_data;
    size_t dummy_size;
    if (cdn_cache_get(store, key, &dummy_data, &dummy_size) == 0) {
        return 1;
    }
    return 0;
}

int cdn_cache_invalidate(cdn_cache_store_t *store, const char *key)
{
    uint32_t i;
    if (!store || !key) return -1;

    for (i = 0; i < store->count; i++) {
        if (strcmp(store->entries[i].key, key) == 0) {
            if (store->total_bytes_stored >= store->entries[i].data_size) {
                store->total_bytes_stored -= store->entries[i].data_size;
            }
            free(store->entries[i].data);
            if (i < store->count - 1) {
                memmove(&store->entries[i], &store->entries[i + 1],
                        (store->count - i - 1) * sizeof(cdn_cache_entry_t));
            }
            store->count--;
            return 0;
        }
    }
    return -1;
}

int cdn_cache_evict(cdn_cache_store_t *store)
{
    if (!store) return -1;
    switch (store->eviction) {
    case CDN_EVICT_LRU:
        return cdn_cache_evict_lru(store);
    case CDN_EVICT_LFU:
        return cdn_cache_evict_lfu(store);
    case CDN_EVICT_TTL_FIRST:
        return cdn_cache_evict_by_ttl(store);
    default:
        return cdn_cache_evict_lru(store);
    }
}

int cdn_cache_evict_lru(cdn_cache_store_t *store)
{
    uint32_t i;
    uint32_t evict_idx = 0;
    uint64_t oldest_access = UINT64_MAX;

    if (!store || store->count == 0) return -1;

    for (i = 0; i < store->count; i++) {
        if (!store->entries[i].pinned &&
            store->entries[i].last_access_ms < oldest_access) {
            oldest_access = store->entries[i].last_access_ms;
            evict_idx = i;
        }
    }

    if (store->total_bytes_stored >= store->entries[evict_idx].data_size) {
        store->total_bytes_stored -= store->entries[evict_idx].data_size;
    }
    free(store->entries[evict_idx].data);

    if (evict_idx < store->count - 1) {
        memmove(&store->entries[evict_idx], &store->entries[evict_idx + 1],
                (store->count - evict_idx - 1) * sizeof(cdn_cache_entry_t));
    }
    store->count--;
    return 0;
}

int cdn_cache_evict_lfu(cdn_cache_store_t *store)
{
    uint32_t i;
    uint32_t evict_idx = 0;
    uint64_t fewest_access = UINT64_MAX;

    if (!store || store->count == 0) return -1;

    for (i = 0; i < store->count; i++) {
        if (!store->entries[i].pinned &&
            store->entries[i].access_count < fewest_access) {
            fewest_access = store->entries[i].access_count;
            evict_idx = i;
        }
    }

    if (store->total_bytes_stored >= store->entries[evict_idx].data_size) {
        store->total_bytes_stored -= store->entries[evict_idx].data_size;
    }
    free(store->entries[evict_idx].data);

    if (evict_idx < store->count - 1) {
        memmove(&store->entries[evict_idx], &store->entries[evict_idx + 1],
                (store->count - evict_idx - 1) * sizeof(cdn_cache_entry_t));
    }
    store->count--;
    return 0;
}

int cdn_cache_evict_by_ttl(cdn_cache_store_t *store)
{
    uint64_t now_ms = (uint64_t)time(NULL) * 1000;
    uint32_t i = 0;
    int evicted = 0;

    if (!store) return -1;

    while (i < store->count) {
        if (!store->entries[i].pinned &&
            now_ms - store->entries[i].create_time_ms > store->entries[i].ttl_ms) {
            if (store->total_bytes_stored >= store->entries[i].data_size) {
                store->total_bytes_stored -= store->entries[i].data_size;
            }
            free(store->entries[i].data);
            if (i < store->count - 1) {
                memmove(&store->entries[i], &store->entries[i + 1],
                        (store->count - i - 1) * sizeof(cdn_cache_entry_t));
            }
            store->count--;
            evicted = 1;
        } else {
            i++;
        }
    }
    return evicted ? 0 : -1;
}

void cdn_cache_stats(cdn_cache_store_t *store, uint32_t *hits, uint32_t *misses,
                     float *hit_ratio, uint64_t *bytes_stored)
{
    if (!store) return;
    if (hits) *hits = store->hit_count;
    if (misses) *misses = store->miss_count;
    if (hit_ratio) {
        uint32_t total = store->hit_count + store->miss_count;
        *hit_ratio = (total > 0) ? (float)store->hit_count / (float)total : 0.0f;
    }
    if (bytes_stored) *bytes_stored = store->total_bytes_stored;
}

uint32_t cdn_hash_key(const char *key)
{
    uint32_t hash = 5381;
    int c;
    if (!key) return 0;
    while ((c = *key++)) {
        hash = ((hash << 5) + hash) + (uint32_t)c;
    }
    return hash;
}

int cdn_prewarm_add(cdn_t *cdn, const char *url)
{
    if (!cdn || !url || cdn->prewarm_count >= CDN_MAX_PREWARM) return -1;
    strncpy(cdn->prewarm_list[cdn->prewarm_count], url, CDN_MAX_URL_LEN - 1);
    cdn->prewarm_count++;
    return 0;
}

int cdn_prewarm_execute(cdn_t *cdn)
{
    uint32_t i;
    if (!cdn) return -1;
    for (i = 0; i < cdn->prewarm_count; i++) {
        cdn_request_log_t log;
        memset(&log, 0, sizeof(log));
        strncpy(log.url, cdn->prewarm_list[i], CDN_MAX_URL_LEN - 1);
        log.request_time_ms = (uint64_t)time(NULL) * 1000;
        log.cache_hit = 0;
    }
    return 0;
}

int cdn_prewarm_next_segment(cdn_t *cdn, const char *current_url,
                             char *next_url, size_t max_len)
{
    (void)cdn;
    (void)current_url;
    (void)next_url;
    (void)max_len;
    return 0;
}

int cdn_add_dns_record(cdn_t *cdn, const char *hostname, const char *ip, uint32_t ttl)
{
    cdn_dns_record_t *rec;
    if (!cdn || !hostname || !ip || cdn->dns_count >= CDN_MAX_DNS_RECORDS) return -1;

    rec = &cdn->dns_records[cdn->dns_count];
    strncpy(rec->hostname, hostname, 255);
    strncpy(rec->ip_address, ip, 63);
    rec->ttl = ttl;
    rec->available = 1;
    cdn->dns_count++;
    return 0;
}

int cdn_resolve_dns(cdn_t *cdn, const char *hostname, cdn_dns_record_t *records,
                    uint32_t *count)
{
    uint32_t i, out = 0;
    if (!cdn || !hostname || !records || !count) return -1;

    for (i = 0; i < cdn->dns_count && out < CDN_MAX_DNS_RECORDS; i++) {
        if (strcmp(cdn->dns_records[i].hostname, hostname) == 0 &&
            cdn->dns_records[i].available) {
            memcpy(&records[out], &cdn->dns_records[i], sizeof(cdn_dns_record_t));
            out++;
        }
    }
    *count = out;
    return out > 0 ? 0 : -1;
}

int cdn_redirect_302(cdn_t *cdn, const char *request_url,
                     char *redirect_url, size_t max_len)
{
    cdn_edge_node_t *edge;
    if (!cdn || !request_url || !redirect_url) return -1;

    if (cdn_select_edge(cdn, &edge) != 0) return -1;

    snprintf(redirect_url, max_len, "%s%s", edge->base_url, request_url);
    return 0;
}

int cdn_route_dns(cdn_t *cdn, const char *hostname, char *edge_url, size_t max_len)
{
    cdn_dns_record_t records[CDN_MAX_DNS_RECORDS];
    uint32_t count = 0;

    if (!cdn || !hostname || !edge_url) return -1;

    if (cdn_resolve_dns(cdn, hostname, records, &count) != 0) return -1;

    snprintf(edge_url, max_len, "http://%s", records[0].ip_address);
    return 0;
}

int cdn_select_edge(cdn_t *cdn, cdn_edge_node_t **edge)
{
    uint32_t i;
    if (!cdn || !edge || cdn->edge_count == 0) return -1;

    switch (cdn->routing_strategy) {
    case CDN_STRATEGY_ROUND_ROBIN: {
        static uint32_t rr_counter = 0;
        uint32_t idx = rr_counter % cdn->edge_count;
        rr_counter++;
        for (i = 0; i < cdn->edge_count; i++) {
            uint32_t check = (idx + i) % cdn->edge_count;
            if (cdn->edges[check].available && !cdn->edges[check].overloaded) {
                *edge = &cdn->edges[check];
                return 0;
            }
        }
        break;
    }
    case CDN_STRATEGY_LATENCY_BASED: {
        double best_latency = 1e12;
        uint32_t best_idx = 0;
        int found = 0;
        for (i = 0; i < cdn->edge_count; i++) {
            if (cdn->edges[i].available && !cdn->edges[i].overloaded &&
                cdn->edges[i].avg_latency_ms < best_latency) {
                best_latency = cdn->edges[i].avg_latency_ms;
                best_idx = i;
                found = 1;
            }
        }
        if (found) {
            *edge = &cdn->edges[best_idx];
            return 0;
        }
        break;
    }
    case CDN_STRATEGY_WEIGHTED: {
        for (i = 0; i < cdn->edge_count; i++) {
            if (cdn->edges[i].available && !cdn->edges[i].overloaded) {
                *edge = &cdn->edges[i];
                return 0;
            }
        }
        break;
    }
    default:
        for (i = 0; i < cdn->edge_count; i++) {
            if (cdn->edges[i].available) {
                *edge = &cdn->edges[i];
                return 0;
            }
        }
        break;
    }

    return -1;
}

int cdn_edge_is_available(cdn_edge_node_t *edge)
{
    if (!edge) return 0;
    return edge->available && !edge->overloaded;
}

int cdn_mark_edge_overloaded(cdn_t *cdn, const char *edge_id)
{
    uint32_t i;
    if (!cdn || !edge_id) return -1;
    for (i = 0; i < cdn->edge_count; i++) {
        if (strcmp(cdn->edges[i].id, edge_id) == 0) {
            cdn->edges[i].overloaded = 1;
            return 0;
        }
    }
    return -1;
}

int cdn_mark_edge_down(cdn_t *cdn, const char *edge_id)
{
    uint32_t i;
    if (!cdn || !edge_id) return -1;
    for (i = 0; i < cdn->edge_count; i++) {
        if (strcmp(cdn->edges[i].id, edge_id) == 0) {
            cdn->edges[i].available = 0;
            return 0;
        }
    }
    return -1;
}

int cdn_log_request(cdn_request_log_t *log)
{
    if (!log) return -1;
    log->response_time_ms = (uint64_t)time(NULL) * 1000;
    return 0;
}

int cdn_simulate_request(cdn_t *cdn, const char *url, cdn_request_log_t *log)
{
    cdn_edge_node_t *edge;
    uint8_t *cached_data;
    size_t cached_size;

    if (!cdn || !url || !log) return -1;

    memset(log, 0, sizeof(*log));
    strncpy(log->url, url, CDN_MAX_URL_LEN - 1);
    log->request_time_ms = (uint64_t)time(NULL) * 1000;
    cdn->request_id_counter++;

    if (cdn_cache_get(&cdn->cache, url, &cached_data, &cached_size) == 0) {
        log->cache_hit = 1;
        log->response_time_ms = log->request_time_ms + 5;
        log->status_code = 200;
        log->bytes_transferred = cached_size;
        return 0;
    }

    if (cdn_select_edge(cdn, &edge) != 0) {
        log->status_code = 503;
        return -1;
    }

    strncpy(log->edge_node_id, edge->id, 63);
    log->redirected = 1;
    log->cache_hit = 0;
    log->response_time_ms = log->request_time_ms +
                             (uint64_t)(edge->avg_latency_ms * 2);
    log->status_code = 200;
    log->bytes_transferred = 100000;

    return 0;
}

const char *cdn_node_type_string(cdn_node_type_t type)
{
    switch (type) {
    case CDN_NODE_ORIGIN:    return "Origin";
    case CDN_NODE_MID_TIER:  return "MidTier";
    case CDN_NODE_EDGE:      return "Edge";
    default:                 return "Unknown";
    }
}

const char *cdn_eviction_policy_string(cdn_eviction_policy_t policy)
{
    switch (policy) {
    case CDN_EVICT_LRU:       return "LRU";
    case CDN_EVICT_LFU:       return "LFU";
    case CDN_EVICT_TTL_FIRST: return "TTLFirst";
    default:                  return "Unknown";
    }
}

const char *cdn_routing_strategy_string(cdn_routing_strategy_t strategy)
{
    switch (strategy) {
    case CDN_STRATEGY_ROUND_ROBIN:    return "RoundRobin";
    case CDN_STRATEGY_GEO_DNS:        return "GeoDNS";
    case CDN_STRATEGY_LATENCY_BASED:  return "LatencyBased";
    case CDN_STRATEGY_WEIGHTED:       return "Weighted";
    default:                          return "Unknown";
    }
}
