//
// Created by Hugo Trippaers on 18/04/2021.
//

#ifndef ESPCAM_ESP32CAM_PINS_H
#define ESPCAM_ESP32CAM_PINS_H

/*
 * See: https://randomnerdtutorials.com/esp32-cam-ai-thinker-pinout/
 */

#define CAM_PIN_PWDN    32 //power down is not used
#define CAM_PIN_RESET   -1 //software reset will be performed
#define CAM_PIN_XCLK     0
#define CAM_PIN_SIOD    26
#define CAM_PIN_SIOC    27

#define CAM_PIN_D7      35
#define CAM_PIN_D6      34
#define CAM_PIN_D5      39
#define CAM_PIN_D4      36
#define CAM_PIN_D3      21
#define CAM_PIN_D2      19
#define CAM_PIN_D1      18
#define CAM_PIN_D0       5
#define CAM_PIN_VSYNC   25
#define CAM_PIN_HREF    23
#define CAM_PIN_PCLK    22

#define SD_CLK          24
#define SD_CMD          15
#define SD_DATA0         2
#define SD_DATA1         4
#define SD_DATA2        12
#define SD_DATA3        13

#define FLASH_LED        4

#define BUILTIN_LED     33

#endif //ESPCAM_ESP32CAM_PINS_H
