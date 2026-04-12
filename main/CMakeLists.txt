FILE(GLOB_RECURSE main ${CMAKE_SOURCE_DIR}/main/*.c)

idf_component_register(SRCS ${main}
                        INCLUDE_DIRS "include" "ui"
                        REQUIRES bsp_extra bsp_display bsp_illuminate bsp_i2c bsp_ina219 bsp_pcnt bsp_ds3231 bsp_sd esp_timer)