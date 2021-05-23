//
// Created by Hugo Trippaers on 21/05/2021.
//

#ifndef ESPCAM_ESP_RTSP_COMMON_H
#define ESPCAM_ESP_RTSP_COMMON_H

#define URL_MAX_LENGTH 1024

typedef enum {
    OPTIONS,
    DESCRIBE,
    SETUP,
    PLAY,
    TEARDOWN
} rtsp_request_type_t;

typedef struct {
    rtsp_request_type_t request_type;
    char url[URL_MAX_LENGTH + 1];
    int protocol_version;
    int cseq;
    int dst_rtp_port;
    int dst_rtcp_port;
} rtsp_req_t;

typedef void* rtsp_parser_handle_t;

int parser_init(rtsp_parser_handle_t *handle);
int parse_request(rtsp_parser_handle_t handle, const char *buffer, size_t len);
int parser_is_complete(rtsp_parser_handle_t handle);
rtsp_req_t *parser_get_request(rtsp_parser_handle_t handle);
int parser_free(rtsp_parser_handle_t handle);

esp_err_t rtsp_server_main();

#endif //ESPCAM_ESP_RTSP_COMMON_H
