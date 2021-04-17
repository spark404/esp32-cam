//
// Created by Hugo Trippaers on 17/04/2021.
//

#ifndef ESPCAM_LOGGING_H
#define ESPCAM_LOGGING_H

#include <esp_log.h>

#define TAG "aws-iot-sdk"

#define LogError(message) ESP_LOGE(TAG, message)
#define LogWarn(message) ESP_LOGW(TAG, message)
#define LogDebug(message) ESP_LOGD(TAG, message)
#define LogInfo(message) ESP_LOGI(TAG, message)

#endif //ESPCAM_LOGGING_H
