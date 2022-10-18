set(COMPONENT_ADD_INCLUDEDIRS ./include)

set(COMPONENT_REQUIRES )
set(COMPONENT_PRIV_REQUIRES esp_http_server esp_http_client nvs_flash)

set(COMPONENT_SRCS
        ./upnp.c
        ./upnp_common.c
        ./uuid.c
        ./control.c
        ./eventing.c
        ./description.c
        ./discovery.c
        )

set(COMPONENT_EMBED_TXTFILES
        ./xml/SoapResponseOk.xml
        ./xml/rootDesc.xml
        ./xml/AVTransport.xml
        ./xml/ConnectionManager.xml
        ./xml/RenderingControl.xml
        ./xml/StateChangeEvent.xml
        ./xml/InitialAVT.xml
        ./xml/GetProtocolInfoEvent.xml
        ./xml/InitialRCS.xml
        )

register_component()