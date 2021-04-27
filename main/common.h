//
// Created by Hugo Trippaers on 20/04/2021.
//

#ifndef ESPCAM_COMMON_H
#define ESPCAM_COMMON_H

esp_err_t wifi_init();

esp_err_t esp32cam_camera_init();

esp_err_t esp32cam_mqtt_init();
esp_err_t esp32cam_mqtt_connect();
esp_err_t esp32cam_mqtt_disconnect();

esp_err_t esp32cam_sdcard_mount();
esp_err_t esp32cam_sdcard_unmount();
esp_err_t esp32cam_sdcard_readfile(const char *path, void **content, size_t *size)

typedef struct {
    void *buffer;
    int len;
} espcam_binary_data_t;

typedef struct {
    espcam_binary_data_t cabundle;
    espcam_binary_data_t device_certificate;
    espcam_binary_data_t device_private_key;
} espcam_tls_config_t;

typedef struct {
    unsigned char *endpoint;
    unsigned char *device_name;
} espcam_aws_iot_config_t;

typedef struct {
    unsigned char *essid;
    unsigned char *essid_secret;
} espcam_wifi_config_t;

typedef struct {
    espcam_wifi_config_t wifi_config;
    espcam_aws_iot_config_t aws_iot_config;
    espcam_tls_config_t tls_config;
} app_config_t;

#endif //ESPCAM_COMMON_H
