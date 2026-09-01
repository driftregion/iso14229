if (CMAKE_VERSION VERSION_LESS 3.13)
    message(FATAL_ERROR "[isotp-c] Fuzzing requires CMake 3.13 or newer")
endif()

if (NOT CMAKE_C_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "[isotp-c] Fuzzing requires Clang with libFuzzer support")
endif()

include(CheckCSourceCompiles)

set(isotpc_FUZZ_SANITIZER_OPTIONS
    -fsanitize=fuzzer,address,undefined
    -fno-omit-frame-pointer
)

set(isotpc_FUZZ_SAVED_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS}")
set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer")
check_c_source_compiles(
    "
    #include <stddef.h>
    #include <stdint.h>
    int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
        (void)data;
        (void)size;
        return 0;
    }
    "
    isotpc_HAVE_LIBFUZZER_SANITIZERS
)
set(CMAKE_REQUIRED_FLAGS "${isotpc_FUZZ_SAVED_REQUIRED_FLAGS}")

if (NOT isotpc_HAVE_LIBFUZZER_SANITIZERS)
    message(FATAL_ERROR "[isotp-c] Fuzzing requires a Clang toolchain that supports -fsanitize=fuzzer,address,undefined")
endif()

function(isotpc_set_fuzz_warnings target)
    target_compile_options(${target} PRIVATE -Werror -Wall -Wpedantic -Wextra)
endfunction()

add_library(isotp_fuzz_receive_subject STATIC ${PROJECT_SOURCE_DIR}/isotp.c)
target_include_directories(isotp_fuzz_receive_subject PUBLIC ${PROJECT_SOURCE_DIR})
target_compile_definitions(isotp_fuzz_receive_subject PUBLIC ISO_TP_MAX_CAN_FRAME_SIZE=64)
target_compile_options(isotp_fuzz_receive_subject PRIVATE ${isotpc_FUZZ_SANITIZER_OPTIONS})
isotpc_set_fuzz_warnings(isotp_fuzz_receive_subject)
target_compile_options(isotp_fuzz_receive_subject PRIVATE -Wno-unknown-pragmas)

add_executable(isotp_fuzz_receive ${CMAKE_CURRENT_SOURCE_DIR}/isotp_receive_fuzzer.c)
target_include_directories(isotp_fuzz_receive PRIVATE ${PROJECT_SOURCE_DIR})
target_compile_options(isotp_fuzz_receive PRIVATE ${isotpc_FUZZ_SANITIZER_OPTIONS})
target_link_options(isotp_fuzz_receive PRIVATE ${isotpc_FUZZ_SANITIZER_OPTIONS})
target_link_libraries(isotp_fuzz_receive PRIVATE isotp_fuzz_receive_subject)
isotpc_set_fuzz_warnings(isotp_fuzz_receive)
