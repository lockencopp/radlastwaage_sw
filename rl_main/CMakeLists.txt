project(rl_main)

add_executable(rl_main
    rl_main.c
    st7735-testfuncs/tst_funcs.c
)

# Pull in basic dependencies
target_link_libraries(rl_main PUBLIC pico_stdlib)
target_link_libraries(rl_main PUBLIC lib-st7735)
target_link_libraries(rl_main PUBLIC hardware_spi)

# create map/bin/hex file etc.
pico_add_extra_outputs(rl_main)

pico_enable_stdio_usb(rl_main 1)
pico_enable_stdio_uart(rl_main 0)

# add url via pico_set_program_url
#example_auto_set_url(spi_master)

# library configuration (override if necessary)
set(SPI_TFT_PORT "spi1" CACHE STRING "TFT spi port-name")
set(SPI_TFT_CS   "13"   CACHE STRING "TFT CS  pin number")
set(SPI_TFT_DC   "9"    CACHE STRING "TFT DC  pin number")
set(SPI_TFT_RST  "8"    CACHE STRING "TFT RST pin number")
set(SPI_TFT_RX   "12"   CACHE STRING "TFT RX  pin number")
set(SPI_TFT_TX   "11"   CACHE STRING "TFT TX  pin number")
set(SPI_TFT_SCK  "10"   CACHE STRING "TFT SCK pin number")
set(TFT_OPTIONS TFT_ENABLE_RED TFT_ENABLE_RESET TFT_ENABLE_TEXT TFT_ENABLE_SHAPES
    TFT_ENABLE_ROTATE TFT_ENABLE_SCROLL
CACHE STRING "TFT options/functions")

# TFT options/functions. Complete list:
#  - TFT_ENABLE_BLACK
#  - TFT_ENABLE_GREEN
#  - TFT_ENABLE_RED
#  - TFT_ENABLE_GENERIC
#  - TFT_ENABLE_TEXT
#  - TFT_ENABLE_FONTS
#  - TFT_ENABLE_ALL
#  - TFT_ENABLE_RESET
#  - TFT_ENABLE_SCROLL
#  - TFT_ENABLE_SHAPES
#  - TFT_ENABLE_ROTATE
#  - TFT_ENABLE_BMP  (not implemented yet)

foreach(opt IN LISTS TFT_OPTIONS)
    message(">>> using: ${opt}")
    add_compile_definitions(${opt})
endforeach(opt)

add_compile_definitions(SPI_TFT_PORT=${SPI_TFT_PORT})
add_compile_definitions(SPI_TFT_CS=${SPI_TFT_CS})
add_compile_definitions(SPI_TFT_DC=${SPI_TFT_DC})
add_compile_definitions(SPI_TFT_RST=${SPI_TFT_RST})
add_compile_definitions(SPI_TFT_PORT=${SPI_TFT_PORT})
add_compile_definitions(SPI_TFT_RX=${SPI_TFT_RX})
add_compile_definitions(SPI_TFT_TX=${SPI_TFT_TX})
add_compile_definitions(SPI_TFT_SCK=${SPI_TFT_SCK})

# ENABLE_FONTS: set on command-line or select from
# VSCode-menu (see .vscode/cmake-variants.json)
if(ENABLE_FONTS)
    message(">>> Font test")
    add_compile_definitions(TFT_ENABLE_FONTS ENABLE_TEST9 ENABLE_TEST9A)
else()
    message(">>> Normal test")
    add_compile_definitions(ENABLE_TESTR ENABLE_TEST1 ENABLE_TEST1A
                            ENABLE_TEST2 ENABLE_TEST3 ENABLE_TEST4
                            ENABLE_TEST5 ENABLE_TEST6 ENABLE_TEST7)
endif()

# add preprocessor-constant DEBUG for Debug-builds
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_VERBOSE_MAKEFILE 1)
    add_compile_definitions(DEBUG)
else()
endif()

add_subdirectory(lib-st7735)
target_include_directories(rl_main PRIVATE
    st7735-testfuncs
)

target_compile_definitions(lib-st7735 PUBLIC
    PIN_TFT_RST=${SPI_TFT_RST}
    PIN_TFT_DC=${SPI_TFT_DC}
    PIN_TFT_CS=${SPI_TFT_CS}
)

include_directories(user_lib/)

target_sources(rl_main PRIVATE
    user_lib/display_helpers.c
)