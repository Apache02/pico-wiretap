pico_sdk_init()

add_subdirectory(shell)
add_subdirectory(commands)

add_executable(
        pico_wiretap
        main.cpp
        usb/usb_descriptors.c
        usb/usb_task.cpp
        shell_task.cpp
        handlers.cpp
)

pico_add_extra_outputs(pico_wiretap)

include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}/config
        ${CMAKE_CURRENT_SOURCE_DIR}/usb
)

target_link_libraries(
        pico_wiretap
        pico_stdlib
        pico_stdio_usb
        tinyusb_device
        FreeRTOS-Kernel
        FreeRTOS-Kernel-Heap4
        shell_main
        shell_commands_common
        commands
)

add_deploy_target(pico_wiretap "${CMAKE_CURRENT_BINARY_DIR}/pico_wiretap.uf2")
