#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include "src/iso14229.h"
#include <fuzzer/FuzzedDataProvider.h>
#include <vector>

#ifdef __cplusplus
extern "C" int LLVMFuzzerTestOneInput(const uint8_t *, size_t);
#else
int LLVMFuzzerTestOneInput(const uint8_t *, size_t);
#endif

static uint64_t g_time_now_us = 0;
static uint8_t client_recv_buf[UDS_TP_MTU];

static UDSErr_t fn(UDSServer_t *srv, UDSEvent_t ev, void *arg) {
    FuzzedDataProvider *fuzzed_data = (FuzzedDataProvider *)srv->fn_data;
    UDSErr_t retval = static_cast<UDSErr_t>(fuzzed_data->ConsumeIntegral<int>());
    // 50% of time to respond positively
    if (fuzzed_data->ConsumeBool()) {
        retval = UDS_PositiveResponse;
    }

    switch (ev) {
    case UDS_EVT_DiagSessCtrl: {
        UDSDiagSessCtrlArgs_t *r = (UDSDiagSessCtrlArgs_t *)arg;
        r->p2_ms = fuzzed_data->ConsumeIntegral<uint16_t>();
        r->p2_star_ms = fuzzed_data->ConsumeIntegral<uint32_t>();

        break;
    }
    case UDS_EVT_EcuReset: {
        UDSECUResetArgs_t *r = (UDSECUResetArgs_t *)arg;
        r->powerDownTimeMillis = fuzzed_data->ConsumeIntegral<uint32_t>();
        break;
    }
    case UDS_EVT_ReadDataByIdent: {
        UDSRDBIArgs_t *r = (UDSRDBIArgs_t *)arg;
        const size_t count = fuzzed_data->ConsumeIntegralInRange<size_t>(0, UDS_TP_MTU);
        std::vector<uint8_t> data = fuzzed_data->ConsumeBytes<uint8_t>(count);
        r->copy(srv, data.data(), data.size());
        break;
    }
    case UDS_EVT_ReadMemByAddr: {
        UDSReadMemByAddrArgs_t *r = (UDSReadMemByAddrArgs_t *)arg;
        const size_t count = fuzzed_data->ConsumeIntegralInRange<size_t>(0, UDS_TP_MTU);
        std::vector<uint8_t> data = fuzzed_data->ConsumeBytes<uint8_t>(count);
        r->copy(srv, data.data(), data.size());
        break;
    }
    case UDS_EVT_CommCtrl: {
        UDSCommCtrlArgs_t *r = (UDSCommCtrlArgs_t *)arg;
        r->ctrlType = fuzzed_data->ConsumeIntegral<uint8_t>();
        r->commType = fuzzed_data->ConsumeIntegral<uint8_t>();
        break;
    }
    case UDS_EVT_SecAccessRequestSeed: {
        UDSSecAccessRequestSeedArgs_t *r = (UDSSecAccessRequestSeedArgs_t *)arg;
        const size_t count = fuzzed_data->ConsumeIntegralInRange<size_t>(0, UDS_TP_MTU);
        std::vector<uint8_t> data = fuzzed_data->ConsumeBytes<uint8_t>(count);
        r->copySeed(srv, data.data(), data.size());
        break;
    }
    case UDS_EVT_SecAccessValidateKey: {
        break;
    }

    case UDS_EVT_Auth: {
    }
    case UDS_EVT_WriteDataByIdent: {
        break;
    }
    case UDS_EVT_RoutineCtrl: {
        UDSRoutineCtrlArgs_t *r = (UDSRoutineCtrlArgs_t *)arg;
        const size_t count = fuzzed_data->ConsumeIntegralInRange<size_t>(0, UDS_TP_MTU);
        std::vector<uint8_t> data = fuzzed_data->ConsumeBytes<uint8_t>(count);
        r->copyStatusRecord(srv, data.data(), data.size());
        break;
    }
    case UDS_EVT_RequestDownload: {
        UDSRequestDownloadArgs_t *r = (UDSRequestDownloadArgs_t *)arg;
        r->maxNumberOfBlockLength = fuzzed_data->ConsumeIntegral<uint16_t>();
        break;
    }
    case UDS_EVT_RequestUpload: {
        UDSRequestUploadArgs_t *r = (UDSRequestUploadArgs_t *)arg;
        r->maxNumberOfBlockLength = fuzzed_data->ConsumeIntegral<uint16_t>();
        break;
    }
    case UDS_EVT_TransferData: {
        UDSTransferDataArgs_t *r = (UDSTransferDataArgs_t *)arg;
        const size_t count = fuzzed_data->ConsumeIntegralInRange<size_t>(0, UDS_TP_MTU);
        std::vector<uint8_t> data = fuzzed_data->ConsumeBytes<uint8_t>(count);
        r->copyResponse(srv, data.data(), data.size());
        break;
    }
    case UDS_EVT_RequestTransferExit: {
        UDSRequestTransferExitArgs_t *r = (UDSRequestTransferExitArgs_t *)arg;
        const size_t count = fuzzed_data->ConsumeIntegralInRange<size_t>(0, UDS_TP_MTU);
        std::vector<uint8_t> data = fuzzed_data->ConsumeBytes<uint8_t>(count);
        r->copyResponse(srv, data.data(), data.size());
        break;
    }
    case UDS_EVT_RequestFileTransfer: {
        UDSRequestFileTransferArgs_t *r = (UDSRequestFileTransferArgs_t *)arg;
        r->maxNumberOfBlockLength = fuzzed_data->ConsumeIntegral<uint16_t>();
        break;
    }
    case UDS_EVT_Custom: {
        UDSCustomArgs_t *r = (UDSCustomArgs_t *)arg;
        const size_t count = fuzzed_data->ConsumeIntegralInRange<size_t>(0, UDS_TP_MTU);
        std::vector<uint8_t> data = fuzzed_data->ConsumeBytes<uint8_t>(count);
        r->copyResponse(srv, data.data(), data.size());
        break;
    }
    default:
        break;
    }
    return retval;
}

uint32_t UDSMillis() { return g_time_now_us / 1000; }

const uint8_t possible_requests[] = {
    0x00, // Random message
    0x01, // Random message
    0x02, // Random message
    0x03, // Random message
    0x04, // Random message
    0x05, // Random message
    0x06, // Random message
    0x07, // Random message
    0x08, // Random message
    0x09, // Random message
    0x10, // Diagnostic Session Control
    0x29, // Authentication
};

std::vector<uint8_t>
generate_0x10_diagnostic_session_control_request(FuzzedDataProvider &fuzzed_data) {
    std::vector<uint8_t> msg;
    msg.push_back(kSID_DIAGNOSTIC_SESSION_CONTROL);

    // Ranges > 10 bytes are tested by possible_requests[0].
    std::vector<uint8_t> random_bytes =
        fuzzed_data.ConsumeBytes<uint8_t>(fuzzed_data.ConsumeIntegralInRange<size_t>(0, 10));
    msg.insert(msg.end(), random_bytes.begin(), random_bytes.end());

    return msg;
}

std::vector<uint8_t> generate_random_data(FuzzedDataProvider &fuzzed_data, size_t min_len,
                                          size_t max_len) {
    size_t msg_len = fuzzed_data.ConsumeIntegralInRange<size_t>(0, max_len);
    return fuzzed_data.ConsumeBytes<uint8_t>(msg_len);
}

std::vector<uint8_t> generate_random_subfunc_data(FuzzedDataProvider &fuzzed_data) {
    return generate_random_data(fuzzed_data, 0, UDS_TP_MTU - 1);
}

std::vector<uint8_t> generate_0x29_authentication_request(FuzzedDataProvider &fuzzed_data) {
    std::vector<uint8_t> msg;
    msg.push_back(kSID_AUTHENTICATION);

    uint8_t subfunc = fuzzed_data.ConsumeIntegralInRange(0, 9);

    switch (subfunc) {
    case 0: {
        msg.push_back(0x00);
        if (fuzzed_data.ConsumeBool()) {
            // add random data 50% of the time. Valid request has no other payload
            auto data = generate_random_subfunc_data(fuzzed_data);
            msg.insert(msg.end(), data.begin(), data.end());
        }
        break;
    }
    case 1: {
    }
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    case 8: {
    }
    case 9: {
        auto data = generate_random_subfunc_data(fuzzed_data);
        msg.insert(msg.end(), data.begin(), data.end());
        break;
    }
    }

    return msg;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    FuzzedDataProvider fuzzed_data(data, size);
    UDSServer_t srv;
    UDSTp_t *mock_client = NULL;
    ISOTPMockArgs_t server_args = {
        .sa_phys = 0x7E0, .ta_phys = 0x7E8, .sa_func = 0x7DF, .ta_func = UDS_TP_NOOP_ADDR};
    ISOTPMockArgs_t client_args = {
        .sa_phys = 0x7E8, .ta_phys = 0x7E0, .sa_func = UDS_TP_NOOP_ADDR, .ta_func = 0x7DF};
    UDSServerInit(&srv);
    srv.fn = fn;
    srv.fn_data = (void *)&fuzzed_data;
    srv.tp = ISOTPMockNew("server", &server_args);
    mock_client = ISOTPMockNew("client", &client_args);

    g_time_now_us = fuzzed_data.ConsumeIntegral<uint32_t>();
    static constexpr int MAX_STEPS = 10;
    for (int step = 0; step < MAX_STEPS; step++) {
        UDSSDU_t sdu_info;
        sdu_info.A_Mtype = static_cast<UDS_A_Mtype_t>(fuzzed_data.ConsumeIntegralInRange<int>(
            UDS_A_MTYPE_DIAG, UDS_A_MTYPE_SECURE_REMOTE_DIAG));
        sdu_info.A_SA = fuzzed_data.ConsumeIntegral<uint32_t>();
        sdu_info.A_TA = fuzzed_data.ConsumeIntegral<uint32_t>();
        sdu_info.A_TA_Type = static_cast<UDS_A_TA_Type_t>(fuzzed_data.ConsumeIntegralInRange<int>(
            UDS_A_TA_TYPE_PHYSICAL, UDS_A_TA_TYPE_FUNCTIONAL));
        sdu_info.A_AE = fuzzed_data.ConsumeIntegral<uint32_t>();

        const size_t possible_request_index = fuzzed_data.ConsumeIntegralInRange<size_t>(

            0, (sizeof(possible_requests) - 1) / sizeof(possible_requests[0]));

        std::vector<uint8_t> msg;

        switch (possible_requests[possible_request_index]) {
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
        case 8:
        case 9: {
            size_t msg_len = fuzzed_data.ConsumeIntegralInRange<size_t>(0, UDS_TP_MTU);
            msg = fuzzed_data.ConsumeBytes<uint8_t>(msg_len);
            break;
        }
        case kSID_DIAGNOSTIC_SESSION_CONTROL:
            msg = generate_0x10_diagnostic_session_control_request(fuzzed_data);
            break;
        case kSID_AUTHENTICATION: {
            msg = generate_0x29_authentication_request(fuzzed_data);
        }
        }

        {
            const size_t wait_ms = fuzzed_data.ConsumeIntegralInRange<size_t>(0, 6000);
            const size_t poll_rate_us = fuzzed_data.ConsumeIntegralInRange<size_t>(100, 1000000);
            for (size_t time_us = 0; time_us < wait_ms * 1000; time_us += poll_rate_us) {
                g_time_now_us += poll_rate_us;
                UDSServerPoll(&srv);
            }

            if (!msg.empty()) {
                UDSTpSend(mock_client, msg.data(), msg.size(), &sdu_info);
            }
        }

        {
            const size_t wait_ms = fuzzed_data.ConsumeIntegralInRange<size_t>(0, 6000);
            const size_t poll_rate_us = fuzzed_data.ConsumeIntegralInRange<size_t>(100, 1000000);
            for (size_t time_us = 0; time_us < wait_ms * 1000; time_us += poll_rate_us) {
                g_time_now_us += poll_rate_us;
                UDSServerPoll(&srv);
                ssize_t ret =
                    UDSTpRecv(mock_client, client_recv_buf, sizeof(client_recv_buf), NULL);
                (void)ret;
            }
        }
    }
    ISOTPMockFree(mock_client);
    ISOTPMockFree(srv.tp);
    return 0;
}
