//
// Created by Hugo Trippaers on 17/04/2021.
//

#ifndef ESPCAM_CORE_MQTT_CONFIG_H
#define ESPCAM_CORE_MQTT_CONFIG_H

#include "esp_tls.h"
#include "esp_log.h"

/**
 * @brief Determines the maximum number of MQTT PUBLISH messages, pending
 * acknowledgement at a time, that are supported for incoming and outgoing
 * direction of messages, separately.
 *
 * QoS 1 and 2 MQTT PUBLISHes require acknowledgement from the server before
 * they can be completed. While they are awaiting the acknowledgement, the
 * client must maintain information about their state. The value of this
 * macro sets the limit on how many simultaneous PUBLISH states an MQTT
 * context maintains, separately, for both incoming and outgoing direction of
 * PUBLISHes.
 *
 * @note The MQTT context maintains separate state records for outgoing
 * and incoming PUBLISHes, and thus, 2 * MQTT_STATE_ARRAY_MAX_COUNT amount
 * of memory is statically allocated for the state records.
 */

#define MQTT_STATE_ARRAY_MAX_COUNT    ( 10U )

/**
 * @brief Number of milliseconds to wait for a ping response to a ping
 * request as part of the keep-alive mechanism.
 *
 * If a ping response is not received before this timeout, then
 * #MQTT_ProcessLoop will return #MQTTKeepAliveTimeout.
 */
#define MQTT_PINGRESP_TIMEOUT_MS      ( 5000U )

#define _Args(...) __VA_ARGS__
#define STRIP_PARENS(X) X
#define PASS_PARAMETERS(X) STRIP_PARENS( _Args X )

#define AWS_IOT_MQTT_TAG "aws-iot-mqtt"

#define LogError(message) ESP_LOGE(AWS_IOT_MQTT_TAG, PASS_PARAMETERS(message))
#define LogWarn(message)  ESP_LOGW(AWS_IOT_MQTT_TAG, PASS_PARAMETERS(message))
#define LogInfo(message)  ESP_LOGI(AWS_IOT_MQTT_TAG, PASS_PARAMETERS(message))
#define LogDebug(message) ESP_LOGD(AWS_IOT_MQTT_TAG, PASS_PARAMETERS(message))

struct NetworkContext {
    esp_tls_t *esp_tls;
};

#endif //ESPCAM_CORE_MQTT_CONFIG_H
