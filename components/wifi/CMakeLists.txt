set(COMPONENT_ADD_INCLUDEDIRS ./include)

set(COMPONENT_REQUIRES )
set(COMPONENT_PRIV_REQUIRES esp_http_server nvs_flash esp_netif esp_wifi)

set(COMPONENT_SRCS
        ./wifi.c
        ./connect.c
        ./provision.c
        ./dns.c
        )
set(COMPONENT_EMBED_TXTFILES
        ./html/form.html
        ./html/thank_you.html
        )

register_component()