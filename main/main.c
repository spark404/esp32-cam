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

#define TAG "main"

extern esp_err_t wifi_init();
extern esp_err_t esp32cam_camera_init();
extern esp_err_t esp32cam_mqtt_init();
extern esp_err_t esp32cam_mqtt_connect();
extern esp_err_t esp32cam_mqtt_disconnect();

void app_main()
{
    // Initialize NVS.
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK( err );

    // Initialize Event Loop
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Call our own init functions
    ESP_ERROR_CHECK(wifi_init());
    ESP_ERROR_CHECK(esp32cam_camera_init());
    ESP_ERROR_CHECK(esp32cam_mqtt_init());
    ESP_LOGI(TAG, "System init OK");

    ESP_ERROR_CHECK(esp32cam_mqtt_connect());
    ESP_LOGI(TAG, "MQTT connected OK");

    ESP_ERROR_CHECK(esp32cam_mqtt_disconnect());
}