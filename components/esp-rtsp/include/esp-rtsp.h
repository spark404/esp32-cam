//
// Created by Hugo Trippaers on 17/05/2021.
//

#ifndef ESPCAM_ESP_RTSP_H
#define ESPCAM_ESP_RTSP_H

typedef struct {
    char *jpeg;
    size_t jpeg_length;
    char *jpeg_data_start;
    size_t jpeg_data_length;
    char *quant_table_0;
    char *quant_table_1;
} esp_rtsp_jpeg_data_t;

esp_err_t esp_rtsp_server_start();

esp_err_t esp_rtsp_jpeg_decode(char *buffer, size_t length, esp_rtsp_jpeg_data_t *rtsp_jpeg_data);

#endif //ESPCAM_ESP_RTSP_H
