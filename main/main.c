#include <argz.h>

#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <esp_ota_ops.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_event.h"

#include "esp_log.h"

#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"

#include "nvs.h"
#include "nvs_flash.h"

#include "sdkconfig.h"

#include "esp-rtsp.h"
#include "common.h"

#define TAG "main"

static app_config_t app_config;

#define IS_KEY(x, y) strcmp(x, y) == 0

esp_err_t load_app_config() {
    esp_err_t err = esp32cam_sdcard_mount();
    if (err != ESP_OK) {
        return ESP_FAIL;
    }

    char *settings;
    size_t settings_length;
    err = esp32cam_sdcard_readfile("settings.ini", (void **)&settings, &settings_length);
    if (err != ESP_OK || settings == NULL) {
        ESP_LOGE(TAG, "Failed to read settings.ini");
        esp32cam_sdcard_unmount();
        return ESP_FAIL;
    }

    int pos = 0;
    char *key = NULL;
    char *value = NULL;
    for (int i = 0; i < settings_length; i++) {
        char current = *(settings + i);

        if (current == '=') {
            int len = i - pos;
            key = malloc(len + 1);
            bzero(key, len + 1);
            bcopy(settings + pos, key, len);
            pos = i + 1;
        }
        else if (current == '\r' || current == '\n')  {
            if (value != NULL) {
                continue;
            }

            int len = i - pos;
            value = malloc(len + 1);
            bzero(value, len + 1);
            bcopy(settings + pos, value, len);
            pos = i + 1;
        }

        if (key != NULL && value != NULL) {
            if (IS_KEY(key, SETTING_ESSID)) {
                app_config.wifi_config.essid = malloc(strlen(value) + 1);
                memcpy(app_config.wifi_config.essid, value, strlen(value));
                *(app_config.wifi_config.essid + strlen(value)) = 0x0;
            } else if (IS_KEY(key, SETTING_ESSID_SECRET)) {
                app_config.wifi_config.essid_secret = malloc(strlen(value) + 1);
                memcpy(app_config.wifi_config.essid_secret, value, strlen(value));
                *(app_config.wifi_config.essid_secret + strlen(value)) = 0x0;
            } else if (IS_KEY(key, SETTING_DEVICE_ID)) {
                app_config.aws_iot_config.device_name = malloc(strlen(value) + 1);
                memcpy(app_config.aws_iot_config.device_name, value, strlen(value));
                *(app_config.aws_iot_config.device_name + strlen(value)) = 0x0;
            } else if (IS_KEY(key, SETTING_CACERT_BUNDLE)) {
                err = esp32cam_sdcard_readfile(value, (void **)&app_config.tls_config.cabundle.buffer, &app_config.tls_config.cabundle.len);
                if (err != ESP_OK) {
                    ESP_LOGW(TAG, "Failed to read %s", value);
                }
            } else if (IS_KEY(key, SETTING_DEVICE_CERTIFICATE)) {
                err = esp32cam_sdcard_readfile(value, (void **)&app_config.tls_config.device_certificate.buffer, &app_config.tls_config.device_certificate.len);
                if (err != ESP_OK) {
                    ESP_LOGW(TAG, "Failed to read %s", value);
                }
            } else if (IS_KEY(key, SETTING_DEVICE_PRIVATEKEY)) {
                err = esp32cam_sdcard_readfile(value, (void **)&app_config.tls_config.device_private_key.buffer, &app_config.tls_config.device_private_key.len);
                if (err != ESP_OK) {
                    ESP_LOGW(TAG, "Failed to read %s", value);
                }
            } else if (IS_KEY(key, SETTING_DEVICE_ENDPOINT)) {
                app_config.aws_iot_config.endpoint = malloc(strlen(value) + 1);
                memcpy(app_config.aws_iot_config.endpoint, value, strlen(value));
                *(app_config.aws_iot_config.endpoint + strlen(value)) = 0x0;
            }
            free(key);
            free(value);
            key = NULL;
            value = NULL;
        }
    }

    ESP_ERROR_CHECK(esp32cam_sdcard_unmount());

    ESP_LOGI(TAG, "Settings\n\tESSID: %s\n\tESSID secret: %s\n\tDevice ID: %s\n\tCA Bundle: %d bytes\n\tCertificate: %d bytes\n\tPrivate key: %d bytes",
             app_config.wifi_config.essid, app_config.wifi_config.essid_secret, app_config.aws_iot_config.device_name, app_config.tls_config.cabundle.len,
             app_config.tls_config.device_certificate.len, app_config.tls_config.device_private_key.len);

    return ESP_OK;
}

char image_buffer[65536];
size_t image_size;

#define MQTT_CONNECTED BIT0
#define MQTT_DISCONNECTED BIT1
EventGroupHandle_t s_mqtt_event_group;

void mqtt_connection_task(void *pvParameters) {
    esp_err_t err = esp32cam_mqtt_connect(&app_config.aws_iot_config, &app_config.tls_config);
    if (err != ESP_OK) {
        xEventGroupSetBits(s_mqtt_event_group, MQTT_DISCONNECTED);
    }
    xEventGroupSetBits(s_mqtt_event_group, MQTT_CONNECTED);
    
    vTaskDelete(NULL);
}

_Noreturn
void app_main()
{
    ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "[APP] Free heap memory: %d bytes (%d internal)", esp_get_free_heap_size(), esp_get_free_internal_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

    const esp_partition_t *running_partition = esp_ota_get_running_partition();
    ESP_LOGI(TAG, "[APP] Running from partition: %s", running_partition->label);

    const esp_app_desc_t *appDescription = esp_ota_get_app_description();
    ESP_LOGI(TAG, "[APP] Running: %s (version: %s)", appDescription->project_name, appDescription->version);

    // Initialize NVS.
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK( err );

    esp_log_level_set("*", ESP_LOG_VERBOSE);

    // Initialize Event Loop
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Load configuration from the SD card
    ESP_LOGD(TAG, "[PRE load_app_config] Free internal heap  %d bytes", esp_get_free_internal_heap_size());
    ESP_ERROR_CHECK(load_app_config());
    ESP_LOGD(TAG, "[POST load_app_config] Free internal heap  %d bytes", esp_get_free_internal_heap_size());

    // Call our own init functions
    ESP_LOGD(TAG, "[PRE esp32cam_wifi_init] Free internal heap  %d bytes", esp_get_free_internal_heap_size());
    ESP_ERROR_CHECK(esp32cam_wifi_init(&app_config.wifi_config));
    ESP_LOGD(TAG, "[POST esp32cam_wifi_init] Free internal heap  %d bytes", esp_get_free_internal_heap_size());

    ESP_LOGD(TAG, "[PRE esp32cam_camera_init] Free internal heap  %d bytes", esp_get_free_internal_heap_size());
    ESP_ERROR_CHECK(esp32cam_camera_init());
    ESP_LOGD(TAG, "[POST esp32cam_camera_init] Free internal heap  %d bytes", esp_get_free_internal_heap_size());

    ESP_LOGD(TAG, "[PRE esp32cam_mqtt_init] Free internal heap  %d bytes", esp_get_free_internal_heap_size());
    ESP_ERROR_CHECK(esp32cam_mqtt_init());
    ESP_LOGD(TAG, "[POST esp32cam_mqtt_init] Free internal heap  %d bytes", esp_get_free_internal_heap_size());
    ESP_LOGI(TAG, "System init OK");

    s_mqtt_event_group = xEventGroupCreate();
    xTaskCreate(mqtt_connection_task, "mqtt_connect_task", 4096, NULL, 5, NULL);
    EventBits_t bits = xEventGroupWaitBits(s_mqtt_event_group,
                                           MQTT_CONNECTED | MQTT_DISCONNECTED,
                                           pdFALSE,
                                           pdFALSE,
                                           portMAX_DELAY);
    esp_err_t result = bits & MQTT_CONNECTED ? ESP_OK : ESP_FAIL;
    ESP_ERROR_CHECK(result);
    ESP_LOGI(TAG, "MQTT connected OK");

    esp_rtsp_server_handle_t rtsp_server_handle;
    ESP_ERROR_CHECK(esp_rtsp_server_start(&rtsp_server_handle));
    ESP_LOGI(TAG, "RTSP server started on port 554");

    for(;;) {
        err = esp32cam_camera_capture(&esp32cam_mqtt_publish);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to capture image: %d", err);
        }

        ESP_LOGD(TAG, "Free heap: %d, internal %d", esp_get_free_heap_size(), esp_get_free_internal_heap_size());
        vTaskDelay(pdMS_TO_TICKS(5 * 1000));
    }

    ESP_ERROR_CHECK(esp_rtsp_server_stop(rtsp_server_handle));
    ESP_ERROR_CHECK(esp32cam_mqtt_disconnect());
}