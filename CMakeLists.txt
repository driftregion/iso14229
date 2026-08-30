cmake_minimum_required(VERSION 3.15)
project(iso14229)

# Options for each transport layer variant
option(BUILD_UDS_TP_ISOTP_C "Build ISO-TP C variant" OFF)
option(BUILD_UDS_TP_ISOTP_C_SOCKETCAN "Build ISO-TP C SocketCAN variant" OFF)
option(BUILD_UDS_TP_ISOTP_SOCK "Build ISO-TP socket variant" ON)
option(BUILD_UDS_TP_ISOTP_MOCK "Build ISO-TP mock variant" OFF)
option(BUILD_UDS_TP_DOIP "Build DoIP (ISO 13400) variant" OFF)

# Populate VARIANT_LIST based on enabled options
set(VARIANT_LIST)
if(BUILD_UDS_TP_ISOTP_C)
    list(APPEND VARIANT_LIST UDS_TP_ISOTP_C)
endif()
if(BUILD_UDS_TP_ISOTP_C_SOCKETCAN)
    list(APPEND VARIANT_LIST UDS_TP_ISOTP_C_SOCKETCAN)
endif()
if(BUILD_UDS_TP_ISOTP_SOCK)
    list(APPEND VARIANT_LIST UDS_TP_ISOTP_SOCK)
endif()
if(BUILD_UDS_TP_ISOTP_MOCK)
    list(APPEND VARIANT_LIST UDS_TP_ISOTP_MOCK)
endif()
if(BUILD_UDS_TP_DOIP)
    list(APPEND VARIANT_LIST UDS_TP_DOIP)
endif()

# Create a library target for each enabled variant
foreach(VARIANT IN LISTS VARIANT_LIST)
    # Remove UDS_TP_ prefix and convert to lowercase
    string(REPLACE "UDS_TP_" "" VARIANT_SUFFIX ${VARIANT})
    string(TOLOWER ${VARIANT_SUFFIX} VARIANT_SUFFIX_LOWER)

    # Define target name
    set(TARGET_NAME "iso14229_${VARIANT_SUFFIX_LOWER}")

    # Create library target
    add_library(${TARGET_NAME} iso14229.c)

    # Set compile definition for this variant
    target_compile_definitions(${TARGET_NAME} PRIVATE ${VARIANT})

    # When building DoIP, also compile the transport instances (TCP/UDP)
    if (VARIANT STREQUAL "UDS_TP_DOIP")
        target_sources(${TARGET_NAME} PRIVATE
            src/tp/doip/doip_tp_tcp.c
            src/tp/doip/doip_tp_udp.c
        )
    endif()

    message(STATUS "Configured target: ${TARGET_NAME} with variant: ${VARIANT}")
endforeach()


