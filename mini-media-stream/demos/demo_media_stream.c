#include "hls_segmenter.h"
#include "dash_segmenter.h"
#include "abr_engine.h"
#include "cdn_edge.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void demo_hls_live_stream(void)
{
    hls_segmenter_t seg;
    static char playlist[HLS_PLAYLIST_MAX_SIZE];
    int frame;

    printf("--- HLS Live Streaming Simulation ---\n\n");

    hls_segmenter_init(&seg, "./output", "live");
    hls_segmenter_set_type(&seg, HLS_PLAYLIST_LIVE);
    hls_segmenter_set_target_duration(&seg, 2);
    hls_segmenter_set_window_size(&seg, 5);
    hls_segmenter_set_version(&seg, 3);

    printf("Configured: LIVE mode, window=%u segments, targetDuration=%us\n\n",
           seg.playlist.window_size, seg.playlist.target_duration);

    for (frame = 0; frame < 12; frame++) {
        char filename[64];
        snprintf(filename, sizeof(filename), "live_seg_%d.ts", frame);

        hls_segmenter_start_segment(&seg, filename);

        {
            uint8_t ts[188] = { 0 };
            ts[0] = 0x47;
            ts[1] = (uint8_t)((frame & 0x1F) << 3) | 0x01;
            ts[3] = 0x10;
            hls_segmenter_write_packet(&seg, ts, sizeof(ts));
            hls_segmenter_write_packet(&seg, ts, sizeof(ts));
        }

        hls_segmenter_finish_segment(&seg, 2.0);
        hls_segmenter_set_program_date_time(&seg,
            (int64_t)(1715702400000LL + frame * 2000));

        if (frame % 5 == 4) {
            hls_segmenter_mark_discontinuity(&seg);
        }

        printf("[Frame %2d] Segment %d: %s (seq=%u, count=%u)\n",
               frame, frame, filename,
               seg.playlist.media_sequence,
               seg.playlist.segment_count);

        if (frame >= 5) {
            int plen = hls_segmenter_generate_playlist(&seg, playlist, sizeof(playlist));
            if (plen > 0 && frame % 2 == 0) {
                printf("\n  --- Live Playlist Snapshot (frame %d) ---\n", frame);
                printf("%s\n", playlist);
            }
        }
    }

    hls_segmenter_end_playlist(&seg);

    printf("\n--- Final Playlist (ENDLIST added) ---\n");
    {
        int plen = hls_segmenter_generate_playlist(&seg, playlist, sizeof(playlist));
        if (plen > 0) printf("%s\n", playlist);
    }

    hls_segmenter_write_playlist_file(&seg, "live_output.m3u8");
    printf("Written: output/live_output.m3u8\n");

    hls_segmenter_deinit(&seg);
}

static void demo_dash_with_abr(void)
{
    dash_segmenter_t seg;
    abr_engine_t abr;
    static char mpd[DASH_MPD_MAX_SIZE];
    uint32_t seg_num;
    uint32_t selected_bitrate;

    printf("\n\n--- DASH + ABR Streaming Simulation ---\n\n");

    dash_segmenter_init(&seg, "./output", "dash_demo");
    dash_segmenter_mpd_init(&seg, "urn:mpeg:dash:profile:isoff-live:2011", 1);
    dash_segmenter_set_utc_timing(&seg, DASH_TIMING_NTP, "time.example.com");

    dash_segmenter_add_period(&seg, "period_main", 0, 0);

    dash_segmenter_add_adaptation_set(&seg, 0, "video", DASH_CONTENT_VIDEO, NULL);
    dash_segmenter_add_representation(&seg, 0, 0, "v360p",  800000,  640,  360,
                                      "avc1.42c01e", "video/mp4");
    dash_segmenter_add_representation(&seg, 0, 0, "v480p",  1500000, 854,  480,
                                      "avc1.4d401e", "video/mp4");
    dash_segmenter_add_representation(&seg, 0, 0, "v720p",  3000000, 1280, 720,
                                      "avc1.64001f", "video/mp4");
    dash_segmenter_add_representation(&seg, 0, 0, "v1080p", 6000000, 1920, 1080,
                                      "avc1.640028", "video/mp4");

    dash_segmenter_add_adaptation_set(&seg, 0, "audio", DASH_CONTENT_AUDIO, "eng");
    dash_segmenter_add_representation(&seg, 0, 1, "a96k",  96000, 0, 0,
                                      "mp4a.40.2", "audio/mp4");
    dash_segmenter_add_representation(&seg, 0, 1, "a128k", 128000, 0, 0,
                                      "mp4a.40.2", "audio/mp4");

    {
        uint32_t ri;
        for (ri = 0; ri < 4; ri++) {
            dash_segmenter_set_template(&seg, 0, 0, ri, DASH_TEMPLATE_NUMBER, 1, 2000);
        }
        for (ri = 0; ri < 2; ri++) {
            dash_segmenter_set_template(&seg, 0, 1, ri, DASH_TEMPLATE_NUMBER, 1, 2000);
        }
    }

    abr_engine_init(&abr);
    abr_engine_add_representation(&abr, 800000,  640,  360,  "v360p");
    abr_engine_add_representation(&abr, 1500000, 854,  480,  "v480p");
    abr_engine_add_representation(&abr, 3000000, 1280, 720,  "v720p");
    abr_engine_add_representation(&abr, 6000000, 1920, 1080, "v1080p");
    abr_engine_set_algorithm(&abr, ABR_ALGO_HYBRID);
    abr_engine_set_buffer_params(&abr, 30, 10, 60);

    printf("Representations: 4 video + 2 audio\n");
    printf("ABR Algorithm: %s (Hybrid: BOLA + Throughput)\n\n", abr_algo_string(abr.algorithm));

    printf("--- Simulating 10 segment downloads ---\n");
    for (seg_num = 0; seg_num < 10; seg_num++) {
        uint64_t seg_size;
        uint64_t download_ms;
        double buffer;

        selected_bitrate = abr_engine_decide(&abr);
        abr_engine_get_current_bitrate(&abr, &selected_bitrate);

        seg_size = (uint64_t)((double)selected_bitrate * 2.0 / 8.0);
        download_ms = (seg_num == 0) ? 1500 : 500 + (uint64_t)(seg_num * 50);

        abr_engine_record_segment(&abr, abr.current_rep_index, seg_size, download_ms);

        if (seg_num < 5) {
            buffer = 30.0 + (double)(seg_num) * 2.0;
        } else {
            buffer = 40.0 - (double)(seg_num - 5) * 3.0;
        }
        abr_engine_set_current_buffer(&abr, buffer);

        printf("  Seg %2u: Bitrate=%4u kbps | Size=%6llu B | "
               "DL=%3llu ms | Buffer=%.1fs | EstBW=%.1f Mbps\n",
               seg_num + 1, selected_bitrate / 1000,
               (unsigned long long)seg_size,
               (unsigned long long)download_ms, buffer,
               abr_engine_get_estimated_bandwidth(&abr) / 1000000.0);

        abr_engine_decide(&abr);
    }

    printf("\nSwitch statistics: Up=%u Down=%u Oscillations prevented=%u\n",
           abr.switch_up_count, abr.switch_down_count,
           abr.switch_history_len > 0 ? abr.switch_history_len : 0);

    {
        int mpd_len = dash_segmenter_generate_mpd(&seg, mpd, sizeof(mpd));
        if (mpd_len > 0) {
            printf("\n--- MPD Manifest (dynamic) ---\n%s\n", mpd);
        }
    }

    dash_segmenter_write_mpd_file(&seg, "dash_demo.mpd");
    printf("Written: output/dash_demo.mpd\n");

    dash_segmenter_deinit(&seg);
    abr_engine_deinit(&abr);
}

static void demo_cdn_simulation(void)
{
    cdn_t cdn;
    cdn_request_log_t log;
    uint32_t hits, misses;
    float hit_ratio;
    uint64_t bytes_stored;
    int i;

    printf("\n\n--- CDN Edge Simulation ---\n\n");

    cdn_init(&cdn, "https://origin.example.com");

    cdn_add_edge(&cdn, "edge-us-east", "edge1.cdn.example.com", 443,
                 CDN_NODE_EDGE, 100);
    cdn_add_edge(&cdn, "edge-us-west", "edge2.cdn.example.com", 443,
                 CDN_NODE_EDGE, 80);
    cdn_add_edge(&cdn, "edge-eu-west", "edge3.cdn.example.com", 443,
                 CDN_NODE_EDGE, 60);
    cdn_add_edge(&cdn, "mid-tier-1", "mid1.cdn.example.com", 443,
                 CDN_NODE_MID_TIER, 50);
    cdn_add_edge(&cdn, "origin-main", "origin.example.com", 443,
                 CDN_NODE_ORIGIN, 10);

    cdn_add_dns_record(&cdn, "cdn.example.com", "203.0.113.1", 300);
    cdn_add_dns_record(&cdn, "cdn.example.com", "203.0.113.2", 300);

    cdn_set_routing_strategy(&cdn, CDN_STRATEGY_LATENCY_BASED);

    cdn.edges[0].avg_latency_ms = 15.0;
    cdn.edges[1].avg_latency_ms = 45.0;
    cdn.edges[2].avg_latency_ms = 25.0;

    printf("Topology: Origin -> Mid-Tier -> 3 Edge nodes\n");
    printf("Routing: %s\n\n", cdn_routing_strategy_string(cdn.routing_strategy));

    printf("Edge Nodes:\n");
    for (i = 0; i < (int)cdn.edge_count; i++) {
        printf("  [%s] %s port=%u type=%s latency=%.0fms weight=%u\n",
               cdn.edges[i].id, cdn.edges[i].hostname,
               cdn.edges[i].port, cdn_node_type_string(cdn.edges[i].type),
               cdn.edges[i].avg_latency_ms, cdn.edges[i].weight);
    }

    printf("\n--- Simulating 8 segment requests ---\n");
    {
        const char *urls[] = {
            "/live/segment_1.ts",
            "/live/segment_2.ts",
            "/live/segment_3.ts",
            "/live/segment_1.ts",
            "/live/segment_4.ts",
            "/live/segment_2.ts",
            "/live/segment_5.ts",
            "/live/segment_1.ts"
        };

        for (i = 0; i < 8; i++) {
            int rc = cdn_simulate_request(&cdn, urls[i], &log);
            printf("  Request #%d: %s\n", i + 1, urls[i]);
            printf("    Edge: %s | Status: %u | Cache: %s | "
                   "Bytes: %llu | Latency: %llums\n",
                   log.edge_node_id, log.status_code,
                   log.cache_hit ? "HIT" : "MISS",
                   (unsigned long long)log.bytes_transferred,
                   (unsigned long long)(log.response_time_ms - log.request_time_ms));

            if (log.cache_hit) {
                printf("    [Cache HIT - served from edge]\n");
            } else {
                {
                    static uint8_t segment_data[] = {
                        0x47, 0x00, 0x10, 0x00, 0x00, 0x01, 0xE0, 0x00,
                        0x47, 0x01, 0x11, 0x00, 0x00, 0x01, 0xE0, 0x01
                    };
                    cdn_cache_put(&cdn.cache, urls[i], segment_data,
                                 sizeof(segment_data), 3600000);
                }
                printf("    [Cache MISS - stored in cache, TTL=3600s]\n");
            }

            (void)rc;
        }
    }

    printf("\n--- Pre-warming simulation ---\n");
    {
        cdn_prewarm_add(&cdn, "/live/segment_6.ts");
        cdn_prewarm_add(&cdn, "/live/segment_7.ts");
        cdn_prewarm_add(&cdn, "/live/segment_8.ts");

        printf("  Pre-warm list (%u URLs):\n", cdn.prewarm_count);
        for (i = 0; i < (int)cdn.prewarm_count; i++) {
            printf("    %s\n", cdn.prewarm_list[i]);
        }

        cdn_prewarm_execute(&cdn);
        printf("  Pre-warming executed.\n");
    }

    printf("\n--- Cache Statistics ---\n");
    cdn_cache_stats(&cdn.cache, &hits, &misses, &hit_ratio, &bytes_stored);
    printf("  Hits: %u | Misses: %u | Hit Ratio: %.1f%% | Cached: %llu bytes\n",
           hits, misses, hit_ratio * 100.0f, (unsigned long long)bytes_stored);

    printf("\n--- DNS Routing ---\n");
    {
        char edge_url[256];
        if (cdn_route_dns(&cdn, "cdn.example.com", edge_url, sizeof(edge_url)) == 0) {
            printf("  DNS resolution for cdn.example.com -> %s\n", edge_url);
        }
    }

    printf("\n--- 302 Redirect Simulation ---\n");
    {
        char redirect_url[512];
        if (cdn_redirect_302(&cdn, "/live/segment_9.ts", redirect_url,
                             sizeof(redirect_url)) == 0) {
            printf("  Redirect: /live/segment_9.ts -> %s\n", redirect_url);
        }
    }

    cdn_deinit(&cdn);
}

int main(void)
{
    printf("=================================================\n");
    printf("  mini-media-stream: Full Demo\n");
    printf("  HLS + DASH + ABR + CDN\n");
    printf("=================================================\n\n");

    demo_hls_live_stream();
    demo_dash_with_abr();
    demo_cdn_simulation();

    printf("\n=================================================\n");
    printf("  All demos completed.\n");
    printf("=================================================\n");

    return 0;
}
