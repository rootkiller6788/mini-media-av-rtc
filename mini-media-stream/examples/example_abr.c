#include "abr_engine.h"
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    abr_engine_t eng;
    uint32_t i;
    uint32_t selected;

    printf("=== mini-media-stream: ABR Engine Example ===\n\n");

    if (abr_engine_init(&eng) != 0) {
        fprintf(stderr, "Failed to init ABR engine\n");
        return 1;
    }

    abr_engine_add_representation(&eng, 800000,  640,  360,  "360p");
    abr_engine_add_representation(&eng, 1500000, 854,  480,  "480p");
    abr_engine_add_representation(&eng, 3000000, 1280, 720,  "720p");
    abr_engine_add_representation(&eng, 6000000, 1920, 1080, "1080p");

    abr_engine_set_buffer_params(&eng, 30, 10, 60);
    abr_engine_set_algorithm(&eng, ABR_ALGO_HYBRID);

    abr_engine_add_cdn_server(&eng, "https://cdn1.example.com", 50000000, 10);
    abr_engine_add_cdn_server(&eng, "https://cdn2.example.com", 30000000, 5);

    printf("Algorithm: %s\n", abr_algo_string(eng.algorithm));
    printf("Representations: 360p(800k) 480p(1.5M) 720p(3M) 1080p(6M)\n");
    printf("CDN servers: 2\n\n");

    printf("--- BOLA Algorithm Test ---\n");
    abr_engine_set_algorithm(&eng, ABR_ALGO_BOLA);
    {
        double buffers[] = { 5.0, 15.0, 30.0, 45.0, 60.0 };
        for (i = 0; i < 5; i++) {
            abr_engine_set_current_buffer(&eng, buffers[i]);
            selected = abr_engine_decide(&eng);
            printf("  Buffer=%.0fs -> %s (%u kbps)\n",
                   buffers[i],
                   eng.representations[selected].id,
                   eng.representations[selected].bitrate / 1000);
        }
    }

    printf("\n--- Throughput-based Algorithm Test ---\n");
    abr_engine_set_algorithm(&eng, ABR_ALGO_THROUGHPUT);
    {
        uint32_t bandwidths[] = { 500000, 1400000, 3500000, 4500000, 8000000 };
        for (i = 0; i < 5; i++) {
            abr_engine_record_download(&eng, bandwidths[i], 500000, i);
            selected = abr_engine_decide(&eng);
            printf("  BW=%.1f Mbps -> %s (%u kbps, smoothed=%.1f Mbps)\n",
                   (double)bandwidths[i] / 1000000.0,
                   eng.representations[selected].id,
                   eng.representations[selected].bitrate / 1000,
                   abr_engine_get_estimated_bandwidth(&eng) / 1000000.0);
        }
    }

    printf("\n--- Rule-based Algorithm Test ---\n");
    abr_engine_set_algorithm(&eng, ABR_ALGO_RULE_BASED);
    {
        double buffers[] = { 5.0, 3.0, 15.0, 50.0, 65.0 };
        for (i = 0; i < 5; i++) {
            abr_engine_set_current_buffer(&eng, buffers[i]);
            abr_engine_record_segment(&eng, eng.current_rep_index, 300000, 500);
            selected = abr_engine_decide(&eng);
            printf("  Buffer=%.0fs -> %s (%u kbps, switches: up=%u down=%u)\n",
                   buffers[i],
                   eng.representations[selected].id,
                   eng.representations[selected].bitrate / 1000,
                   eng.switch_up_count, eng.switch_down_count);
        }
    }

    printf("\n--- CDN Selection Test ---\n");
    {
        int cdn = abr_engine_select_cdn(&eng);
        if (cdn >= 0) {
            printf("  Active CDN: %s\n", eng.cdn_servers[cdn].base_url);
        }
        abr_engine_mark_cdn_slow(&eng, (uint32_t)cdn);
        printf("  Marked CDN %d as slow, switching...\n", cdn);
        cdn = abr_engine_select_cdn(&eng);
        if (cdn >= 0) {
            printf("  New active CDN: %s\n", eng.cdn_servers[cdn].base_url);
        }
    }

    abr_engine_deinit(&eng);
    printf("\nDone.\n");
    return 0;
}
