#include "dash_segmenter.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    dash_segmenter_t seg;
    static char mpd_buffer[DASH_MPD_MAX_SIZE];
    int mpd_len;

    printf("=== mini-media-stream: DASH Segmenter Example ===\n\n");

    if (dash_segmenter_init(&seg, "./output", "dash") != 0) {
        fprintf(stderr, "Failed to init DASH segmenter\n");
        return 1;
    }

    dash_segmenter_mpd_init(&seg, "urn:mpeg:dash:profile:isoff-live:2011", 0);

    dash_segmenter_set_utc_timing(&seg, DASH_TIMING_NTP, "time.example.com");

    dash_segmenter_add_period(&seg, "p0", 0, 60000);

    dash_segmenter_add_adaptation_set(&seg, 0, "video", DASH_CONTENT_VIDEO, NULL);

    dash_segmenter_add_representation(&seg, 0, 0, "v1080p", 5000000, 1920, 1080,
                                      "avc1.640028", "video/mp4");
    dash_segmenter_set_template(&seg, 0, 0, 0, DASH_TEMPLATE_NUMBER, 1, 1968);
    dash_segmenter_set_init_segment(&seg, 0, 0, 0, "$RepresentationID$/init.mp4");
    dash_segmenter_set_media_segment(&seg, 0, 0, 0, "$RepresentationID$/$Number$.m4s");

    dash_segmenter_add_representation(&seg, 0, 0, "v720p", 2500000, 1280, 720,
                                      "avc1.64001f", "video/mp4");
    dash_segmenter_set_template(&seg, 0, 0, 1, DASH_TEMPLATE_NUMBER, 1, 1968);
    dash_segmenter_set_init_segment(&seg, 0, 0, 1, "$RepresentationID$/init.mp4");
    dash_segmenter_set_media_segment(&seg, 0, 0, 1, "$RepresentationID$/$Number$.m4s");

    dash_segmenter_add_adaptation_set(&seg, 0, "audio", DASH_CONTENT_AUDIO, "eng");

    dash_segmenter_add_representation(&seg, 0, 1, "a128k", 128000, 0, 0,
                                      "mp4a.40.2", "audio/mp4");
    dash_segmenter_set_template(&seg, 0, 1, 0, DASH_TEMPLATE_NUMBER, 1, 2000);
    dash_segmenter_set_init_segment(&seg, 0, 1, 0, "$RepresentationID$/init.mp4");
    dash_segmenter_set_media_segment(&seg, 0, 1, 0, "$RepresentationID$/$Number$.m4s");

    printf("Video AdaptationSet: 2 representations (1080p@5Mbps, 720p@2.5Mbps)\n");
    printf("Audio AdaptationSet: 1 representation (128kbps AAC)\n");
    printf("Segment template: $RepresentationID$/$Number$.m4s\n\n");

    {
        static const uint8_t ftyp[] = {
            0x00, 0x00, 0x00, 0x20, 'f', 't', 'y', 'p',
            'i', 's', 'o', '6', 0x00, 0x00, 0x00, 0x01,
            'i', 's', 'o', '6', 'd', 'a', 's', 'h',
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
        };
        static const uint8_t moov[] = {
            0x00, 0x00, 0x00, 0x08, 'm', 'o', 'o', 'v'
        };
        dash_segmenter_write_init_segment(&seg, ftyp, sizeof(ftyp), moov, sizeof(moov));
        printf("Init segment written: output/init.mp4\n");
    }

    {
        uint32_t i;
        for (i = 0; i < 3; i++) {
            char filename[64];
            snprintf(filename, sizeof(filename), "dash_%u.m4s", i + 1);

            dash_segmenter_add_timeline_entry(&seg, 0, 0, 0,
                                               i * 2000, 2000, i + 1);

            if (dash_segmenter_start_fmp4(&seg, filename, i * 2000) != 0) {
                fprintf(stderr, "Failed to start fMP4 segment\n");
                return 1;
            }

            {
                static const uint8_t moof[] = { 0x00, 0x00, 0x00, 0x08, 'm', 'o', 'o', 'f' };
                static const uint8_t mdat[] = { 0x00, 0x00, 0x00, 0x08, 'm', 'd', 'a', 't' };
                fwrite(moof, 1, sizeof(moof), seg.segment_file);
                fwrite(mdat, 1, sizeof(mdat), seg.segment_file);
            }

            if (dash_segmenter_finish_fmp4(&seg) != 0) {
                fprintf(stderr, "Failed to finish fMP4 segment\n");
                return 1;
            }

            printf("  fMP4 segment %u: %s (moof+mdat)\n", i + 1, filename);
        }
    }

    mpd_len = dash_segmenter_generate_mpd(&seg, mpd_buffer, sizeof(mpd_buffer));
    if (mpd_len > 0) {
        printf("\n--- Generated MPD Manifest ---\n%s", mpd_buffer);
    }

    dash_segmenter_write_mpd_file(&seg, "stream.mpd");
    printf("\nMPD written to output/stream.mpd\n");

    dash_segmenter_deinit(&seg);
    printf("\nDone.\n");
    return 0;
}
