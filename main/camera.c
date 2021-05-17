//
// Created by Hugo Trippaers on 18/04/2021.
//

#include <string.h>
#include "esp32cam_pins.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_camera.h"

#include "common.h"

#define TAG "main_camera"

static camera_config_t camera_config = {
        .pin_pwdn  = CAM_PIN_PWDN,
        .pin_reset = CAM_PIN_RESET,
        .pin_xclk = CAM_PIN_XCLK,
        .pin_sscb_sda = CAM_PIN_SIOD,
        .pin_sscb_scl = CAM_PIN_SIOC,

        .pin_d7 = CAM_PIN_D7,
        .pin_d6 = CAM_PIN_D6,
        .pin_d5 = CAM_PIN_D5,
        .pin_d4 = CAM_PIN_D4,
        .pin_d3 = CAM_PIN_D3,
        .pin_d2 = CAM_PIN_D2,
        .pin_d1 = CAM_PIN_D1,
        .pin_d0 = CAM_PIN_D0,
        .pin_vsync = CAM_PIN_VSYNC,
        .pin_href = CAM_PIN_HREF,
        .pin_pclk = CAM_PIN_PCLK,

        //XCLK 20MHz or 10MHz for OV2640 double FPS (Experimental)
        .xclk_freq_hz = 20000000,
        .ledc_timer = LEDC_TIMER_0,
        .ledc_channel = LEDC_CHANNEL_0,

        .pixel_format = PIXFORMAT_JPEG,//YUV422,GRAYSCALE,RGB565,JPEG
        .frame_size = FRAMESIZE_SVGA,//QQVGA-QXGA Do not use sizes above QVGA when not JPEG

        .jpeg_quality = 12, //0-63 lower number means higher quality
        .fb_count = 1 //if more than one, i2s runs in continuous mode. Use only with JPEG
};


esp_err_t esp32cam_camera_init() {
    //power up the camera if PWDN pin is defined
    if(CAM_PIN_PWDN != -1){
        esp_err_t err = gpio_set_direction(CAM_PIN_PWDN, GPIO_MODE_OUTPUT);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set direction for CAM_PIN_PWDN");
            return err;
        }
        err = gpio_set_level(CAM_PIN_PWDN, 1);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to drive CAM_PIN_PWDN to LOW");
            return err;
        }
    }

    //initialize the camera
    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Camera Init Failed");
        return err;
    }

    return ESP_OK;
}

esp_err_t esp32cam_camera_capture(void *buffer, size_t *len) {
    //acquire a frame
    camera_fb_t * fb = esp_camera_fb_get();
    if (!fb) {
        ESP_LOGE(TAG, "Camera Capture Failed");
        return ESP_FAIL;
    }

    if (fb->len > 65536) {
        ESP_LOGE(TAG, "Not enough space in buffer for frame, %d bytes required", fb->len);
        esp_camera_fb_return(fb);
        return ESP_FAIL;
    }
    memcpy(buffer, fb->buf, fb->len);
    *len = fb->len;

    //return the frame buffer back to the driver for reuse
    esp_camera_fb_return(fb);
    return ESP_OK;
}