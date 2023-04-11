set(COMPONENT_ADD_INCLUDEDIRS ./include)

set(COMPONENT_REQUIRES )
set(COMPONENT_PRIV_REQUIRES)

set(COMPONENT_SRCS
        ./raop.c
        ./raop_common.c
        ./raop_mdns.c
        ./rtp.c
        ./rtsp.c
        )

idf_component_register()