//
// Created by Hugo Trippaers on 20/04/2021.
//
#include <esp_vfs_fat.h>
#include "stddef.h"
#include "esp_vfs.h"

#include "driver/sdmmc_host.h"
#include "driver/spi_common.h"
#include "sdmmc_cmd.h"

#include "sdkconfig.h"

#include "common.h"

#define TAG "esp32cam-certs"

#ifdef CONFIG_ESPCAM_AWS_CERTS_EMBEDDED
extern const uint8_t server_root_cert_pem_start[] asm("_binary_aws_root_ca_pem_start");
extern const uint8_t server_root_cert_pem_end[]   asm("_binary_aws_root_ca_pem_end");

extern const uint8_t client_crt_pem_start[]       asm("_binary_certificate_pem_crt_start");
extern const uint8_t client_crt_pem_end[]         asm("_binary_certificate_pem_crt_end");
extern const uint16_t client_crt_pem_length       asm("certificate_pem_crt_length");

extern const uint8_t client_key_pem_start[]       asm("_binary_private_pem_key_start");
extern const uint8_t client_key_pem_end[]         asm("_binary_private_pem_key_end");
extern const uint16_t client_key_pem_length       asm("private_pem_key_length");

esp_err_t esp32cam_readpem(pem_type_t pem_type, unsigned char **buffer, long *len) {
    switch (pem_type) {
        case CABUNDLE:
            *buffer = (unsigned char *) server_root_cert_pem_start;
            *len = server_root_cert_pem_end - server_root_cert_pem_start;
            break;
        case DEVICE_CERTIFICATE:
            *buffer = (unsigned char *) client_crt_pem_start;
            *len = client_crt_pem_end - client_crt_pem_start;
            break;
        case DEVICE_PRIVATEKEY:
            *buffer = (unsigned char *) client_key_pem_start;
            *len = client_key_pem_end - client_key_pem_start;
            break;
        default:
            ESP_LOGE(TAG, "No content for pem type %d", pem_type);
            return ESP_FAIL;
    }

    return ESP_OK;
}
#endif

#ifdef CONFIG_ESPCAM_AWS_CERTS_SD

esp_err_t esp32cam_readpem(pem_type_t pem_type, unsigned char **buffer, long *len) {
    return ESP_FAIL;
}

#endif