FILE(GLOB app_sources ${CMAKE_CURRENT_SOURCE_DIR}/*.c)

idf_component_register(SRCS ${app_sources}
                       INCLUDE_DIRS "."
                       REQUIRES driver esp_timer esp_wifi esp_event nvs_flash esp_netif esp_http_server)
