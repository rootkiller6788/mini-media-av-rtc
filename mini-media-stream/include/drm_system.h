#ifndef DRM_SYSTEM_H
#define DRM_SYSTEM_H

#include <stdint.h>
#include <stddef.h>

#define DRM_KEY_ID_SIZE          16
#define DRM_AES128_KEY_SIZE      16
#define DRM_AES128_IV_SIZE       16
#define DRM_MAX_PSSH_SIZE      4096
#define DRM_MAX_KEY_URI_LEN     512
#define DRM_MAX_LICENSE_URL_LEN 512
#define DRM_MAX_SYSTEM_ID_LEN    64
#define DRM_PLAYREADY_HEADER_MAX 2048

typedef enum {
    DRM_SCHEME_AES128,
    DRM_SCHEME_CENC,
    DRM_SCHEME_CBCS,
    DRM_SCHEME_CENS,
    DRM_SCHEME_CBC1
} drm_encryption_scheme_t;

typedef enum {
    DRM_KEY_SYSTEM_WIDEVINE,
    DRM_KEY_SYSTEM_PLAYREADY,
    DRM_KEY_SYSTEM_FAIRPLAY,
    DRM_KEY_SYSTEM_CUSTOM
} drm_key_system_t;

typedef enum {
    DRM_SAMPLE_CLEAR,
    DRM_SAMPLE_ENCRYPTED,
    DRM_SAMPLE_MIXED
} drm_sample_type_t;

typedef struct {
    drm_key_system_t  key_system;
    char              system_id[DRM_MAX_SYSTEM_ID_LEN];
    char              license_url[DRM_MAX_LICENSE_URL_LEN];
    uint8_t           key_id[DRM_KEY_ID_SIZE];
    uint8_t           content_key[DRM_AES128_KEY_SIZE];
    uint8_t           iv[DRM_AES128_IV_SIZE];
    uint64_t          creation_time_ms;
    uint64_t          expiration_time_ms;
    uint8_t           has_expiration;
    uint8_t           is_rotated;
} drm_key_info_t;

typedef struct {
    uint8_t  *data;
    size_t    size;
} drm_pssh_box_t;

typedef struct {
    uint8_t  *data;
    size_t    size;
} drm_playready_header_t;

typedef struct {
    drm_key_system_t       system;
    char                   system_id_str[DRM_MAX_SYSTEM_ID_LEN];
    drm_pssh_box_t         pssh;
    drm_playready_header_t playready_header;
    uint8_t                has_pssh;
    uint8_t                has_playready;
} drm_init_data_t;

typedef struct {
    uint32_t  subsample_count;
    uint16_t *bytes_of_clear_data;
    uint32_t *bytes_of_encrypted_data;
    uint8_t   iv[DRM_AES128_IV_SIZE];
    uint8_t   key_id[DRM_KEY_ID_SIZE];
} drm_subsample_encryption_t;

typedef struct {
    drm_sample_type_t           sample_type;
    uint32_t                    sample_index;
    uint32_t                    track_id;
    drm_encryption_scheme_t     scheme;
    uint8_t                     default_iv[DRM_AES128_IV_SIZE];
    uint8_t                     default_key_id[DRM_KEY_ID_SIZE];
    uint8_t                     constant_iv[DRM_AES128_IV_SIZE];
    uint8_t                     use_subsample_encryption;
    drm_subsample_encryption_t  subsample_enc;
} drm_sample_encryption_t;

typedef struct {
    drm_key_info_t        keys[8];
    uint32_t              key_count;
    drm_key_system_t      primary_system;
    drm_encryption_scheme_t scheme;
    char                  default_license_url[DRM_MAX_LICENSE_URL_LEN];
    uint32_t              rotation_interval_sec;
    uint64_t              last_rotation_time_ms;
    uint8_t               rotation_enabled;
    uint8_t               aes128_key_uri[DRM_MAX_KEY_URI_LEN];
    uint8_t               has_aes128_key;
} drm_system_t;

typedef struct {
    uint32_t  request_time_ms;
    uint32_t  response_time_ms;
    uint8_t   success;
    uint8_t   key_id[DRM_KEY_ID_SIZE];
    uint8_t   content_key[DRM_AES128_KEY_SIZE];
    uint64_t  lease_duration_ms;
} drm_license_response_t;

int     drm_system_init(drm_system_t *drm, drm_key_system_t primary_system,
                        drm_encryption_scheme_t scheme);
void    drm_system_deinit(drm_system_t *drm);

int     drm_add_key(drm_system_t *drm, const uint8_t key_id[DRM_KEY_ID_SIZE],
                    const uint8_t content_key[DRM_AES128_KEY_SIZE],
                    drm_key_system_t system, const char *license_url);
int     drm_remove_key(drm_system_t *drm, const uint8_t key_id[DRM_KEY_ID_SIZE]);
int     drm_find_key(drm_system_t *drm, const uint8_t key_id[DRM_KEY_ID_SIZE],
                     drm_key_info_t *key_info);

int     drm_enable_rotation(drm_system_t *drm, uint32_t interval_sec);
int     drm_rotate_keys(drm_system_t *drm);
int     drm_check_rotation(drm_system_t *drm);

int     drm_build_pssh_box(drm_system_t *drm, drm_init_data_t *init_data);
int     drm_build_playready_header(drm_system_t *drm, drm_init_data_t *init_data);
int     drm_build_init_data(drm_system_t *drm, drm_init_data_t *init_data);
void    drm_free_init_data(drm_init_data_t *init_data);

int     drm_encrypt_sample_aes128(const uint8_t *clear_data, size_t clear_size,
                                  const uint8_t key[DRM_AES128_KEY_SIZE],
                                  const uint8_t iv[DRM_AES128_IV_SIZE],
                                  uint8_t *encrypted_data, size_t *encrypted_size);

int     drm_decrypt_sample_aes128(const uint8_t *encrypted_data, size_t encrypted_size,
                                  const uint8_t key[DRM_AES128_KEY_SIZE],
                                  const uint8_t iv[DRM_AES128_IV_SIZE],
                                  uint8_t *clear_data, size_t *clear_size);

int     drm_encrypt_sample_cenc(const uint8_t *clear_data, size_t clear_size,
                                const uint8_t key[DRM_AES128_KEY_SIZE],
                                const uint8_t iv[DRM_AES128_IV_SIZE],
                                const drm_subsample_encryption_t *subsample,
                                uint8_t *encrypted_data, size_t *encrypted_size);

int     drm_decrypt_sample_cenc(const uint8_t *encrypted_data, size_t encrypted_size,
                                const uint8_t key[DRM_AES128_KEY_SIZE],
                                const uint8_t iv[DRM_AES128_IV_SIZE],
                                const drm_subsample_encryption_t *subsample,
                                uint8_t *clear_data, size_t *clear_size);

int     drm_request_license(const char *license_url, const uint8_t key_id[DRM_KEY_ID_SIZE],
                            drm_license_response_t *response);
int     drm_renew_license(drm_system_t *drm, const uint8_t key_id[DRM_KEY_ID_SIZE]);

int     drm_serialize_pssh(const drm_pssh_box_t *pssh, uint8_t *buffer, size_t buf_size,
                           size_t *written);
int     drm_parse_pssh(const uint8_t *data, size_t size, drm_pssh_box_t *pssh);

int     drm_derive_iv(uint8_t iv[DRM_AES128_IV_SIZE], uint64_t sample_index,
                      const uint8_t constant_iv[DRM_AES128_IV_SIZE]);

const char *drm_key_system_string(drm_key_system_t system);
const char *drm_scheme_string(drm_encryption_scheme_t scheme);
const char *drm_key_system_id(drm_key_system_t system);

static const char *DRM_WIDEVINE_SYSTEM_ID   = "edef8ba9-79d6-4ace-a3c8-27dcd51d21ed";
static const char *DRM_PLAYREADY_SYSTEM_ID  = "9a04f079-9840-4286-ab92-e65be0885f95";
static const char *DRM_FAIRPLAY_SYSTEM_ID   = "94ce86fb-07ff-4f43-adb8-93d2fa968ca2";

#endif
