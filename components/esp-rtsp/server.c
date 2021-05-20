//
// Created by Hugo Trippaers on 17/05/2021.
//
#include <string.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"

#include "lwip/err.h"
#include "lwip/sockets.h"

#define TAG "tcp_server"
#define PORT 554  // IANA default for RTSP

#define KEEPALIVE_IDLE              5
#define KEEPALIVE_INTERVAL          5
#define KEEPALIVE_COUNT             3

#define RTSP_PARSER_PARSE_METHOD 0
#define RTSP_PARSER_PARSE_URL 1
#define RTSP_PARSER_PARSE_PROTOCOL 2
#define RTSP_PARSER_OPTIONAL_HEADER 9
#define RTSP_PARSER_PARSE_HEADER 10
#define RTSP_PARSER_PARSE_HEADER_VALUE 11
#define RTSP_PARSER_PARSE_HEADER_WS 12
#define RTSP_PARSER_PARSE_BODY 20

#define URL_MAX_LENGTH 1024

#define PARSE_STATE_INIT() { \
     .parse_complete = false, \
     .state = RTSP_PARSER_PARSE_METHOD, \
     .intermediate_len = 0   \
     }

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
    int transport_porta;
    int transport_portb;
} rtsp_req_t;

typedef struct {
    int state;
    bool parse_complete;
    char intermediate[1024];
    size_t intermediate_len;
    rtsp_req_t *request;
} rtsp_parser_state_t;

inline int min(int a, int b) { return (a < b) ? a : b; }

static bool valid_header_name_char(char c) {
    if (c > 127) return false;
    if (c <=31 || c == 127 ) return false; // CTLs
    if (c == '(' || c == ')' || c == '<' || c == '>' || c == '@' ||
            c == ',' || c == ';' || c == ':' || c == '\\' || c == '"' ||
            c == '/' || c == '[' || c == ']' || c == '?' || c == '=' ||
            c == '{' || c == '}' || c == ' ' || c == '\t' )
        return false; // separators

    return true;
}

static int safe_atoi(char *value) {
    char *end;
    long lv = strtol(value, &end, 10);
    if (*end != '\0' || lv < INT_MIN || lv > INT_MAX) {
        ESP_LOGE(TAG, "Invalid numerical value: %s", value);
        return -1;
    }
    return (int)lv;
}

static int parse_request(rtsp_parser_state_t *rtsp_parser_state, const char *buffer, const size_t len) {
    assert(rtsp_parser_state != NULL);
    assert(!rtsp_parser_state->parse_complete);
    assert(rtsp_parser_state->request != NULL);

    rtsp_req_t *request = rtsp_parser_state->request;

    int position = 0;
    int header_marker = 0;
    int header_value_marker = 0;
    for (int i = 0; i < len; i++) {
        char current = buffer[i];
//        ESP_LOGD(TAG, "Parsing '%c' at %d in state %d", current, position, rtsp_parser_state->state);
        rtsp_parser_state->intermediate[rtsp_parser_state->intermediate_len + 1] = 0x0; // workaround
//        ESP_LOGD(TAG, "Intermediate: %s (%d bytes)", rtsp_parser_state->intermediate, rtsp_parser_state->intermediate_len);

        if (rtsp_parser_state->intermediate_len >= 1023) {
            ESP_LOGE(TAG, "Parse failed, no space in intermediate buffer");
            return -1;
        }

        // Handle CR/LF
        if (current == '\r' && i < len - 1) {
            current = buffer[i++];
            position++;
        }

        switch(rtsp_parser_state->state) {
            case RTSP_PARSER_PARSE_METHOD:
                if (current == ' ') {
                    if (strncmp(rtsp_parser_state->intermediate, "OPTIONS", min(rtsp_parser_state->intermediate_len,7)) == 0) {
                        request->request_type = OPTIONS;
                    } else if (strncmp(rtsp_parser_state->intermediate, "SETUP", min(rtsp_parser_state->intermediate_len,7)) == 0) {
                        request->request_type = SETUP;
                    } else if (strncmp(rtsp_parser_state->intermediate, "DESCRIBE", min(rtsp_parser_state->intermediate_len,7)) == 0) {
                        request->request_type = DESCRIBE;
                    } else if (strncmp(rtsp_parser_state->intermediate, "PLAY", min(rtsp_parser_state->intermediate_len,7)) == 0) {
                        request->request_type = PLAY;
                    } else if (strncmp(rtsp_parser_state->intermediate, "TEARDOWN", min(rtsp_parser_state->intermediate_len,7)) == 0) {
                        request->request_type = TEARDOWN;

                    } else {
                        ESP_LOGE(TAG, "Unsupported method");
                        return -1;
                    }

                    rtsp_parser_state->state = RTSP_PARSER_PARSE_URL;
                    rtsp_parser_state->intermediate_len = 0;
                    position++;

                    ESP_LOGD(TAG, "Method: %d", request->request_type);
                    continue;
                } else if (current < 'A' || current > 'Z') {
                    ESP_LOGE(TAG, "Invalid character in method: %c", current);
                    return -1;
                } else {
                    rtsp_parser_state->intermediate[rtsp_parser_state->intermediate_len] = current;
                    position++;
                    rtsp_parser_state->intermediate_len++;
                }
                break;
            case RTSP_PARSER_PARSE_URL:
                if (current == ' ') {
                    strncpy(request->url, rtsp_parser_state->intermediate, min(rtsp_parser_state->intermediate_len, URL_MAX_LENGTH));
                    request->url[min(rtsp_parser_state->intermediate_len, URL_MAX_LENGTH)] = 0x0;
                    ESP_LOGD(TAG, "Parsed url: %s", request->url);

                    rtsp_parser_state->state = RTSP_PARSER_PARSE_PROTOCOL;
                    rtsp_parser_state->intermediate_len = 0;
                    position++;

                    continue;
                } else if (current == '\r' || current == '\n') {
                    ESP_LOGE(TAG, "Invalid character in url: %c", current);
                    return -1;
                } else {
                    rtsp_parser_state->intermediate[rtsp_parser_state->intermediate_len] = current;
                    position++;
                    rtsp_parser_state->intermediate_len++;
                }
                break;
            case RTSP_PARSER_PARSE_PROTOCOL:
                if (current == '\r' || current == '\n') {
                    if (strncmp(rtsp_parser_state->intermediate, "RTSP/1.0", min(rtsp_parser_state->intermediate_len,9)) != 0) {
                        ESP_LOGE(TAG, "Only supporting RTSP/1.0 but got: %s", rtsp_parser_state->intermediate);
                        return -1;
                    }

                    rtsp_parser_state->state = RTSP_PARSER_OPTIONAL_HEADER;
                    rtsp_parser_state->intermediate_len = 0;
                    position++;
                } else {
                    rtsp_parser_state->intermediate[rtsp_parser_state->intermediate_len] = current;
                    position++;
                    rtsp_parser_state->intermediate_len++;
                }
                break;
            case RTSP_PARSER_OPTIONAL_HEADER:
                if (current == '\r' || current == '\n') {
                    ESP_LOGD(TAG, "Setting parse_complete");
                    rtsp_parser_state->parse_complete = true;
                    return len;
                }
            case RTSP_PARSER_PARSE_HEADER:
                if (current == ':') {
                    rtsp_parser_state->intermediate[rtsp_parser_state->intermediate_len] = 0x0;
                    position++;
                    rtsp_parser_state->intermediate_len++;
                    header_marker = 0;
                    header_value_marker = rtsp_parser_state->intermediate_len;
                    rtsp_parser_state->state = RTSP_PARSER_PARSE_HEADER_WS;
                } else if (!valid_header_name_char(current)) {
                    ESP_LOGE(TAG, "Invalid characters in header name: %c", current);
                    return -1;
                } else {
                    rtsp_parser_state->intermediate[rtsp_parser_state->intermediate_len] = current;
                    position++;
                    rtsp_parser_state->intermediate_len++;
                }
                break;
            case RTSP_PARSER_PARSE_HEADER_WS:
                if (current == ' ') {
                    rtsp_parser_state->state = RTSP_PARSER_PARSE_HEADER_VALUE;
                    continue;
                }
                ESP_LOGE(TAG, "Unexpected character in header: %c", current);
                return -1;
            case RTSP_PARSER_PARSE_HEADER_VALUE:
                if (current == '\r' || current == '\n') {
                    char *header = rtsp_parser_state->intermediate + header_marker;
                    char *value = rtsp_parser_state->intermediate + header_value_marker;

                    ESP_LOGD(TAG, "Header> %s: %s", header, value);
                    if (strcasecmp(header, "cseq") == 0) {
                        char *end;
                        long lv = strtol(value, &end, 10);
                        if (*end != '\0' || lv < INT_MIN || lv > INT_MAX) {
                            ESP_LOGE(TAG, "Invalid numerical value for header %s: %s", header, value);
                            return -1;
                        }
                        request->cseq = (int)lv;
                    } else if (strcasecmp(header, "transport") == 0) {
                        char *saveptr;

                        char *token = strtok_r(value, ";", &saveptr);
                        if (token == NULL || strcmp(token, "RTP/AVP") != 0) {
                            ESP_LOGE(TAG, "Unsupported stream transport: %s", token);
                            return -1;
                        }

                        token = strtok_r(NULL, ";", &saveptr);
                        if (token == NULL || strcmp(token, "unicast") != 0) {
                            ESP_LOGE(TAG, "Unsupported direction transport: %s", token);
                            return -1;
                        }

                        token = strtok_r(NULL, ";", &saveptr);
                        if (token == NULL || strncmp(token, "client_port=", min(strlen(token), 7)) != 0) {
                            ESP_LOGE(TAG, "Expected client_port, got : %s", token);
                            return -1;
                        }

                        char *porta = strtok_r(token+12, "-", &saveptr);
                        request->transport_porta = safe_atoi(porta);

                        char *portb = strtok_r(NULL, "-", &saveptr);
                        request->transport_portb = safe_atoi(portb);

                        if (request->transport_porta < 0 || request->transport_portb < 0) {
                            ESP_LOGE(TAG, "Invalid client_port values: %s", token);
                            return -1;
                        }

                    }
                    rtsp_parser_state->state = RTSP_PARSER_OPTIONAL_HEADER;
                    rtsp_parser_state->intermediate_len = 0;
                    position++;
                } else {
                    rtsp_parser_state->intermediate[rtsp_parser_state->intermediate_len] = current;
                    position++;
                    rtsp_parser_state->intermediate_len++;
                }
                break;

            default:
                continue;
        };
    }
    return len;
};

static void handle_options(rtsp_req_t *request, const int sock) {
    char buffer[2048];
    size_t msgsize = snprintf(buffer, 2048,
                              "RTSP/1.0 200 OK\r\n"
                              "cSeq: %d\r\n"
                              "Public: OPTIONS, DESCRIBE, SETUP, TEARDOWN, PLAY, PAUSE\r\n"
                              "Server: ESP32 Cam Server\r\n"
                              "Date: Wed, 19 May 2021 17:14:24 GMT\r\n"
                              "\r\n",
                              request->cseq);
    size_t sent = send(sock, buffer, msgsize, 0);
    ESP_LOGI(TAG, "RTSP >: %s", buffer);
    if (sent != msgsize) {
        ESP_LOGW(TAG, "Mismatch between msgsize and sent bytes: %d vs %d", msgsize, sent);
    }
}

static void handle_setup(rtsp_req_t *request, const int sock) {
    char buffer[2048];
    size_t msgsize = snprintf(buffer, 2048,
                              "RTSP/1.0 200 OK\r\n"
                              "cSeq: %d\r\n"
                              "Transport: RTP/AVP;unicast;client_port=%d-%d;server_port=9000-9001\r\n"
                              "Session: 12348765\r\n"
                              "\r\n",
                              request->cseq,
                              request->transport_porta,
                              request->transport_portb);
    size_t sent = send(sock, buffer, msgsize, 0);
    ESP_LOGI(TAG, "RTSP >: %s", buffer);
    if (sent != msgsize) {
        ESP_LOGW(TAG, "Mismatch between msgsize and sent bytes: %d vs %d", msgsize, sent);
    }
}

static void handle_describe(rtsp_req_t *request, const int sock) {
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
    size_t sent = send(sock, buffer, msgsize, 0);
    ESP_LOGI(TAG, "RTSP >: %s", buffer);
    if (sent != msgsize) {
        ESP_LOGW(TAG, "Mismatch between msgsize and sent bytes: %d vs %d", msgsize, sent);
    }

    // Send body
    sent = send(sock, sdp, sdp_size, 0);
    ESP_LOGI(TAG, "RTSP >: %s", sdp);
    if (sent != sdp_size) {
        ESP_LOGW(TAG, "Mismatch between sdp_size and sent bytes: %d vs %d", sdp_size, sent);
    }
}

static void do_retransmit(const int sock) {
    char buffer[1024];

    struct timeval to;
    to.tv_sec = 3;
    to.tv_usec = 0;

    if (setsockopt(sock,SOL_SOCKET,SO_RCVTIMEO,&to,sizeof(to)) < 0) {
        return;
    }

    int n = read(sock, buffer, 1023);
    buffer[n] = 0x0;

    rtsp_req_t request;
    rtsp_parser_state_t parser_state = PARSE_STATE_INIT();
    parser_state.request = &request;

    int timeout = 0;

    for (;;) {
        ESP_LOGI(TAG, "RTSP < (%d bytes): %s", n, buffer);

        if (parse_request(&parser_state, buffer, n) < 0) {
            ESP_LOGE(TAG, "Error parsing request");
            send(sock, "RTSP/1.0 500 Internal Server Error\r\n\r\n", 38, 0);
            break;
        }

        if (parser_state.parse_complete) {
            switch (request.request_type) {
                case OPTIONS:
                    handle_options(&request, sock);
                    break;
                case SETUP:
                    handle_setup(&request, sock);
                    break;
                case DESCRIBE:
                    handle_describe(&request, sock);
                    break;
                default:
                    ESP_LOGI(TAG, "RTSP >: %s", "RTSP/1.0 200 OK\r\n\r\n");
                    send(sock, "RTSP/1.0 200 OK\r\n\r\n", 19, 0);
            }

            bzero(&request, sizeof(rtsp_req_t));
            bzero(&parser_state, sizeof (rtsp_parser_state_t));
            parser_state.request = &request;
        }

        n = read(sock, buffer, 1023);
        if (n < 0)
        {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                ESP_LOGW(TAG, "Receive timeout");
                timeout++;
                n = 0;
            } else {
                ESP_LOGE(TAG, "Read error: %d", errno);
                break;
            }
        } else if (n == 0) {
            ESP_LOGD(TAG, "Client closed connection");
            break;
        } else {
            timeout = 0; // reset
        }

        buffer[n] = 0x0;

        if (timeout >= 10) {
            ESP_LOGE(TAG, "No activity on socket, disconnecting");
            break;
        }
    }

    ESP_LOGD(TAG, "Returning socket for closing");
}

static void rtsp_server_task(void *pvParameters)
{
    char addr_str[128];
    int keepAlive = 1;
    int keepIdle = KEEPALIVE_IDLE;
    int keepInterval = KEEPALIVE_INTERVAL;
    int keepCount = KEEPALIVE_COUNT;

    int listen_sock = socket(AF_INET6, SOCK_STREAM, 0);
    if (listen_sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        vTaskDelete(NULL);
        return;
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
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "Socket created");

    int err = bind(listen_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    if (err != 0) {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        goto CLEAN_UP;
    }
    ESP_LOGI(TAG, "Socket bound, port %d", PORT);

    err = listen(listen_sock, 1);
    if (err != 0) {
        ESP_LOGE(TAG, "Error occurred during listen: errno %d", errno);
        goto CLEAN_UP;
    }

    while (1) {

        ESP_LOGI(TAG, "Socket listening");

        struct sockaddr_storage source_addr; // Large enough for both IPv4 or IPv6
        socklen_t addr_len = sizeof(source_addr);
        int sock = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);
        if (sock < 0) {
            ESP_LOGE(TAG, "Unable to accept connection: errno %d", errno);
            break;
        }

        // Set tcp keepalive option
        setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &keepAlive, sizeof(int));
        setsockopt(sock, IPPROTO_TCP, TCP_KEEPIDLE, &keepIdle, sizeof(int));
        setsockopt(sock, IPPROTO_TCP, TCP_KEEPINTVL, &keepInterval, sizeof(int));
        setsockopt(sock, IPPROTO_TCP, TCP_KEEPCNT, &keepCount, sizeof(int));

        // Set nonblocking IO
        // fcntl(sock, O_NONBLOCK);

        // Convert ip address to string
        if (source_addr.ss_family == PF_INET) {
            inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr, addr_str, sizeof(addr_str) - 1);
        }
        else if (source_addr.ss_family == PF_INET6) {
            inet6_ntoa_r(((struct sockaddr_in6 *)&source_addr)->sin6_addr, addr_str, sizeof(addr_str) - 1);
        }
        ESP_LOGI(TAG, "Socket accepted ip address: %s", addr_str);

        do_retransmit(sock);

        shutdown(sock, 0);
        close(sock);
    }

    CLEAN_UP:
    close(listen_sock);
    vTaskDelete(NULL);
}

static TaskHandle_t rtsp_tcp_server_taskhandle;

esp_err_t esp_rtsp_server_init() {
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t esp_rtsp_server_start() {
    BaseType_t result = xTaskCreate(rtsp_server_task, "rtsp_tcp_server", 8192, NULL, 12, &rtsp_tcp_server_taskhandle);
    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create tcp server task");
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t esp_rtsp_server_stop() {
    return ESP_ERR_NOT_SUPPORTED;
}