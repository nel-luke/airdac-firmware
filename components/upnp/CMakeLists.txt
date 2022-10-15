set(COMPONENT_ADD_INCLUDEDIRS ./include)

set(COMPONENT_REQUIRES )
set(COMPONENT_PRIV_REQUIRES esp_http_server nvs_flash)

set(COMPONENT_SRCS
        ./upnp.c
        ./discovery.c
        ./description.c
        )

set(COMPONENT_EMBED_TXTFILES
        ./xml/rootDesc.xml
        ./xml/AVTransport.xml
        ./xml/ConnectionManager.xml
        ./xml/RenderingControl.xml
        )

register_component()