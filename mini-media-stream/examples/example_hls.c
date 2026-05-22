#include "hls_segmenter.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    hls_segmenter_t seg;
    hls_key_t key;
    static char playlist[HLS_PLAYLIST_MAX_SIZE];
    int playlist_len;
    uint32_t i;

    printf("=== mini-media-stream: HLS Segmenter Example ===\n\n");

    if (hls_segmenter_init(&seg, "./output", "segment") != 0) {
        fprintf(stderr, "Failed to init HLS segmenter\n");
        return 1;
    }

    hls_segmenter_set_type(&seg, HLS_PLAYLIST_VOD);
    hls_segmenter_set_target_duration(&seg, 6);
    hls_segmenter_set_version(&seg, 3);

    printf("HLS Playlist Type: %s (Version %u)\n",
           hls_playlist_type_string(seg.playlist.type),
           seg.playlist.version);

    for (i = 0; i < 5; i++) {
        char filename[64];
        snprintf(filename, sizeof(filename), "segment_%u.ts", i);

        if (hls_segmenter_start_segment(&seg, filename) != 0) {
            fprintf(stderr, "Failed to start segment %u\n", i);
            return 1;
        }

        {
            static const uint8_t dummy_data[] = {
                0x47, 0x00, 0x10, 0x00, 0x00, 0x01, 0xE0, 0x00
            };
            hls_segmenter_write_packet(&seg, dummy_data, sizeof(dummy_data));
        }

        if (hls_segmenter_finish_segment(&seg, 5.0) != 0) {
            fprintf(stderr, "Failed to finish segment %u\n", i);
            return 1;
        }

        printf("  Created segment %u: %s (duration=5.0s)\n", i, filename);

        if (i == 2) {
            memset(&key, 0, sizeof(key));
            key.method = HLS_KEY_AES128;
            strcpy(key.uri, "https://keyserver.example.com/key1.bin");
            hls_segmenter_add_key(&seg, &key);
            hls_segmenter_mark_discontinuity(&seg);
            hls_segmenter_set_program_date_time(&seg, 1715702400000LL);
            printf("  [AES-128 Key added, discontinuity marked]\n");
        }
    }

    hls_segmenter_end_playlist(&seg);

    playlist_len = hls_segmenter_generate_playlist(&seg, playlist, sizeof(playlist));
    if (playlist_len > 0) {
        printf("\n--- Generated M3U8 Playlist ---\n%s", playlist);
    }

    hls_segmenter_write_playlist_file(&seg, "playlist.m3u8");
    printf("\nPlaylist written to output/playlist.m3u8\n");

    {
        hls_variant_playlist_t vp;
        static char variant_buf[HLS_PLAYLIST_MAX_SIZE];
        int vlen;

        hls_variant_init(&vp);
        hls_variant_add_stream(&vp, "hi/playlist.m3u8", 5000000, 1920, 1080, "avc1.640028,mp4a.40.2");
        hls_variant_add_stream(&vp, "md/playlist.m3u8", 2500000, 1280, 720,  "avc1.64001f,mp4a.40.2");
        hls_variant_add_stream(&vp, "lo/playlist.m3u8", 800000,  640,  360,  "avc1.42c01e,mp4a.40.2");

        vlen = hls_variant_generate_m3u8(&vp, variant_buf, sizeof(variant_buf));
        if (vlen > 0) {
            printf("\n--- Variant Playlist (Multi-bitrate) ---\n%s", variant_buf);
        }
    }

    hls_segmenter_deinit(&seg);
    printf("\nDone.\n");
    return 0;
}
