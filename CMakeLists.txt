cmake_minimum_required(VERSION 3.10)

###
# Project definition
###
project(isotp LANGUAGES C VERSION 1.9.1 DESCRIPTION "A platform-agnostic ISOTP implementation in C for embedded devices.")

option(isotpc_USE_INCLUDE_DIR "Copy header files to separate include directory in current binary dir for better separation of header files to combat potential naming conflicts." OFF)
option(isotpc_STATIC_LIBRARY "Compile libisotpc as a static library, instead of a shared library." OFF)
option(isotpc_STATIC_LIBRARY_PIC "Make use of position independent code (PIC), when compiling as a static library (enabled automatically when building shared libraries)." OFF)
option(isotpc_PAD_CAN_FRAMES "Pad CAN frames to their full size." ON)
set(isotpc_CAN_FRAME_PAD_VALUE "0xAA" CACHE STRING "Padding byte value to be used in CAN frames if enabled")
option(isotpc_ENABLE_CAN_SEND_ARG "Adds an extra argument to isotp_user_send_can to better support multiple CAN interfaces." OFF)
option(isotpc_ENABLE_CAN_SEND_FLAGS "Adds a frame flags argument to isotp_user_send_can, telling the CAN driver whether to send a CAN FD frame." OFF)
option(isotpc_ENABLE_CAN_FD_BRS "Flag CAN FD frames for transmission using the data phase bit rate; requires isotpc_ENABLE_CAN_SEND_FLAGS." OFF)
set(isotpc_MAX_CAN_FRAME_SIZE "8" CACHE STRING "Maximum CAN frame data length (CAN_DL); 8 for Classical CAN, one of 12, 16, 20, 24, 32, 48, 64 to enable CAN FD.")
set(isotpc_DEFAULT_TX_DL "" CACHE STRING "CAN frame data length used for transmitted frames; defaults to isotpc_MAX_CAN_FRAME_SIZE.")
option(isotpc_ENABLE_TRANSCEIVE_EVENTS "Enable events/callbacks for transmission and reception complete." OFF)
option(isotpc_ENABLE_TRANSMIT_COMPLETE_CALLBACK "Enable transmit complete callback." ON) # These can be disabled separately, as they are controlled by the main event option
option(isotpc_ENABLE_RECEIVE_COMPLETE_CALLBACK "Enable receive complete callback." ON) # These can be disabled separately, as they are controlled by the main event option
option(isotpc_ENABLE_STREAMING "Enable chunked reception of messages larger than the receive buffer." OFF)
option(isotpc_NO_FORMATTED_ERRORS "Omit the formatted error messages, dropping the library's only dependency on snprintf." OFF)
option(isotpc_ENABLE_TESTING "Enable building of test suite." OFF)
option(isotpc_ENABLE_COVERAGE "Instrument the unit-test subjects for gcovr coverage reporting." OFF)
option(isotpc_ENABLE_FUZZING "Enable building of libFuzzer targets." OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (isotpc_STATIC_LIBRARY OR MSVC)
    add_library(isotp STATIC ${CMAKE_CURRENT_SOURCE_DIR}/isotp.c)
else()
    add_library(isotp SHARED ${CMAKE_CURRENT_SOURCE_DIR}/isotp.c)
endif()

###
# Strict building policies
###
if (MSVC)
    target_compile_options(
        isotp PRIVATE
        /W4
    )
else()
    target_compile_options(
        isotp PRIVATE
        -Werror
        -Wall
        -Wpedantic
        -Wextra
        -Wno-unknown-pragmas # ignore unknown pragmas, such as #pragma region
    )
endif()

###
# Enable position independent code (PIC) if required
###
if (NOT isotpc_STATIC_LIBRARY OR isotpc_STATIC_LIBRARY_PIC)
    # Building a shared library or a static library with PIC enabled
    target_compile_options(isotp PRIVATE -fPIC)
endif()

###
# Provide padding configuration
###
if (isotpc_PAD_CAN_FRAMES)
    target_compile_definitions(isotp PRIVATE -DISO_TP_FRAME_PADDING -DISO_TP_FRAME_PADDING_VALUE=${isotpc_CAN_FRAME_PAD_VALUE})
endif()

###
# Configure the CAN frame size (CAN FD support)
###
set(isotpc_VALID_CAN_FRAME_SIZES 8 12 16 20 24 32 48 64)
set_property(CACHE isotpc_MAX_CAN_FRAME_SIZE PROPERTY STRINGS ${isotpc_VALID_CAN_FRAME_SIZES})

if (NOT isotpc_MAX_CAN_FRAME_SIZE IN_LIST isotpc_VALID_CAN_FRAME_SIZES)
    message(FATAL_ERROR "[isotp-c] isotpc_MAX_CAN_FRAME_SIZE must be one of: ${isotpc_VALID_CAN_FRAME_SIZES} (got '${isotpc_MAX_CAN_FRAME_SIZE}')")
endif()

if (isotpc_DEFAULT_TX_DL STREQUAL "")
    set(isotpc_DEFAULT_TX_DL ${isotpc_MAX_CAN_FRAME_SIZE})
elseif (NOT isotpc_DEFAULT_TX_DL IN_LIST isotpc_VALID_CAN_FRAME_SIZES OR isotpc_DEFAULT_TX_DL GREATER isotpc_MAX_CAN_FRAME_SIZE)
    message(FATAL_ERROR "[isotp-c] isotpc_DEFAULT_TX_DL must be one of: ${isotpc_VALID_CAN_FRAME_SIZES} and must not exceed isotpc_MAX_CAN_FRAME_SIZE (got '${isotpc_DEFAULT_TX_DL}')")
endif()

if (NOT isotpc_MAX_CAN_FRAME_SIZE EQUAL 8)
    message(STATUS "[isotp-c] Enabling CAN FD support with a maximum frame size of ${isotpc_MAX_CAN_FRAME_SIZE} bytes")
endif()

# These definitions determine the size of the internal frame buffers, so they must
# be visible to consumers of the library, too.
target_compile_definitions(
    isotp PUBLIC
    -DISO_TP_MAX_CAN_FRAME_SIZE=${isotpc_MAX_CAN_FRAME_SIZE}
    -DISO_TP_DEFAULT_TX_DL=${isotpc_DEFAULT_TX_DL}
)

###
# Include additional arg in isotp_user_send_can if required
###
if (isotpc_ENABLE_CAN_SEND_ARG)
    target_compile_options(isotp PRIVATE -DISO_TP_USER_SEND_CAN_ARG)
endif()

###
# Drop the two formatted error messages, and with them the snprintf dependency
###
if (isotpc_NO_FORMATTED_ERRORS)
    target_compile_options(isotp PRIVATE -DISO_TP_NO_FORMATTED_ERRORS)
endif()

###
# Include the frame flags arg in isotp_user_send_can if required
###
if (isotpc_ENABLE_CAN_SEND_FLAGS)
    target_compile_options(isotp PRIVATE -DISO_TP_USER_SEND_CAN_FLAGS)

    if (isotpc_ENABLE_CAN_FD_BRS)
        target_compile_options(isotp PRIVATE -DISO_TP_CAN_FD_USE_BRS)
    endif()
elseif (isotpc_ENABLE_CAN_FD_BRS)
    message(WARNING "[isotp-c] isotpc_ENABLE_CAN_FD_BRS has no effect without isotpc_ENABLE_CAN_SEND_FLAGS")
endif()

###
# Add support for options added in #51
###
if (isotpc_ENABLE_TRANSCEIVE_EVENTS)
    message(STATUS "[isotp-c] Enabling ISOTP events; backwards compatibility may BREAK!")
    if (isotpc_ENABLE_TRANSMIT_COMPLETE_CALLBACK)
        target_compile_definitions(isotp PRIVATE -DISO_TP_TRANSMIT_COMPLETE_CALLBACK)
    endif()

    if (isotpc_ENABLE_RECEIVE_COMPLETE_CALLBACK)
        target_compile_definitions(isotp PRIVATE -DISO_TP_RECEIVE_COMPLETE_CALLBACK)
    endif()
endif()

if (isotpc_ENABLE_STREAMING)
    target_compile_definitions(isotp PUBLIC -DISO_TP_ENABLE_STREAMING)
endif()

###
# Check for debug builds
###
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(isotp PRIVATE -O0 -g) # don't optimise and add debugging symbols
else()
    target_compile_options(isotp PRIVATE -O2) # optimise code
    if (NOT isotpc_STATIC_LIBRARY AND NOT MSVC)
        target_link_libraries(isotp PRIVATE -s) # strip the library
    endif()
endif()

if (isotpc_USE_INCLUDE_DIR)
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/include/isotp_c)
    
    file(GLOB HEADERS FOLLOW_SYMLINKS ${CMAKE_CURRENT_SOURCE_DIR}/*.h)
    file(COPY ${HEADERS} DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/include/isotp_c/)

    target_include_directories(isotp PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/include)
    target_compile_definitions(
        isotp PUBLIC
        ISOTP_USE_INCLUDE_DIR=1
    )

else()
    target_include_directories(isotp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
endif()

###
# Add library aliases
###
add_library(isotpc ALIAS isotp)
add_library(isotp_c ALIAS isotp)
add_library(simon_cahill::isotp ALIAS isotp)
add_library(simon_cahill::isotpc ALIAS isotp)
add_library(simon_cahill::isotp_c ALIAS isotp)

###
# Enable testing
###
if (isotpc_ENABLE_TESTING)
    if (CMAKE_VERSION VERSION_LESS 3.16)
        message(FATAL_ERROR "[isotp-c] Unit tests require CMake 3.16 or newer")
    endif()

    enable_language(CXX)
    enable_testing()
    add_subdirectory(tests)
elseif (isotpc_ENABLE_COVERAGE)
    message(FATAL_ERROR "[isotp-c] isotpc_ENABLE_COVERAGE requires isotpc_ENABLE_TESTING")
endif()

if (isotpc_ENABLE_FUZZING)
    add_subdirectory(fuzz)
endif()
