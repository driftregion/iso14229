This example is written in conjuction with the [Porting Guide](docs/porting_guide.md).
The project structure was adapted from Kristian Klein-Wengel's excellent tutorial: ["STM32 without CubeIDE"](https://kleinembedded.com/stm32-without-cubeide-part-4-cmake-fpu-and-stm32-libraries/).

# Hardware

- NUCLEO-G474RE development board
- [Waveshare SN65HVD230 CAN Board](https://www.waveshare.com/sn65hvd230-can-board.htm)


# Software

Download the CMSIS and HAL submodules,
```sh
cd vendor/STM32CubeG4
git submodule update --init \
Drivers/CMSIS/Device/ST/STM32G4xx \
Drivers/STM32G4xx_HAL_Driver \
Drivers/BSP/STM32G4xx_Nucleo
```

build the project,
```sh
cmake -Bbuild -DCMAKE_TOOLCHAIN_FILE=cmake/stm32g474re.cmake  
cmake --build build 
```

and flash
```sh
cmake --build build --target flash
```

