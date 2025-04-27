cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

set(PICO_BOARD pico2 CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK
include(pico_sdk_import.cmake)

if (PICO_SDK_VERSION_STRING VERSION_LESS "1.5.1")
  message(FATAL_ERROR "Raspberry Pi Pico SDK version 1.5.1 (or later) required. Your version is ${PICO_SDK_VERSION_STRING}")
endif()

project(wy60pico C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

add_executable(wy60pico src/wy60pico.c)

add_compile_definitions(GPIO_CLOCK_IN=2)
add_compile_definitions(GPIO_DATA_OUT=3)

pico_set_program_name(wy60pico "wy60pico")
pico_set_program_version(wy60pico "1.0")

pico_enable_stdio_uart(wy60pico 1)
pico_enable_stdio_usb(wy60pico 0)

target_include_directories(wy60pico PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src)
target_link_libraries(wy60pico pico_stdlib hardware_pio tinyusb_host tinyusb_board)

pico_add_extra_outputs(wy60pico)
