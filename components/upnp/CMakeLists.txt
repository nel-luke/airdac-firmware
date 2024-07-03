set(COMPONENT_ADD_INCLUDEDIRS ./include)

set(COMPONENT_REQUIRES )
set(COMPONENT_PRIV_REQUIRES esp_http_server esp_http_client nvs_flash audio esp_netif)

set(COMPONENT_SRCS
        ./upnp.c
        ./upnp_common.c
        ./uuid.c
        ./control.c
        ./control/av_transport.c
        ./control/connection_manager.c
        ./control/rendering_control.c
        ./control/control_common.c
        ./eventing.c
        ./description.c
        ./discovery.c
        ./stream.c
        )

set(COMPONENT_EMBED_TXTFILES
        ./xml/SoapResponseOk.xml
        ./xml/SoapResponseErr.xml
        ./xml/rootDesc.xml
        ./xml/AVTransport.xml
        ./xml/ConnectionManager.xml
        ./xml/RenderingControl.xml
        ./xml/StateChangeEvent.xml
        ./xml/GetProtocolInfoEvent.xml
        )

register_component()