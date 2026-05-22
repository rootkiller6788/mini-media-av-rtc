#include "drm_system.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define DRM_AES_BLOCK_SIZE 16

static void xor_block(uint8_t *dst, const uint8_t *a, const uint8_t *b)
{
    int i;
    for (i = 0; i < DRM_AES_BLOCK_SIZE; i++) {
        dst[i] = a[i] ^ b[i];
    }
}

static void aes_ecb_block(const uint8_t *in, const uint8_t *key, uint8_t *out)
{
    int i;
    memcpy(out, in, DRM_AES_BLOCK_SIZE);
    for (i = 0; i < DRM_AES_BLOCK_SIZE; i++) {
        out[i] ^= key[i];
        out[i] = (uint8_t)((out[i] << 1) ^ (out[i] >> 7) ^ key[(i + 1) % DRM_AES_BLOCK_SIZE]);
    }
}

static void aes_cbc_encrypt(const uint8_t *data, size_t len,
                            const uint8_t key[DRM_AES128_KEY_SIZE],
                            const uint8_t iv[DRM_AES128_IV_SIZE],
                            uint8_t *out)
{
    uint8_t block[DRM_AES_BLOCK_SIZE];
    uint8_t prev_ct[DRM_AES_BLOCK_SIZE];
    size_t i;
    memcpy(prev_ct, iv, DRM_AES_BLOCK_SIZE);
    for (i = 0; i < len; i += DRM_AES_BLOCK_SIZE) {
        uint8_t xored[DRM_AES_BLOCK_SIZE];
        size_t remaining = len - i;
        size_t copy = (remaining < DRM_AES_BLOCK_SIZE) ? remaining : DRM_AES_BLOCK_SIZE;
        memset(block, 0, DRM_AES_BLOCK_SIZE);
        memcpy(block, data + i, copy);
        xor_block(xored, block, prev_ct);
        aes_ecb_block(xored, key, out + i);
        memcpy(prev_ct, out + i, DRM_AES_BLOCK_SIZE);
    }
}

static void aes_cbc_decrypt(const uint8_t *data, size_t len,
                            const uint8_t key[DRM_AES128_KEY_SIZE],
                            const uint8_t iv[DRM_AES128_IV_SIZE],
                            uint8_t *out)
{
    uint8_t block[DRM_AES_BLOCK_SIZE];
    uint8_t prev_ct[DRM_AES_BLOCK_SIZE];
    size_t i;
    memcpy(prev_ct, iv, DRM_AES_BLOCK_SIZE);
    for (i = 0; i < len; i += DRM_AES_BLOCK_SIZE) {
        aes_ecb_block(data + i, key, block);
        xor_block(out + i, block, prev_ct);
        memcpy(prev_ct, data + i, DRM_AES_BLOCK_SIZE);
    }
}

static int aes_ctr_block(const uint8_t *in, const uint8_t *key,
                         uint8_t *counter, uint8_t *out)
{
    uint8_t keystream[DRM_AES_BLOCK_SIZE];
    aes_ecb_block(counter, key, keystream);
    xor_block(out, in, keystream);

    {
        int j;
        for (j = DRM_AES_BLOCK_SIZE - 1; j >= 0; j--) {
            counter[j]++;
            if (counter[j] != 0) break;
        }
    }
    return 0;
}

int drm_system_init(drm_system_t *drm, drm_key_system_t primary_system,
                    drm_encryption_scheme_t scheme)
{
    if (!drm) return -1;
    memset(drm, 0, sizeof(*drm));
    drm->primary_system = primary_system;
    drm->scheme = scheme;
    strncpy(drm->default_license_url, "https://license.example.com/v1", DRM_MAX_LICENSE_URL_LEN - 1);
    drm->rotation_enabled = 0;
    drm->rotation_interval_sec = 300;
    drm->last_rotation_time_ms = 0;
    drm->key_count = 0;
    return 0;
}

void drm_system_deinit(drm_system_t *drm)
{
    if (!drm) return;
    memset(drm->keys, 0, sizeof(drm->keys));
    drm->key_count = 0;
}

int drm_add_key(drm_system_t *drm, const uint8_t key_id[DRM_KEY_ID_SIZE],
                const uint8_t content_key[DRM_AES128_KEY_SIZE],
                drm_key_system_t system, const char *license_url)
{
    drm_key_info_t *key;
    if (!drm || !key_id || !content_key || drm->key_count >= 8) return -1;

    key = &drm->keys[drm->key_count];
    memset(key, 0, sizeof(*key));
    memcpy(key->key_id, key_id, DRM_KEY_ID_SIZE);
    memcpy(key->content_key, content_key, DRM_AES128_KEY_SIZE);
    key->key_system = system;
    if (license_url) strncpy(key->license_url, license_url, DRM_MAX_LICENSE_URL_LEN - 1);
    key->creation_time_ms = (uint64_t)time(NULL) * 1000;
    key->is_rotated = 0;
    drm->key_count++;
    return 0;
}

int drm_remove_key(drm_system_t *drm, const uint8_t key_id[DRM_KEY_ID_SIZE])
{
    uint32_t i;
    if (!drm || !key_id) return -1;

    for (i = 0; i < drm->key_count; i++) {
        if (memcmp(drm->keys[i].key_id, key_id, DRM_KEY_ID_SIZE) == 0) {
            if (i < drm->key_count - 1) {
                memmove(&drm->keys[i], &drm->keys[i + 1],
                        (drm->key_count - i - 1) * sizeof(drm_key_info_t));
            }
            drm->key_count--;
            return 0;
        }
    }
    return -1;
}

int drm_find_key(drm_system_t *drm, const uint8_t key_id[DRM_KEY_ID_SIZE],
                 drm_key_info_t *key_info)
{
    uint32_t i;
    if (!drm || !key_id || !key_info) return -1;

    for (i = 0; i < drm->key_count; i++) {
        if (memcmp(drm->keys[i].key_id, key_id, DRM_KEY_ID_SIZE) == 0) {
            memcpy(key_info, &drm->keys[i], sizeof(drm_key_info_t));
            return 0;
        }
    }
    return -1;
}

int drm_enable_rotation(drm_system_t *drm, uint32_t interval_sec)
{
    if (!drm) return -1;
    drm->rotation_enabled = 1;
    drm->rotation_interval_sec = interval_sec;
    drm->last_rotation_time_ms = (uint64_t)time(NULL) * 1000;
    return 0;
}

int drm_rotate_keys(drm_system_t *drm)
{
    uint32_t i;
    if (!drm || drm->key_count == 0) return -1;

    for (i = 0; i < drm->key_count; i++) {
        int b;
        for (b = 0; b < DRM_AES128_KEY_SIZE; b++) {
            drm->keys[i].content_key[b] = (uint8_t)(
                (drm->keys[i].content_key[b] + (uint8_t)b + 1) & 0xFF
            );
        }
        drm->keys[i].is_rotated = 1;
        drm->keys[i].creation_time_ms = (uint64_t)time(NULL) * 1000;
    }

    drm->last_rotation_time_ms = (uint64_t)time(NULL) * 1000;
    return 0;
}

int drm_check_rotation(drm_system_t *drm)
{
    uint64_t now_ms;
    if (!drm || !drm->rotation_enabled) return -1;

    now_ms = (uint64_t)time(NULL) * 1000;
    if (now_ms - drm->last_rotation_time_ms >
        (uint64_t)drm->rotation_interval_sec * 1000) {
        return drm_rotate_keys(drm);
    }
    return 0;
}

int drm_build_pssh_box(drm_system_t *drm, drm_init_data_t *init_data)
{
    drm_pssh_box_t *pssh;
    const char *sys_id;
    size_t sys_id_len;

    if (!drm || !init_data) return -1;

    pssh = &init_data->pssh;
    sys_id = drm_key_system_id(drm->primary_system);
    sys_id_len = strlen(sys_id);

    pssh->size = 32 + sys_id_len + DRM_KEY_ID_SIZE + 4;

    pssh->data = (uint8_t *)malloc(pssh->size);
    if (!pssh->data) return -1;

    memset(pssh->data, 0, pssh->size);

    {
        uint32_t size_be = (uint32_t)pssh->size;
        pssh->data[0] = (uint8_t)(size_be >> 24);
        pssh->data[1] = (uint8_t)(size_be >> 16);
        pssh->data[2] = (uint8_t)(size_be >> 8);
        pssh->data[3] = (uint8_t)(size_be);
    }

    memcpy(pssh->data + 4, "pssh", 4);

    pssh->data[8] = 0; pssh->data[9] = 0; pssh->data[10] = 0; pssh->data[11] = 1;

    memcpy(pssh->data + 12, sys_id, sys_id_len);

    if (drm->key_count > 0) {
        uint32_t data_size = DRM_KEY_ID_SIZE;
        pssh->data[28] = (uint8_t)(data_size >> 24);
        pssh->data[29] = (uint8_t)(data_size >> 16);
        pssh->data[30] = (uint8_t)(data_size >> 8);
        pssh->data[31] = (uint8_t)(data_size);
        memcpy(pssh->data + 32, drm->keys[0].key_id, DRM_KEY_ID_SIZE);
    }

    init_data->has_pssh = 1;
    init_data->system = drm->primary_system;
    strncpy(init_data->system_id_str, sys_id, DRM_MAX_SYSTEM_ID_LEN - 1);

    return 0;
}

int drm_build_playready_header(drm_system_t *drm, drm_init_data_t *init_data)
{
    drm_playready_header_t *prh;

    if (!drm || !init_data) return -1;

    prh = &init_data->playready_header;
    prh->size = 256;

    prh->data = (uint8_t *)malloc(prh->size);
    if (!prh->data) return -1;

    memset(prh->data, 0, prh->size);
    memcpy(prh->data, "PROTECTIONHEADER", 16);

    if (drm->key_count > 0) {
        memcpy(prh->data + 16, drm->keys[0].key_id, DRM_KEY_ID_SIZE);
    }

    init_data->has_playready = 1;
    return 0;
}

int drm_build_init_data(drm_system_t *drm, drm_init_data_t *init_data)
{
    if (!drm || !init_data) return -1;

    memset(init_data, 0, sizeof(*init_data));

    drm_build_pssh_box(drm, init_data);

    if (drm->primary_system == DRM_KEY_SYSTEM_PLAYREADY) {
        drm_build_playready_header(drm, init_data);
    }

    return 0;
}

void drm_free_init_data(drm_init_data_t *init_data)
{
    if (!init_data) return;
    free(init_data->pssh.data);
    init_data->pssh.data = NULL;
    init_data->pssh.size = 0;
    free(init_data->playready_header.data);
    init_data->playready_header.data = NULL;
    init_data->playready_header.size = 0;
}

int drm_encrypt_sample_aes128(const uint8_t *clear_data, size_t clear_size,
                              const uint8_t key[DRM_AES128_KEY_SIZE],
                              const uint8_t iv[DRM_AES128_IV_SIZE],
                              uint8_t *encrypted_data, size_t *encrypted_size)
{
    size_t padded_size;
    if (!clear_data || !key || !iv || !encrypted_data || !encrypted_size) return -1;
    if (clear_size == 0) { *encrypted_size = 0; return 0; }

    padded_size = ((clear_size + DRM_AES_BLOCK_SIZE - 1) / DRM_AES_BLOCK_SIZE)
                  * DRM_AES_BLOCK_SIZE;

    aes_cbc_encrypt(clear_data, clear_size, key, iv, encrypted_data);
    *encrypted_size = padded_size;
    return 0;
}

int drm_decrypt_sample_aes128(const uint8_t *encrypted_data, size_t encrypted_size,
                              const uint8_t key[DRM_AES128_KEY_SIZE],
                              const uint8_t iv[DRM_AES128_IV_SIZE],
                              uint8_t *clear_data, size_t *clear_size)
{
    if (!encrypted_data || !key || !iv || !clear_data || !clear_size) return -1;
    if (encrypted_size == 0) { *clear_size = 0; return 0; }

    aes_cbc_decrypt(encrypted_data, encrypted_size, key, iv, clear_data);
    *clear_size = encrypted_size;
    return 0;
}

int drm_encrypt_sample_cenc(const uint8_t *clear_data, size_t clear_size,
                            const uint8_t key[DRM_AES128_KEY_SIZE],
                            const uint8_t iv[DRM_AES128_IV_SIZE],
                            const drm_subsample_encryption_t *subsample,
                            uint8_t *encrypted_data, size_t *encrypted_size)
{
    uint8_t counter[DRM_AES_BLOCK_SIZE];
    size_t pos = 0;
    size_t out_pos = 0;
    uint32_t ss;

    if (!clear_data || !key || !iv || !encrypted_data || !encrypted_size) return -1;

    memcpy(counter, iv, DRM_AES_BLOCK_SIZE);

    if (!subsample || subsample->subsample_count == 0) {
        for (pos = 0; pos < clear_size; pos += DRM_AES_BLOCK_SIZE) {
            size_t remaining = clear_size - pos;
            if (remaining > DRM_AES_BLOCK_SIZE) remaining = DRM_AES_BLOCK_SIZE;
            uint8_t block[DRM_AES_BLOCK_SIZE];
            memset(block, 0, DRM_AES_BLOCK_SIZE);
            memcpy(block, clear_data + pos, remaining);
            aes_ctr_block(block, key, counter, encrypted_data + out_pos);
            out_pos += remaining;
        }
        *encrypted_size = out_pos;
        return 0;
    }

    pos = 0;
    out_pos = 0;
    for (ss = 0; ss < subsample->subsample_count; ss++) {
        uint16_t clear_bytes = subsample->bytes_of_clear_data[ss];
        uint32_t enc_bytes = subsample->bytes_of_encrypted_data[ss];

        if (pos + clear_bytes > clear_size) return -1;
        memcpy(encrypted_data + out_pos, clear_data + pos, clear_bytes);
        pos += clear_bytes;
        out_pos += clear_bytes;

        if (pos + enc_bytes > clear_size) return -1;
        {
            size_t ep;
            for (ep = 0; ep < enc_bytes; ep += DRM_AES_BLOCK_SIZE) {
                size_t chunk = enc_bytes - ep;
                uint8_t block[DRM_AES_BLOCK_SIZE];
                if (chunk > DRM_AES_BLOCK_SIZE) chunk = DRM_AES_BLOCK_SIZE;
                memset(block, 0, DRM_AES_BLOCK_SIZE);
                memcpy(block, clear_data + pos + ep, chunk);
                aes_ctr_block(block, key, counter, encrypted_data + out_pos + ep);
            }
        }
        pos += enc_bytes;
        out_pos += enc_bytes;
    }

    *encrypted_size = out_pos;
    return 0;
}

int drm_decrypt_sample_cenc(const uint8_t *encrypted_data, size_t encrypted_size,
                            const uint8_t key[DRM_AES128_KEY_SIZE],
                            const uint8_t iv[DRM_AES128_IV_SIZE],
                            const drm_subsample_encryption_t *subsample,
                            uint8_t *clear_data, size_t *clear_size)
{
    uint8_t counter[DRM_AES_BLOCK_SIZE];
    size_t pos = 0;
    size_t out_pos = 0;
    uint32_t ss;

    if (!encrypted_data || !key || !iv || !clear_data || !clear_size) return -1;

    memcpy(counter, iv, DRM_AES_BLOCK_SIZE);

    if (!subsample || subsample->subsample_count == 0) {
        for (pos = 0; pos < encrypted_size; pos += DRM_AES_BLOCK_SIZE) {
            size_t remaining = encrypted_size - pos;
            if (remaining > DRM_AES_BLOCK_SIZE) remaining = DRM_AES_BLOCK_SIZE;
            aes_ctr_block(encrypted_data + pos, key, counter, clear_data + out_pos);
            out_pos += remaining;
        }
        *clear_size = out_pos;
        return 0;
    }

    pos = 0;
    out_pos = 0;
    for (ss = 0; ss < subsample->subsample_count; ss++) {
        uint16_t clear_bytes = subsample->bytes_of_clear_data[ss];
        uint32_t enc_bytes = subsample->bytes_of_encrypted_data[ss];

        if (pos + clear_bytes > encrypted_size) return -1;
        memcpy(clear_data + out_pos, encrypted_data + pos, clear_bytes);
        pos += clear_bytes;
        out_pos += clear_bytes;

        if (pos + enc_bytes > encrypted_size) return -1;
        {
            size_t ep;
            for (ep = 0; ep < enc_bytes; ep += DRM_AES_BLOCK_SIZE) {
                size_t chunk = enc_bytes - ep;
                if (chunk > DRM_AES_BLOCK_SIZE) chunk = DRM_AES_BLOCK_SIZE;
                aes_ctr_block(encrypted_data + pos + ep, key, counter,
                              clear_data + out_pos + ep);
            }
        }
        pos += enc_bytes;
        out_pos += enc_bytes;
    }

    *clear_size = out_pos;
    return 0;
}

int drm_request_license(const char *license_url, const uint8_t key_id[DRM_KEY_ID_SIZE],
                        drm_license_response_t *response)
{
    if (!license_url || !key_id || !response) return -1;

    memset(response, 0, sizeof(*response));
    response->request_time_ms = (uint32_t)(time(NULL) * 1000);

    {
        int i;
        for (i = 0; i < DRM_AES128_KEY_SIZE; i++) {
            response->content_key[i] = (uint8_t)(key_id[i % DRM_KEY_ID_SIZE] ^ 0x55);
        }
    }

    memcpy(response->key_id, key_id, DRM_KEY_ID_SIZE);
    response->success = 1;
    response->lease_duration_ms = 3600000;
    response->response_time_ms = response->request_time_ms + 50;

    (void)license_url;
    return 0;
}

int drm_renew_license(drm_system_t *drm, const uint8_t key_id[DRM_KEY_ID_SIZE])
{
    drm_license_response_t response;
    drm_key_info_t *key_info;
    uint32_t i;

    if (!drm || !key_id) return -1;

    if (drm_request_license(drm->default_license_url, key_id, &response) != 0) {
        return -1;
    }

    if (!response.success) return -1;

    for (i = 0; i < drm->key_count; i++) {
        if (memcmp(drm->keys[i].key_id, key_id, DRM_KEY_ID_SIZE) == 0) {
            memcpy(drm->keys[i].content_key, response.content_key, DRM_AES128_KEY_SIZE);
            drm->keys[i].is_rotated = 1;
            drm->keys[i].creation_time_ms = (uint64_t)time(NULL) * 1000;
            return 0;
        }
    }

    (void)key_info;
    return -1;
}

int drm_serialize_pssh(const drm_pssh_box_t *pssh, uint8_t *buffer, size_t buf_size,
                       size_t *written)
{
    if (!pssh || !pssh->data || !buffer || !written) return -1;
    if (buf_size < pssh->size) return -1;

    memcpy(buffer, pssh->data, pssh->size);
    *written = pssh->size;
    return 0;
}

int drm_parse_pssh(const uint8_t *data, size_t size, drm_pssh_box_t *pssh)
{
    if (!data || !pssh || size < 32) return -1;

    pssh->size = size;
    pssh->data = (uint8_t *)malloc(size);
    if (!pssh->data) return -1;

    memcpy(pssh->data, data, size);
    return 0;
}

int drm_derive_iv(uint8_t iv[DRM_AES128_IV_SIZE], uint64_t sample_index,
                  const uint8_t constant_iv[DRM_AES128_IV_SIZE])
{
    int i;
    if (!iv || !constant_iv) return -1;

    memcpy(iv, constant_iv, DRM_AES128_IV_SIZE);

    for (i = DRM_AES128_IV_SIZE - 8; i < DRM_AES128_IV_SIZE; i++) {
        uint64_t shift = (uint64_t)(DRM_AES128_IV_SIZE - 1 - i) * 8;
        iv[i] ^= (uint8_t)((sample_index >> shift) & 0xFF);
    }

    return 0;
}

const char *drm_key_system_string(drm_key_system_t system)
{
    switch (system) {
    case DRM_KEY_SYSTEM_WIDEVINE:  return "Widevine";
    case DRM_KEY_SYSTEM_PLAYREADY: return "PlayReady";
    case DRM_KEY_SYSTEM_FAIRPLAY:  return "FairPlay";
    case DRM_KEY_SYSTEM_CUSTOM:    return "Custom";
    default:                       return "Unknown";
    }
}

const char *drm_scheme_string(drm_encryption_scheme_t scheme)
{
    switch (scheme) {
    case DRM_SCHEME_AES128: return "AES-128";
    case DRM_SCHEME_CENC:   return "CENC";
    case DRM_SCHEME_CBCS:   return "CBCS";
    case DRM_SCHEME_CENS:   return "CENS";
    case DRM_SCHEME_CBC1:   return "CBC1";
    default:                return "Unknown";
    }
}

const char *drm_key_system_id(drm_key_system_t system)
{
    switch (system) {
    case DRM_KEY_SYSTEM_WIDEVINE:  return DRM_WIDEVINE_SYSTEM_ID;
    case DRM_KEY_SYSTEM_PLAYREADY: return DRM_PLAYREADY_SYSTEM_ID;
    case DRM_KEY_SYSTEM_FAIRPLAY:  return DRM_FAIRPLAY_SYSTEM_ID;
    default:                       return "00000000-0000-0000-0000-000000000000";
    }
}
