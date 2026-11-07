idf_component_register(
    SRCS "${tft_espi_SOURCE_DIR}/TFT_eSPI.cpp"
    INCLUDE_DIRS "${tft_espi_SOURCE_DIR}"
    REQUIRES arduino-esp32 driver
)
target_compile_options(${COMPONENT_LIB} PUBLIC "-include${PROJECT_DIR}/main/tft_setup.h")