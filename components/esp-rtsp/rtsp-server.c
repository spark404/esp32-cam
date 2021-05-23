#include <alloca.h>
//
// Created by Hugo Trippaers on 17/05/2021.
//
#include <string.h>
#include <sys/param.h>
#include "esp_system.h"
#include "esp_log.h"

#include "lwip/err.h"
#include "lwip/sockets.h"

#include "esp-rtsp-common.h"
#include "rtp-udp.h"

#include "esp_camera.h"

#define TAG "rtsp-server"

#define PORT 554  // IANA default for RTSP

#define KEEPALIVE_IDLE              5
#define KEEPALIVE_INTERVAL          5
#define KEEPALIVE_COUNT             3

#define MAX_CLIENTS 3

typedef struct {
    int connection_active;
    int socket;
    char client_addr_string[128];
    rtsp_parser_handle_t parser;
    esp_rtp_session_handle_t rtp_session;
    TaskHandle_t rtp_player_task;
} esp_rtsp_server_connection_t;

esp_rtsp_server_connection_t connections[MAX_CLIENTS];

static void handle_options(esp_rtsp_server_connection_t *connection, rtsp_req_t *request) {
    char buffer[2048];
    size_t msgsize = snprintf(buffer, 2048,
                              "RTSP/1.0 200 OK\r\n"
                              "cSeq: %d\r\n"
                              "Public: OPTIONS, DESCRIBE, SETUP, TEARDOWN, PLAY, PAUSE\r\n"
                              "Server: ESP32 Cam Server\r\n"
                              "Date: Wed, 19 May 2021 17:14:24 GMT\r\n"
                              "\r\n",
                              request->cseq);
    size_t sent = send(connection->socket, buffer, msgsize, 0);
    ESP_LOGI(TAG, "RTSP >: %s", buffer);
    if (sent != msgsize) {
        ESP_LOGW(TAG, "Mismatch between msgsize and sent bytes: %d vs %d", msgsize, sent);
    }
}

static void handle_setup(esp_rtsp_server_connection_t *connection, rtsp_req_t *request) {
    int err = esp_rtp_init(&connection->rtp_session, request->dst_rtp_port, request->dst_rtcp_port, connection->client_addr_string);
    if (err < 0) {
        ESP_LOGW(TAG, "Failed to initialize the rtp connection");
    }

    char buffer[2048];
    size_t msgsize = snprintf(buffer, 2048,
                              "RTSP/1.0 200 OK\r\n"
                              "cSeq: %d\r\n"
                              "Transport: RTP/AVP;unicast;client_port=%d-%d;server_port=%d-%d\r\n"
                              "Session: 12348765\r\n"
                              "\r\n",
                              request->cseq,
                              request->dst_rtp_port,
                              request->dst_rtcp_port,
                              esp_rtp_get_src_rtp_port(connection->rtp_session),
                              esp_rtp_get_src_rtcp_port(connection->rtp_session));
    size_t sent = send(connection->socket, buffer, msgsize, 0);
    ESP_LOGI(TAG, "RTSP >: %s", buffer);
    if (sent != msgsize) {
        ESP_LOGW(TAG, "Mismatch between msgsize and sent bytes: %d vs %d", msgsize, sent);
    }
}

static void handle_describe(esp_rtsp_server_connection_t *connection, rtsp_req_t *request) {
    static char sdp[2048];
    size_t sdp_size = snprintf(sdp, 2048,
                               "v=0\r\n"
                               "o=- %d 1 IN IP4 %s\r\n"
                               "s=\r\n"
                               "t=0 0\r\n"
                               "m=video 0 RTP/AVP 26\r\n"
                               "c=IN IP4 0.0.0.0\r\n",
                               12348765,
                               "192.168.168.135");

    static char buffer[2048];
    size_t msgsize = snprintf(buffer, 2048,
                              "RTSP/1.0 200 OK\r\n"
                              "cSeq: %d\r\n"
                              "Content-Type: application/sdp\r\n"
                              "Content-Base: %s\r\n"
                              "Server: ESP32 Cam Server\r\n"
                              "Date: Wed, 19 May 2021 17:14:24 GMT\r\n"
                              "Content-Length: %d\r\n"
                              "\r\n",
                              request->cseq,
                              request->url,
                              sdp_size);

    // Send header
    size_t sent = send(connection->socket, buffer, msgsize, 0);
    ESP_LOGI(TAG, "RTSP >: %s", buffer);
    if (sent != msgsize) {
        ESP_LOGW(TAG, "Mismatch between msgsize and sent bytes: %d vs %d", msgsize, sent);
    }

    // Send body
    sent = send(connection->socket, sdp, sdp_size, 0);
    ESP_LOGI(TAG, "RTSP >: %s", sdp);
    if (sent != sdp_size) {
        ESP_LOGW(TAG, "Mismatch between sdp_size and sent bytes: %d vs %d", sdp_size, sent);
    }
}

void temporary_player_task(void *pvParameters) {
    if (!pvParameters) {
        ESP_LOGE("player", "Invalid arguments");
        vTaskDelete(NULL);
        return;
    }

    esp_rtp_session_t *session = pvParameters;

    int rate = 200; // delta ms between frames

    for (;;) {
        long timestamp_start = esp_timer_get_time();
        camera_fb_t *fb = esp_camera_fb_get();
        if (!fb) {
            ESP_LOGE(TAG, "Camera Capture Failed");
            goto done;
        }

        esp_rtp_send_jpeg(session, fb->buf, fb->len);

        //return the frame buffer back to the driver for reuse
        esp_camera_fb_return(fb);

        done:
        {
            long timestamp_end = esp_timer_get_time();

            long delta_ms = (timestamp_end - timestamp_start) / 1000;
            if (delta_ms >= rate) {
                rate += 50;
            } else {
                vTaskDelay(pdMS_TO_TICKS(rate - delta_ms));
            }
        }
    }
}

static void handle_play(esp_rtsp_server_connection_t *connection, rtsp_req_t *request) {
    BaseType_t result = xTaskCreate(temporary_player_task, "rtp_server", 32*1024, connection->rtp_session, 6, &connection->rtp_player_task);
    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create rtp server task: %d", result);
        send(connection->socket, "RTSP/1.0 500 Internal Server Error\r\n\r\n", 38, 0);
        return;
    }

    static char buffer[2048];
    size_t msgsize = snprintf(buffer, 2048,
                              "RTSP/1.0 200 OK\r\n"
                              "cSeq: %d\r\n"
                              "Session: %d\r\n"
                              "Server: ESP32 Cam Server\r\n"
                              "Date: Wed, 19 May 2021 17:14:24 GMT\r\n"
                              "Range: npt=0.000-\r\n"
                              "\r\n",
                              request->cseq,
                              12348765);

    size_t sent = send(connection->socket, buffer, msgsize, 0);
    ESP_LOGI(TAG, "RTSP >: %s", buffer);
    if (sent != msgsize) {
        ESP_LOGW(TAG, "Mismatch between msgsize and sent bytes: %d vs %d", msgsize, sent);
    }
}

static void handle_teardown(esp_rtsp_server_connection_t *connection, rtsp_req_t *request) {
    vTaskDelete(connection->rtp_player_task);
    esp_rtp_teardown(connection->rtp_session);

    static char buffer[2048];
    size_t msgsize = snprintf(buffer, 2048,
                              "RTSP/1.0 200 OK\r\n"
                              "cSeq: %d\r\n"
                              "Server: ESP32 Cam Server\r\n"
                              "Date: Wed, 19 May 2021 17:14:24 GMT\r\n"
                              "\r\n",
                              request->cseq);

    size_t sent = send(connection->socket, buffer, msgsize, 0);
    ESP_LOGI(TAG, "RTSP >: %s", buffer);
    if (sent != msgsize) {
        ESP_LOGW(TAG, "Mismatch between msgsize and sent bytes: %d vs %d", msgsize, sent);
    }
}

static int esp_rtsp_server_read_block(esp_rtsp_server_connection_t *connection) {
    char buffer[1024];
    if (!connection->connection_active) {
        return -1;
    }

    int sock = connection->socket;

    int n = read(sock, buffer, 1023);
    if (n < 0) {
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            ESP_LOGW(TAG, "Receive timeout");
            return -1;
        } else {
            ESP_LOGE(TAG, "Read error: %d", errno);
            return -1;
        }
    }

    if (n == 0) {
        return n;
    }


    buffer[n] = 0x0;

    ESP_LOGI(TAG, "RTSP < (%d bytes): %s", n, buffer);

    if (parse_request(connection->parser, buffer, n) < 0) {
        ESP_LOGE(TAG, "Error parsing request");
        return -1;
    }


    return n;
}

static int esp_rtsp_handle_request(esp_rtsp_server_connection_t *connection, rtsp_req_t *request) {
    if (!connection->connection_active) {
        return -1;
    }

    int sock = connection->socket;

    switch (request->request_type) {
        case OPTIONS:
            handle_options(connection, request);
            break;
        case SETUP:
            handle_setup(connection, request);
            break;
        case DESCRIBE:
            handle_describe(connection, request);
            break;
        case PLAY:
            handle_play(connection, request);
            break;
        case TEARDOWN:
            handle_teardown(connection, request);
            break;
        default:
            ESP_LOGI(TAG, "RTSP >: %s", "RTSP/1.0 200 OK\r\n\r\n");
            send(sock, "RTSP/1.0 200 OK\r\n\r\n", 19, 0);
    }

    return 0;
}

static int esp_rtsp_handle_read(esp_rtsp_server_connection_t *connection) {
    if (!connection) {
        ESP_LOGW(TAG, "Read on socket %d, but no registered connection", connection->socket);
        return -1;
    }

    if (!connection->connection_active) {
        ESP_LOGW(TAG, "Read on socket inactive socket %d", connection->socket);
        return -1;
    }

    ssize_t n = esp_rtsp_server_read_block(connection);
    if (n < 0) {
        ESP_LOGE(TAG, "Read failed");
        connection->connection_active = false;
        shutdown(connection->socket, 0);
        close(connection->socket);
        return -1;
    }

    if (n == 0) {
        ESP_LOGD(TAG, "Closing connection");
        connection->connection_active = false;
        shutdown(connection->socket, 0);
        close(connection->socket);
        return 0;
    }

    if (parser_is_complete(connection->parser)) {
        rtsp_req_t *request = parser_get_request(connection->parser);
        parser_free(connection->parser);
        parser_init(&connection->parser);

        if (esp_rtsp_handle_request(connection, request) < 0) {
            ESP_LOGW(TAG, "Failed to handle request %d", request->request_type);
            return -1;
        }
    }

    return 0;
}


static int esp_rtsp_create_listening_socket(int port) {
    int listen_sock = socket(AF_INET6, SOCK_STREAM, 0);
    if (listen_sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        return -1;
    }

    struct in6_addr inaddr_any = IN6ADDR_ANY_INIT;
    struct sockaddr_in6 serv_addr = {
            .sin6_family  = PF_INET6,
            .sin6_addr    = inaddr_any,
            .sin6_port    = htons(PORT)
    };

    int opt = 1;
    if (setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        ESP_LOGE(TAG, "Unable to set socket SO_REUSEADDR: errno %d", errno);
        goto CLEAN_UP;
    }

    ESP_LOGI(TAG, "Socket created");

    int err = bind(listen_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    if (err != 0) {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        goto CLEAN_UP;
    }

    err = listen(listen_sock, 1);
    if (err != 0) {
        ESP_LOGE(TAG, "Error occurred during listen: errno %d", errno);
        goto CLEAN_UP;
    }

    ESP_LOGI(TAG, "Created listening socket on port %d", port);
    return listen_sock;

    CLEAN_UP:
    close(listen_sock);
    return -1;
}

static esp_err_t rtsp_server_accept(int listen_sock) {
    int keepAlive = 1;
    int keepIdle = KEEPALIVE_IDLE;
    int keepInterval = KEEPALIVE_INTERVAL;
    int keepCount = KEEPALIVE_COUNT;

    struct sockaddr_storage source_addr; // Large enough for both IPv4 or IPv6
    socklen_t addr_len = sizeof(source_addr);
    int sock = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);
    if (sock < 0) {
        ESP_LOGE(TAG, "Unable to accept connection: errno %d", errno);
        return ESP_FAIL;
    }

    esp_rtsp_server_connection_t *connection = NULL;

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!connections[i].connection_active) {
            // claim this connection
            connections[i].connection_active = true;
            connection = &connections[i];
        }
    }

    if (!connection) {
        ESP_LOGW(TAG, "No free connections");
        shutdown(sock, 0);
        close(sock);
        return ESP_FAIL;
    }

    // Set tcp keepalive option
    setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &keepAlive, sizeof(int));
    setsockopt(sock, IPPROTO_TCP, TCP_KEEPIDLE, &keepIdle, sizeof(int));
    setsockopt(sock, IPPROTO_TCP, TCP_KEEPINTVL, &keepInterval, sizeof(int));
    setsockopt(sock, IPPROTO_TCP, TCP_KEEPCNT, &keepCount, sizeof(int));

    struct timeval to;
    to.tv_sec = 3;
    to.tv_usec = 0;

    if (setsockopt(sock,SOL_SOCKET,SO_RCVTIMEO,&to,sizeof(to)) < 0) {
        ESP_LOGW(TAG, "Set recv timeout failed");
        shutdown(sock, 0);
        close(sock);
        return ESP_FAIL;
    }

    // Convert ip address to string
    if (source_addr.ss_family == PF_INET) {
        inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr, connection->client_addr_string, sizeof(connection->client_addr_string) - 1);
    }
    else if (source_addr.ss_family == PF_INET6) {
        inet6_ntoa_r(((struct sockaddr_in6 *)&source_addr)->sin6_addr, connection->client_addr_string, sizeof(connection->client_addr_string) - 1);
    }
    ESP_LOGI(TAG, "Socket accepted ip address: %s", connection->client_addr_string);

    connection->socket = sock;
    if (parser_init(&connection->parser) < 0) {
        ESP_LOGW(TAG, "Failed to create parser state for connection");
        shutdown(sock, 0);
        close(sock);
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t rtsp_server_main() {
    int listen_sock = esp_rtsp_create_listening_socket(554);
    if (listen_sock < 0) {
        return ESP_FAIL;
    }

    while (1) {
        int sock_max = listen_sock;

        fd_set read_set;
        FD_ZERO(&read_set);
        FD_SET(listen_sock, &read_set);

        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (connections[i].connection_active) {
                FD_SET(connections[i].socket, &read_set);
                if (connections[i].socket > sock_max) {
                    sock_max = connections[i].socket;
                }
            }
        }

        struct timeval to;
        to.tv_sec = 1;
        to.tv_usec = 0;

        ESP_LOGD(TAG, "Entering select");
        int n = lwip_select(sock_max + 1, &read_set, NULL, NULL, NULL);
        if (n < 0) {
            if (errno == EINTR) {
                ESP_LOGW(TAG, "select interrupted");
                continue;
            }

            ESP_LOGE(TAG, "Failure in select, errno %d", errno);
            break;
        }

        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (FD_ISSET(connections[i].socket, &read_set)) {
                ESP_LOGD(TAG, "Read on connection %d", i);
                esp_rtsp_handle_read(&connections[i]);
            }
        }

        if (FD_ISSET(listen_sock, &read_set)) {
            ESP_LOGD(TAG, "Read on listen socket");
            esp_err_t err = rtsp_server_accept(listen_sock);
            if (err != ESP_OK) {
                ESP_LOGW(TAG, "Failed to accept connection");
            }
        }
   }

    ESP_LOGI(TAG, "Shutting down listening socket");
    close(listen_sock);
    return ESP_OK;
}

