cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(bms_firmware)

target_sources(app PRIVATE
    src/main.c
    src/uart/uart.c
    src/protocol/protocol.c
    src/monitor/init/init.c
    src/monitor/voltage/voltage.c
)

target_include_directories(app PRIVATE
    src
    src/uart
    src/protocol
    src/monitor/init
    src/monitor/register_maps
    src/monitor/voltage
)
