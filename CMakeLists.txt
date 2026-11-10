cmake_minimum_required(VERSION 3.20)

set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/src")
set(COMPONENTS src)
add_compile_definitions(LV_CONF_INCLUDE_SIMPLE LV_KCONFIG_IGNORE)
include_directories("${CMAKE_CURRENT_LIST_DIR}/include")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(knomi_creality)