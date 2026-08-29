cmake_minimum_required(VERSION 3.15)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(stm32g474_iso14229_server
        LANGUAGES C ASM)

add_compile_definitions(
    USE_HAL_DRIVER
)
add_compile_options(-g3 -O0) # Global compile flags

set(SOURCES
    main.c
    core/syscalls.c
    core/sysmem.c
    core/startup_stm32g474retx.s
    core/system_stm32g4xx.c
    core/stm32g4xx_it.c
    core/stm32g4xx_hal_msp.c
    vendor/STM32CubeG4/Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_exti.c
    vendor/STM32CubeG4/Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_pwr.c
    vendor/STM32CubeG4/Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_pwr_ex.c
    vendor/STM32CubeG4/Drivers/BSP/STM32G4xx_Nucleo/stm32g4xx_nucleo.c
    )

add_executable(iso14229_server ${SOURCES})

set_target_properties(iso14229_server PROPERTIES OUTPUT_NAME "iso14229_server.elf")

target_include_directories(iso14229_server PRIVATE 
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/core
    ${CMAKE_SOURCE_DIR}/vendor/STM32CubeG4/Drivers/STM32G4xx_HAL_Driver/Inc
    ${CMAKE_SOURCE_DIR}/vendor/STM32CubeG4/Drivers/BSP/STM32G4xx_Nucleo/
    )

target_compile_options(iso14229_server PRIVATE
    --specs=nano.specs)

target_link_options(iso14229_server PRIVATE
    -T ${CMAKE_SOURCE_DIR}/core/STM32G474RETX_FLASH.ld
    -u _printf_float)

include(cmake/stm32cubeg4.cmake)
target_link_libraries(iso14229_server stm32cubeg4)

set(PROGRAMMER pyocd)

add_custom_target(flash
    COMMAND ${PROGRAMMER} load $<TARGET_FILE:iso14229_server>
    DEPENDS iso14229_server
    VERBATIM)
