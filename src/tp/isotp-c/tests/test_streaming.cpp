#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#ifdef isotpc_USE_INCLUDE_DIR
    #include "isotp_c/isotp.h"
#else
    #include "isotp.h"
#endif // isotpc_USE_INCLUDE_DIR

#include "mocks/isotp_user_mock.hpp"

#define TEST_OUTPUT_CAPACITY 8192u
#define TEST_CHUNK_CAPACITY 1024u

using IsoTpMockCanFrame = struct {
        uint32_t arbitration_id;
        uint8_t  data[ISO_TP_MAX_CAN_FRAME_SIZE];
        uint8_t  size;
};

static std::vector<IsoTpMockCanFrame> g_can_frames;
static std::vector<std::string>       g_debug_messages;
static uint32_t                       g_current_time_us;
static size_t                         g_rx_callback_count;
static std::vector<uint8_t>           g_rx_callback_data;
static void*                          g_rx_callback_link;
static void*                          g_rx_callback_arg;

static void                           reset_platform_state(void) {
    g_can_frames.clear();
    g_debug_messages.clear();
    g_current_time_us   = 0;
    g_rx_callback_count = 0;
    g_rx_callback_data.clear();
    g_rx_callback_link = nullptr;
    g_rx_callback_arg  = nullptr;
}

static int capture_can_frame(uint32_t arbitration_id, const uint8_t* data, uint8_t size, uint8_t, void*) {
    IsoTpMockCanFrame frame = {};
    frame.arbitration_id    = arbitration_id;
    frame.size              = size;
    std::memcpy(frame.data, data, size);
    g_can_frames.push_back(frame);
    return ISOTP_RET_OK;
}

static size_t                   isotp_mock_can_frame_count(void) { return g_can_frames.size(); }
static const IsoTpMockCanFrame* isotp_mock_can_frame(size_t index) { return index < g_can_frames.size() ? &g_can_frames[index] : nullptr; }
static size_t                   isotp_mock_debug_count(void) { return g_debug_messages.size(); }
static void                     isotp_mock_set_time_us(uint32_t time_us) { g_current_time_us = time_us; }
static void                     isotp_mock_advance_time_us(uint32_t elapsed_us) { g_current_time_us += elapsed_us; }
static void                     isotp_mock_rx_done_cb(void* link, const uint8_t* data, uint32_t size, void* argument) {
    ++g_rx_callback_count;
    g_rx_callback_link = link;
    g_rx_callback_arg  = argument;
    g_rx_callback_data.assign(data, data + size);
}
static size_t         isotp_mock_rx_callback_count(void) { return g_rx_callback_count; }
static const uint8_t* isotp_mock_rx_callback_data(void) { return g_rx_callback_data.data(); }
static uint32_t       isotp_mock_rx_callback_size(void) { return static_cast<uint32_t>(g_rx_callback_data.size()); }
static void*          isotp_mock_rx_callback_link(void) { return g_rx_callback_link; }
static void*          isotp_mock_rx_callback_arg(void) { return g_rx_callback_arg; }

class StreamingTest: public testing::Test {
    protected:
        void SetUp() override {
            reset_platform_state();
            isotp_set_user_mock(&user_);
            ON_CALL(user_, get_us()).WillByDefault([] { return g_current_time_us; });
            ON_CALL(user_, send_can(testing::_, testing::_, testing::_, testing::_, testing::_)).WillByDefault(capture_can_frame);
            ON_CALL(user_, debug(testing::_)).WillByDefault([](const std::string& message) { g_debug_messages.push_back(message); });
        }

        void                             TearDown() override { isotp_set_user_mock(nullptr); }

        testing::NiceMock<IsoTpUserMock> user_;
};

using testlink_t = struct testlink_t {
        IsoTpLink               link;
        std::array<uint8_t, 8>  send_buffer{};
        std::array<uint8_t, 64> receive_buffer{};
};

using streamresult_t = struct streamresult_t {
        std::array<uint8_t, TEST_OUTPUT_CAPACITY> output{};
        uint32_t                                  output_size;
        std::array<uint32_t, TEST_CHUNK_CAPACITY> chunk_sizes{};
        size_t                                    chunk_count;
        size_t                                    complete_count;
};

static void fill_payload(uint8_t* payload, uint32_t size) {
    for (uint32_t index = 0; index < size; ++index) { payload[index] = (uint8_t)((index * 37u + 11u) & 0xffu); }
}

static void init_test_link(testlink_t* test_link, uint32_t receive_buffer_size) {
    reset_platform_state();
    *test_link = {};
    isotp_init_link(&test_link->link, 0x731, test_link->send_buffer.data(), sizeof(test_link->send_buffer), test_link->receive_buffer.data(),
                    receive_buffer_size);
}

static uint32_t inject_first_frame(IsoTpLink* link, const uint8_t* payload, uint32_t payload_size) {
    std::array<uint8_t, 8> frame{};
    uint32_t               data_size;

    if (payload_size <= 4095u) {
        frame[0]  = (uint8_t)(0x10u | (payload_size >> 8));
        frame[1]  = (uint8_t)payload_size;
        data_size = 6;
        (void)memcpy(frame.data() + 2, payload, data_size);
    } else {
        frame[0]  = 0x10;
        frame[1]  = 0;
        frame[2]  = (uint8_t)(payload_size >> 24);
        frame[3]  = (uint8_t)(payload_size >> 16);
        frame[4]  = (uint8_t)(payload_size >> 8);
        frame[5]  = (uint8_t)payload_size;
        data_size = 2;
        (void)memcpy(frame.data() + 6, payload, data_size);
    }

    isotp_on_can_message(link, frame.data(), sizeof(frame));
    return data_size;
}

static void inject_consecutive_frame(IsoTpLink* link, const uint8_t* payload, uint32_t payload_size, uint32_t* offset, uint8_t* sequence_number) {
    std::array<uint8_t, 8> frame{};
    uint32_t               data_size = payload_size - *offset;

    if (data_size > 7u) { data_size = 7; }
    frame[0] = (uint8_t)(0x20u | *sequence_number);
    (void)memcpy(frame.data() + 1, payload + *offset, data_size);
    isotp_on_can_message(link, frame.data(), (uint8_t)(data_size + 1u));

    *offset += data_size;
    *sequence_number = (uint8_t)((*sequence_number + 1u) & 0x0fu);
}

static void drain_available_chunks(IsoTpLink* link, streamresult_t* result) {
    while (link->receive_status == ISOTP_RECEIVE_STATUS_FULL) {
        uint32_t chunk_size  = 0;
        bool     is_complete = false;
        int      return_code;

        EXPECT_TRUE(result->chunk_count < TEST_CHUNK_CAPACITY);
        EXPECT_TRUE(result->output_size < TEST_OUTPUT_CAPACITY);

        return_code =
            isotp_receive_streaming(link, result->output.data() + result->output_size, TEST_OUTPUT_CAPACITY - result->output_size, &chunk_size, &is_complete);
        EXPECT_TRUE(return_code == ISOTP_RET_OK);
        result->chunk_sizes[result->chunk_count++] = chunk_size;
        result->output_size += chunk_size;
        if (is_complete) { ++result->complete_count; }
    }
}

static void run_stream_transfer(uint32_t payload_size, uint32_t receive_buffer_size, streamresult_t* result) {
    testlink_t                                test_link;
    std::array<uint8_t, TEST_OUTPUT_CAPACITY> payload{};
    uint32_t                                  offset;
    uint8_t                                   sequence_number = 1;

    EXPECT_TRUE(payload_size <= sizeof(payload));
    EXPECT_TRUE(receive_buffer_size <= sizeof(test_link.receive_buffer));

    fill_payload(payload.data(), payload_size);
    *result = {};
    init_test_link(&test_link, receive_buffer_size);

    offset = inject_first_frame(&test_link.link, payload.data(), payload_size);
    drain_available_chunks(&test_link.link, result);

    while (offset < payload_size) {
        EXPECT_TRUE(test_link.link.receive_status == ISOTP_RECEIVE_STATUS_INPROGRESS);
        inject_consecutive_frame(&test_link.link, payload.data(), payload_size, &offset, &sequence_number);
        drain_available_chunks(&test_link.link, result);
    }

    EXPECT_TRUE(result->output_size == payload_size);
    EXPECT_TRUE(memcmp(result->output.data(), payload.data(), payload_size) == 0);
    EXPECT_TRUE(result->complete_count == 1u);
    EXPECT_TRUE(test_link.link.receive_status == ISOTP_RECEIVE_STATUS_IDLE);
}

static void check_flow_control_frames(uint8_t expected_block_size, size_t expected_count) {
    EXPECT_TRUE(isotp_mock_can_frame_count() == expected_count);
    for (size_t index = 0; index < expected_count; ++index) {
        const IsoTpMockCanFrame* frame = isotp_mock_can_frame(index);

        EXPECT_TRUE(frame != nullptr);
        EXPECT_TRUE(frame->arbitration_id == 0x731u);
        EXPECT_TRUE(frame->size == 8u);
        EXPECT_TRUE((frame->data[0] >> 4) == ISOTP_PCI_TYPE_FLOW_CONTROL_FRAME);
        EXPECT_TRUE((frame->data[0] & 0x0fu) == PCI_FLOW_STATUS_CONTINUE);
        EXPECT_TRUE(frame->data[1] == expected_block_size);
    }
}

TEST_F(StreamingTest, ReceivesSingleFrame) {
    testlink_t             test_link;
    std::array<uint8_t, 4> frame{0x03, 0xa1, 0xb2, 0xc3};
    std::array<uint8_t, 8> output{};
    uint32_t               output_size = 0;
    bool                   is_complete = false;

    init_test_link(&test_link, sizeof(test_link.receive_buffer));
    isotp_on_can_message(&test_link.link, frame.data(), sizeof(frame));

    EXPECT_TRUE(isotp_receive_streaming(&test_link.link, output.data(), sizeof(output), &output_size, &is_complete) == ISOTP_RET_OK);
    EXPECT_TRUE(output_size == 3u);
    EXPECT_TRUE(is_complete);
    EXPECT_TRUE(memcmp(output.data(), frame.data() + 1, output_size) == 0);
    EXPECT_TRUE(isotp_mock_can_frame_count() == 0u);
}

TEST_F(StreamingTest, ReceivesMultiFrameThatFitsBuffer) {
    testlink_t              test_link;
    std::array<uint8_t, 20> payload{};
    std::array<uint8_t, 20> output{};
    uint32_t                output_size = 0;
    uint32_t                offset;
    uint8_t                 sequence_number = 1;
    bool                    is_complete     = false;

    fill_payload(payload.data(), sizeof(payload));
    init_test_link(&test_link, 32);
    offset = inject_first_frame(&test_link.link, payload.data(), sizeof(payload));
    check_flow_control_frames(ISO_TP_DEFAULT_BLOCK_SIZE, 1);

    while (offset < sizeof(payload)) { inject_consecutive_frame(&test_link.link, payload.data(), sizeof(payload), &offset, &sequence_number); }

    EXPECT_TRUE(isotp_receive_streaming(&test_link.link, output.data(), sizeof(output), &output_size, &is_complete) == ISOTP_RET_OK);
    EXPECT_TRUE(output_size == sizeof(payload));
    EXPECT_TRUE(is_complete);
    EXPECT_TRUE(memcmp(output.data(), payload.data(), sizeof(payload)) == 0);
}

TEST_F(StreamingTest, PreservesShortChunkBoundaries) {
    streamresult_t result;

    run_stream_transfer(25, 8, &result);
    EXPECT_TRUE(result.chunk_count == 4u);
    EXPECT_TRUE(result.chunk_sizes[0] == 8u);
    EXPECT_TRUE(result.chunk_sizes[1] == 8u);
    EXPECT_TRUE(result.chunk_sizes[2] == 8u);
    EXPECT_TRUE(result.chunk_sizes[3] == 1u);
    check_flow_control_frames(1, 3);
}

TEST_F(StreamingTest, SupportsOneByteReceiveBuffer) {
    streamresult_t result;

    run_stream_transfer(20, 1, &result);
    EXPECT_TRUE(result.chunk_count == 20u);
    for (size_t index = 0; index < result.chunk_count; ++index) { EXPECT_TRUE(result.chunk_sizes[index] == 1u); }
    check_flow_control_frames(1, 2);
}

TEST_F(StreamingTest, ReceivesIsoTp2016LongMessage) {
    streamresult_t result;

    run_stream_transfer(4100, 13, &result);
    EXPECT_TRUE(result.chunk_count == 316u);
    EXPECT_TRUE(result.chunk_sizes[0] == 13u);
    EXPECT_TRUE(result.chunk_sizes[result.chunk_count - 1u] == 5u);
    check_flow_control_frames(1, 586);
}

TEST_F(StreamingTest, DoesNotConsumeChunkWhenDestinationIsTooSmall) {
    testlink_t              test_link;
    std::array<uint8_t, 20> payload{};
    std::array<uint8_t, 8>  output{};
    uint32_t                output_size = 1234;
    uint32_t                offset;
    uint8_t                 sequence_number = 1;
    bool                    is_complete     = true;

    fill_payload(payload.data(), sizeof(payload));
    init_test_link(&test_link, 8);
    offset = inject_first_frame(&test_link.link, payload.data(), sizeof(payload));
    inject_consecutive_frame(&test_link.link, payload.data(), sizeof(payload), &offset, &sequence_number);
    EXPECT_TRUE(test_link.link.receive_status == ISOTP_RECEIVE_STATUS_FULL);

    EXPECT_TRUE(isotp_receive_streaming(&test_link.link, output.data(), 7, &output_size, &is_complete) == ISOTP_RET_NOSPACE);
    EXPECT_TRUE(test_link.link.receive_status == ISOTP_RECEIVE_STATUS_FULL);
    EXPECT_TRUE(output_size == 1234u);
    EXPECT_TRUE(is_complete);
    EXPECT_TRUE(isotp_mock_can_frame_count() == 1u);

    EXPECT_TRUE(isotp_receive_streaming(&test_link.link, output.data(), sizeof(output), &output_size, &is_complete) == ISOTP_RET_OK);
    EXPECT_TRUE(output_size == sizeof(output));
    EXPECT_TRUE(!is_complete);
    EXPECT_TRUE(memcmp(output.data(), payload.data(), sizeof(output)) == 0);
    EXPECT_TRUE(isotp_mock_can_frame_count() == 2u);
}

TEST_F(StreamingTest, RejectsInvalidArgumentsAndLegacyReceive) {
    testlink_t              test_link;
    std::array<uint8_t, 20> payload{};
    std::array<uint8_t, 32> output{};
    uint32_t                output_size = 0;
    uint32_t                offset;
    uint8_t                 sequence_number = 1;
    bool                    is_complete     = false;

    fill_payload(payload.data(), sizeof(payload));
    init_test_link(&test_link, 8);

    EXPECT_TRUE(isotp_receive_streaming(&test_link.link, output.data(), sizeof(output), &output_size, &is_complete) == ISOTP_RET_NO_DATA);
    EXPECT_TRUE(isotp_receive_streaming(nullptr, output.data(), sizeof(output), &output_size, &is_complete) == ISOTP_RET_ERROR);
    EXPECT_TRUE(isotp_receive_streaming(&test_link.link, nullptr, sizeof(output), &output_size, &is_complete) == ISOTP_RET_ERROR);
    EXPECT_TRUE(isotp_receive_streaming(&test_link.link, output.data(), sizeof(output), nullptr, &is_complete) == ISOTP_RET_ERROR);
    EXPECT_TRUE(isotp_receive_streaming(&test_link.link, output.data(), sizeof(output), &output_size, nullptr) == ISOTP_RET_ERROR);

    offset = inject_first_frame(&test_link.link, payload.data(), sizeof(payload));
    inject_consecutive_frame(&test_link.link, payload.data(), sizeof(payload), &offset, &sequence_number);
    EXPECT_TRUE(isotp_receive(&test_link.link, output.data(), sizeof(output), &output_size) == ISOTP_RET_ERROR);
    EXPECT_TRUE(test_link.link.receive_status == ISOTP_RECEIVE_STATUS_FULL);
}

TEST_F(StreamingTest, CoexistsWithReceiveCallback) {
    testlink_t              test_link;
    streamresult_t          result{};
    std::array<uint8_t, 4>  single_frame{0x03, 0xde, 0xad, 0x42};
    std::array<uint8_t, 20> payload{};
    uint32_t                offset;
    uint8_t                 sequence_number   = 1;
    int32_t                 callback_argument = 17;

    fill_payload(payload.data(), sizeof(payload));
    init_test_link(&test_link, 8);
    isotp_set_rx_done_cb(&test_link.link, isotp_mock_rx_done_cb, &callback_argument);

    isotp_on_can_message(&test_link.link, single_frame.data(), sizeof(single_frame));
    EXPECT_TRUE(isotp_mock_rx_callback_count() == 1u);
    EXPECT_TRUE(isotp_mock_rx_callback_size() == 3u);
    EXPECT_TRUE(memcmp(isotp_mock_rx_callback_data(), single_frame.data() + 1, 3) == 0);
    EXPECT_TRUE(isotp_mock_rx_callback_link() == &test_link.link);
    EXPECT_TRUE(isotp_mock_rx_callback_arg() == &callback_argument);

    offset = inject_first_frame(&test_link.link, payload.data(), sizeof(payload));
    inject_consecutive_frame(&test_link.link, payload.data(), sizeof(payload), &offset, &sequence_number);
    EXPECT_TRUE(isotp_mock_rx_callback_count() == 1u);
    drain_available_chunks(&test_link.link, &result);

    while (offset < sizeof(payload)) {
        inject_consecutive_frame(&test_link.link, payload.data(), sizeof(payload), &offset, &sequence_number);
        drain_available_chunks(&test_link.link, &result);
    }

    EXPECT_TRUE(result.output_size == sizeof(payload));
    EXPECT_TRUE(memcmp(result.output.data(), payload.data(), sizeof(payload)) == 0);
    EXPECT_TRUE(result.complete_count == 1u);
    EXPECT_TRUE(isotp_mock_rx_callback_count() == 1u);
}

TEST_F(StreamingTest, RejectsZeroSizedReceiveBuffer) {
    testlink_t               test_link;
    std::array<uint8_t, 20>  payload{};
    const IsoTpMockCanFrame* flow_control;

    fill_payload(payload.data(), sizeof(payload));
    init_test_link(&test_link, 0);
    (void)inject_first_frame(&test_link.link, payload.data(), sizeof(payload));

    EXPECT_TRUE(test_link.link.receive_status == ISOTP_RECEIVE_STATUS_IDLE);
    EXPECT_TRUE(test_link.link.receive_protocol_result == ISOTP_PROTOCOL_RESULT_BUFFER_OVFLW);
    EXPECT_TRUE(isotp_mock_can_frame_count() == 1u);
    flow_control = isotp_mock_can_frame(0);
    EXPECT_TRUE(flow_control != nullptr);
    EXPECT_TRUE((flow_control->data[0] >> 4) == ISOTP_PCI_TYPE_FLOW_CONTROL_FRAME);
    EXPECT_TRUE((flow_control->data[0] & 0x0fu) == PCI_FLOW_STATUS_OVERFLOW);
    EXPECT_TRUE(flow_control->data[1] == 0u);
    EXPECT_TRUE(isotp_mock_debug_count() == 1u);
}

TEST_F(StreamingTest, AbortsOnWrongSequenceNumber) {
    testlink_t              test_link;
    std::array<uint8_t, 20> payload{};
    std::array<uint8_t, 8>  wrong_frame{0x22};

    fill_payload(payload.data(), sizeof(payload));
    (void)memcpy(wrong_frame.data() + 1, payload.data() + 6, 7);
    init_test_link(&test_link, 8);
    (void)inject_first_frame(&test_link.link, payload.data(), sizeof(payload));
    isotp_on_can_message(&test_link.link, wrong_frame.data(), sizeof(wrong_frame));

    EXPECT_TRUE(test_link.link.receive_status == ISOTP_RECEIVE_STATUS_IDLE);
    EXPECT_TRUE(test_link.link.receive_protocol_result == ISOTP_PROTOCOL_RESULT_WRONG_SN);
}

TEST_F(StreamingTest, AbortsOnReceiveTimeout) {
    testlink_t              test_link;
    std::array<uint8_t, 20> payload{};

    fill_payload(payload.data(), sizeof(payload));
    init_test_link(&test_link, 16);
    isotp_mock_set_time_us(100);
    (void)inject_first_frame(&test_link.link, payload.data(), sizeof(payload));
    EXPECT_TRUE(test_link.link.receive_status == ISOTP_RECEIVE_STATUS_INPROGRESS);

    isotp_mock_advance_time_us(ISO_TP_DEFAULT_RESPONSE_TIMEOUT_US + 1u);
    isotp_poll(&test_link.link);
    EXPECT_TRUE(test_link.link.receive_status == ISOTP_RECEIVE_STATUS_IDLE);
    EXPECT_TRUE(test_link.link.receive_protocol_result == ISOTP_PROTOCOL_RESULT_TIMEOUT_CR);
}

TEST_F(StreamingTest, RejectsMalformedFrames) {
    testlink_t              test_link{};
    std::array<uint8_t, 20> payload{};
    std::array<uint8_t, 2>  short_single_frame{0x03, 0xaa};
    std::array<uint8_t, 7>  short_first_frame{0x10, 0x14};
    std::array<uint8_t, 8>  invalid_first_frame{0x10, 0x07};
    std::array<uint8_t, 2>  short_consecutive_frame{0x21, 0xaa};

    EXPECT_CALL(user_, debug(testing::_)).Times(4);
    fill_payload(payload.data(), sizeof(payload));

    init_test_link(&test_link, 16);
    isotp_on_can_message(&test_link.link, short_single_frame.data(), sizeof(short_single_frame));
    EXPECT_TRUE(test_link.link.receive_status == ISOTP_RECEIVE_STATUS_IDLE);
    EXPECT_TRUE(isotp_mock_debug_count() == 1u);

    init_test_link(&test_link, 16);
    isotp_on_can_message(&test_link.link, short_first_frame.data(), sizeof(short_first_frame));
    EXPECT_TRUE(test_link.link.receive_status == ISOTP_RECEIVE_STATUS_IDLE);
    EXPECT_TRUE(isotp_mock_debug_count() == 1u);

    init_test_link(&test_link, 16);
    isotp_on_can_message(&test_link.link, invalid_first_frame.data(), sizeof(invalid_first_frame));
    EXPECT_TRUE(test_link.link.receive_status == ISOTP_RECEIVE_STATUS_IDLE);
    EXPECT_TRUE(isotp_mock_debug_count() == 1u);

    init_test_link(&test_link, 16);
    (void)inject_first_frame(&test_link.link, payload.data(), sizeof(payload));
    isotp_on_can_message(&test_link.link, short_consecutive_frame.data(), sizeof(short_consecutive_frame));
    EXPECT_TRUE(test_link.link.receive_status == ISOTP_RECEIVE_STATUS_INPROGRESS);
    EXPECT_TRUE(isotp_mock_debug_count() == 1u);
}
