//
// Created by Hugo Trippaers on 18/04/2021.
//

#include "esp_log.h"
#include "esp_tls.h"

// AWS-IOT-SDK Component
#include "core_mqtt.h"
#include "core_mqtt_state.h"
#include "backoff_algorithm.h"
#include "port.h"

#include "common.h"

#define TAG "esp32cam_mqtt"

#define RETRY_MAX_ATTEMPTS            ( 5U )
#define RETRY_MAX_BACKOFF_DELAY_MS    ( 5000U )
#define RETRY_BACKOFF_BASE_MS         ( 500U )

static MQTTContext_t mqttContext;
static TransportInterface_t mqttTransportInterface;

void mqtt_callback(MQTTContext_t *pMqttContext, MQTTPacketInfo_t *pMqttPacketInfo, MQTTDeserializedInfo_t *pMqttDeserializedInfo) {
    ESP_LOGD(TAG, "mqtt_callback");
}

static esp_err_t transport_init(TransportInterface_t *transportInterface) {
    assert(transportInterface != NULL);

    esp_tls_t *esp_tls = esp_tls_init();
    if (esp_tls == NULL) {
        ESP_LOGE(TAG, "Failed to initialize esp-tls");
        return ESP_FAIL;
    }

    NetworkContext_t *context = malloc(sizeof (NetworkContext_t));
    if (context == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for network context");
        return ESP_FAIL;
    }
    context->esp_tls = esp_tls;

    transportInterface->pNetworkContext = context;
    transportInterface->send = networkSend;
    transportInterface->recv = networkRecv;

    return ESP_OK;

}

static esp_err_t transport_connect(TransportInterface_t *transportInterface, espcam_aws_iot_config_t *aws_iot_config, espcam_tls_config_t *tls_config) {

    esp_tls_cfg_t esp_tls_cfg = {
            .use_global_ca_store = false,
            .cacert_buf = tls_config->cabundle.buffer,
            .cacert_bytes = tls_config->cabundle.len,
            .clientcert_buf = tls_config->device_certificate.buffer,
            .clientcert_bytes = tls_config->device_certificate.len,
            .clientkey_buf = tls_config->device_private_key.buffer,
            .clientkey_bytes = tls_config->device_private_key.len
    };

    BackoffAlgorithmStatus_t retryStatus = BackoffAlgorithmSuccess;
    BackoffAlgorithmContext_t retryParams;
    uint16_t nextRetryBackoff = 0;

    BackoffAlgorithm_InitializeParams( &retryParams,
                                       RETRY_BACKOFF_BASE_MS,
                                       RETRY_MAX_BACKOFF_DELAY_MS,
                                       RETRY_MAX_ATTEMPTS );

    esp_tls_t *esp_tls = transportInterface->pNetworkContext->esp_tls;

    int result = -1;
    do {
        result = esp_tls_conn_new_sync((char *) aws_iot_config->endpoint, strlen((char *) aws_iot_config->endpoint),
                                           8883, &esp_tls_cfg, esp_tls);

        if (result != 1) {
            retryStatus = BackoffAlgorithm_GetNextBackoff(&retryParams, rand(), &nextRetryBackoff);
            ESP_LOGW(TAG, "Retrying connection to broker in %d ms", nextRetryBackoff);
            vTaskDelay(pdMS_TO_TICKS(nextRetryBackoff));
        }
    } while ((result != 1) && ( retryStatus != BackoffAlgorithmRetriesExhausted ));

    if (result != 1) {
        ESP_LOGE(TAG, "esp_tls failed to connect");
        return ESP_FAIL;
    }

    return ESP_OK;
}

static esp_err_t transport_disconnect(TransportInterface_t *transportInterface) {
    assert(transportInterface != NULL);
    assert(transportInterface->pNetworkContext != NULL);

    if (esp_tls_conn_destroy(transportInterface->pNetworkContext->esp_tls) != 0) {
        ESP_LOGW(TAG, "Failed to destroy esp-tls connection");
        return ESP_FAIL;
    }

    return ESP_OK;
}

static esp_err_t mqtt_connect(espcam_aws_iot_config_t *aws_iot_config) {
    MQTTStatus_t mqttStatus;
    MQTTFixedBuffer_t networkBuffer;

    networkBuffer.pBuffer = malloc(1024);
    networkBuffer.size = 1024;

    mqttStatus = MQTT_Init( &mqttContext,
                            &mqttTransportInterface,
                            getTimeStampMs,
                            mqtt_callback,
                            &networkBuffer );

    if( mqttStatus != MQTTSuccess )
    {
        LogError( ( "MQTT init failed: Status = %s.", MQTT_Status_strerror( mqttStatus ) ) );
        return ESP_FAIL;
    }

    MQTTConnectInfo_t connectInfo = {
            .pClientIdentifier = (char *)aws_iot_config->device_name,
            .clientIdentifierLength = strlen((char *)aws_iot_config->device_name),
            .cleanSession = true
    };

    bool session_present = false;
    mqttStatus = MQTT_Connect(&mqttContext, &connectInfo, NULL, 10000, &session_present);

    if (mqttStatus != MQTTSuccess) {
        LogError(("MQTT_Connect failed: Status = %s", MQTT_Status_strerror( mqttStatus ) ));
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t esp32cam_mqtt_init() {
    esp_err_t err = transport_init(&mqttTransportInterface);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize esp-tls transport");
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t esp32cam_mqtt_connect(espcam_aws_iot_config_t *aws_iot_config, espcam_tls_config_t *tls_config) {
    esp_err_t err = transport_connect(&mqttTransportInterface, aws_iot_config, tls_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp-tls transport failed to connect");
        return ESP_FAIL;
    }

    err = mqtt_connect(aws_iot_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "mqtt failed to connect");
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t esp32cam_mqtt_disconnect() {
    MQTT_Disconnect(&mqttContext);
    esp_err_t err = transport_disconnect(&mqttTransportInterface);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to disconnect esp-tls");
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t esp32cam_mqtt_publish(uint8_t *buffer, size_t buffer_length) {
    char topic[1024];
    size_t topic_length = snprintf(topic, 1024, "cam/%s", "hugocam");
    MQTTPublishInfo_t publish_info = {
            .pTopicName = topic,
            .topicNameLength = topic_length,
            .pPayload = buffer,
            .payloadLength = buffer_length,
            .qos = MQTTQoS0,
            .retain = false,
            .dup = false
    };

    MQTTStatus_t status = MQTT_Publish(&mqttContext, &publish_info, 0);
    if (status != MQTTSuccess) {
        ESP_LOGE(TAG, "Publish failed to topic %s (%d)", publish_info.pTopicName, status);
        return ESP_FAIL;
    }

    return ESP_OK;
}