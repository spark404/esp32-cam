set(COMPONENT_SRCS "main.c" "wifi.c" "camera.c" "mqtt.c" "certs.c" "sdcard.c")
set(COMPONENT_ADD_INCLUDEDIRS ".")

register_component()

if(CONFIG_ESPCAM_AWS_CERTS_EMBEDDED)
  target_add_binary_data(${COMPONENT_TARGET} "certs/aws-root-ca.pem" TEXT)
  target_add_binary_data(${COMPONENT_TARGET} "certs/certificate.pem.crt" TEXT)
  target_add_binary_data(${COMPONENT_TARGET} "certs/private.pem.key" TEXT)
endif()