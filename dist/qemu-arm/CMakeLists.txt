set_property(SOURCE src/startup.S PROPERTY LANGUAGE C)
add_library(arm-bsp src/crt0.c src/startup.S)
add_dependencies(libzth arm-bsp)
