# Edit following two lines to set component requirements (see docs)
set(COMPONENT_REQUIRES )
set(COMPONENT_PRIV_REQUIRES nvs_flash esp32-camera esp_http_server)

set(COMPONENT_SRCS "main.c")
set(COMPONENT_ADD_INCLUDEDIRS ".")

set(COMPONENT_EMBED_FILES "../static/index.html ../static/pico.min.css")

register_component()

