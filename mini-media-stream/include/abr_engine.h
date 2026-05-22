#ifndef ABR_ENGINE_H
#define ABR_ENGINE_H

#include <stdint.h>
#include <stddef.h>

#define ABR_MAX_REPRESENTATIONS    16
#define ABR_MAX_CDN_SERVERS         4
#define ABR_SEGMENT_HISTORY_SIZE   32
#define ABR_DEFAULT_BUFFER_TARGET  30u
#define ABR_DEFAULT_MIN_BUFFER     10u
#define ABR_DEFAULT_MAX_BUFFER     60u

typedef enum {
    ABR_ALGO_BOLA,
    ABR_ALGO_THROUGHPUT,
    ABR_ALGO_RULE_BASED,
    ABR_ALGO_HYBRID
} abr_algorithm_t;

typedef enum {
    ABR_SWITCH_NONE,
    ABR_SWITCH_UP,
    ABR_SWITCH_DOWN,
    ABR_SWITCH_KEEP
} abr_switch_direction_t;

typedef struct {
    uint32_t bitrate;           /* bits per second */
    uint32_t width;
    uint32_t height;
    uint32_t index;
    char     id[64];
} abr_representation_t;

typedef struct {
    uint32_t bandwidth_bps;     /* estimated throughput */
    uint64_t download_start_ms;
    uint64_t download_end_ms;
    uint64_t segment_size_bytes;
    uint32_t segment_index;
    uint8_t  valid;
} abr_download_record_t;

typedef struct {
    double   buffer_level_sec;
    uint32_t current_bitrate;
    uint32_t target_buffer_sec;
    uint32_t max_buffer_sec;
    uint32_t min_buffer_sec;
    uint32_t utility_offset;
    double   segment_duration_sec;
} abr_bola_state_t;

typedef struct {
    double   beta;
    double   safety_factor;
    uint32_t smoothing_window;
} abr_bola_config_t;

typedef struct {
    uint64_t    response_time_ms;
    double      error_rate;
    uint32_t    bandwidth_bps;
    uint8_t     available;
    uint8_t     priority;
    char        base_url[256];
} abr_cdn_server_t;

typedef struct {
    abr_algorithm_t         algorithm;
    abr_representation_t    representations[ABR_MAX_REPRESENTATIONS];
    uint32_t                rep_count;
    uint32_t                current_rep_index;
    uint32_t                previous_rep_index;

    abr_download_record_t   history[ABR_SEGMENT_HISTORY_SIZE];
    uint32_t                history_count;
    uint32_t                history_head;

    double                  estimated_bandwidth_bps;
    double                  smoothed_bandwidth_bps;
    double                  smoothing_alpha;

    abr_bola_state_t        bola_state;
    abr_bola_config_t       bola_config;

    abr_cdn_server_t        cdn_servers[ABR_MAX_CDN_SERVERS];
    uint32_t                cdn_count;
    uint32_t                active_cdn_index;

    double                  buffer_level_sec;
    double                  playback_rate;

    uint32_t                switch_up_count;
    uint32_t                switch_down_count;
    uint32_t                oscillation_threshold;
    uint64_t                last_switch_time_ms;
    double                  switch_history[16];
    uint32_t                switch_history_len;

    uint8_t                 enabled;
    uint8_t                 pending_switch;
    uint8_t                 rapid_catchup;
} abr_engine_t;

int     abr_engine_init(abr_engine_t *eng);
void    abr_engine_deinit(abr_engine_t *eng);

int     abr_engine_add_representation(abr_engine_t *eng, uint32_t bitrate,
                                      uint32_t width, uint32_t height, const char *id);
int     abr_engine_set_algorithm(abr_engine_t *eng, abr_algorithm_t algo);
int     abr_engine_set_buffer_params(abr_engine_t *eng, uint32_t target_sec,
                                     uint32_t min_sec, uint32_t max_sec);
int     abr_engine_set_bola_config(abr_engine_t *eng, const abr_bola_config_t *config);

int     abr_engine_set_current_buffer(abr_engine_t *eng, double buffer_level_sec);
int     abr_engine_set_playback_rate(abr_engine_t *eng, double rate);

int     abr_engine_record_download(abr_engine_t *eng, uint32_t bandwidth_bps,
                                   uint64_t size_bytes, uint32_t segment_index);
int     abr_engine_record_segment(abr_engine_t *eng, uint32_t rep_index,
                                  uint64_t size_bytes, uint64_t download_time_ms);

uint32_t abr_engine_decide(abr_engine_t *eng);

int     abr_engine_decide_bola(abr_engine_t *eng);
int     abr_engine_decide_throughput(abr_engine_t *eng);
int     abr_engine_decide_rule_based(abr_engine_t *eng);
int     abr_engine_decide_hybrid(abr_engine_t *eng);

int     abr_engine_should_switch(abr_engine_t *eng, abr_switch_direction_t *direction);
int     abr_engine_prevent_oscillation(abr_engine_t *eng, uint32_t new_rep_index);
int     abr_engine_commit_switch(abr_engine_t *eng, uint32_t new_rep_index);

int     abr_engine_add_cdn_server(abr_engine_t *eng, const char *base_url,
                                  uint32_t bandwidth_bps, uint8_t priority);
int     abr_engine_select_cdn(abr_engine_t *eng);
int     abr_engine_switch_cdn(abr_engine_t *eng, uint32_t cdn_index);
int     abr_engine_mark_cdn_slow(abr_engine_t *eng, uint32_t cdn_index);

int     abr_engine_get_current_bitrate(abr_engine_t *eng, uint32_t *bitrate);
uint32_t abr_engine_get_estimated_bandwidth(abr_engine_t *eng);
int     abr_engine_get_representation(abr_engine_t *eng, uint32_t index,
                                      abr_representation_t *rep);

double  abr_smooth_bandwidth(const double *samples, uint32_t count, double alpha);
double  abr_bola_score(double buffer_level, uint32_t bitrate, uint32_t target_buffer,
                       double utility_offset, double segment_duration);

const char *abr_algo_string(abr_algorithm_t algo);

#endif
