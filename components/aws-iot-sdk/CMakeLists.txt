set(aws_sdk_dir aws-iot-device-sdk-embedded-C)

include( ${COMPONENT_DIR}/${aws_sdk_dir}/libraries/standard/coreMQTT/mqttFilePaths.cmake )
include( ${COMPONENT_DIR}/${aws_sdk_dir}/libraries/standard/backoffAlgorithm/backoffAlgorithmFilePaths.cmake )

set(PORT_PUBLIC_INCLUDE "port/include")
set(PORT_SOURCES "port/port.c")

set(COMPONENT_ADD_INCLUDEDIRS
        ${MQTT_INCLUDE_PUBLIC_DIRS}
        ${BACKOFF_ALGORITHM_INCLUDE_PUBLIC_DIRS}
        ${PORT_PUBLIC_INCLUDE})

set(COMPONENT_SRCS
        ${MQTT_SOURCES}
        ${MQTT_SERIALIZER_SOURCES}
        ${BACKOFF_ALGORITHM_SOURCES}
        ${PORT_SOURCES})

idf_component_register(SRCS "${COMPONENT_SRCS}"
        INCLUDE_DIRS "${COMPONENT_ADD_INCLUDEDIRS}"
        REQUIRES esp-tls)

