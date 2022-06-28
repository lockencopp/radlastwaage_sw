add_executable(rl_sub
        rl_sub.c
        )

# Pull in basic dependencies
target_link_libraries(rl_sub pico_stdlib hardware_spi)

include_directories(hx71708/)

target_sources(rl_sub PRIVATE
        hx71708/hx71708.c
)

# create map/bin/hex file etc.
pico_add_extra_outputs(rl_sub)

pico_enable_stdio_usb(rl_sub 1)
pico_enable_stdio_uart(rl_sub 0)

# add url via pico_set_program_url
#example_auto_set_url(rl_sub)
