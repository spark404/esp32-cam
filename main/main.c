
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>

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

#include "common.h"

#define TAG "main"

static app_config_t app_config;

void load_app_config() {
    app_config.wifi_config.essid = (unsigned char *)CONFIG_ESPCAM_WIFI_SSID;
    app_config.wifi_config.essid_secret = (unsigned char *)CONFIG_ESPCAM_WIFI_SECRET;

}

void app_main()
{
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

    // Call our own init functions
    ESP_ERROR_CHECK(wifi_init());
    ESP_ERROR_CHECK(esp32cam_camera_init());
    ESP_ERROR_CHECK(esp32cam_mqtt_init());
    ESP_LOGI(TAG, "System init OK");

    ESP_ERROR_CHECK(esp32cam_sdcard_mount());


    FILE *settings = fopen("/sdcard/settings.ini", "r");
    if (settings != NULL) {
        char *buffer = malloc(1024);
        size_t n = fread(buffer, 1, 1024, settings);
        if (errno != 0) {
            ESP_LOGE(TAG, "Error reading from settings.ini: %d", errno);
        } else {
            ESP_LOGI(TAG, "Read %d bytes from settings.ini", n);
        }
        char *key = NULL;
        char *value = NULL;
        while (n > 0) {
            int pos = 0;
            for (int i = 0; i < n; i++) {
                char current = *(buffer + i);

                if (current == '=') {
                    int len = i - pos;
                    key = malloc(len + 1);
                    bzero(key, len + 1);
                    bcopy(buffer + pos, key, len);
                    pos = i + 1;
                }
                else if (current == '\r' || current == '\n')  {
                    if (value != NULL) {
                        continue;
                    }

                    int len = i - pos;
                    value = malloc(len + 1);
                    bzero(value, len + 1);
                    bcopy(buffer + pos, value, len);
                    pos = i + 1;
                }

                if (key != NULL && value != NULL) {
                    ESP_LOGI(TAG, "Callback '%s': '%s'", key, value);
                    free(key);
                    free(value);
                    key = NULL;
                    value = NULL;
                }
            }
            n = fread(buffer, 1, 1024, settings);
        }
        fclose(settings);
        free(buffer);
    } else {
        ESP_LOGI(TAG, "Failed to open settings file: %d", errno);
    }

    ESP_ERROR_CHECK(esp32cam_sdcard_unmount());

    ESP_ERROR_CHECK(esp32cam_mqtt_connect());
    ESP_LOGI(TAG, "MQTT connected OK");

    vTaskDelay(10 * 1000 / portTICK_PERIOD_MS);

    ESP_ERROR_CHECK(esp32cam_mqtt_disconnect());

}