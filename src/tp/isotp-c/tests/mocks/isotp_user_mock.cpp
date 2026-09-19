#include "mocks/isotp_user_mock.hpp"

#include <cstdarg>
#include <cstdio>
#include <string>

#ifdef isotpc_USE_INCLUDE_DIR
    #include "isotp_c/isotp.h"
#else
    #include "isotp.h"
#endif // isotpc_USE_INCLUDE_DIR

namespace {
IsoTpUserMock* active_mock = nullptr;
}

void isotp_set_user_mock(IsoTpUserMock* mock) { active_mock = mock; }

extern "C" int isotp_user_send_can(const std::uint32_t arbitration_id, const std::uint8_t* data, const std::uint8_t size
#ifdef ISO_TP_USER_SEND_CAN_FLAGS
                                    ,
                                    const std::uint8_t flags
#endif
#ifdef ISO_TP_USER_SEND_CAN_ARG
                                    ,
                                    void* argument
#endif
) {
    if (active_mock == nullptr) { return ISOTP_RET_ERROR; }

#ifndef ISO_TP_USER_SEND_CAN_FLAGS
    constexpr std::uint8_t flags = ISOTP_CAN_FRAME_FLAG_NONE;
#endif
#ifndef ISO_TP_USER_SEND_CAN_ARG
    void* argument = nullptr;
#endif

    return active_mock->send_can(arbitration_id, data, size, flags, argument);
}

extern "C" std::uint32_t isotp_user_get_us(void) {
    return active_mock == nullptr ? 0U : active_mock->get_us();
}

extern "C" void isotp_user_debug(const char* message, ...) {
    char formatted[ISOTP_MAX_ERROR_MSG_SIZE] = {};
    va_list arguments;

    va_start(arguments, message);
    (void)std::vsnprintf(formatted, sizeof(formatted), message, arguments);
    va_end(arguments);

    if (active_mock != nullptr) { active_mock->debug(std::string(formatted)); }
}
