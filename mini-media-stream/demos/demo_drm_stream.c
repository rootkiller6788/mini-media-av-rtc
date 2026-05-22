#include "hls_segmenter.h"
#include "dash_segmenter.h"
#include "drm_system.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void demo_drm_key_management(void)
{
    drm_system_t drm;
    uint8_t key_id[DRM_KEY_ID_SIZE];
    uint8_t content_key[DRM_AES128_KEY_SIZE];
    drm_key_info_t key_info;
    int i;

    printf("--- DRM Key Management ---\n\n");

    for (i = 0; i < DRM_KEY_ID_SIZE; i++) key_id[i] = (uint8_t)(0xA0 + i);
    for (i = 0; i < DRM_AES128_KEY_SIZE; i++) content_key[i] = (uint8_t)(0x10 + i);

    drm_system_init(&drm, DRM_KEY_SYSTEM_WIDEVINE, DRM_SCHEME_CENC);

    printf("Initialized: %s DRM system, scheme: %s\n",
           drm_key_system_string(drm.primary_system),
           drm_scheme_string(drm.scheme));

    drm_add_key(&drm, key_id, content_key, DRM_KEY_SYSTEM_WIDEVINE,
                "https://license.widevine.com/widevine");

    printf("Added key: KeyID=");
    for (i = 0; i < DRM_KEY_ID_SIZE; i++) printf("%02X", drm.keys[0].key_id[i]);
    printf("\n  ContentKey=");
    for (i = 0; i < DRM_AES128_KEY_SIZE; i++) printf("%02X", drm.keys[0].content_key[i]);
    printf("\n  License: %s\n\n", drm.keys[0].license_url);

    if (drm_find_key(&drm, key_id, &key_info) == 0) {
        printf("Key lookup: SUCCESS\n");
        printf("  System: %s\n", drm_key_system_string(key_info.key_system));
    }

    printf("\nEnabling key rotation (interval=30s)...\n");
    drm_enable_rotation(&drm, 30);

    printf("Before rotation: Key[0]=");
    for (i = 0; i < 4; i++) printf("%02X", drm.keys[0].content_key[i]);
    printf("...\n");

    drm_rotate_keys(&drm);

    printf("After  rotation: Key[0]=");
    for (i = 0; i < 4; i++) printf("%02X", drm.keys[0].content_key[i]);
    printf("...\n");
    printf("Key rotation completed.\n\n");

    drm_system_deinit(&drm);
}

static void demo_drm_encryption(void)
{
    drm_system_t drm;
    uint8_t key[DRM_AES128_KEY_SIZE];
    uint8_t iv[DRM_AES128_IV_SIZE];
    uint8_t clear_data[64];
    uint8_t encrypted[128];
    uint8_t decrypted[128];
    size_t enc_size, dec_size;
    int i;

    printf("--- DRM AES-128 CBC Encryption/Decryption ---\n\n");

    for (i = 0; i < DRM_AES128_KEY_SIZE; i++) key[i] = (uint8_t)(0xAB + i);
    for (i = 0; i < DRM_AES128_IV_SIZE; i++)  iv[i]  = (uint8_t)(0xCD + i);

    for (i = 0; i < 64; i++) clear_data[i] = (uint8_t)(i * 3 + 1);

    printf("Key: ");
    for (i = 0; i < DRM_AES128_KEY_SIZE; i++) printf("%02X", key[i]);
    printf("\nIV:  ");
    for (i = 0; i < DRM_AES128_IV_SIZE; i++) printf("%02X", iv[i]);
    printf("\n\nClear data (first 16B): ");
    for (i = 0; i < 16; i++) printf("%02X", clear_data[i]);

    if (drm_encrypt_sample_aes128(clear_data, 64, key, iv, encrypted, &enc_size) == 0) {
        printf("\nEncrypted (first 16B):   ");
        for (i = 0; i < 16; i++) printf("%02X", encrypted[i]);
        printf("\nEncrypted size: %zu bytes (padded to AES block)\n", enc_size);

        if (drm_decrypt_sample_aes128(encrypted, enc_size, key, iv,
                                       decrypted, &dec_size) == 0) {
            printf("Decrypted (first 16B):   ");
            for (i = 0; i < 16; i++) printf("%02X", decrypted[i]);
            printf("\nDecrypted size: %zu bytes\n", dec_size);

            if (memcmp(clear_data, decrypted, 64) == 0) {
                printf("VERIFICATION: AES-128 round-trip PASSED\n");
            } else {
                printf("VERIFICATION: AES-128 round-trip FAILED\n");
            }
        }
    }

    printf("\n--- CENC (CTR mode) Sub-sample Encryption ---\n\n");

    {
        uint8_t cenc_key[DRM_AES128_KEY_SIZE];
        uint8_t cenc_iv[DRM_AES128_IV_SIZE];
        uint8_t cenc_clear[64];
        uint8_t cenc_enc[128];
        uint8_t cenc_dec[128];
        size_t cenc_enc_size, cenc_dec_size;
        uint16_t clear_bytes_arr[2] = { 10, 10 };
        uint32_t enc_bytes_arr[2] = { 22, 22 };
        drm_subsample_encryption_t subsample;

        for (i = 0; i < DRM_AES128_KEY_SIZE; i++) cenc_key[i] = (uint8_t)(0xEF + i);
        for (i = 0; i < DRM_AES128_IV_SIZE; i++) cenc_iv[i]  = (uint8_t)(0x01 + i);
        for (i = 0; i < 64; i++) cenc_clear[i] = (uint8_t)(i * 5 + 7);

        memset(&subsample, 0, sizeof(subsample));
        subsample.subsample_count = 2;
        subsample.bytes_of_clear_data = clear_bytes_arr;
        subsample.bytes_of_encrypted_data = enc_bytes_arr;

        printf("Sub-sample pattern: [10B clear | 22B encrypted] x2\n");
        printf("Total clear data: 64 bytes\n\n");

        if (drm_encrypt_sample_cenc(cenc_clear, 64, cenc_key, cenc_iv,
                                     &subsample, cenc_enc, &cenc_enc_size) == 0) {
            printf("CENC encrypted: %zu bytes\n", cenc_enc_size);
            printf("  First 10B (clear): ");
            for (i = 0; i < 10; i++) printf("%02X", cenc_enc[i]);
            printf("\n  Next 10B (enc):    ");
            for (i = 10; i < 20; i++) printf("%02X", cenc_enc[i]);

            if (drm_decrypt_sample_cenc(cenc_enc, cenc_enc_size, cenc_key, cenc_iv,
                                         &subsample, cenc_dec, &cenc_dec_size) == 0) {
                printf("\n\nCENC decrypted: %zu bytes\n", cenc_dec_size);
                printf("  First 10B (clear): ");
                for (i = 0; i < 10; i++) printf("%02X", cenc_dec[i]);

                if (memcmp(cenc_clear, cenc_dec, 64) == 0) {
                    printf("\nVERIFICATION: CENC round-trip PASSED\n");
                } else {
                    printf("\nVERIFICATION: CENC round-trip FAILED\n");
                }
            }
        }
    }

    drm_system_deinit(&drm);
}

static void demo_drm_init_data(void)
{
    drm_system_t drm;
    drm_init_data_t init_data;
    uint8_t key_id[DRM_KEY_ID_SIZE];
    uint8_t content_key[DRM_AES128_KEY_SIZE];
    int i;

    printf("\n--- DRM Initialization Data (PSSH) ---\n\n");

    for (i = 0; i < DRM_KEY_ID_SIZE; i++) key_id[i] = (uint8_t)(0xDE + i);
    for (i = 0; i < DRM_AES128_KEY_SIZE; i++) content_key[i] = (uint8_t)(0xAD + i);

    printf("Testing Widevine PSSH...\n");
    drm_system_init(&drm, DRM_KEY_SYSTEM_WIDEVINE, DRM_SCHEME_CENC);
    drm_add_key(&drm, key_id, content_key, DRM_KEY_SYSTEM_WIDEVINE,
                "https://license.widevine.com/cenc");

    if (drm_build_init_data(&drm, &init_data) == 0) {
        printf("  System: %s\n", init_data.system_id_str);
        printf("  PSSH box size: %zu bytes\n", init_data.pssh.size);
        printf("  PSSH hex (first 32B): ");
        for (i = 0; i < 32 && i < (int)init_data.pssh.size; i++) {
            printf("%02X", init_data.pssh.data[i]);
        }
        printf("\n  has_playready: %s\n\n", init_data.has_playready ? "yes" : "no");
    }
    drm_free_init_data(&init_data);
    drm_system_deinit(&drm);

    printf("Testing PlayReady PSSH + PlayReady header...\n");
    drm_system_init(&drm, DRM_KEY_SYSTEM_PLAYREADY, DRM_SCHEME_CENC);
    drm_add_key(&drm, key_id, content_key, DRM_KEY_SYSTEM_PLAYREADY,
                "https://license.playready.com/rightsmanager.asmx");

    if (drm_build_init_data(&drm, &init_data) == 0) {
        printf("  System: %s\n", init_data.system_id_str);
        printf("  PSSH box size: %zu bytes\n", init_data.pssh.size);
        printf("  PlayReady header size: %zu bytes\n", init_data.playready_header.size);
        printf("  has_pssh: %s\n", init_data.has_pssh ? "yes" : "no");
        printf("  has_playready: %s\n\n", init_data.has_playready ? "yes" : "no");
    }
    drm_free_init_data(&init_data);
    drm_system_deinit(&drm);

    printf("Testing FairPlay PSSH...\n");
    drm_system_init(&drm, DRM_KEY_SYSTEM_FAIRPLAY, DRM_SCHEME_CBCS);
    drm_add_key(&drm, key_id, content_key, DRM_KEY_SYSTEM_FAIRPLAY,
                "https://fp.license.server.com/ckc");

    if (drm_build_init_data(&drm, &init_data) == 0) {
        printf("  System: %s\n", init_data.system_id_str);
        printf("  PSSH box size: %zu bytes\n", init_data.pssh.size);
        printf("  has_playready: %s\n\n", init_data.has_playready ? "yes" : "no");
    }
    drm_free_init_data(&init_data);
    drm_system_deinit(&drm);
}

static void demo_drm_license_flow(void)
{
    drm_system_t drm;
    uint8_t key_id[DRM_KEY_ID_SIZE];
    uint8_t content_key[DRM_AES128_KEY_SIZE];
    drm_license_response_t response;
    int i;

    printf("--- DRM License Acquisition ---\n\n");

    for (i = 0; i < DRM_KEY_ID_SIZE; i++) key_id[i] = (uint8_t)(0x42 + i);

    drm_system_init(&drm, DRM_KEY_SYSTEM_WIDEVINE, DRM_SCHEME_CENC);

    printf("Requesting license from: %s\n", drm.default_license_url);
    printf("Key ID: ");
    for (i = 0; i < DRM_KEY_ID_SIZE; i++) printf("%02X", key_id[i]);
    printf("\n\n");

    if (drm_request_license(drm.default_license_url, key_id, &response) == 0) {
        printf("License response received:\n");
        printf("  Success: %s\n", response.success ? "YES" : "NO");
        printf("  Request time: %ums\n", response.request_time_ms);
        printf("  Response time: %ums\n", response.response_time_ms);
        printf("  Latency: %ums\n", response.response_time_ms - response.request_time_ms);
        printf("  Lease duration: %llums\n", (unsigned long long)response.lease_duration_ms);
        printf("  Content key: ");
        for (i = 0; i < DRM_AES128_KEY_SIZE; i++) printf("%02X", response.content_key[i]);
        printf("\n\n");
    }

    printf("Adding acquired key to DRM system...\n");
    drm_add_key(&drm, key_id, response.content_key, DRM_KEY_SYSTEM_WIDEVINE,
                drm.default_license_url);
    printf("Key stored. Total keys: %u\n\n", drm.key_count);

    printf("Renewing license...\n");
    if (drm_renew_license(&drm, key_id) == 0) {
        printf("License renewed successfully.\n");
        printf("New content key: ");
        for (i = 0; i < DRM_AES128_KEY_SIZE; i++) printf("%02X", drm.keys[0].content_key[i]);
        printf("\n");
    }

    printf("\n--- IV Derivation ---\n");
    {
        uint8_t constant_iv[DRM_AES128_IV_SIZE];
        uint8_t derived_iv[DRM_AES128_IV_SIZE];
        for (i = 0; i < DRM_AES128_IV_SIZE; i++) constant_iv[i] = (uint8_t)(0x55 + i);

        printf("Constant IV: ");
        for (i = 0; i < DRM_AES128_IV_SIZE; i++) printf("%02X", constant_iv[i]);

        {
            uint64_t sample;
            for (sample = 0; sample < 3; sample++) {
                drm_derive_iv(derived_iv, sample, constant_iv);
                printf("\n  Sample %llu IV: ", (unsigned long long)sample);
                for (i = 0; i < DRM_AES128_IV_SIZE; i++) printf("%02X", derived_iv[i]);
            }
        }
        printf("\n");
    }

    printf("\nAll DRM key systems:\n");
    printf("  %s -> %s\n", drm_key_system_string(DRM_KEY_SYSTEM_WIDEVINE),
           drm_key_system_id(DRM_KEY_SYSTEM_WIDEVINE));
    printf("  %s -> %s\n", drm_key_system_string(DRM_KEY_SYSTEM_PLAYREADY),
           drm_key_system_id(DRM_KEY_SYSTEM_PLAYREADY));
    printf("  %s -> %s\n", drm_key_system_string(DRM_KEY_SYSTEM_FAIRPLAY),
           drm_key_system_id(DRM_KEY_SYSTEM_FAIRPLAY));

    drm_system_deinit(&drm);
}

static void demo_hls_with_drm(void)
{
    hls_segmenter_t seg;
    hls_key_t drm_key;
    static char playlist[HLS_PLAYLIST_MAX_SIZE];
    int i;

    printf("\n--- HLS with AES-128 Encryption ---\n\n");

    hls_segmenter_init(&seg, "./output", "encrypted");
    hls_segmenter_set_type(&seg, HLS_PLAYLIST_VOD);
    hls_segmenter_set_target_duration(&seg, 6);

    for (i = 0; i < 4; i++) {
        char filename[64];
        snprintf(filename, sizeof(filename), "enc_seg_%d.ts", i);

        hls_segmenter_start_segment(&seg, filename);

        {
            uint8_t ts_packet[188];
            int j;
            for (j = 0; j < 188; j++) ts_packet[j] = (uint8_t)(j + i);
            ts_packet[0] = 0x47;
            ts_packet[1] = 0x40;
            ts_packet[2] = (uint8_t)i;
            hls_segmenter_write_packet(&seg, ts_packet, sizeof(ts_packet));
        }

        hls_segmenter_finish_segment(&seg, 6.0);

        if (i == 1) {
            memset(&drm_key, 0, sizeof(drm_key));
            drm_key.method = HLS_KEY_AES128;
            strcpy(drm_key.uri, "https://keyserver.example.com/hls/key.bin");
            drm_key.has_iv = 1;
            {
                int j;
                for (j = 0; j < 16; j++) drm_key.iv[j] = (uint8_t)(0x10 + j);
            }
            hls_segmenter_add_key(&seg, &drm_key);
            printf("  [AES-128 key applied starting segment %d]\n", i);
        }

        printf("  Segment %d: %s (encrypted: %s)\n",
               i, filename, seg.playlist.segments[i].has_key ? "YES" : "NO");
    }

    hls_segmenter_end_playlist(&seg);

    {
        int plen = hls_segmenter_generate_playlist(&seg, playlist, sizeof(playlist));
        if (plen > 0) {
            printf("\n--- HLS Playlist with DRM ---\n%s\n", playlist);
        }
    }

    hls_segmenter_deinit(&seg);
}

int main(void)
{
    printf("=================================================\n");
    printf("  mini-media-stream: DRM Demo\n");
    printf("  AES-128, CENC, Widevine, PlayReady, FairPlay\n");
    printf("=================================================\n\n");

    demo_drm_key_management();
    demo_drm_encryption();
    demo_drm_init_data();
    demo_drm_license_flow();
    demo_hls_with_drm();

    printf("=================================================\n");
    printf("  All DRM demos completed.\n");
    printf("=================================================\n");

    return 0;
}
