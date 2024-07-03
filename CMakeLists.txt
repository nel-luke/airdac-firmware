# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(airdac_firmware)

target_add_binary_data(airdac_firmware.elf "./logo.png" TXT)
idf_build_set_property(COMPILE_OPTIONS "-Wno-format" APPEND)