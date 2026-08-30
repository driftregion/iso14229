set(STM32CUBEG4_DIR "${CMAKE_SOURCE_DIR}/vendor/STM32CubeG4")
set(HAL_ROOT_DIR "${STM32CUBEG4_DIR}/Drivers/STM32G4xx_HAL_Driver")
set(HAL_SOURCE_DIR "${HAL_ROOT_DIR}/Src")
set(HAL_INCLUDE_DIR "${HAL_ROOT_DIR}/Inc")

set(HAL_SOURCES
    "${HAL_SOURCE_DIR}/stm32g4xx_hal.c"
    "${HAL_SOURCE_DIR}/stm32g4xx_hal_rcc.c"
    "${HAL_SOURCE_DIR}/stm32g4xx_hal_cortex.c"
    "${HAL_SOURCE_DIR}/stm32g4xx_hal_exti.c"
    "${HAL_SOURCE_DIR}/stm32g4xx_hal_gpio.c"
    "${HAL_SOURCE_DIR}/stm32g4xx_hal_pwr.c"
    "${HAL_SOURCE_DIR}/stm32g4xx_hal_pwr_ex.c"
    "${HAL_SOURCE_DIR}/stm32g4xx_hal_uart.c"
    "${HAL_SOURCE_DIR}/stm32g4xx_hal_uart_ex.c"
    "${HAL_SOURCE_DIR}/stm32g4xx_hal_dma.c"
    "${HAL_SOURCE_DIR}/stm32g4xx_hal_fdcan.c"
    "${STM32CUBEG4_DIR}/Drivers/BSP/STM32G4xx_Nucleo/stm32g4xx_nucleo.c"
    )

add_library(stm32cubeg4 STATIC ${HAL_SOURCES})

target_include_directories(stm32cubeg4 PUBLIC
    ${STM32CUBEG4_DIR}/Drivers/CMSIS/Core/Include
    ${STM32CUBEG4_DIR}/Drivers/CMSIS/Device/ST/STM32G4xx/Include
    ${STM32CUBEG4_DIR}/Drivers/BSP/STM32G4xx_Nucleo/
    ${HAL_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/core)
