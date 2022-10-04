set(COMPONENT_ADD_INCLUDEDIRS ./include)

set(COMPONENT_REQUIRES )
set(COMPONENT_PRIV_REQUIRES esp_http_server nvs_flash)

set(COMPONENT_SRCS
        ./wifi.c
        ./connect.c
        ./provision.c
        ./dns.c
        )
set(COMPONENT_EMBED_TXTFILES form.html thank_you.html)

register_component()