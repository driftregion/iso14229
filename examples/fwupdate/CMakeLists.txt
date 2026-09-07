cmake_minimum_required(VERSION 3.28.0)

find_package(Zephyr)
project(mcuboot_app)

test_sysbuild()

set(ISO14229_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../..)

target_sources(app PRIVATE
    src/main.c
    ${ISO14229_DIR}/iso14229.c
)
target_include_directories(app PRIVATE ${ISO14229_DIR})
target_compile_definitions(app PRIVATE
    UDS_SERVER_SEND_BUF_SIZE=300
    UDS_SERVER_RECV_BUF_SIZE=300
)
