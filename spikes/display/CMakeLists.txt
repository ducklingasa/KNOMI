cmake_minimum_required(VERSION 3.16)

include(FetchContent)
FetchContent_Declare(tft_espi
    URL https://github.com/Bodmer/TFT_eSPI/archive/6ae4c97c5a07bd64fce8993e9cfa95d9a81e7c81.zip
    SOURCE_SUBDIR knomi_fetch_only
)
FetchContent_MakeAvailable(tft_espi)

add_compile_definitions(LV_CONF_INCLUDE_SIMPLE LV_KCONFIG_IGNORE)
include_directories("${CMAKE_CURRENT_LIST_DIR}/main")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(knomi_display_spike)