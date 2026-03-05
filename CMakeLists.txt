cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# specified board definitions
list(APPEND PICO_BOARD_HEADER_DIRS ${CMAKE_CURRENT_LIST_DIR}/src/boards)
set(PICO_BOARD pico)

include(pico_sdk_import.cmake)
include(scripts/functions.cmake)
include(FreeRTOS_Kernel_import.cmake)

project(pico_wiretap C CXX ASM)


add_subdirectory(src)
