//
// Created by Hugo Trippaers on 20/04/2021.
//

#ifndef ESPCAM_ESP_RTSP_COMMON_H
#define ESPCAM_COMMON_H

// keys in settings.ini
#define SETTING_ESSID "essid"
#define SETTING_ESSID_SECRET "essid_secret"
#define SETTING_DEVICE_ID "device_id"
#define SETTING_CACERT_BUNDLE "cacert_bundle"
#define SETTING_DEVICE_CERTIFICATE "device_certificate"
#define SETTING_DEVICE_PRIVATEKEY "device_privatekey"
#define SETTING_DEVICE_ENDPOINT "device_endpoint"


typedef struct {
    void *buffer;
    size_t len;
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

esp_err_t esp32cam_wifi_init(espcam_wifi_config_t *wifi_config);

esp_err_t esp32cam_camera_init();
esp_err_t esp32cam_camera_capture(void *buffer, size_t *len);

esp_err_t esp32cam_mqtt_init();
esp_err_t esp32cam_mqtt_connect(espcam_aws_iot_config_t *aws_iot_config, espcam_tls_config_t *tls_config);
esp_err_t esp32cam_mqtt_disconnect();
esp_err_t esp32cm_mqtt_publish(void *buffer, size_t buffer_length);

esp_err_t esp32cam_sdcard_mount();
esp_err_t esp32cam_sdcard_unmount();
esp_err_t esp32cam_sdcard_readfile(const char *filename, void **content, size_t *size);


#endif //ESPCAM_ESP_RTSP_COMMON_H
