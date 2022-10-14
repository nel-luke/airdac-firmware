set(COMPONENT_ADD_INCLUDEDIRS ./include)

set(COMPONENT_REQUIRES )
set(COMPONENT_PRIV_REQUIRES nvs_flash)

set(COMPONENT_SRCS
        ./upnp.c
        ./discovery.c
        )

register_component()