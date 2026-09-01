###
# CROSS variable
# Use for cross-compilation.
# Uncommenting and setting this variable to the prefix of
# your cross compiler will allow you to cross compile this library.
###
#CROSS=powerpc-linux-gnu

###
# LANG variable
# Set this value according to the language
# you wish to compile against.
# Possible (legal) values:
#   - C [c]
#   - C++ [c++]
###
LANG := C

###
# STD variables.
# Do NOT set the STD variable.
# Instead, set the C/C++ STD variables 
# according to the standard you wish to use.
###
CSTD := gnu99
CPPSTD := c++0x

###
# OUTPUT_NAME variable.
# This variable contains the name of the output file (the .so).
###
LIB_NAME := libisotp.so
MAJOR_VER := 1
MINOR_VER := 9
REVISION := 1
OUTPUT_NAME := $(LIB_NAME).$(MAJOR_VER).$(MINOR_VER).$(REVISION)

###
# INSTALL_DIR variable.
# Set this variable to the location where the lib should be installed.
###
INSTALL_DIR := /usr/lib

###
# MAX_CAN_FRAME_SIZE variable.
# The maximum amount of data bytes a single CAN frame may carry.
# Set this to 8 for Classical CAN, or to one of the CAN FD frame lengths
# (12, 16, 20, 24, 32, 48, 64) to enable CAN FD support.
###
MAX_CAN_FRAME_SIZE ?= 8

###
# Compute compiler and language standard to use
# This section determines which compiler to use.
###
ifeq ($(subst ",,$(LANG)),C++)
	STD := -std=$(CPPSTD)
	ifeq ($(strip $(CROSS)),)
		COMP := g++
		AR := ar
	else
		COMP := $(CROSS)g++
		AR := $(CROSS)ar
	endif
endif

ifeq ($(subst ",,$(LANG)),C)
	STD := -std=$(CSTD)
	ifeq ($(strip $(CROSS)),)
		COMP := gcc
		AR := ar
	else
		COMP := $(CROSS)gcc
		AR := $(CROSS)ar
	endif
endif

###
# Allows use of CMake in Makefile.
# These options configure options in the code to ensure compatibility with CMake builds
###

CMAKE_BUILD_BY_DEFAULT ?= OFF

# Encapsulate headers in a separate include/isotp_c directory.
USE_INCLUDE_DIR ?= OFF
# Build a static archive instead of a shared library.
USE_STATIC_LIBRARY ?= OFF
# Enable position-independent code in the static build.
ENABLE_STATIC_LIBRARY_PIC ?= OFF
# Enable streaming of messages larger than the receiver's buffer.
ENABLE_STREAMING ?= OFF
# Pad CAN frames to their full size.
ENABLE_FRAME_PADDING ?= ON
CAN_FRAME_PAD_VALUE ?= 0xAA
ENABLE_CAN_SEND_ARG ?= OFF
ENABLE_CAN_SEND_FLAGS ?= OFF
ENABLE_CAN_FD_BRS ?= OFF
DEFAULT_TX_DL ?= $(MAX_CAN_FRAME_SIZE)
ENABLE_TRANSCEIVE_EVENTS ?= OFF
ENABLE_TRANSMIT_COMPLETE_CALLBACK ?= ON
ENABLE_RECEIVE_COMPLETE_CALLBACK ?= ON
NO_FORMATTED_ERRORS ?= OFF

BUILD_DIR ?= build
TEST_BUILD_DIR ?= build-tests
FUZZ_BUILD_DIR ?= build-fuzz
