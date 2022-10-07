set(COMPONENT_ADD_INCLUDEDIRS ./include)

set(COMPONENT_REQUIRES )
set(COMPONENT_PRIV_REQUIRES esp_http_client nvs_flash)

set(COMPONENT_SRCS
        ./dlna.c
        ./ssdp.c
        )

register_component()