set(COMPONENT_SRCS "server.c" "rtp-udp.c" "jpeg.c")
set(COMPONENT_ADD_INCLUDEDIRS "include")
set(COMPONENT_PRIV_INCLUDEDIRS "priv")

set(COMPONENT_REQUIRES lwip)
set(COMPONENT_PRIV_REQUIRES freertos nvs_flash)

register_component()
