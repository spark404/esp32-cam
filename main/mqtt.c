//
// Created by Hugo Trippaers on 18/04/2021.
//

#include "esp_log.h"
#include "esp_tls.h"

// AWS-IOT-SDK Component
#include "core_mqtt.h"
#include "core_mqtt_state.h"
#include "port.h"

#include "common.h"

#define TAG "esp32cam_mqtt"
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

static esp_err_t transport_connect(TransportInterface_t *transportInterface) {
    unsigned char *cabundle;
    long cabundle_len;
    ESP_ERROR_CHECK(esp32cam_readpem(CABUNDLE, &cabundle, &cabundle_len));

    unsigned char *certificate;
    long certificate_len;
    ESP_ERROR_CHECK(esp32cam_readpem(DEVICE_CERTIFICATE, &certificate, &certificate_len));

    unsigned char *privatekey;
    long privatekey_len;
    ESP_ERROR_CHECK(esp32cam_readpem(DEVICE_PRIVATEKEY, &privatekey, &privatekey_len));

    esp_tls_cfg_t esp_tls_cfg = {
            .use_global_ca_store = false,
            .cacert_buf = cabundle,
            .cacert_bytes = cabundle_len,
            .clientcert_buf = certificate,
            .clientcert_bytes = certificate_len,
            .clientkey_buf = privatekey,
            .clientkey_bytes = privatekey_len
    };

    esp_tls_t *esp_tls = transportInterface->pNetworkContext->esp_tls;
    int result = esp_tls_conn_new_sync(CONFIG_ESPCAM_AWS_IOT_ENDPOINT, strlen(CONFIG_ESPCAM_AWS_IOT_ENDPOINT), 8883, &esp_tls_cfg, esp_tls);
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

static esp_err_t mqtt_connect() {
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
            .pClientIdentifier = "esp_cam_01",
            .clientIdentifierLength = 10,
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

esp_err_t esp32cam_mqtt_connect() {
    esp_err_t err = transport_connect(&mqttTransportInterface);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp-tls transport failed to connect");
        return ESP_FAIL;
    }

    err = mqtt_connect();
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