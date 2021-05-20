//
// Created by Hugo Trippaers on 19/05/2021.
//

#ifndef ESPCAM_RTP_UDP_H
#define ESPCAM_RTP_UDP_H

typedef struct {
    uint16_t rtp_socket;
    uint16_t rtcp_socket;

    uint16_t src_rtp_port;
    uint16_t src_rtcp_port;

    uint16_t dst_rtp_port;
    uint16_t dst_rtcp_port;
    uint8_t dst_ip[4];

    uint32_t timestamp;
    uint32_t sequence_number;
} esp_rtp_session_t;

typedef struct {
    uint8_t payload_type;
    uint8_t marker;
    uint16_t sequence_number;
    uint32_t timestamp;
    uint32_t ssrc;
} esp_rtp_header_t;

typedef struct {
    uint8_t type_specific;
    uint16_t fragment_offset;
    uint8_t type;
    uint8_t q;
    uint16_t width;
    uint16_t height;
} esp_rtp_jpeg_header_t;

typedef struct {
    uint8_t mbz;
    uint8_t precision;
    uint16_t length;
    char table0[64];
    char table1[64];
} esp_rtp_quant_t;

#define RTP_QUANT_DEFAULT() { \
    .mbz = 0,                 \
    .precision = 0,           \
    .length = 128             \
}

esp_err_t esp_rtp_init(esp_rtp_session_t *rtp_session);
esp_err_t esp_rtp_teardown(esp_rtp_session_t *rtp_session);
esp_err_t esp_rtp_send_jpeg(esp_rtp_session_t *rtp_session, char *jpeg, int jpeg_length);

#endif //ESPCAM_RTP_UDP_H
