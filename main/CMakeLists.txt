# This file was automatically generated for projects
# without default 'CMakeLists.txt' file.
FILE(GLOB_RECURSE app_sources
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)


# 打印 CMAKE_SOURCE_DIR 变量
# message(STATUS "app_sources = ${app_sources}")

idf_component_register(SRCS "${app_sources}"
    PRIV_REQUIRES bt nvs_flash esp_http_server esp_wifi esp_netif esp_event
    esp_driver_uart esp_driver_usb_serial_jtag driver esp_driver_rmt esp_driver_ledc lwip nanopb spiffs
    INCLUDE_DIRS "./include"
)

spiffs_create_partition_image(spiffs ${CMAKE_SOURCE_DIR}/data FLASH_IN_PROJECT)

