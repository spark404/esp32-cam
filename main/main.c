#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"
#include "esp_tls.h"

#include "nvs.h"
#include "nvs_flash.h"

#include "core_mqtt.h"
#include "core_mqtt_state.h"
#include "port.h"

#include "sdkconfig.h"

#define TAG "main"

extern esp_err_t wifi_init();

extern const uint8_t server_root_cert_pem_start[] asm("_binary_aws_root_ca_pem_start");
extern const uint8_t server_root_cert_pem_end[]   asm("_binary_aws_root_ca_pem_end");

extern const uint8_t client_crt_pem_start[]       asm("_binary_certificate_pem_crt_start");
extern const uint8_t client_crt_pem_end[]         asm("_binary_certificate_pem_crt_end");
extern const uint16_t client_crt_pem_length      asm("certificate_pem_crt_length");

extern const uint8_t client_key_pem_start[]       asm("_binary_private_pem_key_start");
extern const uint8_t client_key_pem_end[]         asm("_binary_private_pem_key_end");
extern const uint16_t client_key_pem_length      asm("private_pem_key_length");

void mqtt_callback(MQTTContext_t *pMqttContext, MQTTPacketInfo_t *pMqttPacketInfo, MQTTDeserializedInfo_t *pMqttDeserializedInfo) {
    ESP_LOGD(TAG, "mqtt_callback");
}

void app_main()
{
    // Initialize NVS.
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK( err );

    // Initialize Event Loop
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Initialize and connect WiFi
    wifi_init();

    esp_tls_t *tls;
    tls = esp_tls_init();

    esp_tls_cfg_t esp_tls_cfg = {
            .use_global_ca_store = false,
            .cacert_buf = (const unsigned char *) server_root_cert_pem_start,
            .cacert_bytes = server_root_cert_pem_end - server_root_cert_pem_start,
            .clientcert_buf = (const unsigned char *) client_crt_pem_start,
            .clientcert_bytes = client_crt_pem_end - client_crt_pem_start,
            .clientkey_buf = (const unsigned char *) client_key_pem_start,
            .clientkey_bytes = client_key_pem_end - client_key_pem_start
    };
    int result = esp_tls_conn_new_sync(CONFIG_ESPCAM_AWS_IOT_ENDPOINT, strlen(CONFIG_ESPCAM_AWS_IOT_ENDPOINT), 8883, &esp_tls_cfg, tls);
    if (result != 1) {
        ESP_LOGE(TAG, "esp_tls failed to connect");
    }

    MQTTStatus_t mqttStatus;
    MQTTFixedBuffer_t networkBuffer;

    NetworkContext_t context = {
            .esp_tls = tls
    };

    TransportInterface_t  transportInterface = {
            .pNetworkContext = &context,
            .recv = networkRecv,
            .send = networkSend
    };

    networkBuffer.pBuffer = malloc(1024);
    networkBuffer.size = 1024;

    MQTTContext_t pMqttContext;

    mqttStatus = MQTT_Init( &pMqttContext,
                            &transportInterface,
                            getTimeStampMs,
                            mqtt_callback,
                            &networkBuffer );

    if( mqttStatus != MQTTSuccess )
    {
        LogError( ( "MQTT init failed: Status = %s.", MQTT_Status_strerror( mqttStatus ) ) );
    }

    MQTTConnectInfo_t connectInfo = {
            .pClientIdentifier = "esp_cam_01",
            .clientIdentifierLength = 10,
            .cleanSession = true
    };

    bool session_present = false;
    mqttStatus = MQTT_Connect(&pMqttContext, &connectInfo, NULL, 10000, &session_present);

    if (mqttStatus != MQTTSuccess) {
        LogError(("MQTT_Connect failed: Status = %s", MQTT_Status_strerror( mqttStatus ) ));
    }

    MQTT_Disconnect(&pMqttContext);
    esp_tls_conn_destroy(tls);
}