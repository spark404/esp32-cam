//
// Created by Hugo Trippaers on 17/04/2021.
//

#include "core_mqtt.h"
#include "esp_tls.h"
#include "esp_log.h"

int32_t networkSend( NetworkContext_t * pContext, const void * pBuffer, size_t bytes ) {
    assert(pContext != NULL);
    assert(pContext->esp_tls != NULL);

    return esp_tls_conn_write(pContext->esp_tls, pBuffer, bytes);
}

int32_t networkRecv( NetworkContext_t * pContext, void * pBuffer, size_t bytes ) {
    assert(pContext != NULL);
    assert(pContext->esp_tls != NULL);

    return esp_tls_conn_read(pContext->esp_tls, pBuffer, bytes);
}

uint32_t getTimeStampMs() {
    return esp_timer_get_time() / 1000;
}