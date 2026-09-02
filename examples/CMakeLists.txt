function(isotpc_add_example example_name)
    add_library(${example_name} OBJECT ${example_name}.c)
    target_include_directories(${example_name} PRIVATE ${PROJECT_SOURCE_DIR})
    target_compile_definitions(${example_name} PRIVATE ${ARGN})

    if (MSVC)
        target_compile_options(${example_name} PRIVATE /W4 /WX)
    else()
        target_compile_options(${example_name} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
endfunction()

isotpc_add_example(isotp_example_polling
    ISO_TP_MAX_CAN_FRAME_SIZE=8
)

isotpc_add_example(isotp_example_streaming
    ISO_TP_MAX_CAN_FRAME_SIZE=8
    ISO_TP_ENABLE_STREAMING
)

isotpc_add_example(isotp_example_callbacks
    ISO_TP_MAX_CAN_FRAME_SIZE=8
    ISO_TP_TRANSMIT_COMPLETE_CALLBACK
    ISO_TP_RECEIVE_COMPLETE_CALLBACK
)

isotpc_add_example(isotp_example_can_fd
    ISO_TP_MAX_CAN_FRAME_SIZE=64
    ISO_TP_DEFAULT_TX_DL=64
    ISO_TP_USER_SEND_CAN_FLAGS
    ISO_TP_CAN_FD_USE_BRS
    ISO_TP_USER_SEND_CAN_ARG
)

add_custom_target(
    isotpc_examples
    DEPENDS
        isotp_example_polling
        isotp_example_streaming
        isotp_example_callbacks
        isotp_example_can_fd
)
