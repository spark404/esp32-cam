//
// Created by Hugo Trippaers on 21/04/2021.
//
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <esp_vfs_fat.h>

#include "stddef.h"
#include "esp_vfs.h"

#include "driver/sdmmc_host.h"
#include "driver/spi_common.h"
#include "sdmmc_cmd.h"

#include "sdkconfig.h"

#include "common.h"

#define TAG "espcam32-sdcard"

static sdmmc_card_t *sdmmc_card;

esp_err_t esp32cam_sdcard_mount() {
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();

    gpio_set_pull_mode(SDMMC_SLOT1_IOMUX_PIN_NUM_CMD, GPIO_PULLUP_ONLY);  // CMD, needed in 4- and 1- line modes
    gpio_set_pull_mode(SDMMC_SLOT1_IOMUX_PIN_NUM_D0, GPIO_PULLUP_ONLY);   // D0, needed in 4- and 1-line modes
    gpio_set_pull_mode(SDMMC_SLOT1_IOMUX_PIN_NUM_D1, GPIO_PULLUP_ONLY);   // D1, needed in 4-line mode only
    gpio_set_pull_mode(SDMMC_SLOT1_IOMUX_PIN_NUM_D2, GPIO_PULLUP_ONLY);   // D2, needed in 4-line mode only
    gpio_set_pull_mode(SDMMC_SLOT1_IOMUX_PIN_NUM_D3, GPIO_PULLUP_ONLY);   // D3, needed in 4- and 1-line modes

    esp_vfs_fat_mount_config_t mount_config = {
            .allocation_unit_size = 16 * 1024,
            .format_if_mount_failed = false,
            .max_files = 5
    };

    return esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot_config, &mount_config, &sdmmc_card);
}

esp_err_t esp32cam_sdcard_unmount() {
    esp_err_t err =  esp_vfs_fat_sdmmc_unmount();

    if (err == ESP_OK) {
        /*
         * Workaround for the flash on the esp32 cam.
         * As the line is also used by the sd card, we reset it to turn off the flash
         */
        gpio_config_t flash_config = {
                .mode = GPIO_MODE_OUTPUT,
                .intr_type = GPIO_INTR_DISABLE,
                .pull_down_en = GPIO_PULLDOWN_DISABLE,
                .pull_up_en = GPIO_PULLUP_DISABLE,
                .pin_bit_mask = 1 << 4
        };
        gpio_config(&flash_config);
    }

    return err;
}

esp_err_t esp32cam_sdcard_readfile(const char *filename, void **content, size_t *size) {
    char path[128];
    snprintf(path, 127, "/sdcard/%s", filename);

    FILE *file = fopen(path, "r");
    if (file == NULL) {
        ESP_LOGE(TAG, "Failed to read %s: %d", path, errno);
        return ESP_FAIL;
    }

    char buffer[512];
    size_t file_size = 0;
    size_t n = fread(buffer, 1, 512, file);
    while (n > 0) {
        file_size += n;
        n = fread(buffer, 1, 512, file);
    }

    if (file_size == 0) {
        ESP_LOGE(TAG, "No bytes read or empty file");
        fclose(file);
        return ESP_FAIL;
    }

    ESP_LOGD(TAG, "Reading file %s (%d bytes)", path, file_size);
    fseek(file, 0, 0); // Reset file to start

    void *read_buffer = malloc(file_size + 1);
    if (read_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate %d bytes of memory to hold the file contents", file_size);
        fclose(file);
        return ESP_FAIL;
    }
    n = fread(read_buffer, 1, file_size, file);
    *((char *)read_buffer + n) = 0x0; // Workaround; esp-tls want the certificates in PEM format NULL terminated

    if (n != file_size) {
        ESP_LOGE(TAG, "File size mismatch, calculated %d but read %d", file_size, n);
        fclose(file);
        return ESP_FAIL;
    }

    fclose(file);

    // Set outgoing variables
    *content = read_buffer;
    *size = file_size + 1;

    return ESP_OK;
}