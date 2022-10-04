set(COMPONENT_ADD_INCLUDEDIRS ./include)

set(COMPONENT_REQUIRES )
set(COMPONENT_PRIV_REQUIRES esp_http_server)

set(COMPONENT_SRCS
        ./wifi_provisioner.c
        )
set(COMPONENT_EMBED_TXTFILES root.html)

register_component()