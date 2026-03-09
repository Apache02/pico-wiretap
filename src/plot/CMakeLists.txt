add_library(plot INTERFACE)

target_sources(
        plot INTERFACE
        plot.cpp
        cdc_printf.c
)

target_link_libraries(
        plot INTERFACE
        hardware_gpio
        hardware_adc
)
