#ifndef ISOTP_USER_MOCK_HPP
#define ISOTP_USER_MOCK_HPP

#include <cstdint>
#include <string>

#include <gmock/gmock.h>

class IsoTpUserMock {
   public:
        MOCK_METHOD(int, send_can, (std::uint32_t arbitration_id, const std::uint8_t* data, std::uint8_t size, std::uint8_t flags, void* argument));
        MOCK_METHOD(std::uint32_t, get_us, ());
        MOCK_METHOD(void, debug, (const std::string& message));
};

void isotp_set_user_mock(IsoTpUserMock* mock);

#endif
