#include "test/env.h"
#include <stdint.h>

int Setup(void **state) {
    Env_t *env = malloc(sizeof(Env_t));
    memset(env, 0, sizeof(Env_t));
    env->server = malloc(sizeof(UDSServer_t));
    UDSServerInit(env->server);
    env->server->tp = ISOTPMockNew("server", &(ISOTPMockArgs_t){.sa_phys = 0x7E0,
                                                                .ta_phys = 0x7E8,
                                                                .sa_func = 0x7DF,
                                                                .ta_func = UDS_TP_NOOP_ADDR});
    env->client_tp = ISOTPMockNew("client", &(ISOTPMockArgs_t){.sa_phys = 0x7E8,
                                                               .ta_phys = 0x7E0,
                                                               .sa_func = UDS_TP_NOOP_ADDR,
                                                               .ta_func = 0x7DF});
    *state = env;
    return 0;
}

int Teardown(void **state) {
    Env_t *env = *state;
    ISOTPMockFree(env->server->tp);
    ISOTPMockFree(env->client_tp);
    ISOTPMockReset();
    free(env->server);
    free(env);
    return 0;
}

int fn_test_session_timeout(UDSServer_t *srv, UDSEvent_t ev, void *arg) {
    if (ev == UDS_EVT_SessionTimeout) {
        int *call_count = (int *)srv->fn_data;
        TEST_INT_EQUAL(UDS_EVT_SessionTimeout, ev);
        (*call_count)++;
        return UDS_OK;
    } else if (ev == UDS_EVT_AuthTimeout) {
        return UDS_OK;
    }

    return UDS_NRC_SubFunctionNotSupported;
}

void test_default_session_does_not_timeout(void **state) {
    Env_t *e = *state;
    int call_count = 0;

    // When a server is initialized with a default session
    e->server->fn = fn_test_session_timeout;
    e->server->fn_data = &call_count;
    e->server->sessionType = UDS_LEV_DS_DS;

    // and the server is run for a long time with no communication
    EnvRunMillis(e, 10000);

    // the session should not timeout
    TEST_INT_EQUAL(call_count, 0);
}

void test_programming_session_times_out(void **state) {
    Env_t *e = *state;
    int call_count = 0;

    // When a server is initialized with a programming session
    e->server->fn = fn_test_session_timeout;
    e->server->fn_data = &call_count;
    e->server->sessionType = UDS_LEV_DS_PRGS;

    // and the server is run for a long time with no communication
    EnvRunMillis(e, 10000);

    // the session should timeout
    TEST_INT_GE(call_count, 1);
}

void test_0x10_no_fn_results_in_negative_resp(void **state) {
    Env_t *e = *state;
    uint8_t buf[8] = {0};

    // When a diagnostic session control request is sent to the server
    const uint8_t REQ[] = {0x10, 0x02};
    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    // the server should respond with a negative response within p2 ms
    const uint8_t EXP_RESP[] = {0x7f, 0x10, 0x11};
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXP_RESP, sizeof(EXP_RESP));
}

void test_0x10_no_fn_results_in_negative_resp_functional(void **state) {
    Env_t *e = *state;
    uint8_t buf[8] = {0};

    // When a diagnostic session control request is sent to the server in functional mode
    const uint8_t REQ[] = {0x10, 0x02};
    UDSTpSend(e->client_tp, REQ, sizeof(REQ), &(UDSSDU_t){.A_TA_Type = UDS_A_TA_TYPE_FUNCTIONAL});

    // the server should respond with a negative response within p2 ms
    const uint8_t EXP_RESP[] = {0x7f, 0x10, 0x11};
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXP_RESP, sizeof(EXP_RESP));
}

int fn_test_0x10_diagnostic_session_control(UDSServer_t *srv, UDSEvent_t ev, void *arg) {
    return UDS_PositiveResponse;
}

void test_0x10_suppress_pos_resp(void **state) {
    Env_t *e = *state;
    e->server->fn = fn_test_0x10_diagnostic_session_control;
    uint8_t buf[8] = {0};

    // When a diagnostic session control request is sent to the server with the
    // suppressPositiveResponse bit set
    const uint8_t REQ[] = {
        0x10, // DiagnosticSessionControl
        0x83, // ExtendedDiagnosticSession, suppressPositiveResponse bit set
    };
    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    // even after running for a long time, but not long enough to timeout
    EnvRunMillis(e, 1000);

    // there should be no response from the server
    int len = UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL);
    TEST_INT_EQUAL(len, 0);

    // however, the server sessionType should have changed
    TEST_INT_EQUAL(e->server->sessionType, UDS_LEV_DS_EXTDS);
}

int fn_test_0x11_no_send_recv_after_ECU_reset(UDSServer_t *srv, UDSEvent_t ev, void *arg) {
    int *call_count = (int *)srv->fn_data;
    switch (ev) {
    case UDS_EVT_EcuReset:
        (*call_count)++;
        return UDS_PositiveResponse;
    default:
        TEST_INT_EQUAL(UDS_EVT_DoScheduledReset, ev);
        return UDS_PositiveResponse;
    }
}

void test_0x11_no_send_after_ECU_reset(void **state) {
    Env_t *e = *state;
    int call_count = 0;
    uint8_t buf[8] = {0};

    // When a server handler function is installed
    e->server->fn = fn_test_0x11_no_send_recv_after_ECU_reset;
    e->server->fn_data = &call_count;

    // and an ECU reset request is sent to the server
    const uint8_t REQ[] = {0x11, 0x01};
    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    // the server should respond with a positive response within p2 ms
    const uint8_t RESP[] = {0x51, 0x01};
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, RESP, sizeof(RESP));

    const unsigned LONG_TIME_MS = 5000;
    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);
    EXPECT_WHILE_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) == 0, LONG_TIME_MS);

    // Additionally the ECU reset handler should have been called exactly once.
    TEST_INT_EQUAL(call_count, 1);
}

int fn_test_0x14(UDSServer_t *srv, UDSEvent_t ev, void *arg) { return UDS_PositiveResponse; }

// ISO14229-1 2020 12.2.5 Message flow example ClearDiagnosticInformation
void test_0x14_positive_response(void **state) {
    Env_t *e = *state;
    uint8_t buf[20] = {0};

    e->server->fn = fn_test_0x14;
    e->server->fn_data = NULL;

    /* Request per ISO14229-1 2020 Table 300 */
    const uint8_t REQ[] = {
        0x14, /* SID */
        0xFF, /* GroupOfDTC [High Byte] */
        0xFF, /* GroupOfDTC [Middle Byte] */
        0x33, /* GroupOfDTC [Low Byte] */
    };

    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    /* Response per ISO14229-1 2020 Table 301 */
    const uint8_t EXPECTED_RESP[] = {
        0x54, /* Response SID */
    };

    /* the client transport should receive a positive response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP, sizeof(EXPECTED_RESP));
}

void test_0x14_incorrect_request_length(void **state) {
    Env_t *e = *state;
    uint8_t buf[20] = {0};

    e->server->fn = fn_test_0x14;
    e->server->fn_data = NULL;

    const uint8_t REQ[] = {
        0x14, /* SID */
        0xFF, /* GroupOfDTC [High Byte] */
        0xFF, /* GroupOfDTC [Middle Byte] */
        /* MISSING required GroupOfDTC [Low Byte] */
    };

    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    const uint8_t EXPECTED_RESP[] = {
        0x7F, /* Response SID */
        0x14, /* Original Request SID */
        0x13, /* NRC: IncorrectMessageLengthOrInvalidFormat */
    };

    /* the client transport should receive a positive response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP, sizeof(EXPECTED_RESP));
}

int fn_test_0x14_negative_response(UDSServer_t *srv, UDSEvent_t ev, void *arg) {
    UDSCDIArgs_t *args = (UDSCDIArgs_t *)arg;
    TEST_INT_EQUAL(ev, UDS_EVT_ClearDiagnosticInfo);
    TEST_INT_EQUAL(args->groupOfDTC, 0x00FFDD33);
    TEST_INT_GE(args->hasMemorySelection, 1);
    TEST_INT_EQUAL(args->memorySelection, 0x45);

    return UDS_NRC_RequestOutOfRange;
}

void test_0x14_negative_response(void **state) {
    Env_t *e = *state;
    uint8_t buf[20] = {0};

    e->server->fn = fn_test_0x14_negative_response;
    e->server->fn_data = NULL;

    const uint8_t REQ[] = {
        0x14, /* SID */
        0xFF, /* GroupOfDTC [High Byte] */
        0xDD, /* GroupOfDTC [Middle Byte] */
        0x33, /* GroupOfDTC [Low Byte] */
        0x45, /* MemorySelection */
    };

    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    const uint8_t EXPECTED_RESP[] = {
        0x7F, /* Response SID */
        0x14, /* Original Request SID */
        0x31, /* NRC: RequestOutOfRange */
    };

    /* the client transport should receive a positive response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP, sizeof(EXPECTED_RESP));
}

// Container to provide data and length to the 0x19 handler function
typedef struct {
    void *data;
    size_t len;
    char *test_identifier;
} Test0x19FnData_t;

UDSErr_t fn_test_0x19(UDSServer_t *srv, UDSEvent_t ev, void *arg) {
    TEST_INT_EQUAL(ev, UDS_EVT_ReadDTCInformation);
    UDSRDTCIArgs_t *r = (UDSRDTCIArgs_t *)arg;
    Test0x19FnData_t *fnData = (Test0x19FnData_t *)srv->fn_data;

    if (fnData->test_identifier != NULL &&
        strcmp("malformed_response", fnData->test_identifier) == 0) {
        /* We don't care about the contents and just want to provoke a malformed response */
        return r->copy(srv, fnData->data, fnData->len);
    }

    if (fnData->test_identifier != NULL &&
        strcmp("shrink_default_response_len", fnData->test_identifier) == 0) {
        /* We want to force a response length smaller than the default */
        srv->r.send_len = 1;
        return UDS_PositiveResponse;
    }

    switch (r->type) {
    case 0x01: /* reportNumberOfDTCByStatusMask */
        TEST_INT_EQUAL(r->subFuncArgs.numOfDTCByStatusMaskArgs.mask, 0x8);
        break;
    case 0x02: /* reportDTCByStatusMask */
        if (strcmp("test_0x19_Sub_2", fnData->test_identifier) == 0) {
            TEST_INT_EQUAL(r->subFuncArgs.dtcStatusByMaskArgs.mask, 0x84);
        } else if (strcmp("test_0x19_Sub_2_no_matching_dtc", fnData->test_identifier) == 0) {
            TEST_INT_EQUAL(r->subFuncArgs.dtcStatusByMaskArgs.mask, 0x01);
        }
        break;
    case 0x03: /* reportDTCSnapshotIdentification */
    case 0x0A: /* reportSupportedDTC */
    case 0x0B: /* reportFirstTestFailedDTC */
    case 0x0C: /* reportFirstConfirmedDTC */
    case 0x0D: /* reportMostRecentTestFailedDTC */
    case 0x0E: /* reportMostRecentConfirmedDTC */
    case 0x14: /* reportDTCFaultDetectionCounter */
    case 0x15: /* reportDTCWithPermanentStatus */
        /* No additional arguments to check */
        break;
    case 0x04: /* reportDTCSnapshotRecordByDTCNumber */
        TEST_INT_EQUAL(r->subFuncArgs.dtcSnapshotRecordbyDTCNumArgs.dtc, 0x00123456);
        TEST_INT_EQUAL(r->subFuncArgs.dtcSnapshotRecordbyDTCNumArgs.snapshotNum, 0x02);
        break;
    case 0x05: /* reportDTCStoredDataByRecordNumber */
        TEST_INT_EQUAL(r->subFuncArgs.dtcStoredDataByRecordNumArgs.recordNum, 0x02);
        break;
    case 0x06: /* reportDTCExtDataRecordByDTCNumber */
        TEST_INT_EQUAL(r->subFuncArgs.dtcExtDtaRecordByDTCNumArgs.dtc, 0x00123456);
        TEST_INT_EQUAL(r->subFuncArgs.dtcExtDtaRecordByDTCNumArgs.extDataRecNum, 0xFF);
        break;
    case 0x07: /* reportNumberOfDTCBySeverityMaskRecord */
    case 0x08: /* reportDTCBySeverityMaskRecord */
        TEST_INT_EQUAL(r->subFuncArgs.numOfDTCBySeverityMaskArgs.severityMask, 0xC0);
        TEST_INT_EQUAL(r->subFuncArgs.numOfDTCBySeverityMaskArgs.statusMask, 0x01);
        break;
    case 0x09: /* reportDTCBySeverityMaskRecord */
        TEST_INT_EQUAL(r->subFuncArgs.severityInfoOfDTCArgs.dtc, 0x00080511);
        break;
    case 0x16: /* reportDTCExtDataRecordByNumber */
        TEST_INT_EQUAL(r->subFuncArgs.dtcExtDataRecordByRecordNumArgs.recordNum, 0x05);
        break;
    case 0x17: /* reportUserDefMemoryDTCByStatusMask */
        if (strcmp("test_0x19_Sub_0x17", fnData->test_identifier) == 0) {
            TEST_INT_EQUAL(r->subFuncArgs.userDefMemoryDTCByStatusMaskArgs.mask, 0x84);
        } else if (strcmp("test_0x19_Sub_0x17_no_matching_dtc", fnData->test_identifier) == 0) {
            TEST_INT_EQUAL(r->subFuncArgs.userDefMemoryDTCByStatusMaskArgs.mask, 0x01);
        }

        TEST_INT_EQUAL(r->subFuncArgs.userDefMemoryDTCByStatusMaskArgs.memory, 0x18);
        break;

    case 0x18: /* reportUserDefMemoryDTCSnapshotRecordByDTCNumber */
        TEST_INT_EQUAL(r->subFuncArgs.userDefMemDTCSnapshotRecordByDTCNumArgs.dtc, 0x00123456);
        TEST_INT_EQUAL(r->subFuncArgs.userDefMemDTCSnapshotRecordByDTCNumArgs.snapshotNum, 0x06);
        TEST_INT_EQUAL(r->subFuncArgs.userDefMemDTCSnapshotRecordByDTCNumArgs.memory, 0x18);
        break;
    case 0x19: /* reportUserDefMemoryDTCExtDataRecordByDTCNumber */
        TEST_INT_EQUAL(r->subFuncArgs.userDefMemDTCExtDataRecordByDTCNumArgs.dtc, 0x00123456);
        TEST_INT_EQUAL(r->subFuncArgs.userDefMemDTCExtDataRecordByDTCNumArgs.extDataRecNum, 0xFF);
        TEST_INT_EQUAL(r->subFuncArgs.userDefMemDTCExtDataRecordByDTCNumArgs.memory, 0x18);
        break;
    case 0x1A: /* reportDTCExtendedDataRecordIdentification */
        TEST_INT_EQUAL(r->subFuncArgs.dtcExtDataRecordIdArgs.recordNum, 0x91);
        break;
    case 0x42: /* reportWWHOBDDTCByMaskRecord */
        TEST_INT_EQUAL(r->subFuncArgs.wwhobdDTCByMaskArgs.functionalGroup, 0x33);
        TEST_INT_EQUAL(r->subFuncArgs.wwhobdDTCByMaskArgs.statusMask, 0x08);
        TEST_INT_EQUAL(r->subFuncArgs.wwhobdDTCByMaskArgs.severityMask, 0xFF);
        break;
    case 0x55: /* reportWWHOBDDTCWithPermanentStatus */
        TEST_INT_EQUAL(r->subFuncArgs.wwhobdDTCByMaskArgs.functionalGroup, 0x33);
        break;
    case 0x56: /* reportDTCInformationByDTCReadinessGroupIdentifier */
        TEST_INT_EQUAL(r->subFuncArgs.dtcInfoByDTCReadinessGroupIdArgs.functionalGroup, 0x33);
        TEST_INT_EQUAL(r->subFuncArgs.dtcInfoByDTCReadinessGroupIdArgs.readinessGroup, 0x01);
        break;

    default:
        return UDS_NRC_ConditionsNotCorrect;
    }

    if (fnData->len == 0) {
        // No data to copy, just return positive response
        return UDS_PositiveResponse;
    }

    return r->copy(srv, fnData->data, fnData->len);
}

// ISO14229-1 2020 12.3.5.2 Example #1 - ReadDTCInformation, SubFunction =
// reportNumberOfDTCByStatusMask
void test_0x19_sub_0x01(void **state) {
    Env_t *e = *state;
    uint8_t buf[512] = {0};

    uint8_t ResponseData[4] = {0x2F, 0x01, 0x00, 0x01};
    Test0x19FnData_t fn_data = {.data = ResponseData, .len = sizeof(ResponseData)};

    e->server->fn = fn_test_0x19;
    e->server->fn_data = &fn_data;

    /* Request per ISO14229-1 2020 Table 340 */
    const uint8_t REQ[] = {
        0x19, /* SID */
        0x01, /* reportNumberOfDTCByStatusMask */
        0x08, /* DTCStatusMask */
    };

    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    /* Responser per ISO14229-1 2020 Table 341 */
    const uint8_t EXPECTED_RESP[] = {
        0x59, /* Response SID */
        0x01, /* reportType = SubFunction */
        0x2F, /* DTCStatusAvailabilityMask */
        0x01, /* DTCFormatIdentifier */
        0x00, /* DTCCount [High Byte]*/
        0x01, /* DTCCount [Low Byte] */
    };

    // the client transport should receive a positive response within client_p2 ms
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP, sizeof(EXPECTED_RESP));
}

// ISO14229-1 2020 12.3.5.3 Example #2 - ReadDTCInformation, SubFunction =
// reportDTCByStatusMask, matching DTCs returned
void test_0x19_sub_0x02(void **state) {
    Env_t *e = *state;
    uint8_t buf[512] = {0};

    uint8_t ResponseData[9] = {
        0x07F, 0x0A, 0x9B, 0x17, 0x24, 0x08, 0x05, 0x11, 0x2F,
    };
    Test0x19FnData_t fn_data = {
        .data = ResponseData, .len = sizeof(ResponseData), .test_identifier = "test_0x19_Sub_2"};

    e->server->fn = fn_test_0x19;
    e->server->fn_data = &fn_data;

    /* Request per ISO14229-1 2020 Table 345 */
    const uint8_t REQ[] = {
        0x19, /* SID */
        0x02, /* reportDTCByStatusMask */
        0x84, /* DTCStatusMask */
    };

    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    /* Response per ISO14229-1 2020 Table 346 */
    const uint8_t EXPECTED_RESP[] = {
        0x59, /* Response SID */
        0x02, /* SubFunction */
        0x7F, /* DTCStatusAvailabilityMask */
        0x0A, /* DTCAndStatusRecord#1 [DTC High Byte] */
        0x9B, /* DTCAndStatusRecord#1 [DTC Middle Byte] */
        0x17, /* DTCAndStatusRecord#1 [DTC Low Byte] */
        0x24, /* DTCAndStatusRecord#1 [DTC Status Byte] */
        0x08, /* DTCAndStatusRecord#2 [DTC High Byte] */
        0x05, /* DTCAndStatusRecord#2 [DTC Middle Byte] */
        0x11, /* DTCAndStatusRecord#2 [DTC Low Byte] */
        0x2F, /* DTCAndStatusRecord#2 [DTC Status Byte] */
    };

    /* the client transport should receive a positive response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP, sizeof(EXPECTED_RESP));
}

// ISO14229-1 2020 12.3.5.4 Example #3 - ReadDTCInformation, SubFunction =
// reportDTCByStatusMask, no matching DTCs returned
void test_0x19_sub_0x02_no_matching_dtc(void **state) {
    Env_t *e = *state;
    uint8_t buf[512] = {0};

    uint8_t ResponseData[9] = {
        0x07F,
    };
    Test0x19FnData_t fn_data = {.data = ResponseData,
                                .len = sizeof(ResponseData),
                                .test_identifier = "test_0x19_Sub_2_no_matching_dtc"};

    e->server->fn = fn_test_0x19;
    e->server->fn_data = &fn_data;

    /* Request per ISO14229-1 2020 Table 349 */
    const uint8_t REQ[] = {
        0x19, /* SID */
        0x02, /* reportDTCByStatusMask */
        0x01, /* DTCStatusMask */
    };

    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    /* Response per ISO14229-1 2020 Table 350 */
    const uint8_t EXPECTED_RESP[] = {
        0x59, /* Response SID */
        0x02, /* SubFunction */
        0x7F, /* DTCStatusAvailabilityMask */
    };

    /* the client transport should receive a positive response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP, sizeof(EXPECTED_RESP));
}

// ISO14229-1 2020 12.3.5.5 Example #4 - ReadDTCInformation, SubFunction =
// reportDTCSnapshotIdentification
void test_0x19_sub_0x03(void **state) {
    Env_t *e = *state;
    uint8_t buf[512] = {0};

    uint8_t ResponseData[] = {
        0x12, 0x34, 0x56, 0x01, 0x12, 0x34, 0x57, 0x02, 0x78, 0x9A, 0xBC, 0x01,
    };
    Test0x19FnData_t fn_data = {.data = ResponseData, .len = sizeof(ResponseData)};

    e->server->fn = fn_test_0x19;
    e->server->fn_data = &fn_data;

    /* Request per ISO14229-1 2020 Table 351 */
    const uint8_t REQ[] = {
        0x19, /* SID */
        0x03, /* reportDTCSnapshotIdentification */
    };

    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    /* Response per ISO14229-1 2020 Table 352 */
    const uint8_t EXPECTED_RESP[] = {
        0x59, /* Response SID */
        0x03, /* SubFunction */
        0x12, /* DTCAndStatusRecord#1 [DTC High Byte] */
        0x34, /* DTCAndStatusRecord#1 [DTC Middle Byte */
        0x56, /* DTCAndStatusRecord#1 [DTC Low Byte] */
        0x01, /* DTCAndStatusRecord#1 [DTC Snapshot Record Number] */
        0x12, /* DTCAndStatusRecord#2 [DTC High Byte] */
        0x34, /* DTCAndStatusRecord#2 [DTC Middle Byte] */
        0x57, /* DTCAndStatusRecord#2 [DTC Low Byte] */
        0x02, /* DTCAndStatusRecord#2 [DTC Snapshot Record Number] */
        0x78, /* DTCAndStatusRecord#3 [DTC High Byte] */
        0x9A, /* DTCAndStatusRecord#3 [DTC Middle Byte] */
        0xBC, /* DTCAndStatusRecord#3 [DTC Low Byte] */
        0x01, /* DTCAndStatusRecord#3 [DTC Snapshot Record Number] */
    };

    /* the client transport should receive a positive response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP, sizeof(EXPECTED_RESP));
}

void test_0x19_sub_0x03_no_snapshots(void **state) {
    Env_t *e = *state;
    uint8_t buf[512] = {0};

    Test0x19FnData_t fn_data = {.data = NULL, .len = 0};

    e->server->fn = fn_test_0x19;
    e->server->fn_data = &fn_data;

    const uint8_t REQ[] = {
        0x19, /* SID */
        0x03, /* reportDTCSnapshotIdentification */
    };

    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    const uint8_t EXPECTED_RESP[] = {
        0x59, /* Response SID */
        0x03, /* SubFunction */
    };

    /* the client transport should receive a positive response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP, sizeof(EXPECTED_RESP));
}

// ISO14229-1 2020 12.3.5.6 Example #5 - ReadDTCInformation, SubFunction =
// reportDTCSnapshotRecordByDTCNumber
void test_0x19_sub_0x04(void **state) {
    Env_t *e = *state;
    uint8_t buf[512] = {0};

    uint8_t ResponseData[] = {
        0x12, 0x34, 0x56, 0x24, 0x02, 0x01, 0x47, 0x11, 0xA6, 0x66, 0x07, 0x50, 0x20,
    };
    Test0x19FnData_t fn_data = {.data = ResponseData, .len = sizeof(ResponseData)};

    e->server->fn = fn_test_0x19;
    e->server->fn_data = &fn_data;

    /* Request per ISO14229-1 2020 Table 354 */
    const uint8_t REQ[] = {
        0x19, /* SID */
        0x04, /* reportDTCSnapshotRecordByDTCNumber */
        0x12, /* DTCMaskRecord [DTC High Byte] */
        0x34, /* DTCMaskRecord [DTC Middle Byte] */
        0x56, /* DTCMaskRecord [DTC Low Byte] */
        0x02, /* DTCSnapshotRecordNumber */
    };

    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    /* Request per ISO14229-1 2020 Table 355 */
    const uint8_t EXPECTED_RESP[] = {
        0x59, /* Response SID */
        0x04, /* SubFunction */
        0x12, /* DTCAndStatusRecord#1 [DTC High Byte] */
        0x34, /* DTCAndStatusRecord#1 [DTC Middle Byte */
        0x56, /* DTCAndStatusRecord#1 [DTC Low Byte] */
        0x24, /* DTCAndStatusRecord#1 [status of DTC] */
        0x02, /* DTCSnapshotRecordNumber */
        0x01, /* DTCSnapshotRecordNumberOfIdentifiers */
        0x47, /* DataIdentifier [High Byte] */
        0x11, /* DataIdentifier [Low Byte] */
        0xA6, /* DTCSnapshotRecordData#1 */
        0x66, /* DTCSnapshotRecordData#2 */
        0x07, /* DTCSnapshotRecordData#3 */
        0x50, /* DTCSnapshotRecordData#4 */
        0x20, /* DTCSnapshotRecordData#5 */
    };

    /* the client transport should receive a positive response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP, sizeof(EXPECTED_RESP));
}

void test_0x19_sub_0x04_no_records(void **state) {
    Env_t *e = *state;
    uint8_t buf[512] = {0};

    uint8_t ResponseData[] = {
        0x12,
        0x34,
        0x56,
        0x24,
    };
    Test0x19FnData_t fn_data = {.data = ResponseData, .len = sizeof(ResponseData)};

    e->server->fn = fn_test_0x19;
    e->server->fn_data = &fn_data;

    const uint8_t REQ[] = {
        0x19, /* SID */
        0x04, /* reportDTCSnapshotRecordByDTCNumber */
        0x12, /* DTCMaskRecord [DTC High Byte] */
        0x34, /* DTCMaskRecord [DTC Middle Byte] */
        0x56, /* DTCMaskRecord [DTC Low Byte] */
        0x02, /* DTCSnapshotRecordNumber */
    };

    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    const uint8_t EXPECTED_RESP[] = {
        0x59, /* Response SID */
        0x04, /* SubFunction */
        0x12, /* DTCAndStatusRecord#1 [DTC High Byte] */
        0x34, /* DTCAndStatusRecord#1 [DTC Middle Byte */
        0x56, /* DTCAndStatusRecord#1 [DTC Low Byte] */
        0x24, /* DTCAndStatusRecord#1 [status of DTC] */
    };

    /* the client transport should receive a positive response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP, sizeof(EXPECTED_RESP));
}

// ISO14229-1 2020 12.3.5.7 Example #6 - ReadDTCInformation, SubFunction =
// reportDTCStoredDataByRecordNumber
void test_0x19_sub_0x05(void **state) {
    Env_t *e = *state;
    uint8_t buf[512] = {0};

    uint8_t ResponseData[] = {0x02, 0x12, 0x34, 0x56, 0x24, 0x01, 0x47,
                              0x11, 0xA6, 0x66, 0x07, 0x50, 0x20};
    Test0x19FnData_t fn_data = {.data = ResponseData, .len = sizeof(ResponseData)};

    e->server->fn = fn_test_0x19;
    e->server->fn_data = &fn_data;

    /* Request per ISO14229-1 2020 Table 357 */
    const uint8_t REQ[] = {
        0x19, /* SID */
        0x05, /* reportDTCStoredDataByRecordNumber */
        0x02, /* DTCStoredDataRecordNumber */
    };

    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    /* Response per ISO14229-1 2020 Table 358 */
    const uint8_t EXPECTED_RESP[] = {
        0x59, /* Response SID */
        0x05, /* reportType = SubFunction */
        0x02, /* DTCStoredDataRecordNumber */
        0x12, /* DTCAndStatusRecord#1 [High Byte] */
        0x34, /* DTCAndStatusRecord#1 [Middle Byte] */
        0x56, /* DTCAndStatusRecord#1 [Low Byte] */
        0x24, /* DTCAndStatusRecord#1 [DTC Status Byte] */
        0x01, /* DTCStoredDataRecordNumberOfIdentifiers#1 */
        0x47, /* DataIdentifier#1 [High Byte] */
        0x11, /* DataIdentifier#1 [Low Byte] */
        0xA6, /* DTCStoredDataRecordData#1-1 */
        0x66, /* DTCStoredDataRecordData#1-2 */
        0x07, /* DTCStoredDataRecordData#1-3 */
        0x50, /* DTCStoredDataRecordData#1-4 */
        0x20, /* DTCStoredDataRecordData#1-5 */
    };

    /* the client transport should receive a positive response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP, sizeof(EXPECTED_RESP));
}

void test_0x19_sub_0x05_no_data(void **state) {
    Env_t *e = *state;
    uint8_t buf[512] = {0};

    uint8_t ResponseData[] = {
        0x02,
    };
    Test0x19FnData_t fn_data = {.data = ResponseData, .len = sizeof(ResponseData)};

    e->server->fn = fn_test_0x19;
    e->server->fn_data = &fn_data;

    const uint8_t REQ[] = {
        0x19, /* SID */
        0x05, /* reportDTCStoredDataByRecordNumber */
        0x02, /* DTCStoredDataRecordNumber */
    };

    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    const uint8_t EXPECTED_RESP[] = {
        0x59, /* Response SID */
        0x05, /* reportType = SubFunction */
        0x02, /* DTCStoredDataRecordNumber */
    };

    /* the client transport should receive a positive response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP, sizeof(EXPECTED_RESP));
}

// ISO14229-1 2020 12.3.5.8 Example #7 - ReadDTCInformation, SubFunction =
// reportDTCExtDataRecordByDTCNumber
void test_0x19_sub_0x06(void **state) {
    Env_t *e = *state;
    uint8_t buf[512] = {0};

    uint8_t ResponseData[] = {0x12, 0x34, 0x56, 0x24, 0x05, 0x17, 0x10, 0x79};
    Test0x19FnData_t fn_data = {.data = ResponseData, .len = sizeof(ResponseData)};

    e->server->fn = fn_test_0x19;
    e->server->fn_data = &fn_data;

    /* Request per ISO14229-1 2020 Table 361 */
    const uint8_t REQ[] = {
        0x19, /* SID */
        0x06, /* reportDTCExtDataRecordByDTCNumber */
        0x12, /* DTCAndStatusRecord [High Byte] */
        0x34, /* DTCAndStatusRecord [Middle Byte] */
        0x56, /* DTCAndStatusRecord [Low Byte] */
        0xFF, /* DTCExtDataRecordNumber */
    };

    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    /* Response per ISO14229-1 2020 Table 362 */
    const uint8_t EXPECTED_RESP[] = {
        0x59, /* Response SID */
        0x06, /* reportType = SubFunction */
        0x12, /* DTCAndStatusRecord [High Byte] */
        0x34, /* DTCAndStatusRecord [Middle Byte] */
        0x56, /* DTCAndStatusRecord [Low Byte] */
        0x24, /* DTCAndStatusRecord [status of DTC] */
        0x05, /* DTCExtDataRecordNumber#1 */
        0x17, /* DTCExtDataRecord#1 [Byte 1] */
        0x10, /* DTCExtDataRecordNumber#2 */
        0x79, /* DTCExtDataRecord#2 [Byte 1] */
    };

    /* the client transport should receive a positive response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP, sizeof(EXPECTED_RESP));
}

void test_0x19_sub_0x06_no_data(void **state) {
    Env_t *e = *state;
    uint8_t buf[512] = {0};

    uint8_t ResponseData[] = {0x12, 0x34, 0x56, 0x24};
    Test0x19FnData_t fn_data = {.data = ResponseData, .len = sizeof(ResponseData)};

    e->server->fn = fn_test_0x19;
    e->server->fn_data = &fn_data;

    const uint8_t REQ[] = {
        0x19, /* SID */
        0x06, /* reportDTCExtDataRecordByDTCNumber */
        0x12, /* DTCAndStatusRecord [High Byte] */
        0x34, /* DTCAndStatusRecord [Middle Byte] */
        0x56, /* DTCAndStatusRecord [Low Byte] */
        0xFF, /* DTCExtDataRecordNumber */
    };

    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    const uint8_t EXPECTED_RESP[] = {
        0x59, /* Response SID */
        0x06, /* reportType = SubFunction */
        0x12, /* DTCAndStatusRecord [High Byte] */
        0x34, /* DTCAndStatusRecord [Middle Byte] */
        0x56, /* DTCAndStatusRecord [Low Byte] */
        0x24, /* DTCAndStatusRecord [status of DTC] */
    };

    /* the client transport should receive a positive response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP, sizeof(EXPECTED_RESP));
}
// ISO14229-1 2020 12.3.5.9 Example #8 - ReadDTCInformation, SubFunction =
// reportNumberOfDTC-BySeverityMaskRecord
void test_0x19_sub_0x07(void **state) {
    Env_t *e = *state;
    uint8_t buf[512] = {0};

    uint8_t ResponseData[] = {
        0x09,
        0x01,
        0x00,
        0x01,
    };
    Test0x19FnData_t fn_data = {.data = ResponseData, .len = sizeof(ResponseData)};

    e->server->fn = fn_test_0x19;
    e->server->fn_data = &fn_data;

    /* Request per ISO14229-1 2020 Table 366 */
    const uint8_t REQ[] = {
        0x19, /* SID */
        0x07, /* reportNumberOfDTCBySeverityMaskRecord */
        0xC0, /* DTCSeverityMaskRecord [DTC Severity Mask] */
        0x01, /* DTCSeverityMaskRecord [DTC Status Mask] */
    };

    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    /* Response per ISO14229-1 2020 Table 367 */
    const uint8_t EXPECTED_RESP[] = {
        0x59, /* Response SID */
        0x07, /* reportType = SubFunction */
        0x09, /* DTCStatusAvailabilityMask */
        0x01, /* DTCFormatIdentifier */
        0x00, /* DTCCount [High Byte] */
        0x01, /* DTCCount [Low Byte] */
    };

    /* the client transport should receive a positive response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP, sizeof(EXPECTED_RESP));
}

// ISO14229-1 2020 12.3.5.10Example #9 - ReadDTCInformation, SubFunction =
// reportDTCBySeverityMaskRecord
void test_0x19_sub_0x08(void **state) {
    Env_t *e = *state;
    uint8_t buf[512] = {0};

    uint8_t ResponseData[] = {0x7F, 0x40, 0x10, 0x08, 0x05, 0x11, 0x2F};
    Test0x19FnData_t fn_data = {.data = ResponseData, .len = sizeof(ResponseData)};

    e->server->fn = fn_test_0x19;
    e->server->fn_data = &fn_data;

    /* Request per ISO14229-1 2020 Table 368 */
    const uint8_t REQ[] = {
        0x19, /* SID */
        0x08, /* reportNumberOfDTCBySeverityMaskRecord */
        0xC0, /* DTCSeverityMaskRecord [Severity Mask] */
        0x01, /* DTCSeverityMaskRecord [Status Mask] */
    };

    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    /* Response per ISO14229-1 2020 Table 369 */
    const uint8_t EXPECTED_RESP[] = {
        0x59, /* Response SID */
        0x08, /* reportType = SubFunction */
        0x7F, /* DTCStatusAvailabilityMask */
        0x40, /* DTCSeverityRecord#1 [Severity] */
        0x10, /* DTCSeverityRecord#1 [Functional Unit] */
        0x08, /* DTCSeverityRecord#1 [High Byte] */
        0x05, /* DTCSeverityRecord#1 [Middle Byte] */
        0x11, /* DTCSeverityRecord#1 [Low Byte] */
        0x2F, /* DTCSeverityRecord#1 [Status] */
    };

    /* the client transport should receive a positive response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP, sizeof(EXPECTED_RESP));
}

void test_0x19_sub_0x08_no_dtc(void **state) {
    Env_t *e = *state;
    uint8_t buf[512] = {0};

    uint8_t ResponseData[] = {
        0x7F,
    };
    Test0x19FnData_t fn_data = {.data = ResponseData, .len = sizeof(ResponseData)};

    e->server->fn = fn_test_0x19;
    e->server->fn_data = &fn_data;

    const uint8_t REQ[] = {
        0x19, /* SID */
        0x08, /* reportNumberOfDTCBySeverityMaskRecord */
        0xC0, /* DTCSeverityMaskRecord [Severity Mask] */
        0x01, /* DTCSeverityMaskRecord [Status Mask] */
    };

    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    const uint8_t EXPECTED_RESP[] = {
        0x59, /* Response SID */
        0x08, /* reportType = SubFunction */
        0x7F, /* DTCStatusAvailabilityMask */
    };

    /* the client transport should receive a positive response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP, sizeof(EXPECTED_RESP));
}

// ISO14229-1 2020 12.3.5.11Example #10 - ReadDTCInformation, SubFunction =
// reportSeverityInformationOfDTC
void test_0x19_sub_0x09(void **state) {
    Env_t *e = *state;
    uint8_t buf[512] = {0};

    uint8_t ResponseData[] = {0x7F, 0x40, 0x10, 0x08, 0x05, 0x11, 0x2F};
    Test0x19FnData_t fn_data = {.data = ResponseData, .len = sizeof(ResponseData)};

    e->server->fn = fn_test_0x19;
    e->server->fn_data = &fn_data;

    /* Request per ISO14229-1 2020 Table 370 */
    const uint8_t REQ[] = {
        0x19, /* SID */
        0x09, /* reportSeverityInformationOfDTC */
        0x08, /* DTC Mask record [High Byte] */
        0x05, /* DTC Mask record [Middle Byte] */
        0x11, /* DTC Mask record [Low Byte] */
    };

    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    /* Response per ISO14229-1 2020 Table 371 */
    const uint8_t EXPECTED_RESP[] = {
        0x59, /* Response SID */
        0x09, /* reportType = SubFunction */
        0x7F, /* DTCStatusAvailabilityMask */
        0x40, /* DTCSeverityRecord#1 [Severity] */
        0x10, /* DTCSeverityRecord#1 [Functional Unit] */
        0x08, /* DTCSeverityRecord#1 [High Byte] */
        0x05, /* DTCSeverityRecord#1 [Middle Byte] */
        0x11, /* DTCSeverityRecord#1 [Low Byte] */
        0x2F, /* DTCSeverityRecord#1 [Status] */
    };

    /* the client transport should receive a positive response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP, sizeof(EXPECTED_RESP));
}

void test_0x19_sub_0x09_no_dtc(void **state) {
    Env_t *e = *state;
    uint8_t buf[512] = {0};

    uint8_t ResponseData[] = {
        0x7F,
    };
    Test0x19FnData_t fn_data = {.data = ResponseData, .len = sizeof(ResponseData)};

    e->server->fn = fn_test_0x19;
    e->server->fn_data = &fn_data;

    const uint8_t REQ[] = {
        0x19, /* SID */
        0x09, /* reportSeverityInformationOfDTC */
        0x08, /* DTC Mask record [High Byte] */
        0x05, /* DTC Mask record [Middle Byte] */
        0x11, /* DTC Mask record [Low Byte] */
    };

    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    const uint8_t EXPECTED_RESP[] = {
        0x59, /* Response SID */
        0x09, /* reportType = SubFunction */
        0x7F, /* DTCStatusAvailabilityMask */
    };

    /* the client transport should receive a positive response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP, sizeof(EXPECTED_RESP));
}

// ISO14229-1 2020 12.3.5.12 Example #11 – ReadDTCInformation - SubFunction =
// reportSupportedDTCs
void test_0x19_sub_0x0A(void **state) {
    Env_t *e = *state;
    uint8_t buf[512] = {0};

    uint8_t ResponseData[] = {0x7F, 0x12, 0x34, 0x56, 0x24, 0x23, 0x45,
                              0x05, 0x00, 0xAB, 0xCD, 0x01, 0x2F};
    Test0x19FnData_t fn_data = {.data = ResponseData, .len = sizeof(ResponseData)};

    e->server->fn = fn_test_0x19;
    e->server->fn_data = &fn_data;

    /* Request per ISO14229-1 2020 Table 375 */
    const uint8_t REQ[] = {
        0x19, /* SID */
        0x0A, /* reportSupportedDTC */

    };

    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    /* Response per ISO14229-1 2020 Table 376 */
    const uint8_t EXPECTED_RESP[] = {
        0x59, /* Response SID */
        0x0A, /* reportType = SubFunction */
        0x7F, /* DTCStatusAvailabilityMask */
        0x12, /* DTCAndStatusRecord#1 [High Byte] */
        0x34, /* DTCAndStatusRecord#1 [Middle Byte] */
        0x56, /* DTCAndStatusRecord#1 [Low Byte] */
        0x24, /* DTCAndStatusRecord#1 [Status] */
        0x23, /* DTCAndStatusRecord#2 [High Byte] */
        0x45, /* DTCAndStatusRecord#2 [Middle Byte] */
        0x05, /* DTCAndStatusRecord#2 [Low Byte] */
        0x00, /* DTCAndStatusRecord#2 [Status] */
        0xAB, /* DTCAndStatusRecord#3 [High Byte] */
        0xCD, /* DTCAndStatusRecord#3 [Middle Byte] */
        0x01, /* DTCAndStatusRecord#3 [Low Byte] */
        0x2F, /* DTCAndStatusRecord#3 [Status] */
    };

    /* the client transport should receive a positive response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP, sizeof(EXPECTED_RESP));
}

// ISO14229-1 2020 12.3.5.13Example #12 - ReadDTCInformation, SubFunction =
// reportFirstTestFailedDTC, information available
void test_0x19_sub_0x0B(void **state) {
    Env_t *e = *state;
    uint8_t buf[512] = {0};

    uint8_t ResponseData[] = {0xFF, 0x12, 0x34, 0x56, 0x26};
    Test0x19FnData_t fn_data = {.data = ResponseData, .len = sizeof(ResponseData)};

    e->server->fn = fn_test_0x19;
    e->server->fn_data = &fn_data;

    /* Request per ISO14229-1 2020 Table 378 */
    const uint8_t REQ[] = {
        0x19, /* SID */
        0x0B, /* reportFirstTestFailedDTC */

    };

    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    /* Response per ISO14229-1 2020 Table 376 */
    const uint8_t EXPECTED_RESP[] = {
        0x59, /* Response SID */
        0x0B, /* reportType = SubFunction */
        0xFF, /* DTCStatusAvailabilityMask */
        0x12, /* DTCAndStatusRecord [High Byte] */
        0x34, /* DTCAndStatusRecord [Middle Byte] */
        0x56, /* DTCAndStatusRecord [Low Byte] */
        0x26, /* DTCAndStatusRecord [Status] */
    };

    /* the client transport should receive a positive response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP, sizeof(EXPECTED_RESP));
}

// ISO14229-1 2020 12.3.5.14Example #13 - ReadDTCInformation, SubFunction =
// reportFirstTestFailedDTC, no information available
void test_0x19_sub_0x0B_no_info(void **state) {
    Env_t *e = *state;
    uint8_t buf[512] = {0};

    uint8_t ResponseData[] = {0xFF};
    Test0x19FnData_t fn_data = {.data = ResponseData, .len = sizeof(ResponseData)};

    e->server->fn = fn_test_0x19;
    e->server->fn_data = &fn_data;

    /* Request per ISO14229-1 2020 Table 380 */
    const uint8_t REQ[] = {
        0x19, /* SID */
        0x0B, /* reportFirstTestFailedDTC */

    };

    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    /* Response per ISO14229-1 2020 Table 381 */
    const uint8_t EXPECTED_RESP[] = {
        0x59, /* Response SID */
        0x0B, /* reportType = SubFunction */
        0xFF, /* DTCStatusAvailabilityMask */
    };

    /* the client transport should receive a positive response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP, sizeof(EXPECTED_RESP));
}

// closely follows ISO14229-1 2020 12.3.5.13Example #12 but for SubFunction =
// reportMostRecentTestFailedDTC
void test_0x19_sub_0x0D(void **state) {
    Env_t *e = *state;
    uint8_t buf[512] = {0};

    uint8_t ResponseData[] = {0xFF, 0x12, 0x34, 0x56, 0x26};
    Test0x19FnData_t fn_data = {.data = ResponseData, .len = sizeof(ResponseData)};

    e->server->fn = fn_test_0x19;
    e->server->fn_data = &fn_data;

    /* Request per ISO14229-1 2020 Table 378 */
    const uint8_t REQ[] = {
        0x19, /* SID */
        0x0D, /* reportMostRecentTestFailedDTC */

    };

    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    /* Response per ISO14229-1 2020 Table 376 */
    const uint8_t EXPECTED_RESP[] = {
        0x59, /* Response SID */
        0x0D, /* reportType = SubFunction */
        0xFF, /* DTCStatusAvailabilityMask */
        0x12, /* DTCAndStatusRecord [High Byte] */
        0x34, /* DTCAndStatusRecord [Middle Byte] */
        0x56, /* DTCAndStatusRecord [Low Byte] */
        0x26, /* DTCAndStatusRecord [Status] */
    };

    /* the client transport should receive a positive response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP, sizeof(EXPECTED_RESP));
}

// closely follows ISO14229-1 2020 12.3.5.14Example #13 but for SubFunction =
// reportMostRecentTestFailedDTC
void test_0x19_sub_0x0D_no_info(void **state) {
    Env_t *e = *state;
    uint8_t buf[512] = {0};

    uint8_t ResponseData[] = {0xFF};
    Test0x19FnData_t fn_data = {.data = ResponseData, .len = sizeof(ResponseData)};

    e->server->fn = fn_test_0x19;
    e->server->fn_data = &fn_data;

    /* Request per ISO14229-1 2020 Table 380 */
    const uint8_t REQ[] = {
        0x19, /* SID */
        0x0D, /* reportMostRecentTestFailedDTC */

    };

    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    /* Response per ISO14229-1 2020 Table 381 */
    const uint8_t EXPECTED_RESP[] = {
        0x59, /* Response SID */
        0x0D, /* reportType = SubFunction */
        0xFF, /* DTCStatusAvailabilityMask */
    };

    /* the client transport should receive a positive response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP, sizeof(EXPECTED_RESP));
}

// closely follows ISO14229-1 2020 12.3.5.13Example #12 but for SubFunction =
// reportFirstConfirmedDTC
void test_0x19_sub_0x0C(void **state) {
    Env_t *e = *state;
    uint8_t buf[512] = {0};

    uint8_t ResponseData[] = {0xFF, 0x12, 0x34, 0x56, 0x2E};
    Test0x19FnData_t fn_data = {.data = ResponseData, .len = sizeof(ResponseData)};

    e->server->fn = fn_test_0x19;
    e->server->fn_data = &fn_data;

    /* Request per ISO14229-1 2020 Table 378 */
    const uint8_t REQ[] = {
        0x19, /* SID */
        0x0C, /* reportFirstConfirmedDTC */

    };

    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    /* Response per ISO14229-1 2020 Table 376 */
    const uint8_t EXPECTED_RESP[] = {
        0x59, /* Response SID */
        0x0C, /* reportType = SubFunction */
        0xFF, /* DTCStatusAvailabilityMask */
        0x12, /* DTCAndStatusRecord [High Byte] */
        0x34, /* DTCAndStatusRecord [Middle Byte] */
        0x56, /* DTCAndStatusRecord [Low Byte] */
        0x2E, /* DTCAndStatusRecord [Status] */
    };

    /* the client transport should receive a positive response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP, sizeof(EXPECTED_RESP));
}

// closely follows ISO14229-1 2020 12.3.5.14Example #13 but for SubFunction =
// reportFirstConfirmedDTC
void test_0x19_sub_0x0C_no_info(void **state) {
    Env_t *e = *state;
    uint8_t buf[512] = {0};

    uint8_t ResponseData[] = {0xFF};
    Test0x19FnData_t fn_data = {.data = ResponseData, .len = sizeof(ResponseData)};

    e->server->fn = fn_test_0x19;
    e->server->fn_data = &fn_data;

    /* Request per ISO14229-1 2020 Table 380 */
    const uint8_t REQ[] = {
        0x19, /* SID */
        0x0C, /* reportFirstConfirmedDTC */

    };

    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    /* Response per ISO14229-1 2020 Table 381 */
    const uint8_t EXPECTED_RESP[] = {
        0x59, /* Response SID */
        0x0C, /* reportType = SubFunction */
        0xFF, /* DTCStatusAvailabilityMask */
    };

    /* the client transport should receive a positive response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP, sizeof(EXPECTED_RESP));
}

// closely follows ISO14229-1 2020 12.3.5.13Example #12 but for SubFunction =
// reportMostRecentConfirmedDTC
void test_0x19_sub_0x0E(void **state) {
    Env_t *e = *state;
    uint8_t buf[512] = {0};

    uint8_t ResponseData[] = {0xFF, 0x12, 0x34, 0x56, 0x2E};
    Test0x19FnData_t fn_data = {.data = ResponseData, .len = sizeof(ResponseData)};

    e->server->fn = fn_test_0x19;
    e->server->fn_data = &fn_data;

    /* Request per ISO14229-1 2020 Table 378 */
    const uint8_t REQ[] = {
        0x19, /* SID */
        0x0E, /* reportMostRecentConfirmedDTC */
    };

    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    /* Response per ISO14229-1 2020 Table 376 */
    const uint8_t EXPECTED_RESP[] = {
        0x59, /* Response SID */
        0x0E, /* reportType = SubFunction */
        0xFF, /* DTCStatusAvailabilityMask */
        0x12, /* DTCAndStatusRecord [High Byte] */
        0x34, /* DTCAndStatusRecord [Middle Byte] */
        0x56, /* DTCAndStatusRecord [Low Byte] */
        0x2E, /* DTCAndStatusRecord [Status] */
    };

    /* the client transport should receive a positive response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP, sizeof(EXPECTED_RESP));
}

// closely follows ISO14229-1 2020 12.3.5.14Example #13 but for SubFunction =
// reportMostRecentConfirmedDTC
void test_0x19_sub_0x0E_no_info(void **state) {
    Env_t *e = *state;
    uint8_t buf[512] = {0};

    uint8_t ResponseData[] = {0xFF};
    Test0x19FnData_t fn_data = {.data = ResponseData, .len = sizeof(ResponseData)};

    e->server->fn = fn_test_0x19;
    e->server->fn_data = &fn_data;

    /* Request per ISO14229-1 2020 Table 380 */
    const uint8_t REQ[] = {
        0x19, /* SID */
        0x0E, /* reportMostRecentConfirmedDTC */

    };

    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    /* Response per ISO14229-1 2020 Table 381 */
    const uint8_t EXPECTED_RESP[] = {
        0x59, /* Response SID */
        0x0E, /* reportType = SubFunction */
        0xFF, /* DTCStatusAvailabilityMask */
    };

    /* the client transport should receive a positive response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP, sizeof(EXPECTED_RESP));
}

// ISO14229-1 2020 12.3.5.15Example #14 - ReadDTCInformation, SubFunction =
// reportDTCFaultDetectionCounter
// There is a descrepancy in the example and the definition in that the example
// sends an additional StatusOfDTC byte before the DTCFaultDetectionCounter byte.
// This is removed here
void test_0x19_sub_0x14(void **state) {
    Env_t *e = *state;
    uint8_t buf[512] = {0};

    uint8_t ResponseData[] = {0x12, 0x34, 0x56, 0x60};
    Test0x19FnData_t fn_data = {.data = ResponseData, .len = sizeof(ResponseData)};

    e->server->fn = fn_test_0x19;
    e->server->fn_data = &fn_data;

    /* Request per ISO14229-1 2020 Table 382 */
    const uint8_t REQ[] = {
        0x19, /* SID */
        0x14, /* reportDTCFaultDetectionCounter */
    };

    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    /* Response per ISO14229-1 2020 Table 383 */
    const uint8_t EXPECTED_RESP[] = {
        0x59, /* Response SID */
        0x14, /* reportType = SubFunction */
        0x12, /* DTCAndStatusRecord [High Byte] */
        0x34, /* DTCAndStatusRecord [Middle Byte] */
        0x56, /* DTCAndStatusRecord [Low Byte] */
        0x60, /* DTCFaultDetectionCounter */
    };

    /* the client transport should receive a positive response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP, sizeof(EXPECTED_RESP));
}

// closely follows ISO14229-1 2020 12.3.5.13Example #12 but for SubFunction =
// reportDTCWithPermanentStatus
void test_0x19_sub_0x15(void **state) {
    Env_t *e = *state;
    uint8_t buf[512] = {0};

    uint8_t ResponseData[] = {0xFF, 0x12, 0x34, 0x56, 0x26};
    Test0x19FnData_t fn_data = {.data = ResponseData, .len = sizeof(ResponseData)};

    e->server->fn = fn_test_0x19;
    e->server->fn_data = &fn_data;

    /* Request per ISO14229-1 2020 Table 378 */
    const uint8_t REQ[] = {
        0x19, /* SID */
        0x15, /* reportDTCWithPermanentStatus */

    };

    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    /* Response per ISO14229-1 2020 Table 376 */
    const uint8_t EXPECTED_RESP[] = {
        0x59, /* Response SID */
        0x15, /* reportType = SubFunction */
        0xFF, /* DTCStatusAvailabilityMask */
        0x12, /* DTCAndStatusRecord [High Byte] */
        0x34, /* DTCAndStatusRecord [Middle Byte] */
        0x56, /* DTCAndStatusRecord [Low Byte] */
        0x26, /* DTCAndStatusRecord [Status] */
    };

    /* the client transport should receive a positive response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP, sizeof(EXPECTED_RESP));
}

void test_0x19_sub_0x15_no_data(void **state) {
    Env_t *e = *state;
    uint8_t buf[512] = {0};

    uint8_t ResponseData[] = {0xFF};
    Test0x19FnData_t fn_data = {.data = ResponseData, .len = sizeof(ResponseData)};

    e->server->fn = fn_test_0x19;
    e->server->fn_data = &fn_data;

    const uint8_t REQ[] = {
        0x19, /* SID */
        0x15, /* reportDTCWithPermanentStatus */

    };

    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    const uint8_t EXPECTED_RESP[] = {
        0x59, /* Response SID */
        0x15, /* reportType = SubFunction */
        0xFF, /* DTCStatusAvailabilityMask */
    };

    /* the client transport should receive a positive response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP, sizeof(EXPECTED_RESP));
}

// ISO14229-1 2020 12.3.5.16Example #15 - ReadDTCInformation, SubFunction =
// reportDTCExtDataRecordByRecordNumber
void test_0x19_sub_0x16(void **state) {
    Env_t *e = *state;
    uint8_t buf[512] = {0};

    uint8_t ResponseData[] = {0x05, 0x12, 0x34, 0x56, 0x24, 0x17, 0x34, 0x56, 0x24,
                              0x12, 0x34, 0x56, 0x24, 0x17, 0x34, 0x21, 0x24};
    Test0x19FnData_t fn_data = {.data = ResponseData, .len = sizeof(ResponseData)};

    e->server->fn = fn_test_0x19;
    e->server->fn_data = &fn_data;

    /* Request per ISO14229-1 2020 Table 378 */
    const uint8_t REQ[] = {
        0x19, /* SID */
        0x16, /* reportDTCExtDataRecordByNumber */
        0x05, /* DTCExtDataRecordNumber */

    };

    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    /* Response per ISO14229-1 2020 Table 376 */
    const uint8_t EXPECTED_RESP[] = {
        0x59, /* Response SID */
        0x16, /* reportType = SubFunction */
        0x05, /* DTCExtDataRecordNumber */
        0x12, /* DTCAndStatusRecord#1 [High Byte] */
        0x34, /* DTCAndStatusRecord#1 [Middle Byte] */
        0x56, /* DTCAndStatusRecord#1 [Low Byte] */
        0x24, /* DTCAndStatusRecord#1 [Status] */
        0x17, /* DTCExtDataRecord#1 [Byte 1] */
        0x34, /* DTCExtDataRecord#1 [Byte 2] */
        0x56, /* DTCExtDataRecord#1 [Byte 3] */
        0x24, /* DTCExtDataRecord#1 [Byte 4] */
        0x12, /* DTCAndStatusRecord#2 [High Byte] */
        0x34, /* DTCAndStatusRecord#2 [Middle Byte] */
        0x56, /* DTCAndStatusRecord#2 [Low Byte] */
        0x24, /* DTCAndStatusRecord#2 [Status] */
        0x17, /* DTCExtDataRecord#2 [Byte 1] */
        0x34, /* DTCExtDataRecord#2 [Byte 2] */
        0x21, /* DTCExtDataRecord#2 [Byte 3] */
        0x24, /* DTCExtDataRecord#2 [Byte 4] */
    };

    /* the client transport should receive a positive response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP, sizeof(EXPECTED_RESP));
}

void test_0x19_sub_0x16_no_record(void **state) {
    Env_t *e = *state;
    uint8_t buf[512] = {0};

    uint8_t ResponseData[] = {0x05};
    Test0x19FnData_t fn_data = {.data = ResponseData, .len = sizeof(ResponseData)};

    e->server->fn = fn_test_0x19;
    e->server->fn_data = &fn_data;

    const uint8_t REQ[] = {
        0x19, /* SID */
        0x16, /* reportDTCExtDataRecordByNumber */
        0x05, /* DTCExtDataRecordNumber */

    };

    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    const uint8_t EXPECTED_RESP[] = {
        0x59, /* Response SID */
        0x16, /* reportType = SubFunction */
        0x05, /* DTCExtDataRecordNumber */
    };

    /* the client transport should receive a positive response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP, sizeof(EXPECTED_RESP));
}

// closely follows ISO14229-1 2020 12.3.5.3 Example #2 but for SubFunction =
// reportUserDefMemoryDTCByStatusMask
void test_0x19_sub_0x17(void **state) {
    Env_t *e = *state;
    uint8_t buf[512] = {0};

    uint8_t ResponseData[] = {
        0x18, 0x07F, 0x0A, 0x9B, 0x17, 0x24, 0x08, 0x05, 0x11, 0x2F,
    };
    Test0x19FnData_t fn_data = {
        .data = ResponseData, .len = sizeof(ResponseData), .test_identifier = "test_0x19_Sub_0x17"};

    e->server->fn = fn_test_0x19;
    e->server->fn_data = &fn_data;

    /* Request per ISO14229-1 2020 Table 345 with an additional MemorySelection byte */
    const uint8_t REQ[] = {
        0x19, /* SID */
        0x17, /* reportUserDefMemoryDTCByStatusMask */
        0x84, /* DTCStatusMask */
        0x18, /* MemorySelection */
    };

    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    /* Response per ISO14229-1 2020 Table 346 */
    const uint8_t EXPECTED_RESP[] = {
        0x59, /* Response SID */
        0x17, /* SubFunction */
        0x18, /* MemorySelection */
        0x7F, /* DTCStatusAvailabilityMask */
        0x0A, /* DTCAndStatusRecord#1 [High Byte] */
        0x9B, /* DTCAndStatusRecord#1 [Middle Byte] */
        0x17, /* DTCAndStatusRecord#1 [Low Byte] */
        0x24, /* DTCAndStatusRecord#1 [Status Byte] */
        0x08, /* DTCAndStatusRecord#2 [High Byte] */
        0x05, /* DTCAndStatusRecord#2 [Middle Byte] */
        0x11, /* DTCAndStatusRecord#2 [Low Byte] */
        0x2F, /* DTCAndStatusRecord#2 [Status Byte] */
    };

    /* the client transport should receive a positive response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP, sizeof(EXPECTED_RESP));
}

// closely follows ISO14229-1 2020 12.3.5.4 Example #3 but for SubFunction =
// reportUserDefMemoryDTCByStatusMask
void test_0x19_sub_0x17_no_matching_dtc(void **state) {
    Env_t *e = *state;
    uint8_t buf[512] = {0};

    uint8_t ResponseData[] = {
        0x18,
        0x07F,
    };
    Test0x19FnData_t fn_data = {.data = ResponseData,
                                .len = sizeof(ResponseData),
                                .test_identifier = "test_0x19_Sub_0x17_no_matching_dtc"};

    e->server->fn = fn_test_0x19;
    e->server->fn_data = &fn_data;

    /* Request per ISO14229-1 2020 Table 349 with an additional MemorySelection byte */
    const uint8_t REQ[] = {
        0x19, /* SID */
        0x17, /* reportUserDefMemoryDTCByStatusMask */
        0x01, /* DTCStatusMask */
        0x18, /* MemorySelection */
    };

    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    /* Response per ISO14229-1 2020 Table 350 */
    const uint8_t EXPECTED_RESP[] = {
        0x59, /* Response SID */
        0x17, /* SubFunction */
        0x18, /* MemorySelection */
        0x7F, /* DTCStatusAvailabilityMask */
    };

    /* the client transport should receive a positive response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP, sizeof(EXPECTED_RESP));
}

void test_0x19_sub_0x18(void **state) {
    Env_t *e = *state;
    uint8_t buf[512] = {0};

    uint8_t ResponseData[] = {0x18, 0x12, 0x34, 0x56, 0x24, 0x03, 0x02, 0x01, 0xBC, 0x01,
                              0x0B, 0x23, 0x45, 0x67, 0x24, 0x08, 0x01, 0x01, 0xCD, 0xEE};
    Test0x19FnData_t fn_data = {
        .data = ResponseData,
        .len = sizeof(ResponseData),
    };

    e->server->fn = fn_test_0x19;
    e->server->fn_data = &fn_data;

    const uint8_t REQ[] = {
        0x19, /* SID */
        0x18, /* reportUserDefMemoryDTCSnapshotRecordByDTCNumber */
        0x12, /* DTCAndStatusRecord#1 [High Byte] */
        0x34, /* DTCAndStatusRecord#1 [Middle Byte] */
        0x56, /* DTCAndStatusRecord#1 [Low Byte] */
        0x06, /* UserDefDTCSnapshotRecordNumber */
        0x18, /* MemorySelection */
    };

    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    const uint8_t EXPECTED_RESP[] = {
        0x59, /* Response SID */
        0x18, /* reportType = SubFunction  */
        0x18, /* MemorySelection */
        0x12, /* DTCAndStatusRecord#1 [High Byte] */
        0x34, /* DTCAndStatusRecord#1 [Middle Byte] */
        0x56, /* DTCAndStatusRecord#1 [Low Byte] */
        0x24, /* DTCAndStatusRecord#1 [Status Byte] */
        0x03, /* UserDefDTCSnapshotRecordNumber#1 */
        0x02, /* DTCSnaphotRecordNumberOfIdentifiers#1 */
        0x01, /* DataIdentifier#1 [High Byte] */
        0xBC, /* DataIdentifier#1 [Low Byte] */
        0x01, /* SnapshotData#1 [Byte 1] */
        0x0B, /* SnapshotData#1 [Byte 2] */
        0x23, /* DTCAndStatusRecord#1 [High Byte] */
        0x45, /* DTCAndStatusRecord#1 [Middle Byte] */
        0x67, /* DTCAndStatusRecord#1 [Low Byte] */
        0x24, /* DTCAndStatusRecord#1 [Status Byte] */
        0x08, /* UserDefDTCSnapshotRecordNumber#2 */
        0x01, /* DTCSnaphotRecordNumberOfIdentifiers#2 */
        0x01, /* DataIdentifier#2 [High Byte] */
        0xCD, /* DataIdentifier#2 [Low Byte] */
        0xEE, /* SnapshotData#2 [Byte 1] */
    };

    /* the client transport should receive a positive response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP, sizeof(EXPECTED_RESP));
}

void test_0x19_sub_0x18_no_record(void **state) {
    Env_t *e = *state;
    uint8_t buf[512] = {0};

    uint8_t ResponseData[] = {0x18, 0x12, 0x34, 0x56, 0x24};
    Test0x19FnData_t fn_data = {
        .data = ResponseData,
        .len = sizeof(ResponseData),
    };

    e->server->fn = fn_test_0x19;
    e->server->fn_data = &fn_data;

    const uint8_t REQ[] = {
        0x19, /* SID */
        0x18, /* reportUserDefMemoryDTCSnapshotRecordByDTCNumber */
        0x12, /* DTCAndStatusRecord#1 [High Byte] */
        0x34, /* DTCAndStatusRecord#1 [Middle Byte] */
        0x56, /* DTCAndStatusRecord#1 [Low Byte] */
        0x06, /* UserDefDTCSnapshotRecordNumber */
        0x18, /* MemorySelection */
    };

    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    const uint8_t EXPECTED_RESP[] = {
        0x59, /* Response SID */
        0x18, /* reportType = SubFunction  */
        0x18, /* MemorySelection */
        0x12, /* DTCAndStatusRecord#1 [High Byte] */
        0x34, /* DTCAndStatusRecord#1 [Middle Byte] */
        0x56, /* DTCAndStatusRecord#1 [Low Byte] */
        0x24, /* DTCAndStatusRecord#1 [Status Byte] */
    };

    /* the client transport should receive a positive response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP, sizeof(EXPECTED_RESP));
}

// closely follows ISO14229-1 2020 12.3.5.8 Example #7 but for SubFunction =
// reportUserDefMemoryDTCExtDataRecordByDTCNumber
void test_0x19_sub_0x19(void **state) {
    Env_t *e = *state;
    uint8_t buf[512] = {0};

    uint8_t ResponseData[] = {0x18, 0x12, 0x34, 0x56, 0x24, 0x05, 0x17, 0x10, 0x79};
    Test0x19FnData_t fn_data = {.data = ResponseData, .len = sizeof(ResponseData)};

    e->server->fn = fn_test_0x19;
    e->server->fn_data = &fn_data;

    /* Request per ISO14229-1 2020 Table 361 with memory selection byte added*/
    const uint8_t REQ[] = {
        0x19, /* SID */
        0x19, /* reportUserDefMemoryDTCExtDataRecordByDTCNumber */
        0x12, /* DTCAndStatusRecord [High Byte] */
        0x34, /* DTCAndStatusRecord [Middle Byte] */
        0x56, /* DTCAndStatusRecord [Low Byte] */
        0xFF, /* DTCExtDataRecordNumber */
        0x18, /* MemorySelection */
    };

    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    /* Response per ISO14229-1 2020 Table 362 with memory selection byte added */
    const uint8_t EXPECTED_RESP[] = {
        0x59, /* Response SID */
        0x19, /* reportType = SubFunction */
        0x18, /* MemorySelection */
        0x12, /* DTCAndStatusRecord [High Byte] */
        0x34, /* DTCAndStatusRecord [Middle Byte] */
        0x56, /* DTCAndStatusRecord [Low Byte] */
        0x24, /* DTCAndStatusRecord [status of DTC] */
        0x05, /* DTCExtDataRecordNumber#1 */
        0x17, /* DTCExtDataRecord#1 [Byte 1] */
        0x10, /* DTCExtDataRecordNumber#2 */
        0x79, /* DTCExtDataRecord#2 [Byte 1] */
    };

    /* the client transport should receive a positive response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP, sizeof(EXPECTED_RESP));
}

void test_0x19_sub_0x19_no_data(void **state) {
    Env_t *e = *state;
    uint8_t buf[512] = {0};

    uint8_t ResponseData[] = {0x18, 0x12, 0x34, 0x56, 0x24};
    Test0x19FnData_t fn_data = {.data = ResponseData, .len = sizeof(ResponseData)};

    e->server->fn = fn_test_0x19;
    e->server->fn_data = &fn_data;

    const uint8_t REQ[] = {
        0x19, /* SID */
        0x19, /* reportUserDefMemoryDTCExtDataRecordByDTCNumber */
        0x12, /* DTCAndStatusRecord [High Byte] */
        0x34, /* DTCAndStatusRecord [Middle Byte] */
        0x56, /* DTCAndStatusRecord [Low Byte] */
        0xFF, /* DTCExtDataRecordNumber */
        0x18, /* MemorySelection */
    };

    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    const uint8_t EXPECTED_RESP[] = {
        0x59, /* Response SID */
        0x19, /* reportType = SubFunction */
        0x18, /* MemorySelection */
        0x12, /* DTCAndStatusRecord [High Byte] */
        0x34, /* DTCAndStatusRecord [Middle Byte] */
        0x56, /* DTCAndStatusRecord [Low Byte] */
        0x24, /* DTCAndStatusRecord [status of DTC] */
    };

    /* the client transport should receive a positive response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP, sizeof(EXPECTED_RESP));
}

// ISO14229-1 2020 12.3.5.17Example #16 – ReadDTCInformation - SubFunction =
// reportDTCExtendedDataRecordIdentification
void test_0x19_sub_0x1A(void **state) {
    Env_t *e = *state;
    uint8_t buf[512] = {0};

    uint8_t ResponseData[] = {0x7F, 0x91, 0x12, 0x34, 0x56, 0x24, 0x23,
                              0x45, 0x05, 0x00, 0xAB, 0xCD, 0x01, 0x2F};
    Test0x19FnData_t fn_data = {.data = ResponseData, .len = sizeof(ResponseData)};

    e->server->fn = fn_test_0x19;
    e->server->fn_data = &fn_data;

    /* Request per ISO14229-1 2020 Table 388 */
    const uint8_t REQ[] = {
        0x19, /* SID */
        0x1A, /* reportDTCExtendedDataRecordIdentification */
        0x91, /* DTCExtDataRecordNumber */

    };

    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    /* Response per ISO14229-1 2020 Table 389 */
    const uint8_t EXPECTED_RESP[] = {
        0x59, /* Response SID */
        0x1A, /* reportType = SubFunction */
        0x7F, /* DTCStatusAvailabilityMask */
        0x91, /* DTCExtendedRecordNumber */
        0x12, /* DTCAndStatusRecord#1 [High Byte] */
        0x34, /* DTCAndStatusRecord#1 [Middle Byte] */
        0x56, /* DTCAndStatusRecord#1 [Low Byte] */
        0x24, /* DTCAndStatusRecord#1 [Status] */
        0x23, /* DTCAndStatusRecord#2 [High Byte] */
        0x45, /* DTCAndStatusRecord#2 [Middle Byte] */
        0x05, /* DTCAndStatusRecord#2 [Low Byte] */
        0x00, /* DTCAndStatusRecord#2 [Status] */
        0xAB, /* DTCAndStatusRecord#3 [High Byte] */
        0xCD, /* DTCAndStatusRecord#3 [Middle Byte] */
        0x01, /* DTCAndStatusRecord#3 [Low Byte] */
        0x2F, /* DTCAndStatusRecord#3 [Status] */
    };

    /* the client transport should receive a positive response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP, sizeof(EXPECTED_RESP));
}

void test_0x19_sub_0x1A_no_data(void **state) {
    Env_t *e = *state;
    uint8_t buf[512] = {0};

    uint8_t ResponseData[] = {
        0x7F,
    };
    Test0x19FnData_t fn_data = {.data = ResponseData, .len = sizeof(ResponseData)};

    e->server->fn = fn_test_0x19;
    e->server->fn_data = &fn_data;

    const uint8_t REQ[] = {
        0x19, /* SID */
        0x1A, /* reportDTCExtendedDataRecordIdentification */
        0x91, /* DTCExtDataRecordNumber */

    };

    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    const uint8_t EXPECTED_RESP[] = {
        0x59, /* Response SID */
        0x1A, /* reportType = SubFunction */
        0x7F, /* DTCStatusAvailabilityMask */
    };

    /* the client transport should receive a positive response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP, sizeof(EXPECTED_RESP));
}

// ISO14229-1 2020 12.3.5.18Example #17 - ReadDTCInformation, SubFunction =
// reportWWHOBDDTCByMaskRecord
void test_0x19_sub_0x42(void **state) {
    Env_t *e = *state;
    uint8_t buf[512] = {0};

    uint8_t ResponseData[] = {0x33, 0xFF, 0xFF, 0x04, 0x20, 0x25, 0x22, 0x1F, 0x2F};
    Test0x19FnData_t fn_data = {.data = ResponseData, .len = sizeof(ResponseData)};

    e->server->fn = fn_test_0x19;
    e->server->fn_data = &fn_data;

    /* Request per ISO14229-1 2020 Table 390 */
    const uint8_t REQ[] = {
        0x19, /* SID */
        0x42, /* reportWWHOBDDTCByMaskRecord */
        0x33, /* FunctionalGroupIdentifier */
        0x08, /* DTCSeverityMaskRecord [Status] */
        0xFF, /* DTCSeverityMaskRecord [Severity] */
    };

    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    /* Response per ISO14229-1 2020 Table 391 */
    const uint8_t EXPECTED_RESP[] = {
        0x59, /* Response SID */
        0x42, /* reportType = SubFunction */
        0x33, /* FunctionalGroupIdentifier */
        0xFF, /* DTCStatusMaskAvailability */
        0xFF, /* DTCSeverityMaskAvailability */
        0x04, /* DTCFormatIdentifier */
        0x20, /* DTCAndSeverityRecord [Severity] */
        0x25, /* DTCAndSeverityRecord [High Byte] */
        0x22, /* DTCAndSeverityRecord [Middle Byte] */
        0x1F, /* DTCAndSeverityRecord [Low Byte] */
        0x2F, /* DTCAndSeverityRecord [Status] */
    };

    /* the client transport should receive a positive response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP, sizeof(EXPECTED_RESP));
}

void test_0x19_sub_0x42_no_record(void **state) {
    Env_t *e = *state;
    uint8_t buf[512] = {0};

    uint8_t ResponseData[] = {0x33, 0xFF, 0xFF, 0x04};
    Test0x19FnData_t fn_data = {.data = ResponseData, .len = sizeof(ResponseData)};

    e->server->fn = fn_test_0x19;
    e->server->fn_data = &fn_data;

    const uint8_t REQ[] = {
        0x19, /* SID */
        0x42, /* reportWWHOBDDTCByMaskRecord */
        0x33, /* FunctionalGroupIdentifier */
        0x08, /* DTCSeverityMaskRecord [Status] */
        0xFF, /* DTCSeverityMaskRecord [Severity] */
    };

    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    const uint8_t EXPECTED_RESP[] = {
        0x59, /* Response SID */
        0x42, /* reportType = SubFunction */
        0x33, /* FunctionalGroupIdentifier */
        0xFF, /* DTCStatusMaskAvailability */
        0xFF, /* DTCSeverityMaskAvailability */
        0x04, /* DTCFormatIdentifier */
    };

    /* the client transport should receive a positive response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP, sizeof(EXPECTED_RESP));
}

// ISO14229-1 2020 12.3.5.19Example #18 - ReadDTCInformation, SubFunction =
// reportOBDDTCWithPermanentStatus, matching DTCs returned
void test_0x19_sub_0x55(void **state) {
    Env_t *e = *state;
    uint8_t buf[512] = {0};

    uint8_t ResponseData[] = {0x33, 0xFF, 0x04, 0x08, 0x05, 0x11, 0x2F};
    Test0x19FnData_t fn_data = {.data = ResponseData, .len = sizeof(ResponseData)};

    e->server->fn = fn_test_0x19;
    e->server->fn_data = &fn_data;

    /* Request per ISO14229-1 2020 Table 394 */
    const uint8_t REQ[] = {
        0x19, /* SID */
        0x55, /* reportWWHOBDDTCWithPermanentStatus */
        0x33, /* FunctionalGroupIdentifier */
    };

    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    /* Response per ISO14229-1 2020 Table 395 */
    const uint8_t EXPECTED_RESP[] = {
        0x59, /* Response SID */
        0x55, /* reportType = SubFunction */
        0x33, /* FunctionalGroupIdentifier */
        0xFF, /* DTCStatusAvailabilityMask */
        0x04, /* DTCFormatIdentifier */
        0x08, /* DTCAndStatusRecord#1 [High Byte] */
        0x05, /* DTCAndStatusRecord#1 [Middle Byte] */
        0x11, /* DTCAndStatusRecord#1 [Low Byte] */
        0x2F, /* DTCAndStatusRecord#1 [Status] */
    };

    /* the client transport should receive a positive response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP, sizeof(EXPECTED_RESP));
}

void test_0x19_sub_0x55_no_record(void **state) {
    Env_t *e = *state;
    uint8_t buf[512] = {0};

    uint8_t ResponseData[] = {0x33, 0xFF, 0x04};
    Test0x19FnData_t fn_data = {.data = ResponseData, .len = sizeof(ResponseData)};

    e->server->fn = fn_test_0x19;
    e->server->fn_data = &fn_data;

    const uint8_t REQ[] = {
        0x19, /* SID */
        0x55, /* reportWWHOBDDTCWithPermanentStatus */
        0x33, /* FunctionalGroupIdentifier */
    };

    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    const uint8_t EXPECTED_RESP[] = {
        0x59, /* Response SID */
        0x55, /* reportType = SubFunction */
        0x33, /* FunctionalGroupIdentifier */
        0xFF, /* DTCStatusAvailabilityMask */
        0x04, /* DTCFormatIdentifier */
    };

    /* the client transport should receive a positive response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP, sizeof(EXPECTED_RESP));
}

// ISO14229-1 2020 12.3.5.20Example #19 – ReadDTCInformation - SubFunction =
// reportDTCByReadinessGroupIdentifier
void test_0x19_sub_0x56(void **state) {
    Env_t *e = *state;
    uint8_t buf[512] = {0};

    uint8_t ResponseData[] = {0x33, 0xFF, 0x04, 0x01, 0x12, 0x34, 0x56, 0x24,
                              0x23, 0x45, 0x05, 0x00, 0xAB, 0xCD, 0x01, 0x2F};
    Test0x19FnData_t fn_data = {.data = ResponseData, .len = sizeof(ResponseData)};

    e->server->fn = fn_test_0x19;
    e->server->fn_data = &fn_data;

    /* Request per ISO14229-1 2020 Table 396 */
    const uint8_t REQ[] = {
        0x19, /* SID */
        0x56, /* reportDTCInformationByDTCReadinessGroupIdentifier */
        0x33, /* FunctionalGroupIdentifier */
        0x01, /* DTCReadinessGroupIdentifier */
    };

    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    /* Response per ISO14229-1 2020 Table 397 */
    const uint8_t EXPECTED_RESP[] = {
        0x59, /* Response SID */
        0x56, /* reportType = SubFunction */
        0x33, /* FunctionalGroupIdentifier */
        0xFF, /* DTCStatusAvailabilityMask */
        0x04, /* DTCFormatIdentifier */
        0x01, /* DTCReadinessGroupIdentifier */
        0x12, /* DTCAndStatusRecord#1 [High Byte] */
        0x34, /* DTCAndStatusRecord#1 [Middle Byte] */
        0x56, /* DTCAndStatusRecord#1 [Low Byte] */
        0x24, /* DTCAndStatusRecord#1 [Status] */
        0x23, /* DTCAndStatusRecord#2 [High Byte] */
        0x45, /* DTCAndStatusRecord#2 [Middle Byte] */
        0x05, /* DTCAndStatusRecord#2 [Low Byte] */
        0x00, /* DTCAndStatusRecord#2 [Status] */
        0xAB, /* DTCAndStatusRecord#3 [High Byte] */
        0xCD, /* DTCAndStatusRecord#3 [Middle Byte] */
        0x01, /* DTCAndStatusRecord#3 [Low Byte] */
        0x2F, /* DTCAndStatusRecord#3 [Status] */
    };

    /* the client transport should receive a positive response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP, sizeof(EXPECTED_RESP));
}

void test_0x19_sub_0x56_no_record(void **state) {
    Env_t *e = *state;
    uint8_t buf[512] = {0};

    uint8_t ResponseData[] = {0x33, 0xFF, 0x04, 0x01};
    Test0x19FnData_t fn_data = {.data = ResponseData, .len = sizeof(ResponseData)};

    e->server->fn = fn_test_0x19;
    e->server->fn_data = &fn_data;

    const uint8_t REQ[] = {
        0x19, /* SID */
        0x56, /* reportDTCInformationByDTCReadinessGroupIdentifier */
        0x33, /* FunctionalGroupIdentifier */
        0x01, /* DTCReadinessGroupIdentifier */
    };

    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    const uint8_t EXPECTED_RESP[] = {
        0x59, /* Response SID */
        0x56, /* reportType = SubFunction */
        0x33, /* FunctionalGroupIdentifier */
        0xFF, /* DTCStatusAvailabilityMask */
        0x04, /* DTCFormatIdentifier */
        0x01, /* DTCReadinessGroupIdentifier */
    };

    /* the client transport should receive a positive response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP, sizeof(EXPECTED_RESP));
}

int fn_test_0x19_subfunc_not_suported(UDSServer_t *srv, UDSEvent_t ev, void *arg) {
    /* This function should never be called */
    TEST_INT_EQUAL(1, 2);
    return UDS_PositiveResponse;
}

void test_0x19_subfunc_not_suported(void **state) {
    Env_t *e = *state;
    uint8_t buf[512] = {0};

    /* We should not reach this function */
    e->server->fn = fn_test_0x19_subfunc_not_suported;
    e->server->fn_data = NULL;

    /* Request with unknown subfunc */
    const uint8_t REQ[] = {
        0x19, /* SID */
        0x57, /* Unknown SubFunc */
        0x33,
        0x01,
    };

    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    const uint8_t EXPECTED_RESP[] = {
        0x7F, /* Response SID */
        0x19, /* Unknown SubFunc */
        0x12, /* NRC: SubFunctionNotSupported */
    };

    /* the client transport should receive a positive response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP, sizeof(EXPECTED_RESP));
}

int fn_test_0x19_invalid_req_len(UDSServer_t *srv, UDSEvent_t ev, void *arg) {
    /* This function should never be called */
    TEST_INT_EQUAL(1, 2);
    return UDS_PositiveResponse;
}

void test_0x19_invalid_req_len(void **state) {
    Env_t *e = *state;
    uint8_t buf[512] = {0};

    Test0x19FnData_t fn_data = {.data = NULL, .len = 0};

    e->server->fn = fn_test_0x19_invalid_req_len;
    e->server->fn_data = &fn_data;

    /* Invalid Request: Missing DTCStatusMask field */
    const uint8_t REQ[] = {
        0x19, /* SID */
        0x01, /* reportNumberOfDTCByStatusMask */
    };

    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    const uint8_t EXPECTED_RESP[] = {
        0x7F, /* Response SID */
        0x19, /* reportType = SubFunction */
        0x13, /* NRC: IncorrectMessageLengthOrInvalidFormat */
    };

    /* the client transport should receive a positive response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP, sizeof(EXPECTED_RESP));
}

void test_0x19_invalid_subFunction(void **state) {
    Env_t *e = *state;
    uint8_t buf[512] = {0};

    Test0x19FnData_t fn_data = {.data = NULL, .len = 0};

    e->server->fn = fn_test_0x19;
    e->server->fn_data = &fn_data;

    const uint8_t REQ[] = {
        0x19, /* SID */
        0xFF, /* Invalid Subfunction */
    };

    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    const uint8_t EXPECTED_RESP[] = {
        0x7F, /* Response SID */
        0x19, /* reportType = SubFunction */
        0x12, /* NRC: SubFunctionNotSupported */
    };

    /* the client transport should receive a response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP, sizeof(EXPECTED_RESP));
}

void test_0x19_shrink_default_response_len(void **state) {
    Env_t *e = *state;
    uint8_t buf[512] = {0};

    Test0x19FnData_t fn_data = {.test_identifier = "shrink_default_response_len"};

    e->server->fn = fn_test_0x19;
    e->server->fn_data = &fn_data;

    const uint8_t REQ[] = {
        0x19, /* SID */
        0x01, /* reportNumberOfDTCByStatusMask */
        0x08, /* DTCStatusMask */
    };

    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    const uint8_t EXPECTED_RESP[] = {
        0x7F, /* Negative Response SID */
        0x19, /* Original SID */
        0x10, /* NRC: GeneralReject */
    };

    /* the client transport should receive a response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP, sizeof(EXPECTED_RESP));
}

// Container to hold request and responses for malformed response tests
typedef struct {
    uint8_t request[10];
    size_t request_len;
    uint8_t response[20];
    size_t response_len;
} Test0x19FnMalformedResponseData_t;

/* List of requests with malformed responses */
/* SubFunc ID is the second byte (index 1) in the request */
const Test0x19FnMalformedResponseData_t test_0x19_malformed_response_data[] = {
    {
        .request = {0x19, 0x01, 0x08},
        .request_len = 3,
        .response = {0x2F, 0x01, 0x00},
        .response_len = 3,
    },
    {
        .request = {0x19, 0x02, 0x84},
        .request_len = 3,
        .response = {0x7F, 0x0A},
        .response_len = 2,
    },
    {
        .request = {0x19, 0x03},
        .request_len = 2,
        .response = {0x12, 0x34, 0x56, 0x01, 0x12, 0x34, 0x57, 0x02, 0x78, 0x9A, 0xBC},
        .response_len = 11,
    },
    {
        .request = {0x19, 0x04, 0x12, 0x34, 0x56, 0x02},
        .request_len = 6,
        .response = {0x12, 0x34, 0x56},
        .response_len = 3,
    },
    {
        .request = {0x19, 0x05, 0x02},
        .request_len = 3,
        .response = {},
        .response_len = 0,
    },
    {
        .request = {0x19, 0x06, 0x12, 0x34, 0x56, 0xFF},
        .request_len = 6,
        .response = {0x12, 0x34, 0x56},
        .response_len = 3,
    },
    {
        .request = {0x19, 0x07, 0xC0, 0x01},
        .request_len = 4,
        .response = {0x09, 0x01, 0x00},
        .response_len = 3,
    },
    {
        .request = {0x19, 0x08, 0xC0, 0x01},
        .request_len = 4,
        .response = {0x7F, 0x40},
        .response_len = 2,
    },
    {
        .request = {0x19, 0x09, 0x08, 0x05, 0x11},
        .request_len = 5,
        .response = {0x59, 0x09},
        .response_len = 2,
    },
    {
        .request = {0x19, 0x0A},
        .request_len = 2,
        .response = {0x7F, 0x12, 0x34, 0x56, 0x24, 0x23, 0x45, 0x05, 0x00, 0xAB, 0xCD, 0x2F},
        .response_len = 12,
    },
    {
        .request = {0x19, 0x0B},
        .request_len = 2,
        .response = {0xFF, 0x12, 0x34, 0x56},
        .response_len = 4,
    },
    {
        .request = {0x19, 0x0C},
        .request_len = 2,
        .response = {0xFF, 0x12, 0x34, 0x56},
        .response_len = 4,
    },
    {
        .request = {0x19, 0x0D},
        .request_len = 2,
        .response = {0xFF, 0x12, 0x34, 0x56},
        .response_len = 4,
    },
    {
        .request = {0x19, 0x0E},
        .request_len = 2,
        .response = {0xFF, 0x12, 0x34, 0x56},
        .response_len = 4,
    },
    {
        .request = {0x19, 0x14},
        .request_len = 2,
        .response = {0x12, 0x34, 0x56},
        .response_len = 3,
    },
    {
        .request = {0x19, 0x15},
        .request_len = 2,
        .response = {0xFF, 0x12, 0x34, 0x56},
        .response_len = 4,
    },
    {
        .request = {0x19, 0x16, 0x05},
        .request_len = 3,
        .response = {},
        .response_len = 0,
    },
    {
        .request = {0x19, 0x17, 0x84, 0x18},
        .request_len = 4,
        .response = {0x07F},
        .response_len = 1,
    },
    {
        .request = {0x19, 0x18, 0x12, 0x34, 0x56, 0x06, 0x18},
        .request_len = 7,
        .response = {0x18, 0x12, 0x34, 0x56},
        .response_len = 4,
    },
    {
        .request = {0x19, 0x19, 0x12, 0x34, 0x56, 0xFF, 0x18},
        .request_len = 7,
        .response = {0x18, 0x12, 0x34, 0x56},
        .response_len = 4,
    },
    {
        .request = {0x19, 0x1A, 0x91},
        .request_len = 3,
        .response = {0x7F, 0x91},
        .response_len = 2,
    },
    {
        .request = {0x19, 0x42, 0x33, 0x08, 0xFF},
        .request_len = 5,
        .response = {0x33, 0xFF, 0xFF, 0x04, 0x20},
        .response_len = 5,
    },
    {
        .request = {0x19, 0x55, 0x33},
        .request_len = 3,
        .response = {0x33, 0xFF, 0x04, 0x08},
        .response_len = 4,
    },
    {
        .request = {0x19, 0x56, 0x33, 0x01},
        .request_len = 4,
        .response = {},
        .response_len = 0,
    },
};

void test_0x19_malformed_responses(void **state) {
    Env_t *e = *state;
    uint8_t buf[512] = {0};

    size_t num_tests =
        sizeof(test_0x19_malformed_response_data) / sizeof(test_0x19_malformed_response_data[0]);

    for (size_t i = 0; i < num_tests; i++) {

        printf("\t\tTesting subfunction: 0x%02X\n",
               test_0x19_malformed_response_data[i].request[1]);
        memset(buf, 0, sizeof(buf));

        Test0x19FnData_t fn_data = {
            .data = (void *)(test_0x19_malformed_response_data[i].response_len != 0
                                 ? test_0x19_malformed_response_data[i].response
                                 : NULL),
            .len = test_0x19_malformed_response_data[i].response_len,
            .test_identifier = "malformed_response"};

        e->server->fn = fn_test_0x19;
        e->server->fn_data = &fn_data;

        UDSTpSend(e->client_tp, test_0x19_malformed_response_data[i].request,
                  test_0x19_malformed_response_data[i].request_len, NULL);

        // the server should respond with a General Reject negative response within p2 ms
        const uint8_t EXPECTED_RESP[] = {
            0x7F, /* Negative Response SID */
            0x19, /* Original SID */
            0x10, /* NRC: GeneralReject */
        };

        EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                         UDS_CLIENT_DEFAULT_P2_MS);
        TEST_MEMORY_EQUAL(buf, EXPECTED_RESP, sizeof(EXPECTED_RESP));
    }
}

int fn_test_0x22(UDSServer_t *srv, UDSEvent_t ev, void *arg) {
    TEST_INT_EQUAL(UDS_EVT_ReadDataByIdent, ev);

    const uint8_t vin[] = {0x57, 0x30, 0x4C, 0x30, 0x30, 0x30, 0x30, 0x34, 0x33,
                           0x4D, 0x42, 0x35, 0x34, 0x31, 0x33, 0x32, 0x36};
    const uint8_t data_0x010A[] = {0xA6, 0x66, 0x07, 0x50, 0x20, 0x1A,
                                   0x00, 0x63, 0x4A, 0x82, 0x7E};
    const uint8_t data_0x0110[] = {0x8C};

    UDSRDBIArgs_t *r = (UDSRDBIArgs_t *)arg;
    switch (r->dataId) {
    case 0xF190:
        return r->copy(srv, vin, sizeof(vin));
    case 0x010A:
        return r->copy(srv, data_0x010A, sizeof(data_0x010A));
    case 0x0110:
        return r->copy(srv, data_0x0110, sizeof(data_0x0110));
    default:
        return UDS_NRC_RequestOutOfRange;
    }
    return UDS_PositiveResponse;
}

// 11.2.5.2 Example #1 read single dataIdentifier 0xF190
void test_0x22(void **state) {
    Env_t *e = *state;
    uint8_t buf[32] = {0};

    // When a server handler function is installed
    e->server->fn = fn_test_0x22;

    // and a request is sent to the server
    const uint8_t REQ[] = {0x22, 0xF1, 0x90};
    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    // the server should respond with the correct data
    const uint8_t RESP[] = {0x62, 0xF1, 0x90, 0x57, 0x30, 0x4C, 0x30, 0x30, 0x30, 0x30,
                            0x34, 0x33, 0x4D, 0x42, 0x35, 0x34, 0x31, 0x33, 0x32, 0x36};
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) == sizeof(RESP),
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, RESP, sizeof(RESP));
}

void test_0x22_nonexistent(void **state) {
    Env_t *e = *state;
    uint8_t buf[8] = {0};

    // When a server handler function is installed
    e->server->fn = fn_test_0x22;

    // and a request is sent to the server for a nonexistent data identifier
    const uint8_t REQ[] = {0x22, 0xF1, 0x91};
    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    // the server should respond with a negative response
    const uint8_t RESP[] = {0x7F, 0x22, 0x31};
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) == sizeof(RESP),
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, RESP, sizeof(RESP));
}

int fn_test_0x22_misuse(UDSServer_t *srv, UDSEvent_t ev, void *arg) { return UDS_PositiveResponse; }

void test_0x22_misuse(void **state) {
    Env_t *e = *state;
    uint8_t buf[8] = {0};

    // When a server handler function is installed that does not handle the
    // UDS_EVT_ReadDataByIdent event
    e->server->fn = fn_test_0x22_misuse;

    // and a request is sent to the server
    const uint8_t REQ[] = {0x22, 0xF1, 0x90};
    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    // the server should respond with a negative response
    const uint8_t RESP[] = {0x7F, 0x22, 0x10};
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) == sizeof(RESP),
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, RESP, sizeof(RESP));
}

int fn_test_0x23(UDSServer_t *srv, UDSEvent_t ev, void *arg) {
    TEST_INT_EQUAL(ev, UDS_EVT_ReadMemByAddr);
    UDSReadMemByAddrArgs_t *r = (UDSReadMemByAddrArgs_t *)arg;
    TEST_PTR_EQUAL(r->memAddr, (void *)0x20481392);
    TEST_INT_EQUAL(r->memSize, 259);
    return r->copy(srv, srv->fn_data, r->memSize);
}

void test_0x23(void **state) {
    Env_t *e = *state;
    uint8_t buf[512] = {0};

    uint8_t FakeData[259];
    for (int i = 0; i < sizeof(FakeData); i++) {
        FakeData[i] = i % 256;
    }

    uint8_t EXPECTED_RESP[sizeof(FakeData) + 1] = {0x63}; // SID 0x23 + 0x40
    for (int i = 0; i < sizeof(FakeData); i++) {
        EXPECTED_RESP[i + 1] = FakeData[i];
    }

    e->server->fn = fn_test_0x23;
    e->server->fn_data = FakeData;

    // Request per ISO14229-1 2020 Table 200
    const uint8_t REQ[] = {
        0x23, // SID
        0x24, // AddressAndLengthFormatIdentifier
        0x20, // memoryAddress byte #1 (MSB)
        0x48, // memoryAddress byte #2
        0x13, // memoryAddress byte #3
        0x92, // memoryAddress byte #4 (LSB)
        0x01, // memorySize byte #1 (MSB)
        0x03, // memorySize byte #2 (LSB)
    };

    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    // the client transport should receive a positive response within client_p2 ms
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP, sizeof(EXPECTED_RESP));
}

typedef struct {
    const void *expectedMemAddr;
    const size_t expectedMemSize;
    const void *expectedMemData;
} Test0x3DTestFnData_t;

int fn_test_0x3D(UDSServer_t *srv, UDSEvent_t ev, void *arg) {
    Test0x3DTestFnData_t *fnData = (Test0x3DTestFnData_t *)srv->fn_data;

    TEST_INT_EQUAL(ev, UDS_EVT_WriteMemByAddr);
    UDSWriteMemByAddrArgs_t *r = (UDSWriteMemByAddrArgs_t *)arg;

    TEST_PTR_EQUAL(r->memAddr, fnData->expectedMemAddr);
    TEST_INT_EQUAL(r->memSize, fnData->expectedMemSize);
    TEST_MEMORY_EQUAL(r->data, fnData->expectedMemData, r->memSize);

    return UDS_PositiveResponse;
}

void test_0x3D_example_1(void **state) {
    Env_t *e = *state;
    uint8_t buf[512] = {0};

    uint8_t expected_mem_data[] = {0x00, 0x8C};

    Test0x3DTestFnData_t fnData = {
        .expectedMemAddr = (void *)0x00002048,
        .expectedMemSize = 2,
        .expectedMemData = expected_mem_data,
    };

    e->server->fn = fn_test_0x3D;
    e->server->fn_data = &fnData;

    // Request per ISO14229-1 2020 Table 289
    const uint8_t REQ[] = {
        0x3D, // SID
        0x12, // AddressAndLengthFormatIdentifier
        0x20, // memoryAddress byte #1 (MSB)
        0x48, // memoryAddress byte #2 (LSB)
        0x02, // memorySize byte #1
        0x00, // data byte #1
        0x8C, // data byte #2
    };

    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    // the client transport should receive a positive response within client_p2 ms
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);

    // Response per ISO14229-1 2020 Table 290
    const uint8_t EXPECTED_RESP[] = {
        0x7D, // SID 0x3D + 0x40
        0x12, // AddressAndLengthFormatIdentifier
        0x20, // memoryAddress byte #1 (MSB)
        0x48, // memoryAddress byte #2 (LSB)
        0x02, // memorySize byte #1
    };

    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP, sizeof(EXPECTED_RESP));
}

void test_0x3D_example_2(void **state) {
    Env_t *e = *state;
    uint8_t buf[512] = {0};

    uint8_t expected_mem_data[] = {0x00, 0x01, 0x8C};

    Test0x3DTestFnData_t fnData = {
        .expectedMemAddr = (void *)0x00204813,
        .expectedMemSize = 3,
        .expectedMemData = expected_mem_data,
    };

    e->server->fn = fn_test_0x3D;
    e->server->fn_data = &fnData;

    // Request per ISO14229-1 2020 Table 291
    const uint8_t REQ[] = {
        0x3D, // SID
        0x13, // AddressAndLengthFormatIdentifier
        0x20, // memoryAddress byte #1 (MSB)
        0x48, // memoryAddress byte #2
        0x13, // memoryAddress byte #3 (LSB)
        0x03, // memorySize byte #1
        0x00, // data byte #1
        0x01, // data byte #2
        0x8C, // data byte #3
    };

    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    // the client transport should receive a positive response within client_p2 ms
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);

    // Response per ISO14229-1 2020 Table 292
    const uint8_t EXPECTED_RESP[] = {
        0x7D, // SID 0x3D + 0x40
        0x13, // AddressAndLengthFormatIdentifier
        0x20, // memoryAddress byte #1 (MSB)
        0x48, // memoryAddress byte #2
        0x13, // memoryAddress byte #3 (LSB)
        0x03, // memorySize byte #1
    };

    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP, sizeof(EXPECTED_RESP));
}

void test_0x3D_example_3(void **state) {
    Env_t *e = *state;
    uint8_t buf[512] = {0};

    uint8_t expected_mem_data[] = {0x00, 0x01, 0x8C, 0x09, 0xAF};

    Test0x3DTestFnData_t fnData = {
        .expectedMemAddr = (void *)0x000020481309,
        .expectedMemSize = 5,
        .expectedMemData = expected_mem_data,
    };

    e->server->fn = fn_test_0x3D;
    e->server->fn_data = &fnData;

    // Request per ISO14229-1 2020 Table 289
    const uint8_t REQ[] = {
        0x3D, // SID
        0x14, // AddressAndLengthFormatIdentifier
        0x20, // memoryAddress byte #1 (MSB)
        0x48, // memoryAddress byte #2
        0x13, // memoryAddress byte #3
        0x09, // memoryAddress byte #4 (LSB)
        0x05, // memorySize byte #1
        0x00, // data byte #1
        0x01, // data byte #2
        0x8C, // data byte #3
        0x09, // data byte #4
        0xAF, // data byte #5
    };

    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    // the client transport should receive a positive response within client_p2 ms
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);

    // Response per ISO14229-1 2020 Table 290
    const uint8_t EXPECTED_RESP[] = {
        0x7D, // SID 0x3D + 0x40
        0x14, // AddressAndLengthFormatIdentifier
        0x20, // memoryAddress byte #1 (MSB)
        0x48, // memoryAddress byte #2
        0x13, // memoryAddress byte #3
        0x09, // memoryAddress byte #4 (LSB)
        0x05, // memorySize byte #1
    };

    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP, sizeof(EXPECTED_RESP));
}

void test_0x27_level_is_zero_at_init(void **state) {
    Env_t *e = *state;
    TEST_INT_EQUAL(e->server->securityLevel, 0);
}

// Implemented to match IS014229-1 2013 9.4.5.2, 9.4.5.3
int fn_test_0x27_security_access(UDSServer_t *srv, UDSEvent_t ev, void *arg) {
    switch (ev) {
    case UDS_EVT_SecAccessRequestSeed: {
        UDSSecAccessRequestSeedArgs_t *r = (UDSSecAccessRequestSeedArgs_t *)arg;
        const uint8_t seed[] = {0x36, 0x57};
        TEST_INT_NE(r->level, srv->securityLevel);
        return r->copySeed(srv, seed, sizeof(seed));
    }
    case UDS_EVT_SecAccessValidateKey: {
        UDSSecAccessValidateKeyArgs_t *r = (UDSSecAccessValidateKeyArgs_t *)arg;
        const uint8_t expected_key[] = {0xC9, 0xA9};
        if (memcmp(r->key, expected_key, sizeof(expected_key))) {
            return UDS_NRC_SecurityAccessDenied;
        } else {
            return UDS_PositiveResponse;
        }
    }
    default:
        assert(0);
    }
    return UDS_PositiveResponse;
}

// 0x27 SecurityAccess Happy Path
void test_0x27_unlock(void **state) {
    Env_t *e = *state;
    uint8_t buf[8] = {0};

    // When a server handler function is installed
    e->server->fn = fn_test_0x27_security_access;

    // and the anti-brute-force timeout has expired
    EnvRunMillis(e, UDS_SERVER_0x27_BRUTE_FORCE_MITIGATION_BOOT_DELAY_MS + 10);

    // and a seed request is sent to the server
    const uint8_t SEED_REQUEST[] = {0x27, 0x01};
    UDSTpSend(e->client_tp, SEED_REQUEST, sizeof(SEED_REQUEST), NULL);

    // the server should respond with a seed within p2 ms
    const uint8_t SEED_RESPONSE[] = {0x67, 0x01, 0x36, 0x57};
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, SEED_RESPONSE, sizeof(SEED_RESPONSE));

    // and the server security level should still be 0
    TEST_INT_EQUAL(e->server->securityLevel, 0);

    // When an unlock request is sent to the server
    const uint8_t UNLOCK_REQUEST[] = {0x27, 0x02, 0xC9, 0xA9};
    UDSTpSend(e->client_tp, UNLOCK_REQUEST, sizeof(UNLOCK_REQUEST), NULL);

    // the server should respond with a positive response within p2 ms
    const uint8_t UNLOCK_RESPONSE[] = {0x67, 0x02};
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, UNLOCK_RESPONSE, sizeof(UNLOCK_RESPONSE));

    // and the server security level should now be 1
    TEST_INT_EQUAL(e->server->securityLevel, 1);

    // When another seed request is sent to the server
    UDSTpSend(e->client_tp, SEED_REQUEST, sizeof(SEED_REQUEST), NULL);

    // the server should now respond with a "already unlocked" response
    const uint8_t ALREADY_UNLOCKED_RESPONSE[] = {0x67, 0x01, 0x00, 0x00};
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, ALREADY_UNLOCKED_RESPONSE, sizeof(ALREADY_UNLOCKED_RESPONSE));

    // And the server security level should still be 1
    TEST_INT_EQUAL(e->server->securityLevel, 1);
}

void test_0x27_brute_force_prevention_1(void **state) {
    Env_t *e = *state;
    uint8_t buf[8] = {0};

    // When a server handler function is installed and the anti-brute-force timeout has not
    // expired
    e->server->fn = fn_test_0x27_security_access;

    // sending a seed request
    const uint8_t SEED_REQUEST[] = {0x27, 0x01};
    UDSTpSend(e->client_tp, SEED_REQUEST, sizeof(SEED_REQUEST), NULL);

    // should get this response
    const uint8_t NEG_RESPONSE[] = {0x7F, 0x27, 0x37};
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, NEG_RESPONSE, sizeof(NEG_RESPONSE));

    // the server security level should still be 0
    TEST_INT_EQUAL(e->server->securityLevel, 0);
}

void test_0x27_brute_force_prevention_2(void **state) {
    Env_t *e = *state;
    uint8_t buf[8] = {0};

    // When a server handler function is installed
    e->server->fn = fn_test_0x27_security_access;

    // and the anti-brute-force timeout has expired
    EnvRunMillis(e, UDS_SERVER_0x27_BRUTE_FORCE_MITIGATION_BOOT_DELAY_MS + 10);

    // sending a seed request
    const uint8_t SEED_REQUEST[] = {0x27, 0x01};
    UDSTpSend(e->client_tp, SEED_REQUEST, sizeof(SEED_REQUEST), NULL);

    // should get this response
    const uint8_t SEED_RESPONSE[] = {0x67, 0x01, 0x36, 0x57};
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, SEED_RESPONSE, sizeof(SEED_RESPONSE));

    // the server security level should still be 0
    TEST_INT_EQUAL(e->server->securityLevel, 0);

    // sending a bad unlock request
    const uint8_t UNLOCK_REQUEST[] = {0x27, 0x02, 0xFF, 0xFF};
    UDSTpSend(e->client_tp, UNLOCK_REQUEST, sizeof(UNLOCK_REQUEST), NULL);

    // should get a negative response
    const uint8_t NEG_RESPONSE[] = {0x7F, 0x27, 0x33};
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, NEG_RESPONSE, sizeof(NEG_RESPONSE));

    // the server security level should still be 0
    TEST_INT_EQUAL(e->server->securityLevel, 0);

    // and sending another seed request right away
    UDSTpSend(e->client_tp, SEED_REQUEST, sizeof(SEED_REQUEST), NULL);

    // should get a negative response due to brute force prevention
    const uint8_t DENIED[] = {0x7F, 0x27, 0x36};
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS)
    TEST_MEMORY_EQUAL(buf, DENIED, sizeof(DENIED));
}

UDSErr_t fn_test_0x28_comm_ctrl(UDSServer_t *srv, UDSEvent_t ev, void *arg) {
    TEST_INT_EQUAL(ev, UDS_EVT_CommCtrl);

    UDSCommCtrlArgs_t *args = arg;

    switch (args->ctrlType) {
    case 0x01:
        TEST_INT_EQUAL(args->commType, 0x02);
        TEST_INT_EQUAL(args->nodeId, 0x00); /* Default NodeID should be 0 */
        return UDS_PositiveResponse;
    case 0x02:
        srv->r.send_len = 1; /* Force malformed response length */
        return UDS_PositiveResponse;

    case 0x04:
        TEST_INT_EQUAL(args->commType, 0x01);
        TEST_INT_EQUAL(args->nodeId, 0x000A);
        return UDS_PositiveResponse;
    }

    return UDS_NRC_GeneralProgrammingFailure;
}

// ISO14229-1 2020 10.5.5 Message flow example CommunicationControl (disable transmission of network
// management messages)
void test_0x28_comm_ctrl_example1(void **state) {
    Env_t *e = *state;
    uint8_t buf[20] = {0};

    e->server->fn = fn_test_0x28_comm_ctrl;
    e->server->fn_data = NULL;

    /* Request per ISO14229-1 2020 Table 59 */
    const uint8_t REQ[] = {
        0x28, /* SID */
        0x01, /* ControlType */
        0x02, /* CommunicationType */
    };

    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    /* Response per ISO14229-1 2020 Table 60 */
    const uint8_t EXPECTED_RESP1[] = {
        0x68, /* Response SID */
        0x01, /* ControlType */
    };

    /* the client transport should receive a response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP1, sizeof(EXPECTED_RESP1));
}

// ISO14229-1 2020 10.5.6 Message flow example CommunicationControl (switch a remote network into
// the diagnostic-only scheduling mode where the node with address 000A16 is connected to)
void test_0x28_comm_ctrl_example2(void **state) {
    Env_t *e = *state;
    uint8_t buf[20] = {0};

    e->server->fn = fn_test_0x28_comm_ctrl;
    e->server->fn_data = NULL;

    /* Request per ISO14229-1 2020 Table 61 */
    const uint8_t REQ[] = {
        0x28, /* SID */
        0x04, /* ControlType */
        0x01, /* CommunicationType */
        0x00, /* NodeIdentificationNumber [High Byte] */
        0x0A, /* NodeIdentificationNumber [Low Byte] */
    };

    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    /* Response per ISO14229-1 2020 Table 62 */
    const uint8_t EXPECTED_RESP1[] = {
        0x68, /* Response SID */
        0x04, /* ControlType */
    };

    /* the client transport should receive a response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP1, sizeof(EXPECTED_RESP1));
}

void test_0x28_comm_ctrl_invalid_request(void **state) {
    Env_t *e = *state;
    uint8_t buf[20] = {0};

    e->server->fn = fn_test_0x28_comm_ctrl;
    e->server->fn_data = NULL;

    const uint8_t REQ[] = {
        0x28, /* SID */
        0x04, /* ControlType */
        /* MISSING required CommunicationType */
    };

    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    const uint8_t EXPECTED_RESP1[] = {
        0x7F, /* Response SID */
        0x28, /* Original Request SID */
        0x13, /* NRC: IncorrectMessageLengthOrInvalidFormat*/
    };

    /* the client transport should receive a response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP1, sizeof(EXPECTED_RESP1));
}

void test_0x28_comm_ctrl_forced_malformed_response_in_event_handler(void **state) {
    Env_t *e = *state;
    uint8_t buf[20] = {0};

    e->server->fn = fn_test_0x28_comm_ctrl;
    e->server->fn_data = NULL;

    const uint8_t REQ[] = {
        0x28, /* SID */
        0x02, /* ControlType */
        0x02, /* CommunicationType */
    };

    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    const uint8_t EXPECTED_RESP1[] = {
        0x68, /* Response SID */
        0x02, /* Original Request SID */
    };

    /* the client transport should receive a response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP1, sizeof(EXPECTED_RESP1));
}

UDSErr_t fn_test_0x29_auth(UDSServer_t *srv, UDSEvent_t ev, void *arg) {
    TEST_INT_EQUAL(ev, UDS_EVT_Auth);

    UDSAuthArgs_t *args = arg;

    switch (args->type) {
    case UDS_LEV_AT_DA:
        return args->set_auth_state(srv, UDS_AT_DAS);
    case UDS_LEV_AT_VCU: {
        TEST_INT_EQUAL(args->subFuncArgs.verifyCertArgs.commConf, 0x02);
        TEST_INT_EQUAL(args->subFuncArgs.verifyCertArgs.certLen, 0x01);
        uint8_t expected_cert[] = {0xFE};
        TEST_MEMORY_EQUAL(args->subFuncArgs.verifyCertArgs.cert, expected_cert,
                          sizeof(expected_cert));
        TEST_INT_EQUAL(args->subFuncArgs.verifyCertArgs.challengeLen, 0x00);

        args->set_auth_state(srv, UDS_AT_CVOVN);
        const uint8_t data[] = {0x00, 0x01, 0xDD, 0x00, 0x00};
        return args->copy(srv, data, sizeof(data));
    }
    case UDS_LEV_AT_VCB: {
        TEST_INT_EQUAL(args->subFuncArgs.verifyCertArgs.commConf, 0x02);
        TEST_INT_EQUAL(args->subFuncArgs.verifyCertArgs.certLen, 0x01);
        uint8_t expected_cert[] = {0xFE};
        TEST_MEMORY_EQUAL(args->subFuncArgs.verifyCertArgs.cert, expected_cert,
                          sizeof(expected_cert));
        TEST_INT_EQUAL(args->subFuncArgs.verifyCertArgs.challengeLen, 0x03);
        uint8_t expected_challenge[] = {0x11, 0x22, 0x33};
        TEST_MEMORY_EQUAL(args->subFuncArgs.verifyCertArgs.challenge, expected_challenge,
                          sizeof(expected_challenge));

        args->set_auth_state(srv, UDS_AT_CVOVN);
        const uint8_t data[] = {0x00, 0x01, 0xEE, 0x00, 0x02, 0x12, 0x34,
                                0x00, 0x01, 0xAB, 0x00, 0x01, 0xFE};
        return args->copy(srv, data, sizeof(data));
    }
    case UDS_LEV_AT_POWN: {
        TEST_INT_EQUAL(args->subFuncArgs.pownArgs.pownLen, 0x01);
        uint8_t expected_pown[] = {0xFE};
        TEST_MEMORY_EQUAL(args->subFuncArgs.pownArgs.pown, expected_pown, sizeof(expected_pown));
        TEST_INT_EQUAL(args->subFuncArgs.pownArgs.publicKeyLen, 0x03);
        uint8_t expected_key[] = {0x11, 0x22, 0x33};
        TEST_MEMORY_EQUAL(args->subFuncArgs.pownArgs.publicKey, expected_key, sizeof(expected_key));

        args->set_auth_state(srv, UDS_AT_OVAC);
        const uint8_t data[] = {0x00, 0x02, 0xBC, 0xDE};
        return args->copy(srv, data, sizeof(data));
    }
    case UDS_LEV_AT_TC: {
        TEST_INT_EQUAL(args->subFuncArgs.transCertArgs.evalId, 0x1A);
        TEST_INT_EQUAL(args->subFuncArgs.transCertArgs.len, 0x02);
        uint8_t expected_cert[] = {0x43, 0x21};
        TEST_MEMORY_EQUAL(args->subFuncArgs.pownArgs.pown, expected_cert, sizeof(expected_cert));

        return args->set_auth_state(srv, UDS_AT_CV);
    }
    case UDS_LEV_AT_RCFA: {
        TEST_INT_EQUAL(args->subFuncArgs.reqChallengeArgs.commConf, 0x1A);
        uint8_t expected_algo[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                                   0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F};
        TEST_MEMORY_EQUAL(args->subFuncArgs.pownArgs.pown, expected_algo, sizeof(expected_algo));

        args->set_auth_state(srv, UDS_AT_RA);
        const uint8_t data[] = {0x00, 0x02, 0x0B, 0xB0, 0x00, 0x01, 0x99};
        return args->copy(srv, data, sizeof(data));
    }
    case UDS_LEV_AT_VPOWNU:
    case UDS_LEV_AT_VPOWNB: {
        uint8_t expected_algo[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                                   0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F};
        TEST_MEMORY_EQUAL(args->subFuncArgs.verifyPownArgs.algoInd, expected_algo,
                          sizeof(expected_algo));

        TEST_INT_EQUAL(args->subFuncArgs.verifyPownArgs.pownLen, 0x02);
        uint8_t expected_pown[] = {0xFE, 0xDC};
        TEST_MEMORY_EQUAL(args->subFuncArgs.verifyPownArgs.pown, expected_pown,
                          sizeof(expected_pown));

        TEST_INT_EQUAL(args->subFuncArgs.verifyPownArgs.challengeLen, 0x01);
        uint8_t expected_challenge[] = {0x11};
        TEST_MEMORY_EQUAL(args->subFuncArgs.verifyPownArgs.challenge, expected_challenge,
                          sizeof(expected_challenge));

        TEST_INT_EQUAL(args->subFuncArgs.verifyPownArgs.addParamLen, 0x03);
        uint8_t expected_param[] = {0x22, 0x33, 0x44};
        TEST_MEMORY_EQUAL(args->subFuncArgs.verifyPownArgs.addParam, expected_param,
                          sizeof(expected_param));

        args->set_auth_state(srv, UDS_AT_OVAC);
        if (args->type == UDS_LEV_AT_VPOWNU) {
            const uint8_t data[] = {0x00, 0x02, 0x0B, 0xB0};
            return args->copy(srv, data, sizeof(data));
        } else {
            const uint8_t data[] = {0x00, 0x03, 0xC1, 0xC2, 0xC3, 0x00, 0x02, 0x0B, 0xB0};
            return args->copy(srv, data, sizeof(data));
        }
    }
    case UDS_LEV_AT_AC:
        return args->set_auth_state(srv, UDS_AT_ACACRSC);
    }

    return UDS_NRC_GeneralReject;
}

void test_0x29_auth_de_auth(void **state) {
    Env_t *e = *state;
    uint8_t buf[20] = {0};

    e->server->fn = fn_test_0x29_auth;
    e->server->fn_data = NULL;

    const uint8_t REQ[] = {
        0x29,          /* SID */
        UDS_LEV_AT_DA, /* AuthenticationTask */
    };

    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    const uint8_t EXPECTED_RESP[] = {
        0x69,          /* Response SID */
        UDS_LEV_AT_DA, /* AuthenticationTask */
        UDS_AT_DAS,    /* Authentication Return Parameter */
    };

    /* the client transport should receive a response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP, sizeof(EXPECTED_RESP));
}

void test_0x29_req_too_short(void **state) {
    Env_t *e = *state;
    uint8_t buf[20] = {0};

    e->server->fn = fn_test_0x29_auth;
    e->server->fn_data = NULL;

    const uint8_t REQ[] = {
        0x29, /* SID */
              /* Missing required SubFunc */
    };

    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    const uint8_t EXPECTED_RESP[] = {
        0x7F, /* Response SID */
        0x29, /* Original Request SID */
        0x13, /* NRC: IncorrectMessageLengthOrInvalidFormat */
    };

    /* the client transport should receive a response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP, sizeof(EXPECTED_RESP));
}

void test_0x29_auth_verify_certificate_unidirectional(void **state) {
    Env_t *e = *state;
    uint8_t buf[20] = {0};

    e->server->fn = fn_test_0x29_auth;
    e->server->fn_data = NULL;

    const uint8_t REQ[] = {
        0x29,           /* SID */
        UDS_LEV_AT_VCU, /* AuthenticationTask */
        0x02,           /* communicationConfiguration */
        0x00,           /* lengthOfCertificate [High Byte]  */
        0x01,           /* lengthOfCertificate [Low Byte]  */
        0xFE,           /* certificate[0] */
        0x00,           /* lengthOfChallenge [High Byte] */
        0x00,           /* lengthOfChallenge [Low Byte] */
    };

    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    const uint8_t EXPECTED_RESP[] = {
        0x69,           /* Response SID */
        UDS_LEV_AT_VCU, /* AuthenticationTask */
        UDS_AT_CVOVN,   /* authenticationReturnParameter */
        0x00,           /* lengthOfChallenge [High Byte]  */
        0x01,           /* lengthOfChallenge [Low Byte]  */
        0xDD,           /* challenge[0] */
        0x00,           /* lengthOfEphemeralPublicKey [High Byte] */
        0x00,           /* lengthOfEphemeralPublicKey [Low Byte] */
    };

    /* the client transport should receive a response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP, sizeof(EXPECTED_RESP));
}

void test_0x29_auth_verify_certificate_bidirectional(void **state) {
    Env_t *e = *state;
    uint8_t buf[20] = {0};

    e->server->fn = fn_test_0x29_auth;
    e->server->fn_data = NULL;

    const uint8_t REQ[] = {
        0x29,           /* SID */
        UDS_LEV_AT_VCB, /* AuthenticationTask */
        0x02,           /* communicationConfiguration */
        0x00,           /* lengthOfCertificate [High Byte]  */
        0x01,           /* lengthOfCertificate [Low Byte]  */
        0xFE,           /* certificate[0] */
        0x00,           /* lengthOfChallenge [High Byte] */
        0x03,           /* lengthOfChallenge [Low Byte] */
        0x11,           /* challenge[0] */
        0x22,           /* challenge[1] */
        0x33,           /* challenge[2] */
    };

    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    const uint8_t EXPECTED_RESP[] = {
        0x69,           /* Response SID */
        UDS_LEV_AT_VCB, /* AuthenticationTask */
        UDS_AT_CVOVN,   /* authenticationReturnParameter */
        0x00,           /* lengthOfChallenge [High Byte]  */
        0x01,           /* lengthOfChallenge [Low Byte]  */
        0xEE,           /* challenge[0] */
        0x00,           /* lengthOfCertificate [High Byte] */
        0x02,           /* lengthOfCertificate [Low Byte] */
        0x12,           /* certificate[0] */
        0x34,           /* certificate[1] */
        0x00,           /* lengthOfProofOfOwnership [High Byte] */
        0x01,           /* lengthOfProofOfOwnership [Low Byte] */
        0xAB,           /* proofOfOwnership[0] */
        0x00,           /* lengthOfEphemeralPublicKey [High Byte] */
        0x01,           /* lengthOfEphemeralPublicKey [Low Byte] */
        0xFE,           /* ephemeralPublicKey[0] */
    };

    /* the client transport should receive a response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP, sizeof(EXPECTED_RESP));
}

void test_0x29_auth_proof_of_ownership(void **state) {
    Env_t *e = *state;
    uint8_t buf[20] = {0};

    e->server->fn = fn_test_0x29_auth;
    e->server->fn_data = NULL;

    const uint8_t REQ[] = {
        0x29,            /* SID */
        UDS_LEV_AT_POWN, /* AuthenticationTask */
        0x00,            /* lengthOfPOWN [High Byte]  */
        0x01,            /* lengthOfPOWN [Low Byte]  */
        0xFE,            /* POWN[0] */
        0x00,            /* lengthOfEphemeralPublicKey [High Byte] */
        0x03,            /* lengthOfEphemeralPublicKey [Low Byte] */
        0x11,            /* key[0] */
        0x22,            /* key[1] */
        0x33,            /* key[2] */
    };

    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    const uint8_t EXPECTED_RESP[] = {
        0x69,            /* Response SID */
        UDS_LEV_AT_POWN, /* AuthenticationTask */
        UDS_AT_OVAC,     /* authenticationReturnParameter */
        0x00,            /* lengthOfSessionKeyInfo [High Byte]  */
        0x02,            /* lengthOfSessionKeyInfo [Low Byte]  */
        0xBC,            /* sessionKey[0] */
        0xDE,            /* sessionKey[1] */
    };

    /* the client transport should receive a response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP, sizeof(EXPECTED_RESP));
}

void test_0x29_auth_transmit_cert(void **state) {
    Env_t *e = *state;
    uint8_t buf[20] = {0};

    e->server->fn = fn_test_0x29_auth;
    e->server->fn_data = NULL;

    const uint8_t REQ[] = {
        0x29,          /* SID */
        UDS_LEV_AT_TC, /* AuthenticationTask */
        0x1A,          /* certificateEvaluationId */
        0x00,          /* lengthOfCert [High Byte]  */
        0x02,          /* lengthOfCert [Low Byte]  */
        0x43,          /* Cert[0] */
        0x21,          /* Cert[1] */
    };

    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    const uint8_t EXPECTED_RESP[] = {
        0x69,          /* Response SID */
        UDS_LEV_AT_TC, /* AuthenticationTask */
        UDS_AT_CV,     /* authenticationReturnParameter */
    };

    /* the client transport should receive a response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP, sizeof(EXPECTED_RESP));
}

void test_0x29_auth_req_challenge_for_auth(void **state) {
    Env_t *e = *state;
    uint8_t buf[30] = {0};

    e->server->fn = fn_test_0x29_auth;
    e->server->fn_data = NULL;

    const uint8_t REQ[] = {
        0x29,            /* SID */
        UDS_LEV_AT_RCFA, /* AuthenticationTask */
        0x1A,            /* communicationConfiguration */
        0x00,            /* AlgorithmIndicator[0]  */
        0x01,            /* AlgorithmIndicator[1]  */
        0x02,            /* AlgortihmIndicator[2] */
        0x03,            /* AlgortihmIndicator[3] */
        0x04,            /* AlgortihmIndicator[4] */
        0x05,            /* AlgortihmIndicator[5] */
        0x06,            /* AlgortihmIndicator[6] */
        0x07,            /* AlgortihmIndicator[7] */
        0x08,            /* AlgortihmIndicator[8] */
        0x09,            /* AlgortihmIndicator[9] */
        0x0A,            /* AlgortihmIndicator[10] */
        0x0B,            /* AlgortihmIndicator[11] */
        0x0C,            /* AlgortihmIndicator[12] */
        0x0D,            /* AlgortihmIndicator[13] */
        0x0E,            /* AlgortihmIndicator[14] */
        0x0F,            /* AlgortihmIndicator[15] */
        0x10,            /* AlgortihmIndicator[16] */
    };

    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    const uint8_t EXPECTED_RESP[] = {
        0x69,            /* Response SID */
        UDS_LEV_AT_RCFA, /* AuthenticationTask */
        UDS_AT_RA,       /* authenticationReturnParameter */
        0x00,            /* AlgorithmIndicator[0]  */
        0x01,            /* AlgorithmIndicator[1]  */
        0x02,            /* AlgortihmIndicator[2] */
        0x03,            /* AlgortihmIndicator[3] */
        0x04,            /* AlgortihmIndicator[4] */
        0x05,            /* AlgortihmIndicator[5] */
        0x06,            /* AlgortihmIndicator[6] */
        0x07,            /* AlgortihmIndicator[7] */
        0x08,            /* AlgortihmIndicator[8] */
        0x09,            /* AlgortihmIndicator[9] */
        0x0A,            /* AlgortihmIndicator[10] */
        0x0B,            /* AlgortihmIndicator[11] */
        0x0C,            /* AlgortihmIndicator[12] */
        0x0D,            /* AlgortihmIndicator[13] */
        0x0E,            /* AlgortihmIndicator[14] */
        0x0F,            /* AlgortihmIndicator[15] */
        0x00,            /* lengthOfChallenge [High Byte] */
        0x02,            /* lengthOfChallenge [Low Byte] */
        0x0B,            /* challenge[0] */
        0xB0,            /* challenge[1] */
        0x00,            /* lengthOfAdditionalParam [High Byte] */
        0x01,            /* lengthOfAdditionalParam [Low Byte] */
        0x99,            /* additionalParam[0] */
    };

    /* the client transport should receive a response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP, sizeof(EXPECTED_RESP));
}

void test_0x29_auth_verify_pown_unidirectional(void **state) {
    Env_t *e = *state;
    uint8_t buf[30] = {0};

    e->server->fn = fn_test_0x29_auth;
    e->server->fn_data = NULL;

    const uint8_t REQ[] = {
        0x29,              /* SID */
        UDS_LEV_AT_VPOWNU, /* AuthenticationTask */
        0x00,              /* AlgorithmIndicator[0]  */
        0x01,              /* AlgorithmIndicator[1]  */
        0x02,              /* AlgortihmIndicator[2] */
        0x03,              /* AlgortihmIndicator[3] */
        0x04,              /* AlgortihmIndicator[4] */
        0x05,              /* AlgortihmIndicator[5] */
        0x06,              /* AlgortihmIndicator[6] */
        0x07,              /* AlgortihmIndicator[7] */
        0x08,              /* AlgortihmIndicator[8] */
        0x09,              /* AlgortihmIndicator[9] */
        0x0A,              /* AlgortihmIndicator[10] */
        0x0B,              /* AlgortihmIndicator[11] */
        0x0C,              /* AlgortihmIndicator[12] */
        0x0D,              /* AlgortihmIndicator[13] */
        0x0E,              /* AlgortihmIndicator[14] */
        0x0F,              /* AlgortihmIndicator[15] */
        0x00,              /* lengthOfPOWN [High Byte] */
        0x02,              /* lengthOfPOWN [Low Byte]  */
        0xFE,              /* POWN[0] */
        0xDC,              /* POWN[1] */
        0x00,              /* lengthOfChallenge [High Byte] */
        0x01,              /* lengthOfChallenge [Low Byte] */
        0x11,              /* challenge[0] */
        0x00,              /* lengthOfAdditionalParam [High Byte] */
        0x03,              /* lengthOfAdditionalParam [Low Byte] */
        0x22,              /* additionalParam[0] */
        0x33,              /* additionalParam[1] */
        0x44,              /* additionalParam[2] */
    };

    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    const uint8_t EXPECTED_RESP[] = {
        0x69,              /* Response SID */
        UDS_LEV_AT_VPOWNU, /* AuthenticationTask */
        UDS_AT_OVAC,       /* authenticationReturnParameter */
        0x00,              /* AlgorithmIndicator[0]  */
        0x01,              /* AlgorithmIndicator[1]  */
        0x02,              /* AlgortihmIndicator[2] */
        0x03,              /* AlgortihmIndicator[3] */
        0x04,              /* AlgortihmIndicator[4] */
        0x05,              /* AlgortihmIndicator[5] */
        0x06,              /* AlgortihmIndicator[6] */
        0x07,              /* AlgortihmIndicator[7] */
        0x08,              /* AlgortihmIndicator[8] */
        0x09,              /* AlgortihmIndicator[9] */
        0x0A,              /* AlgortihmIndicator[10] */
        0x0B,              /* AlgortihmIndicator[11] */
        0x0C,              /* AlgortihmIndicator[12] */
        0x0D,              /* AlgortihmIndicator[13] */
        0x0E,              /* AlgortihmIndicator[14] */
        0x0F,              /* AlgortihmIndicator[15] */
        0x00,              /* lengthOfSessionKey [High Byte] */
        0x02,              /* lengthOfSessionKey [Low Byte] */
        0x0B,              /* sessionKey[0] */
        0xB0,              /* sessionKey[1] */
    };

    /* the client transport should receive a response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP, sizeof(EXPECTED_RESP));
}

void test_0x29_auth_verify_pown_bidirectional(void **state) {
    Env_t *e = *state;
    uint8_t buf[30] = {0};

    e->server->fn = fn_test_0x29_auth;
    e->server->fn_data = NULL;

    /* Request ist the same as for unidirectional variant */
    const uint8_t REQ[] = {
        0x29,              /* SID */
        UDS_LEV_AT_VPOWNB, /* AuthenticationTask */
        0x00,              /* AlgorithmIndicator[0]  */
        0x01,              /* AlgorithmIndicator[1]  */
        0x02,              /* AlgortihmIndicator[2] */
        0x03,              /* AlgortihmIndicator[3] */
        0x04,              /* AlgortihmIndicator[4] */
        0x05,              /* AlgortihmIndicator[5] */
        0x06,              /* AlgortihmIndicator[6] */
        0x07,              /* AlgortihmIndicator[7] */
        0x08,              /* AlgortihmIndicator[8] */
        0x09,              /* AlgortihmIndicator[9] */
        0x0A,              /* AlgortihmIndicator[10] */
        0x0B,              /* AlgortihmIndicator[11] */
        0x0C,              /* AlgortihmIndicator[12] */
        0x0D,              /* AlgortihmIndicator[13] */
        0x0E,              /* AlgortihmIndicator[14] */
        0x0F,              /* AlgortihmIndicator[15] */
        0x00,              /* lengthOfPOWN [High Byte] */
        0x02,              /* lengthOfPOWN [Low Byte]  */
        0xFE,              /* POWN[0] */
        0xDC,              /* POWN[1] */
        0x00,              /* lengthOfChallenge [High Byte] */
        0x01,              /* lengthOfChallenge [Low Byte] */
        0x11,              /* challenge[0] */
        0x00,              /* lengthOfAdditionalParam [High Byte] */
        0x03,              /* lengthOfAdditionalParam [Low Byte] */
        0x22,              /* additionalParam[0] */
        0x33,              /* additionalParam[1] */
        0x44,              /* additionalParam[2] */
    };

    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    const uint8_t EXPECTED_RESP[] = {
        0x69,              /* Response SID */
        UDS_LEV_AT_VPOWNB, /* AuthenticationTask */
        UDS_AT_OVAC,       /* authenticationReturnParameter */
        0x00,              /* AlgorithmIndicator[0]  */
        0x01,              /* AlgorithmIndicator[1]  */
        0x02,              /* AlgortihmIndicator[2] */
        0x03,              /* AlgortihmIndicator[3] */
        0x04,              /* AlgortihmIndicator[4] */
        0x05,              /* AlgortihmIndicator[5] */
        0x06,              /* AlgortihmIndicator[6] */
        0x07,              /* AlgortihmIndicator[7] */
        0x08,              /* AlgortihmIndicator[8] */
        0x09,              /* AlgortihmIndicator[9] */
        0x0A,              /* AlgortihmIndicator[10] */
        0x0B,              /* AlgortihmIndicator[11] */
        0x0C,              /* AlgortihmIndicator[12] */
        0x0D,              /* AlgortihmIndicator[13] */
        0x0E,              /* AlgortihmIndicator[14] */
        0x0F,              /* AlgortihmIndicator[15] */
        0x00,              /* lengthOfPOWN [High Byte] */
        0x03,              /* lengthOfPOWN [Low Byte]  */
        0xC1,              /* POWN[0] */
        0xC2,              /* POWN[1] */
        0xC3,              /* POWN[2] */
        0x00,              /* lengthOfSessionKey [High Byte] */
        0x02,              /* lengthOfSessionKey [Low Byte] */
        0x0B,              /* sessionKey[0] */
        0xB0,              /* sessionKey[1] */
    };

    /* the client transport should receive a response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP, sizeof(EXPECTED_RESP));
}

void test_0x29_auth_config(void **state) {
    Env_t *e = *state;
    uint8_t buf[20] = {0};

    e->server->fn = fn_test_0x29_auth;
    e->server->fn_data = NULL;

    const uint8_t REQ[] = {
        0x29,          /* SID */
        UDS_LEV_AT_AC, /* AuthenticationTask */
    };

    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    const uint8_t EXPECTED_RESP[] = {
        0x69,           /* Response SID */
        UDS_LEV_AT_AC,  /* AuthenticationTask */
        UDS_AT_ACACRSC, /* AuthenticationConfiguration */
    };

    /* the client transport should receive a response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP, sizeof(EXPECTED_RESP));
}

UDSErr_t fn_test_0x29_auth_timeout(UDSServer_t *srv, UDSEvent_t ev, void *arg) {
    if (ev == UDS_EVT_DiagSessCtrl || ev == UDS_EVT_SessionTimeout) {
        return UDS_OK;
    }

    if (ev == UDS_EVT_AuthTimeout) {
        *(bool *)srv->fn_data = true;
        return UDS_OK;
    }

    return UDS_NRC_SubFunctionNotSupported;
}

void test_0x29_auth_timeout_on_session_timeout(void **state) {
    Env_t *e = *state;
    uint8_t buf[20] = {0};

    bool authTimeoutReceived = false;
    e->server->fn = fn_test_0x29_auth_timeout;
    e->server->fn_data = &authTimeoutReceived;

    const uint8_t REQ[] = {
        0x10,            /* SID */
        UDS_LEV_DS_PRGS, /* Session */
    };

    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    const uint8_t EXPECTED_RESP[] = {
        0x50,            /* Response SID */
        UDS_LEV_DS_PRGS, /* Session */
        /* Session parameter don't interest us here */
    };

    /* the client transport should receive a response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP, sizeof(EXPECTED_RESP));
    TEST_INT_EQUAL(authTimeoutReceived, false);

    /* Provoke Timeout */
    EnvRunMillis(e, 10000);
    TEST_INT_EQUAL(authTimeoutReceived, true);
}

void test_0x29_auth_timeout_on_session_switch_to_default(void **state) {
    Env_t *e = *state;
    uint8_t buf[20] = {0};

    bool authTimeoutReceived = false;
    e->server->fn = fn_test_0x29_auth_timeout;
    e->server->fn_data = &authTimeoutReceived;

    const uint8_t REQ1[] = {
        0x10,            /* SID */
        UDS_LEV_DS_PRGS, /* Session */
    };

    UDSTpSend(e->client_tp, REQ1, sizeof(REQ1), NULL);

    const uint8_t EXPECTED_RESP1[] = {
        0x50,            /* Response SID */
        UDS_LEV_DS_PRGS, /* Session */
        /* Session parameter don't interest us here */
    };

    /* the client transport should receive a response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP1, sizeof(EXPECTED_RESP1));
    TEST_INT_EQUAL(authTimeoutReceived, false);

    const uint8_t REQ2[] = {
        0x10,          /* SID */
        UDS_LEV_DS_DS, /* Session */
    };

    UDSTpSend(e->client_tp, REQ2, sizeof(REQ2), NULL);

    const uint8_t EXPECTED_RESP2[] = {
        0x50,          /* Response SID */
        UDS_LEV_DS_DS, /* Session */
        /* Session parameter don't interest us here */
    };

    /* the client transport should receive a response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP2, sizeof(EXPECTED_RESP2));
    TEST_INT_EQUAL(authTimeoutReceived, true);
}

UDSErr_t fn_test_0x29_example_1(UDSServer_t *srv, UDSEvent_t ev, void *arg) {
    TEST_INT_EQUAL(ev, UDS_EVT_Auth);

    UDSAuthArgs_t *args = arg;

    switch (args->type) {
    case UDS_LEV_AT_AC:
        return args->set_auth_state(srv, UDS_AT_ACAPCE);
    case UDS_LEV_AT_VCU: {
        args->set_auth_state(srv, UDS_AT_CVOVN);

        uint8_t challengeLength[] = {0x00, 0x40};
        args->copy(srv, challengeLength, sizeof(challengeLength));

        uint8_t challenge[] = {0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xB0, 0xB1, 0xB2, 0xB3, 0xB4,
                               0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF,
                               0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA,
                               0xCB, 0xCC, 0xCD, 0xCE, 0xCF, 0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5,
                               0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF, 0xE0,
                               0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0x44};
        args->copy(srv, challenge, sizeof(challenge));

        uint8_t publicKeyLength[] = {0x00, 0x00};
        return args->copy(srv, publicKeyLength, sizeof(publicKeyLength));
    }
    case UDS_LEV_AT_POWN: {
        args->set_auth_state(srv, UDS_AT_OVAC);

        uint8_t sessionKeyLength[] = {0x00, 0x00};
        return args->copy(srv, sessionKeyLength, sizeof(sessionKeyLength));
    }
    }

    return UDS_NRC_GeneralReject;
}

// ISO14229-1 2020 10.6.8.1 Example #1 - Unidirectional Authentication with PKI Certificate Exchange
// without session key establishment (happy path)
// This example skips step 4 as it is not part of the authentication service
void test_0x29_auth_example_1(void **state) {
    Env_t *e = *state;
    uint8_t buf[500] = {0};

    e->server->fn = fn_test_0x29_example_1;
    e->server->fn_data = NULL;

    /* Request per ISO14229-1 2020 Table 87 */
    const uint8_t REQ[] = {
        0x29,          /* Authentication Request SID (ARS) */
        UDS_LEV_AT_AC, /* authenticationTask = authenticationConfiguration (LEV_AT_AC) */
    };

    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    /* Response per ISO14229-1 2020 Table 88 */
    const uint8_t EXPECTED_RESP1[] = {
        0x69,          /* Authentication Response SID (ARS) */
        UDS_LEV_AT_AC, /* authenticationTask = authenticationConfiguration (LEV_AT_AC) */
        UDS_AT_ACAPCE, /* returnValue = Authentication Configuration APCE (RV_ACAPCE) */
    };

    /* the client transport should receive a response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP1, sizeof(EXPECTED_RESP1));

    /* Request per ISO14229-1 2020 Table 89 */
    const uint8_t REQ2[] = {
        0x29,           /* Authentication Request SID (ARS) */
        UDS_LEV_AT_VCU, /* authenticationTask = verifyCertificateUnidirectional (LEV_AT_VCU) */
        0x00,           /* communicationConfiguration = no secure communication (COCO) */

        0x01, /* lengthOfCertificateClient[0] */
        0xF4, /* lengthOfCertificateClient[1] */

        /* certificateClient [0-499] */
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E,
        0x3F, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D,
        0x4E, 0x4F, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C,
        0x5D, 0x5E, 0x5F, 0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x6B,
        0x6C, 0x6D, 0x6E, 0x6F, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A,
        0x7B, 0x7C, 0x7D, 0x7E, 0x7F, 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
        0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F, 0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98,
        0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F, 0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7,
        0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6,
        0xB7, 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF, 0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5,
        0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF, 0xD0, 0xD1, 0xD2, 0xD3, 0xD4,
        0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF, 0xE0, 0xE1, 0xE2, 0xE3,
        0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF, 0xF0, 0xF1, 0xF2,
        0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF, 0x00, 0x01,
        0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,
        0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
        0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E,
        0x2F, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D,
        0x3E, 0x3F, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C,
        0x4D, 0x4E, 0x4F, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B,
        0x5C, 0x5D, 0x5E, 0x5F, 0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A,
        0x6B, 0x6C, 0x6D, 0x6E, 0x6F, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79,
        0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F, 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88,
        0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F, 0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,
        0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F, 0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6,
        0xA7, 0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5,
        0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF, 0xC0, 0xC1, 0xC2, 0xC3, 0xC4,
        0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF, 0xD0, 0xD1, 0xD2, 0xD3,
        0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF, 0xE0, 0xE1, 0xE2,
        0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF, 0xF0, 0xF1,
        0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF, 0x00,
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E,
        0x1F, 0x20, 0x21, 0x22, 0xAD,

        0x00, /* lengthOfChallengeClient[0] */
        0x20, /* lengthOfChallengeClient[1] */

        /* challengeClient [0-31] */
        0xAA, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D,
        0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C,
        0x1D, 0x44};

    UDSTpSend(e->client_tp, REQ2, sizeof(REQ2), NULL);

    /* Response per ISO14229-1 2020 Table 90 */
    const uint8_t EXPECTED_RESP2[] = {
        0x69,           /* Authentication Response SID (ARS) */
        UDS_LEV_AT_VCU, /* authenticationTask = verifyCertificateUnidirectional (LEV_AT_VCU) */
        UDS_AT_CVOVN,   /* returnValue = Certificate verified, Ownership verification necessary
                              (RV_CVOVN) */

        0x00, /* lengthOfChallengeServer[0] */
        0x40, /* lengthOfChallengeServer[1] */

        /* challengeServer [0-63] */
        0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8,
        0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF, 0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7,
        0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF, 0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6,
        0xD7, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF, 0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5,
        0xE6, 0xE7, 0xE8, 0x44,

        /* The example gives additional 2 bytes for the length of the ephemeral public key, but as
           this is not part of the response message definition, it is not set here */
    };

    /* the client transport should receive a response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP2, sizeof(EXPECTED_RESP2));

    /* Request per ISO14229-1 2020 Table 91 */
    const uint8_t REQ3[] = {
        0x29,            /* Authentication Request SID (ARS) */
        UDS_LEV_AT_POWN, /* authenticationTask = proofOfOwnership (LEV_AT_POWN) */

        0x01, /* lengthOfProofOfOwnershipClient[0] */
        0x50, /* lengthOfProofOfOwnershipClient[1] */

        /* proofOfOwnershipClient [0-335] */
        0x7F, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D,
        0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C,
        0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B,
        0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A,
        0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
        0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
        0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F, 0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,
        0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76,
        0x77, 0x78, 0x79, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F, 0x80, 0x81, 0x82, 0x83, 0x84, 0x85,
        0x86, 0x87, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F, 0x90, 0x91, 0x92, 0x93, 0x94,
        0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F, 0xA0, 0xA1, 0xA2, 0xA3,
        0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xB0, 0xB1, 0xB2,
        0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF, 0xC0, 0xC1,
        0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF, 0xD0,
        0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF,
        0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE,
        0xEF, 0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD,
        0xFE, 0xFF, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C,
        0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B,
        0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A,
        0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
        0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
        0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0xB7,

        0x00, /* lengthOfEphemeralPublicKeyClient[0] */
        0x00, /* lengthOfEphemeralPublicKeyClient[1] */
    };

    UDSTpSend(e->client_tp, REQ3, sizeof(REQ3), NULL);

    /* Response per ISO14229-1 2020 Table 92 */
    const uint8_t EXPECTED_RESP3[] = {
        0x69,            /* Authentication Response SID (ARS) */
        UDS_LEV_AT_POWN, /* authenticationTask = proofOfOwnership (LEV_AT_POWN) */
        0x12,            /* returnValue = Ownership verified, Authentication complete (RV_OVAC) */
        0x00,            /* lengthOfSessionKeyInfo[0] (LOSKI) */
        0x00,            /* lengthOfSessionKeyInfo[1] (LOSKI) */
    };

    /* the client transport should receive a response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP3, sizeof(EXPECTED_RESP3));
}

UDSErr_t fn_test_0x29_example_2(UDSServer_t *srv, UDSEvent_t ev, void *arg) {
    TEST_INT_EQUAL(ev, UDS_EVT_Auth);

    UDSAuthArgs_t *args = arg;

    switch (args->type) {
    case UDS_LEV_AT_VCU:
        return UDS_NRC_CertficateVerificationFailedInvalidTimePeriod;
    }

    return UDS_NRC_GeneralReject;
}

// ISO14229-1 2020 10.6.8.2 Example #2 - Unidirectional Authentication with PKI Certificate Exchange
// without session key establishment (failure path)
// This example skips step 2 as it is not part of the authentication service
void test_0x29_auth_example_2(void **state) {
    Env_t *e = *state;
    uint8_t buf[700] = {0};

    e->server->fn = fn_test_0x29_example_2;
    e->server->fn_data = NULL;

    /* Request per ISO14229-1 2020 Table 95 */
    const uint8_t REQ[] = {
        0x29,           /* Authentication Request SID (ARS) */
        UDS_LEV_AT_VCU, /* authenticationTask = verifyCertificateUnidirectional (LEV_AT_VCU) */
        0x00,           /* communicationConfiguration = no secure communication (COCO) */

        0x01, /* lengthOfCertificateClient[0] */
        0xF4, /* lengthOfCertificateClient[1] */

        /* certificateClient [0-499] */
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E,
        0x3F, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D,
        0x4E, 0x4F, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C,
        0x5D, 0x5E, 0x5F, 0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x6B,
        0x6C, 0x6D, 0x6E, 0x6F, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A,
        0x7B, 0x7C, 0x7D, 0x7E, 0x7F, 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
        0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F, 0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98,
        0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F, 0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7,
        0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6,
        0xB7, 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF, 0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5,
        0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF, 0xD0, 0xD1, 0xD2, 0xD3, 0xD4,
        0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF, 0xE0, 0xE1, 0xE2, 0xE3,
        0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF, 0xF0, 0xF1, 0xF2,
        0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF, 0x00, 0x01,
        0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,
        0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
        0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E,
        0x2F, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D,
        0x3E, 0x3F, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C,
        0x4D, 0x4E, 0x4F, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B,
        0x5C, 0x5D, 0x5E, 0x5F, 0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A,
        0x6B, 0x6C, 0x6D, 0x6E, 0x6F, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79,
        0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F, 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88,
        0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F, 0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,
        0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F, 0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6,
        0xA7, 0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5,
        0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF, 0xC0, 0xC1, 0xC2, 0xC3, 0xC4,
        0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF, 0xD0, 0xD1, 0xD2, 0xD3,
        0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF, 0xE0, 0xE1, 0xE2,
        0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF, 0xF0, 0xF1,
        0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF, 0x00,
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E,
        0x1F, 0x20, 0x21, 0x22, 0xAD,

        0x00, /* lengthOfChallengeClient[0] (LOCHCL) */
        0x20, /* lengthOfChallengeClient[1] (LOCHCL) */

        /* challengeClient [0-31] */
        0xAA, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D,
        0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C,
        0x1D, 0x44};

    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    /* Response per ISO14229-1 2020 Table 96 */
    const uint8_t EXPECTED_RESP1[] = {
        0x7F, /* Negative Response SID (SIDRSIDNRQ) */
        0x29, /* Authentication Request SID (ARS) */
        UDS_NRC_CertficateVerificationFailedInvalidTimePeriod,
    };

    /* the client transport should receive a response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP1, sizeof(EXPECTED_RESP1));
}

UDSErr_t fn_test_0x29_example_3(UDSServer_t *srv, UDSEvent_t ev, void *arg) {
    TEST_INT_EQUAL(ev, UDS_EVT_Auth);

    UDSAuthArgs_t *args = arg;

    switch (args->type) {
    case UDS_LEV_AT_TC:
        return args->set_auth_state(srv, UDS_AT_CV);
    }

    return UDS_NRC_GeneralReject;
}

// ISO14229-1 2020 10.6.8.3 Example #3 – Transmit Certificate after Authentication with PKI
// Certificate Exchange (happy path)
void test_0x29_auth_example_3(void **state) {
    Env_t *e = *state;
    uint8_t buf[700] = {0};

    e->server->fn = fn_test_0x29_example_3;
    e->server->fn_data = NULL;

    /* Request per ISO14229-1 2020 Table 99 */
    const uint8_t REQ1[] = {
        0x29,          /* Authentication Request SID (ARS) */
        UDS_LEV_AT_TC, /* authenticationTask = transmitCertificate (LEV_AT_TC) */

        0x00, /* Custom added certificateEvaluationID (missing in sample) */

        0x01, /* lengthOfCertificateData[0] (LOCEDA) */
        0xF4, /* lengthOfCertificateData[1] (LOCEDA) */

        /* certificateData [0-499] */
        0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
        0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E,
        0x4F, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D,
        0x5E, 0x5F, 0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C,
        0x6D, 0x6E, 0x6F, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x7B,
        0x7C, 0x7D, 0x7E, 0x7F, 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A,
        0x8B, 0x8C, 0x8D, 0x8E, 0x8F, 0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99,
        0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F, 0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8,
        0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7,
        0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF, 0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6,
        0xC7, 0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF, 0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5,
        0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF, 0xE0, 0xE1, 0xE2, 0xE3, 0xE4,
        0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF, 0xF0, 0xF1, 0xF2, 0xF3,
        0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF, 0x00, 0x01, 0x02,
        0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11,
        0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20,
        0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E,
        0x3F, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D,
        0x4E, 0x4F, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C,
        0x5D, 0x5E, 0x5F, 0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x6B,
        0x6C, 0x6D, 0x6E, 0x6F, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A,
        0x7B, 0x7C, 0x7D, 0x7E, 0x7F, 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
        0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F, 0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98,
        0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F, 0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7,
        0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6,
        0xB7, 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF, 0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5,
        0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF, 0xD0, 0xD1, 0xD2, 0xD3, 0xD4,
        0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF, 0xE0, 0xE1, 0xE2, 0xE3,
        0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF, 0xF0, 0xF1, 0xF2,
        0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF, 0x00, 0x01,
        0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,
        0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
        0x20, 0x21, 0x23, 0x24, 0xAC};

    UDSTpSend(e->client_tp, REQ1, sizeof(REQ1), NULL);

    /* Response per ISO14229-1 2020 Table 100 */
    const uint8_t EXPECTED_RESP1[] = {
        0x69,          /* Authentication Response SID (ARS) */
        UDS_LEV_AT_TC, /* authenticationTask = transmitCertificate (LEV_AT_TC) */
        UDS_AT_CV,     /* returnValue = Certificate verified (RV_CV) */
    };

    /* the client transport should receive a response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP1, sizeof(EXPECTED_RESP1));
}

UDSErr_t fn_test_0x29_example_4(UDSServer_t *srv, UDSEvent_t ev, void *arg) {
    TEST_INT_EQUAL(ev, UDS_EVT_Auth);

    UDSAuthArgs_t *args = arg;

    switch (args->type) {
    case UDS_LEV_AT_AC:
        return args->set_auth_state(srv, UDS_AT_ACACRAC);

    case UDS_LEV_AT_RCFA:
        uint8_t challengeLength[] = {0x00, 0x40};
        args->copy(srv, challengeLength, sizeof(challengeLength));

        uint8_t challenge[] = {0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xB0, 0xB1, 0xB2, 0xB3, 0xB4,
                               0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF,
                               0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA,
                               0xCB, 0xCC, 0xCD, 0xCE, 0xCF, 0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5,
                               0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF, 0xE0,
                               0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0x44};

        args->copy(srv, challenge, sizeof(challenge));

        uint8_t paramLength[] = {0x00, 0x00};
        args->copy(srv, paramLength, sizeof(paramLength));
        return args->set_auth_state(srv, UDS_AT_RA);

    case UDS_LEV_AT_VPOWNU:
        uint8_t sessionKeyLength[] = {0x00, 0x00};
        args->copy(srv, sessionKeyLength, sizeof(sessionKeyLength));
        return args->set_auth_state(srv, UDS_AT_OVAC);
    }

    return UDS_NRC_GeneralReject;
}

// ISO14229-1 2020 10.6.8.4 Example #4 - Unidirectional Authentication using Challenge-Response with
// asymmetric cryptography without session key establishment (happy path)
void test_0x29_auth_example_4(void **state) {
    Env_t *e = *state;
    uint8_t buf[700] = {0};

    e->server->fn = fn_test_0x29_example_4;
    e->server->fn_data = NULL;

    /* Request per ISO14229-1 2020 Table 101 */
    const uint8_t REQ1[] = {
        0x29,          /* Authentication Request SID (ARS) */
        UDS_LEV_AT_AC, /* authenticationTask = authenticationConfiguration (LEV_AT_AC) */
    };

    UDSTpSend(e->client_tp, REQ1, sizeof(REQ1), NULL);

    /* Response per ISO14229-1 2020 Table 102 */
    const uint8_t EXPECTED_RESP1[] = {
        0x69,           /* Authentication Response SID (ARS) */
        UDS_LEV_AT_AC,  /* authenticationTask = authenticationConfiguration (LEV_AT_AC) */
        UDS_AT_ACACRAC, /* returnValue = AuthenticationConfiguration ACR with asymmetric
                         cryptography (RV_ACACR) */
    };

    /* the client transport should receive a response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP1, sizeof(EXPECTED_RESP1));

    /* Request per ISO14229-1 2020 Table 103 */
    const uint8_t REQ2[] = {
        0x29, /* Authentication Request SID (ARS) */
        0x05, /* authenticationTask = requestChallengeForAuthentication (LEV_AT_RCFA) */
        0x00, /* communicationConfiguration = no secure communication (COCO) */

        0x06, /* algorithmIndicator[0] */
        0x07, /* algorithmIndicator[1] */
        0x08, /* algorithmIndicator[2] */
        0x09, /* algorithmIndicator[3] */
        0x0A, /* algorithmIndicator[4] */
        0x0B, /* algorithmIndicator[5] */
        0x0C, /* algorithmIndicator[6] */
        0x0D, /* algorithmIndicator[7] */
        0x0E, /* algorithmIndicator[8] */
        0x0F, /* algorithmIndicator[9] */
        0x10, /* algorithmIndicator[10] */
        0x0A, /* algorithmIndicator[11] */
        0x11, /* algorithmIndicator[12] */
        0x12, /* algorithmIndicator[13] */
        0x13, /* algorithmIndicator[14] */
        0x00, /* algorithmIndicator[15] */
    };

    UDSTpSend(e->client_tp, REQ2, sizeof(REQ2), NULL);

    /* Response per ISO14229-1 2020 Table 104 */
    const uint8_t EXPECTED_RESP2[] = {
        0x69,            /* Authentication Response SID (ARS) */
        UDS_LEV_AT_RCFA, /* authenticationTask = requestChallengeForAuthentication (LEV_AT_RCFA) */
        UDS_AT_RA,       /* returnValue = Request accepted (RV_RA) */

        0x06, /* algorithmIndicator[0] */
        0x07, /* algorithmIndicator[1] */
        0x08, /* algorithmIndicator[2] */
        0x09, /* algorithmIndicator[3] */
        0x0A, /* algorithmIndicator[4] */
        0x0B, /* algorithmIndicator[5] */
        0x0C, /* algorithmIndicator[6] */
        0x0D, /* algorithmIndicator[7] */
        0x0E, /* algorithmIndicator[8] */
        0x0F, /* algorithmIndicator[9] */
        0x10, /* algorithmIndicator[10] */
        0x0A, /* algorithmIndicator[11] */
        0x11, /* algorithmIndicator[12] */
        0x12, /* algorithmIndicator[13] */
        0x13, /* algorithmIndicator[14] */
        0x00, /* algorithmIndicator[15] */

        0x00, /* lengthOfChallengeServer[0] */
        0x40, /* lengthOfChallengeServer[1] */

        /* challengeServer [0-63] */
        0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8,
        0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF, 0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7,
        0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF, 0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6,
        0xD7, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF, 0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5,
        0xE6, 0xE7, 0xE8, 0x44,

        0x00, /* lengthOfNeededAdditionalParameter[0] */
        0x00, /* lengthOfNeededAdditionalParameter[1] */
    };

    /* the client transport should receive a response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP2, sizeof(EXPECTED_RESP2));

    /* Request per ISO14229-1 2020 Table 105 */
    const uint8_t REQ3[] = {
        0x29,              /* Authentication Request SID (ARS) */
        UDS_LEV_AT_VPOWNU, /* authenticationTask = verifyProofOfOwnershipUnidirectional
                              (LEV_AT_VPOWNU) */

        0x06, /* algorithmIndicator[0] */
        0x07, /* algorithmIndicator[1] */
        0x08, /* algorithmIndicator[2] */
        0x09, /* algorithmIndicator[3] */
        0x0A, /* algorithmIndicator[4] */
        0x0B, /* algorithmIndicator[5] */
        0x0C, /* algorithmIndicator[6] */
        0x0D, /* algorithmIndicator[7] */
        0x0E, /* algorithmIndicator[8] */
        0x0F, /* algorithmIndicator[9] */
        0x10, /* algorithmIndicator[10] */
        0x0A, /* algorithmIndicator[11] */
        0x11, /* algorithmIndicator[12] */
        0x12, /* algorithmIndicator[13] */
        0x13, /* algorithmIndicator[14] */
        0x00, /* algorithmIndicator[15] */

        0x01, /* lengthOfProofOfOwnershipClient[0] (LPOWNCL) */
        0x50, /* lengthOfProofOfOwnershipClient[1] (LPOWNCL) */

        /* proofOfOwnershipClient [0-335] */
        0x7F, 0x21, 0x82, 0x01, 0x4B, 0x7F, 0x4E, 0x44, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
        0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15,
        0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24,
        0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32, 0x33,
        0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x40, 0x41, 0x42,
        /* bytes 75..79 per table */
        0x5F, 0x37, 0x82, 0x01, 0x00,

        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E,
        0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D,
        0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C,
        0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B,
        0x3C, 0x3D, 0x3E, 0x3F, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A,
        0x4B, 0x4C, 0x4D, 0x4E, 0x4F, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
        0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F, 0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
        0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
        0x78, 0x79, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F, 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86,
        0x87, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F, 0x90, 0x91, 0x92, 0x93, 0x94, 0x95,
        0x96, 0x97, 0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F, 0xA0, 0xA1, 0xA2, 0xA3, 0xA4,
        0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xB0, 0xB1, 0xB2, 0xB3,
        0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF, 0xC0, 0xC1, 0xC2,
        0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF, 0xD0, 0xD1,
        0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF, 0xE0,
        0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF,
        0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE,
        0xFF,

        0x00, /* lengthOfChallengeClient[0] (LOCHCL) */
        0x20, /* lengthOfChallengeClient[1] (LOCHCL) */

        /* challengeClient [0-31] */
        0xAA, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D,
        0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C,
        0x44,

        0x00, /* lengthOfAdditionalParameter[0] (LOAP) */
        0x00, /* lengthOfAdditionalParameter[1] (LOAP) */

    };

    UDSTpSend(e->client_tp, REQ3, sizeof(REQ3), NULL);

    /* Response per ISO14229-1 2020 Table 106 */
    const uint8_t EXPECTED_RESP3[] = {
        0x69,              /* Authentication Response SID (ARS) */
        UDS_LEV_AT_VPOWNU, /* authenticationTask = verifyProofOfOwnershipUnidirectional
                              (LEV_AT_VPOWNU) */
        UDS_AT_OVAC,       /* returnValue = Ownership verified, Authentication complete (RV_OVAC) */

        0x06, /* algorithmIndicator[0] */
        0x07, /* algorithmIndicator[1] */
        0x08, /* algorithmIndicator[2] */
        0x09, /* algorithmIndicator[3] */
        0x0A, /* algorithmIndicator[4] */
        0x0B, /* algorithmIndicator[5] */
        0x0C, /* algorithmIndicator[6] */
        0x0D, /* algorithmIndicator[7] */
        0x0E, /* algorithmIndicator[8] */
        0x0F, /* algorithmIndicator[9] */
        0x10, /* algorithmIndicator[10] */
        0x0A, /* algorithmIndicator[11] */
        0x11, /* algorithmIndicator[12] */
        0x12, /* algorithmIndicator[13] */
        0x13, /* algorithmIndicator[14] */
        0x00, /* algorithmIndicator[15] */

        0x00, /* lengthOfSessionKeyInfo[0] (LOSKI) */
        0x00, /* lengthOfSessionKeyInfo[1] (LOSKI) */
    };

    /* the client transport should receive a response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP3, sizeof(EXPECTED_RESP3));
}

UDSErr_t fn_test_0x29_example_5(UDSServer_t *srv, UDSEvent_t ev, void *arg) {
    TEST_INT_EQUAL(ev, UDS_EVT_Auth);

    UDSAuthArgs_t *args = arg;

    switch (args->type) {
    case UDS_LEV_AT_RCFA: {
        TEST_INT_EQUAL(args->subFuncArgs.reqChallengeArgs.commConf, 0x00);
        uint8_t expected_algo[] = {0x06, 0x09, 0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D,
                                   0x01, 0x01, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00};
        TEST_MEMORY_EQUAL(args->subFuncArgs.pownArgs.pown, expected_algo, sizeof(expected_algo));

        args->set_auth_state(srv, UDS_AT_RA);
        const uint8_t lenOfChallenge[] = {0x00, 0x40};
        args->copy(srv, lenOfChallenge, sizeof(lenOfChallenge));

        const uint8_t challenge[] = {
            0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6,
            0xB7, 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF, 0xC0, 0xC1, 0xC2, 0xC3,
            0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF, 0xD0,
            0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD,
            0xDE, 0xDF, 0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0x44,
        };
        args->copy(srv, challenge, sizeof(challenge));

        const uint8_t lenOfparam[] = {0x00, 0x00};
        return args->copy(srv, lenOfparam, sizeof(lenOfparam));
    }
    case UDS_LEV_AT_VPOWNU:
        return UDS_NRC_CertficateVerificationFailedInvalidSignature;
    }

    return UDS_NRC_GeneralReject;
}

// ISO14229-1 2020 10.6.8.5 Example #5 - Unidirectional Authentication using Challenge-Response
// (ACR) with asymmetric cryptography without session key establishment (failure path)
void test_0x29_auth_example_5(void **state) {
    Env_t *e = *state;
    uint8_t buf[500] = {0};

    e->server->fn = fn_test_0x29_example_5;
    e->server->fn_data = NULL;

    /* Request per ISO14229-1 2020 Table 107 */
    const uint8_t REQ[] = {
        0x29,            /* Request SID */
        UDS_LEV_AT_RCFA, /* authenticationTask */
        0x00,            /* communicationConfiguration */
        0x06,            /* algorithmIndicator[0] */
        0x09,            /* algorithmIndicator[1] */
        0x2A,            /* algorithmIndicator[2] */
        0x86,            /* algorithmIndicator[3] */
        0x48,            /* algorithmIndicator[4] */
        0x86,            /* algorithmIndicator[5] */
        0xF7,            /* algorithmIndicator[6] */
        0x0D,            /* algorithmIndicator[7] */
        0x01,            /* algorithmIndicator[8] */
        0x01,            /* algorithmIndicator[9] */
        0x0A,            /* algorithmIndicator[10] */
        0x00,            /* algorithmIndicator[11] */
        0x00,            /* algorithmIndicator[12] */
        0x00,            /* algorithmIndicator[13] */
        0x00,            /* algorithmIndicator[14] */
        0x00,            /* algorithmIndicator[15] */
    };

    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    /* Response per ISO14229-1 2020 Table 108 */
    const uint8_t EXPECTED_RESP1[] = {
        0x69,            /* Authentication Response SID (ARS) */
        UDS_LEV_AT_RCFA, /* authenticationTask = requestChallengeForAuthentication (LEV_AT_RCFA) */
        UDS_AT_RA,       /* returnValue = Request accepted (RV_RA) */
        0x06,            /* algorithmIndicator[0] (predefined by sample) */
        0x09,            /* algorithmIndicator[1] */
        0x2A,            /* algorithmIndicator[2] */
        0x86,            /* algorithmIndicator[3] */
        0x48,            /* algorithmIndicator[4] */
        0x86,            /* algorithmIndicator[5] */
        0xF7,            /* algorithmIndicator[6] */
        0x0D,            /* algorithmIndicator[7] */
        0x01,            /* algorithmIndicator[8] */
        0x01,            /* algorithmIndicator[9] */
        0x0A,            /* algorithmIndicator[10 */
        0x00,            /* algorithmIndicator[11] (predefined by sample) */
        0x00,            /* algorithmIndicator[12] */
        0x00,            /* algorithmIndicator[13] */
        0x00,            /* algorithmIndicator[14] */
        0x00,            /* algorithmIndicator[15] (predefined by sample) */
        0x00,            /* lengthOfChallengeServer[0] */
        0x40,            /* lengthOfChallengeServer[1] */

        /* challengeServer [0-63] */
        0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8,
        0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF, 0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7,
        0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF, 0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6,
        0xD7, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF, 0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5,
        0xE6, 0xE7, 0xE8, 0x44,

        0x00, /* lengthOfNeededAdditionalParameter[0] */
        0x00, /* lengthOfNeededAdditionalParameter[1] */
    };

    /* the client transport should receive a response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP1, sizeof(EXPECTED_RESP1));

    /* Request per ISO14229-1 2020 Table 109 */
    const uint8_t REQ2[] = {
        0x29,              /* Authentication Request SID (ARS) */
        UDS_LEV_AT_VPOWNU, /* authenticationTask = verifyProofOfOwnershipUnidirectional
                              (LEV_AT_VPOWNU) */

        0x06, /* algorithmIndicator[0] */
        0x07, /* algorithmIndicator[1] */
        0x08, /* algorithmIndicator[2] */
        0x09, /* algorithmIndicator[3] */
        0x0A, /* algorithmIndicator[4] */
        0x0B, /* algorithmIndicator[5] */
        0x0C, /* algorithmIndicator[6] */
        0x0D, /* algorithmIndicator[7] */
        0x0E, /* algorithmIndicator[8] */
        0x0F, /* algorithmIndicator[9] */
        0x10, /* algorithmIndicator[10] */
        0x0A, /* algorithmIndicator[11] */
        0x11, /* algorithmIndicator[12] */
        0x12, /* algorithmIndicator[13] */
        0x13, /* algorithmIndicator[14] */
        0x00, /* algorithmIndicator[15] */

        0x01, /* lengthOfProofOfOwnershipClient[0] */
        0x50, /* lengthOfProofOfOwnershipClient[1] */

        /* proofOfOwnershipClient [0-335] */
        0x7F, 0x21, 0x82, 0x01, 0x4B, 0x7F, 0x4E, 0x44,
        /* ascending fill 0x00.. for bytes 8..73 */
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E,
        0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D,
        0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C,
        0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B,
        0x3C, 0x3D, 0x3E, 0x3F, 0x40, 0x41,
        /* byte 74 */
        0x42,
        /* bytes 75..79 fixed by table */
        0x5F, 0x37, 0x82, 0x01, 0x00,
        /* ascending fill 0x00..0xFF for bytes 80..335 */
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E,
        0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D,
        0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C,
        0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B,
        0x3C, 0x3D, 0x3E, 0x3F, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A,
        0x4B, 0x4C, 0x4D, 0x4E, 0x4F, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
        0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F, 0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
        0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
        0x78, 0x79, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F, 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86,
        0x87, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F, 0x90, 0x91, 0x92, 0x93, 0x94, 0x95,
        0x96, 0x97, 0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F, 0xA0, 0xA1, 0xA2, 0xA3, 0xA4,
        0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xB0, 0xB1, 0xB2, 0xB3,
        0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF, 0xC0, 0xC1, 0xC2,
        0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF, 0xD0, 0xD1,
        0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF, 0xE0,
        0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF,
        0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE,
        0xFF,

        0x00, /* lengthOfChallengeClient[0] */
        0x20, /* lengthOfChallengeClient[1] */

        /* challengeClient [0-31] */
        0xAA, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D,
        0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C,
        0x44,

        0x00, /* lengthOfAdditionalParameter[0] */
        0x00, /* lengthOfAdditionalParameter[1] */
    };

    UDSTpSend(e->client_tp, REQ2, sizeof(REQ2), NULL);

    /* Response per ISO14229-1 2020 Table 110 */
    const uint8_t EXPECTED_RESP2[] = {
        0x7F, /* Response SID */
        0x29, /* Original Request SID */
        UDS_NRC_CertficateVerificationFailedInvalidSignature,
    };

    /* the client transport should receive a response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP2, sizeof(EXPECTED_RESP2));
}

UDSErr_t fn_test_0x29_example_6(UDSServer_t *srv, UDSEvent_t ev, void *arg) {
    TEST_INT_EQUAL(ev, UDS_EVT_Auth);

    UDSAuthArgs_t *args = arg;

    switch (args->type) {
    case UDS_LEV_AT_RCFA: {
        args->set_auth_state(srv, UDS_AT_RA);

        uint8_t challengeLength[] = {0x00, 0x10};
        args->copy(srv, challengeLength, sizeof(challengeLength));

        uint8_t challengeServer[] = {0x32, 0x43, 0xF6, 0xA8, 0x88, 0x5A, 0x30, 0x8D,
                                     0x31, 0x31, 0x98, 0xA2, 0xE0, 0x37, 0x07, 0x34};
        args->copy(srv, challengeServer, sizeof(challengeServer));

        uint8_t parameterLength[] = {0x00, 0x00};
        return args->copy(srv, parameterLength, sizeof(parameterLength));
    }
    case UDS_LEV_AT_VPOWNU: {
        args->set_auth_state(srv, UDS_AT_OVAC);

        uint8_t sessionKeyLength[] = {0x00, 0x00};
        return args->copy(srv, sessionKeyLength, sizeof(sessionKeyLength));
    }
    }

    return UDS_NRC_GeneralReject;
}

// ISO14229-1 2020 10.6.8.6 Example #6 Unidirectional Authentication using Challenge-Response (ACR)
// with symmetric cryptography without session key establishment (happy path)
void test_0x29_auth_example_6(void **state) {
    Env_t *e = *state;
    uint8_t buf[100] = {0};

    e->server->fn = fn_test_0x29_example_6;
    e->server->fn_data = NULL;

    /* Request per ISO14229-1 2020 Table 111 */
    const uint8_t REQ1[] = {
        0x29,            /* Authentication Request SID (ARS) */
        UDS_LEV_AT_RCFA, /* authenticationTask = requestChallengeForAuthentication (LEV_AT_RCFA) */
        0x00,            /* communicationConfiguration = no secure communication (COCO) */

        0x06, /* algorithmIndicator[0] */
        0x07, /* algorithmIndicator[1] */
        0x08, /* algorithmIndicator[2] */
        0x09, /* algorithmIndicator[3] */
        0x0A, /* algorithmIndicator[4] */
        0x0B, /* algorithmIndicator[5] */
        0x0C, /* algorithmIndicator[6] */
        0x0D, /* algorithmIndicator[7] */
        0x0E, /* algorithmIndicator[8] */
        0x0F, /* algorithmIndicator[9] */
        0x02, /* algorithmIndicator[10] */
        0x11, /* algorithmIndicator[11] */
        0x12, /* algorithmIndicator[12] */
        0x13, /* algorithmIndicator[13] */
        0x14, /* algorithmIndicator[14] */
        0x00, /* algorithmIndicator[15] */
    };

    UDSTpSend(e->client_tp, REQ1, sizeof(REQ1), NULL);

    /* Response per ISO14229-1 2020 Table 112 */
    const uint8_t EXPECTED_RESP1[] = {
        0x69,            /* Authentication Response SID (ARS) */
        UDS_LEV_AT_RCFA, /* authenticationTask = requestChallengeForAuthentication (LEV_AT_RCFA) */
        UDS_AT_RA,       /* returnValue = Request accepted (RV_RA) */

        0x06, /* algorithmIndicator[0] */
        0x07, /* algorithmIndicator[1] */
        0x08, /* algorithmIndicator[2] */
        0x09, /* algorithmIndicator[3] */
        0x0A, /* algorithmIndicator[4] */
        0x0B, /* algorithmIndicator[5] */
        0x0C, /* algorithmIndicator[6] */
        0x0D, /* algorithmIndicator[7] */
        0x0E, /* algorithmIndicator[8] */
        0x0F, /* algorithmIndicator[9] */
        0x02, /* algorithmIndicator[10] */
        0x11, /* algorithmIndicator[11] */
        0x12, /* algorithmIndicator[12] */
        0x13, /* algorithmIndicator[13] */
        0x14, /* algorithmIndicator[14] */
        0x00, /* algorithmIndicator[15] */

        0x00, /* lengthOfChallengeServer[0] (high byte) */
        0x10, /* lengthOfChallengeServer[1] (low byte) */

        /* challengeServer [0-15] */
        0x32, 0x43, 0xF6, 0xA8, 0x88, 0x5A, 0x30, 0x8D, 0x31, 0x31, 0x98, 0xA2, 0xE0, 0x37, 0x07,
        0x34,

        0x00, /* lengthOfNeededAdditionalParameter[0] (high byte) */
        0x00, /* lengthOfNeededAdditionalParameter[1] (low byte) */
    };

    /* the client transport should receive a response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP1, sizeof(EXPECTED_RESP1));

    /* Request per ISO14229-1 2020 Table 113 */
    const uint8_t REQ2[] = {
        0x29,              /* Authentication Request SID (ARS) */
        UDS_LEV_AT_VPOWNU, /* authenticationTask = verifyProofOfOwnershipUnidirectional
                              (LEV_AT_VPOWNU) */

        0x06, /* algorithmIndicator[0] */
        0x07, /* algorithmIndicator[1] */
        0x08, /* algorithmIndicator[2] */
        0x09, /* algorithmIndicator[3] */
        0x0A, /* algorithmIndicator[4] */
        0x0B, /* algorithmIndicator[5] */
        0x0C, /* algorithmIndicator[6] */
        0x0D, /* algorithmIndicator[7] */
        0x0E, /* algorithmIndicator[8] */
        0x0F, /* algorithmIndicator[9] */
        0x10, /* algorithmIndicator[10] */
        0x02, /* algorithmIndicator[11] */
        0x11, /* algorithmIndicator[12] */
        0x12, /* algorithmIndicator[13] */
        0x13, /* algorithmIndicator[14] */
        0x00, /* algorithmIndicator[15] */

        0x00, /* lengthOfProofOfOwnershipClient[0] (high byte, LPOWNCL_HB) */
        0x10, /* lengthOfProofOfOwnershipClient[1] (low byte, LPOWNCL_LB) */

        /* proofOfOwnershipClient [0-15] */
        0x39, 0x25, 0x84, 0x1D, 0x02, 0xDC, 0x09, 0xFB, 0xDC, 0x11, 0x85, 0x97, 0x19, 0x6A, 0x0B,
        0x32,

        0x00, /* lengthOfChallengeClient[0] (LOCHCL) */
        0x00, /* lengthOfChallengeClient[1] (LOCHCL) */

        0x00, /* lengthOfAdditionalParameter[0] (high byte, LOAP_HB) */
        0x00, /* lengthOfAdditionalParameter[1] (low byte, LOAP_LB) */
    };

    UDSTpSend(e->client_tp, REQ2, sizeof(REQ2), NULL);

    /* Response per ISO14229-1 2020 Table 114 */
    const uint8_t EXPECTED_RESP2[] = {
        0x69,              /* Authentication Response SID (ARS) */
        UDS_LEV_AT_VPOWNU, /* authenticationTask = verifyProofOfOwnershipUnidirectional
                              (LEV_AT_VPOWNU) */
        UDS_AT_OVAC,       /* returnValue = Ownership verified, Authentication complete (RV_OVAC) */

        0x06, /* algorithmIndicator[0] */
        0x07, /* algorithmIndicator[1] */
        0x08, /* algorithmIndicator[2] */
        0x09, /* algorithmIndicator[3] */
        0x0A, /* algorithmIndicator[4] */
        0x0B, /* algorithmIndicator[5] */
        0x0C, /* algorithmIndicator[6] */
        0x0D, /* algorithmIndicator[7] */
        0x0E, /* algorithmIndicator[8] */
        0x0F, /* algorithmIndicator[9] */
        0x10, /* algorithmIndicator[10] */
        0x02, /* algorithmIndicator[11] */
        0x11, /* algorithmIndicator[12] */
        0x12, /* algorithmIndicator[13] */
        0x13, /* algorithmIndicator[14] */
        0x00, /* algorithmIndicator[15] */

        0x00, /* lengthOfSessionKeyInfo[0] (LOSKI) */
        0x00, /* lengthOfSessionKeyInfo[1] (LOSKI) */
    };

    /* the client transport should receive a response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP2, sizeof(EXPECTED_RESP2));
}

UDSErr_t fn_test_0x29_example_7(UDSServer_t *srv, UDSEvent_t ev, void *arg) {
    TEST_INT_EQUAL(ev, UDS_EVT_Auth);

    UDSAuthArgs_t *args = arg;

    switch (args->type) {
    case UDS_LEV_AT_RCFA: {
        args->set_auth_state(srv, UDS_AT_RA);

        uint8_t challengeLength[] = {0x00, 0x10};
        args->copy(srv, challengeLength, sizeof(challengeLength));

        uint8_t challengeServer[] = {0x32, 0x43, 0xF6, 0xA8, 0x88, 0x5A, 0x30, 0x8D,
                                     0x31, 0x31, 0x98, 0xA2, 0xE0, 0x37, 0x07, 0x34};
        args->copy(srv, challengeServer, sizeof(challengeServer));

        uint8_t parameterLength[] = {0x00, 0x00};
        return args->copy(srv, parameterLength, sizeof(parameterLength));
    }
    case UDS_LEV_AT_VPOWNU:
        return UDS_NRC_OwnershipVerificationFailed;
    }

    return UDS_NRC_GeneralReject;
}

// ISO14229-1 2020 10.6.8.7 Example #7 Unidirectional Authentication using Challenge-Response (ACR)
// with symmetric cryptography without session key establishment (failure path)
void test_0x29_auth_example_7(void **state) {
    Env_t *e = *state;
    uint8_t buf[100] = {0};

    e->server->fn = fn_test_0x29_example_7;
    e->server->fn_data = NULL;

    /* Request per ISO14229-1 2020 Table 115 */
    const uint8_t REQ1[] = {
        0x29,            /* Authentication Request SID (ARS) */
        UDS_LEV_AT_RCFA, /* authenticationTask = requestChallengeForAuthentication (LEV_AT_RCFA) */
        0x00,            /* communicationConfiguration = no secure communication (COCO) */

        0x06, /* algorithmIndicator[0] */
        0x07, /* algorithmIndicator[1] */
        0x08, /* algorithmIndicator[2] */
        0x09, /* algorithmIndicator[3] */
        0x0A, /* algorithmIndicator[4] */
        0x0B, /* algorithmIndicator[5] */
        0x0C, /* algorithmIndicator[6] */
        0x0D, /* algorithmIndicator[7] */
        0x0E, /* algorithmIndicator[8] */
        0x0F, /* algorithmIndicator[9] */
        0x02, /* algorithmIndicator[10] */
        0x11, /* algorithmIndicator[11] */
        0x12, /* algorithmIndicator[12] */
        0x13, /* algorithmIndicator[13] */
        0x14, /* algorithmIndicator[14] */
        0x00, /* algorithmIndicator[15] */
    };

    UDSTpSend(e->client_tp, REQ1, sizeof(REQ1), NULL);

    /* Response per ISO14229-1 2020 Table 116 */
    const uint8_t EXPECTED_RESP1[] = {
        0x69,            /* Authentication Response SID (ARS) */
        UDS_LEV_AT_RCFA, /* authenticationTask = requestChallengeForAuthentication (LEV_AT_RCFA) */
        UDS_AT_RA,       /* returnValue = Request accepted (RV_RA) */

        0x06, /* algorithmIndicator[0] */
        0x07, /* algorithmIndicator[1] */
        0x08, /* algorithmIndicator[2] */
        0x09, /* algorithmIndicator[3] */
        0x0A, /* algorithmIndicator[4] */
        0x0B, /* algorithmIndicator[5] */
        0x0C, /* algorithmIndicator[6] */
        0x0D, /* algorithmIndicator[7] */
        0x0E, /* algorithmIndicator[8] */
        0x0F, /* algorithmIndicator[9] */
        0x02, /* algorithmIndicator[10] */
        0x11, /* algorithmIndicator[11] */
        0x12, /* algorithmIndicator[12] */
        0x13, /* algorithmIndicator[13] */
        0x14, /* algorithmIndicator[14] */
        0x00, /* algorithmIndicator[15] */

        0x00, /* lengthOfChallengeServer[0] (high byte) */
        0x10, /* lengthOfChallengeServer[1] (low byte) */

        /* challengeServer [0-15] */
        0x32, 0x43, 0xF6, 0xA8, 0x88, 0x5A, 0x30, 0x8D, 0x31, 0x31, 0x98, 0xA2, 0xE0, 0x37, 0x07,
        0x34,

        0x00, /* lengthOfNeededAdditionalParameter[0] (high byte) */
        0x00, /* lengthOfNeededAdditionalParameter[1] (low byte) */
    };

    /* the client transport should receive a response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP1, sizeof(EXPECTED_RESP1));

    /* Request per ISO14229-1 2020 Table 117 */
    const uint8_t REQ2[] = {
        0x29,              /* Authentication Request SID (ARS) */
        UDS_LEV_AT_VPOWNU, /* authenticationTask = verifyProofOfOwnershipUnidirectional
                              (LEV_AT_VPOWNU) */

        0x06, /* algorithmIndicator[0] */
        0x07, /* algorithmIndicator[1] */
        0x08, /* algorithmIndicator[2] */
        0x09, /* algorithmIndicator[3] */
        0x0A, /* algorithmIndicator[4] */
        0x0B, /* algorithmIndicator[5] */
        0x0C, /* algorithmIndicator[6] */
        0x0D, /* algorithmIndicator[7] */
        0x0E, /* algorithmIndicator[8] */
        0x0F, /* algorithmIndicator[9] */
        0x10, /* algorithmIndicator[10] */
        0x02, /* algorithmIndicator[11] */
        0x11, /* algorithmIndicator[12] */
        0x12, /* algorithmIndicator[13] */
        0x13, /* algorithmIndicator[14] */
        0x00, /* algorithmIndicator[15] */

        0x00, /* lengthOfProofOfOwnershipClient[0] (high byte) */
        0x10, /* lengthOfProofOfOwnershipClient[1] (low byte) */

        /* proofOfOwnershipClient [0-15] */
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
        0x11,

        0x00, /* lengthOfChallengeClient[0] (LOCHCL) */
        0x00, /* lengthOfChallengeClient[1] (LOCHCL) */

        0x00, /* lengthOfAdditionalParameter[0] (high byte) */
        0x00, /* lengthOfAdditionalParameter[1] (low byte) */
    };

    UDSTpSend(e->client_tp, REQ2, sizeof(REQ2), NULL);

    /* Response per ISO14229-1 2020 Table 118 */
    const uint8_t EXPECTED_RESP2[] = {
        0x7F,                                /* Negative Response SID (SIDRSIDNRQ) */
        0x29,                                /* Authentication Request SID (ARS) */
        UDS_NRC_OwnershipVerificationFailed, /* responseCode = Ownership verification failed
                                                (NRC_OVF) */
    };

    /* the client transport should receive a response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP2, sizeof(EXPECTED_RESP2));
}

UDSErr_t fn_test_0x2C(UDSServer_t *srv, UDSEvent_t ev, void *arg) {
    TEST_INT_EQUAL(ev, UDS_EVT_DynamicDefineDataId);
    UDSDDDIArgs_t *args = arg;

    if (!args->allDataIds && args->dynamicDataId == 0xFFFF) {
        return UDS_NRC_ConditionsNotCorrect;
    }

    uint32_t *event_counter = ((uint32_t *)srv->fn_data);

    switch (args->type) {
    case 0x01: // defineByIdentifier
        TEST_INT_EQUAL(args->allDataIds, 0);
        TEST_INT_EQUAL(args->dynamicDataId, 0xF301);

        switch (*event_counter) {
        case 0:
            TEST_INT_EQUAL(args->subFuncArgs.defineById.sourceDataId, 0x1234);
            TEST_INT_EQUAL(args->subFuncArgs.defineById.position, 1);
            TEST_INT_EQUAL(args->subFuncArgs.defineById.size, 2);
            break;
        case 1:
            TEST_INT_EQUAL(args->subFuncArgs.defineById.sourceDataId, 0x5678);
            TEST_INT_EQUAL(args->subFuncArgs.defineById.position, 1);
            TEST_INT_EQUAL(args->subFuncArgs.defineById.size, 1);
            break;
        case 2:
            TEST_INT_EQUAL(args->subFuncArgs.defineById.sourceDataId, 0x9ABC);
            TEST_INT_EQUAL(args->subFuncArgs.defineById.position, 1);
            TEST_INT_EQUAL(args->subFuncArgs.defineById.size, 4);
            break;
        default:
            break;
        }

        *event_counter += 1;

        return UDS_PositiveResponse;
    case 0x02: { // defineByMemoryAddress
        TEST_INT_EQUAL(args->allDataIds, 0);
        TEST_INT_EQUAL(args->dynamicDataId, 0xF302);

        switch (*event_counter) {

        case 0:
            TEST_INT_EQUAL((uintptr_t)args->subFuncArgs.defineByMemAddress.memAddr, 0x21091969);
            TEST_INT_EQUAL(args->subFuncArgs.defineByMemAddress.memSize, 0x01);
            break;

        case 1:
            TEST_INT_EQUAL((uintptr_t)args->subFuncArgs.defineByMemAddress.memAddr, 0x2109196B);
            TEST_INT_EQUAL(args->subFuncArgs.defineByMemAddress.memSize, 0x02);
            break;

        case 2:
            TEST_INT_EQUAL((uintptr_t)args->subFuncArgs.defineByMemAddress.memAddr, 0x13101995);
            TEST_INT_EQUAL(args->subFuncArgs.defineByMemAddress.memSize, 0x01);
            break;
        }

        *event_counter += 1;
        return UDS_PositiveResponse;
    }
    case 0x03: // clearDynamicDataId
        if (!args->allDataIds) {
            TEST_INT_EQUAL(args->dynamicDataId, 0xF303);
        }

        if (srv->fn_data != NULL) {
            return UDS_NRC_ConditionsNotCorrect;
        }
        return UDS_PositiveResponse;
    }

    return UDS_NRC_RequestOutOfRange;
}

void test_0x2C_request_too_short(void **state) {
    Env_t *e = *state;
    uint8_t buf[20] = {0};

    e->server->fn = fn_test_0x2C;
    e->server->fn_data = NULL;

    const uint8_t REQ[] = {
        0x2C, /* SID */
        /* Missing required SubFunction */
    };

    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    const uint8_t EXPECTED_RESP[] = {
        0x7F, /* Response SID */
        0x2C, /* Original Request SID */
        0x13, /* NRC: IncorrectMessageLengthOrInvalidFormat */
    };

    /* the client transport should receive a response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP, sizeof(EXPECTED_RESP));
}

// ISO14229-1 2020 11.6.5.2 Example #1: DynamicallyDefineDataIdentifier, SubFunction =
// defineByIdentifier
void test_0x2C_sub_0x01(void **state) {
    Env_t *e = *state;
    uint8_t buf[20] = {0};

    uint32_t event_counter = 0;
    e->server->fn = fn_test_0x2C;
    e->server->fn_data = &event_counter;

    /* Request per ISO14229-1 2020 Table 247 */
    const uint8_t REQ[] = {
        0x2C, /* SID */
        0x01, /* SubFunction */
        0xF3, /* DynamicallyDefinedDataIdentifier [High Byte] */
        0x01, /* DynamicallyDefinedDataIdentifier [Low Byte] */
        0x12, /* SourceDataIdentifier#1 [High Byte] */
        0x34, /* SourceDataIdentifier#1 [Low Byte] */
        0x01, /* PositionInSourceDataRecord#1 */
        0x02, /* MemorySize#1 */
        0x56, /* SourceDataIdentifier#2 [High Byte] */
        0x78, /* SourceDataIdentifier#2 [Low Byte] */
        0x01, /* PositionInSourceDataRecord#2 */
        0x01, /* MemorySize#2 */
        0x9A, /* SourceDataIdentifier#3 [High Byte] */
        0xBC, /* SourceDataIdentifier#3 [Low Byte] */
        0x01, /* PositionInSourceDataRecord#3 */
        0x04, /* MemorySize#3 */
    };

    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    /* Response per ISO14229-1 2020 Table 248 */
    const uint8_t EXPECTED_RESP[] = {
        0x6C, /* Response SID */
        0x01, /* SubFunction */
        0xF3, /* DynamicallyDefinedDataIdentifier [High Byte] */
        0x01, /* DynamicallyDefinedDataIdentifier [Low Byte] */
    };

    /* the client transport should receive a response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP, sizeof(EXPECTED_RESP));
}

void test_0x2C_sub_0x01_request_too_short(void **state) {
    Env_t *e = *state;
    uint8_t buf[20] = {0};

    e->server->fn = fn_test_0x2C;
    e->server->fn_data = NULL;

    const uint8_t REQ[] = {
        0x2C, /* SID */
        0x01, /* SubFunction */
        0xF3, /* DynamicallyDefinedDataIdentifier [High Byte] */
        0x01, /* DynamicallyDefinedDataIdentifier [Low Byte] */
        0x12, /* SourceDataIdentifier#1 [High Byte] */
        0x34, /* SourceDataIdentifier#1 [Low Byte] */
        0x01, /* PositionInSourceDataRecord#1 */
              /* Missing required MemorySize#1 */
    };

    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    const uint8_t EXPECTED_RESP[] = {
        0x7F, /* Response SID */
        0x2C, /* Original Request SID */
        0x13, /* NRC: IncorrectMessageLengthOrInvalidFormat */
    };

    /* the client transport should receive a response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP, sizeof(EXPECTED_RESP));
}

void test_0x2C_sub_0x01_negative_response(void **state) {
    Env_t *e = *state;
    uint8_t buf[20] = {0};

    e->server->fn = fn_test_0x2C;
    e->server->fn_data = NULL;

    const uint8_t REQ[] = {
        0x2C, /* SID */
        0x01, /* SubFunction */
        0xFF, /* DynamicallyDefinedDataIdentifier [High Byte] */
        0xFF, /* DynamicallyDefinedDataIdentifier [Low Byte] */
        0x01, /* SourceDataIdentifier#1 [High Byte] */
        0x23, /* SourceDataIdentifier#1 [Low Byte] */
        0x01, /* PositionInSourceDataRecord#1 */
        0x01  /* Missing required MemorySize#1 */
    };

    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    const uint8_t EXPECTED_RESP[] = {
        0x7F, /* Response SID */
        0x2C, /* Original Request SID */
        0x22, /* NRC: ConditionsNotCorrect */
    };

    /* the client transport should receive a response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP, sizeof(EXPECTED_RESP));
}

// ISO14229-1 2020 11.6.5.4 Example #3: DynamicallyDefineDataIdentifier, SubFunction =
// defineByMemoryAddress
void test_0x2C_sub_0x02(void **state) {
    Env_t *e = *state;
    uint8_t buf[20] = {0};

    uint32_t counter = 0;
    e->server->fn = fn_test_0x2C;
    e->server->fn_data = &counter;

    /* Request per ISO14229-1 2020 Table 255 */
    const uint8_t REQ[] = {
        0x2C, /* SID */
        0x02, /* SubFunction */
        0xF3, /* DynamicallyDefinedDataIdentifier [High Byte] */
        0x02, /* DynamicallyDefinedDataIdentifier [Low Byte] */
        0x14, /* AddressAndLengthFormatIdentifier */
        0x21, /* memoryAddres#1 [High Byte] */
        0x09, /* memoryAddres#1 [Middle Byte] */
        0x19, /* memoryAddres#1 [Middle Byte] */
        0x69, /* memoryAddres#1 [Low Byte] */
        0x01, /* memorySize#1 */
        0x21, /* memoryAddres#2 [High Byte] */
        0x09, /* memoryAddres#2 [Middle Byte] */
        0x19, /* memoryAddres#2 [Middle Byte] */
        0x6B, /* memoryAddres#2 [Low Byte] */
        0x02, /* memorySize#2 */
        0x13, /* memoryAddres#3 [High Byte] */
        0x10, /* memoryAddres#3 [Middle Byte] */
        0x19, /* memoryAddres#3 [Middle Byte] */
        0x95, /* memoryAddres#3 [Low Byte] */
        0x01  /* memorySize#3 */
    };

    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    /* Response per ISO14229-1 2020 Table 256 */
    const uint8_t EXPECTED_RESP[] = {
        0x6C, /* Response SID */
        0x02, /* SubFunction */
        0xF3, /* DynamicallyDefinedDataIdentifier [High Byte] */
        0x02, /* DynamicallyDefinedDataIdentifier [Low Byte] */
    };

    /* the client transport should receive a response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP, sizeof(EXPECTED_RESP));
}

void test_0x2C_sub_0x02_request_malformed(void **state) {
    Env_t *e = *state;
    uint8_t buf[20] = {0};

    e->server->fn = fn_test_0x2C;
    e->server->fn_data = NULL;

    const uint8_t REQ[] = {
        0x2C, /* SID */
        0x02, /* SubFunction */
        0xF3, /* DynamicallyDefinedDataIdentifier [High Byte] */
        0x02, /* DynamicallyDefinedDataIdentifier [Low Byte] */
        0x14, /* AddressAndLengthFormatIdentifier */
        0x21, /* memoryAddres#1 [High Byte] */
        0x09, /* memoryAddres#1 [Middle Byte] */
        0x19, /* memoryAddres#1 [Middle Byte] */
        0x69, /* memoryAddres#1 [Low Byte] */
              /* Missing memorySize#1 */
    };

    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);
    /* Response per ISO14229-1 2020 Table 256 */
    const uint8_t EXPECTED_RESP[] = {
        0x7F, /* Response SID */
        0x2C, /* Original Request SID */
        0x13, /* NRC: IncorrectMessageLengthOrInvalidFormat */
    };

    /* the client transport should receive a response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP, sizeof(EXPECTED_RESP));
}

void test_0x2C_sub_0x02_request_too_short(void **state) {
    Env_t *e = *state;
    uint8_t buf[20] = {0};

    e->server->fn = fn_test_0x2C;
    e->server->fn_data = NULL;

    const uint8_t REQ[] = {
        0x2C, /* SID */
        0x02, /* SubFunction */
        0xF3, /* DynamicallyDefinedDataIdentifier [High Byte] */
        0x02, /* DynamicallyDefinedDataIdentifier [Low Byte] */
              /* Missing required AddressAndLengthFormatIdentifier */
    };

    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);
    /* Response per ISO14229-1 2020 Table 256 */
    const uint8_t EXPECTED_RESP[] = {
        0x7F, /* Response SID */
        0x2C, /* Original Request SID */
        0x13, /* NRC: IncorrectMessageLengthOrInvalidFormat */
    };

    /* the client transport should receive a response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP, sizeof(EXPECTED_RESP));
}

void test_0x2C_sub_0x02_negative_response(void **state) {
    Env_t *e = *state;
    uint8_t buf[20] = {0};

    e->server->fn = fn_test_0x2C;
    e->server->fn_data = NULL;

    const uint8_t REQ[] = {
        0x2C, /* SID */
        0x02, /* SubFunction */
        0xFF, /* DynamicallyDefinedDataIdentifier [High Byte] */
        0xFF, /* DynamicallyDefinedDataIdentifier [Low Byte] */
        0x14, /* AddressAndLengthFormatIdentifier */
        0x21, /* memoryAddres#1 [High Byte] */
        0x09, /* memoryAddres#1 [Middle Byte] */
        0x19, /* memoryAddres#1 [Middle Byte] */
        0x69, /* memoryAddres#1 [Low Byte] */
        0x01, /* memorySize#1 */
    };

    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);
    const uint8_t EXPECTED_RESP[] = {
        0x7F, /* Response SID */
        0x2C, /* Original Request SID */
        0x22, /* NRC: ConditionsNotCorrect */
    };

    /* the client transport should receive a response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP, sizeof(EXPECTED_RESP));
}

void test_0x2C_sub_0x02_zero_address_and_length_format_identifier(void **state) {
    Env_t *e = *state;
    uint8_t buf[20] = {0};

    uint32_t counter = 0;
    e->server->fn = fn_test_0x2C;
    e->server->fn_data = &counter;

    /* Valid request with 0 as AddressAndLengthFormatIdentifier */
    const uint8_t REQ[] = {
        0x2C, /* SID */
        0x02, /* SubFunction */
        0xF3, /* DynamicallyDefinedDataIdentifier [High Byte] */
        0x02, /* DynamicallyDefinedDataIdentifier [Low Byte] */
        0x00, /* **Force 0 here** AddressAndLengthFormatIdentifier */
        0x21, /* memoryAddres#1 [High Byte] */
        0x09, /* memoryAddres#1 [Middle Byte] */
        0x19, /* memoryAddres#1 [Middle Byte] */
        0x69, /* memoryAddres#1 [Low Byte] */
        0x01, /* memorySize#1 */
        0x21, /* memoryAddres#2 [High Byte] */
        0x09, /* memoryAddres#2 [Middle Byte] */
        0x19, /* memoryAddres#2 [Middle Byte] */
        0x6B, /* memoryAddres#2 [Low Byte] */
        0x02, /* memorySize#2 */
        0x13, /* memoryAddres#3 [High Byte] */
        0x10, /* memoryAddres#3 [Middle Byte] */
        0x19, /* memoryAddres#3 [Middle Byte] */
        0x95, /* memoryAddres#3 [Low Byte] */
        0x01  /* memorySize#3 */
    };

    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    const uint8_t EXPECTED_RESP[] = {
        0x7F, /* Response SID */
        0x2C, /* Original Request SID */
        0x31, /* NRC: RequestOutOfRange */
    };

    /* the client transport should receive a response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP, sizeof(EXPECTED_RESP));
}

// ISO14229-1 2020 11.6.5.6 Example #5: DynamicallyDefineDataIdentifier, SubFunction =
// clearDynamicallyDefined-DataIdentifier
void test_0x2C_sub_0x03(void **state) {
    Env_t *e = *state;
    uint8_t buf[20] = {0};

    e->server->fn = fn_test_0x2C;
    e->server->fn_data = NULL;

    /* Request per ISO14229-1 2020 Table 265 */
    const uint8_t REQ[] = {
        0x2C, /* SID */
        0x03, /* SubFunction */
        0xF3, /* DynamicallyDefinedDataIdentifier [High Byte] */
        0x03, /* DynamicallyDefinedDataIdentifier [Low Byte] */
    };

    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    /* Response per ISO14229-1 2020 Table 266 */
    const uint8_t EXPECTED_RESP[] = {
        0x6C, /* Response SID */
        0x03, /* SubFunction */
        0xF3, /* DynamicallyDefinedDataIdentifier [High Byte] */
        0x03, /* DynamicallyDefinedDataIdentifier [Low Byte] */
    };

    /* the client transport should receive a response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP, sizeof(EXPECTED_RESP));
}

void test_0x2C_sub_0x03_clear_all(void **state) {
    Env_t *e = *state;
    uint8_t buf[20] = {0};

    e->server->fn = fn_test_0x2C;
    e->server->fn_data = NULL;

    const uint8_t REQ[] = {
        0x2C, /* SID */
        0x03, /* SubFunction */
    };

    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    const uint8_t EXPECTED_RESP[] = {
        0x6C, /* Response SID */
        0x03, /* SubFunction */
    };

    /* the client transport should receive a response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP, sizeof(EXPECTED_RESP));
}

void test_0x2C_sub_0x03_negative_response(void **state) {
    Env_t *e = *state;
    uint8_t buf[20] = {0};

    e->server->fn = fn_test_0x2C;
    // Just set != NULL to distinguish tests
    e->server->fn_data = fn_test_0x2C;

    const uint8_t REQ[] = {
        0x2C, /* SID */
        0x03, /* SubFunction */
    };

    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    const uint8_t EXPECTED_RESP[] = {
        0x7F, /* Response SID */
        0x2C, /* Original Request SID */
        0x22, /* NRC: ConditionsNotCorrect */
    };

    /* the client transport should receive a response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP, sizeof(EXPECTED_RESP));
}

int fn_test_0x2F(UDSServer_t *srv, UDSEvent_t ev, void *arg) {
    UDSIOCtrlArgs_t *args = arg;

    TEST_INT_EQUAL(ev, UDS_EVT_IOControl);
    TEST_INT_EQUAL(args->dataId, 0x9B00);
    if (args->ioCtrlParam == 0x00) {
        TEST_INT_EQUAL(args->ctrlStateAndMaskLen, 0x00);
        const uint8_t response_data[] = {0x3A};
        return args->copy(srv, response_data, sizeof(response_data));

    } else if (args->ioCtrlParam == 0x02) {
        // Provoke a negative response
        return UDS_NRC_SecurityAccessDenied;

    } else if (args->ioCtrlParam == 0x03) {
        TEST_INT_EQUAL(args->ctrlStateAndMaskLen, 0x01);
        const uint8_t expected_data[] = {0x3C};
        TEST_MEMORY_EQUAL(args->ctrlStateAndMask, expected_data, args->ctrlStateAndMaskLen);
        const uint8_t response_data[] = {0x0C};
        return args->copy(srv, response_data, sizeof(response_data));
    }

    return UDS_NRC_RequestOutOfRange;
}

// ISO14229-1 2020 13.2.5.2 Example #1 - "Air Inlet Door Position" shortTermAdjustment
// This test just simulates the 0x2F function request/responses of the example.
void test_0x2F_example(void **state) {
    Env_t *e = *state;
    uint8_t buf[20] = {0};

    e->server->fn = fn_test_0x2F;
    e->server->fn_data = NULL;

    /* Request per ISO14229-1 2020 Table 406 */
    const uint8_t REQ1[] = {
        0x2F, /* SID */
        0x9B, /* DataIdentifier [High Byte] */
        0x00, /* DataIdentifier [Low Byte] */
        0x03, /* ControlOptionRecord [inputOutputControlParamter] */
        0x3C, /* ControlOptionRecord [State#1] */
    };

    UDSTpSend(e->client_tp, REQ1, sizeof(REQ1), NULL);

    /* Response per ISO14229-1 2020 Table 407 */
    const uint8_t EXPECTED_RESP1[] = {
        0x6F, /* Response SID */
        0x9B, /* DataIdentifier [High Byte] */
        0x00, /* DataIdentifier [Low Byte] */
        0x03, /* ControlOptionRecord [inputOutputControlParamter] */
        0x0C, /* ControlOptionRecord [State#1] */
    };

    /* the client transport should receive a positive response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP1, sizeof(EXPECTED_RESP1));

    /* Request per ISO14229-1 2020 Table 410 */
    const uint8_t REQ2[] = {
        0x2F, /* SID */
        0x9B, /* DataIdentifier [High Byte] */
        0x00, /* DataIdentifier [Low Byte] */
        0x00, /* ControlOptionRecord [inputOutputControlParamter] */
    };

    UDSTpSend(e->client_tp, REQ2, sizeof(REQ2), NULL);

    /* Response per ISO14229-1 2020 Table 411 */
    const uint8_t EXPECTED_RESP2[] = {
        0x6F, /* Response SID */
        0x9B, /* DataIdentifier [High Byte] */
        0x00, /* DataIdentifier [Low Byte] */
        0x00, /* ControlOptionRecord [inputOutputControlParamter] */
        0x3A, /* ControlOptionRecord [State#1] */
    };

    /* the client transport should receive a positive response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP2, sizeof(EXPECTED_RESP2));
}

void test_0x2F_incorrect_request_length(void **state) {
    Env_t *e = *state;
    uint8_t buf[20] = {0};

    e->server->fn = fn_test_0x2F;
    e->server->fn_data = NULL;

    const uint8_t REQ[] = {
        0x2F, /* SID */
        0x9B, /* DataIdentifier [High Byte] */
        0x00, /* DataIdentifier [Low Byte] */
              /* MISSING required ControlOptionRecord [inputOutputControlParamter] */
    };

    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    const uint8_t EXPECTED_RESP[] = {
        0x7F, /* Response SID */
        0x2F, /* Original Request SID */
        0x13, /* NRC: IncorrectMessageLengthOrInvalidFormat */
    };

    /* the client transport should receive a response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP, sizeof(EXPECTED_RESP));
}

void test_0x2F_negative_response(void **state) {
    Env_t *e = *state;
    uint8_t buf[20] = {0};

    e->server->fn = fn_test_0x2F;
    e->server->fn_data = NULL;

    const uint8_t REQ[] = {
        0x2F, /* SID */
        0x9B, /* DataIdentifier [High Byte] */
        0x00, /* DataIdentifier [Low Byte] */
        0x02, /* ControlOptionRecord [inputOutputControlParamter] */
    };

    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    const uint8_t EXPECTED_RESP[] = {
        0x7F, /* Response SID */
        0x2F, /* Original Request SID */
        0x33, /* NRC: SecurityAccessDenied */
    };

    /* the client transport should receive a response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP, sizeof(EXPECTED_RESP));
}

int fn_test_0x31_RCRRP(UDSServer_t *srv, UDSEvent_t ev, void *arg) {
    return *(int *)(srv->fn_data);
}

void test_0x31_RCRRP(void **state) {
    Env_t *e = *state;
    uint8_t buf[8] = {0};

    // when a server handler func initially returns RRCRP
    int resp = UDS_NRC_RequestCorrectlyReceived_ResponsePending;
    e->server->fn_data = &resp;
    e->server->fn = fn_test_0x31_RCRRP;

    // and a request is sent to the server
    const uint8_t REQ[] = {0x31, 0x01, 0x12, 0x34};
    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    // the server should respond with RCRRP within p2 ms
    const uint8_t RCRRP[] = {0x7F, 0x31, 0x78};
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, RCRRP, sizeof(RCRRP));

    // The server should again respond within p2_star * 0.3 ms
    EXPECT_IN_APPROX_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                        e->server->p2_star_ms * 0.3);
    TEST_MEMORY_EQUAL(buf, RCRRP, sizeof(RCRRP));

    // and keep responding at intervals of p2_star * 0.3 ms indefinitely
    EXPECT_IN_APPROX_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                        e->server->p2_star_ms * 0.3);
    TEST_MEMORY_EQUAL(buf, RCRRP, sizeof(RCRRP));

    EXPECT_IN_APPROX_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                        e->server->p2_star_ms * 0.3);
    TEST_MEMORY_EQUAL(buf, RCRRP, sizeof(RCRRP));

    // When the server handler func now returns a positive response
    resp = UDS_PositiveResponse;

    // the server's next response should be a positive one
    // and it should arrive within p2 ms
    const uint8_t POSITIVE_RESPONSE[] = {0x71, 0x01, 0x12, 0x34};
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, POSITIVE_RESPONSE, sizeof(POSITIVE_RESPONSE));
}

void test_0x34_no_handler(void **state) {
    Env_t *e = *state;
    uint8_t buf[8] = {0};

    // When no handler function is installed
    e->server->fn = NULL; // (noop, NULL by default)

    // sending this request to the server
    const uint8_t REQ[] = {0x34, 0x11, 0x33, 0x60, 0x20, 0x00, 0x00, 0xFF, 0xFF};
    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    // should return a UDS_NRC_ServiceNotSupported response
    const uint8_t RESP[] = {0x7F, 0x34, 0x11};
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, RESP, sizeof(RESP));
}

int fn_test_0x34(UDSServer_t *srv, UDSEvent_t ev, void *arg) {
    TEST_INT_EQUAL(ev, UDS_EVT_RequestDownload);
    UDSRequestDownloadArgs_t *r = (UDSRequestDownloadArgs_t *)arg;
    TEST_INT_EQUAL(0x11, r->dataFormatIdentifier);
    TEST_PTR_EQUAL((void *)0x602000, r->addr);
    TEST_INT_EQUAL(0x00FFFF, r->size);
    TEST_INT_EQUAL(r->maxNumberOfBlockLength, UDS_SERVER_DEFAULT_XFER_DATA_MAX_BLOCKLENGTH);
    r->maxNumberOfBlockLength = 0x0081;
    return UDS_PositiveResponse;
}

void test_0x34(void **state) {
    Env_t *e = *state;
    uint8_t buf[8] = {0};

    // When a handler is installed that implements UDS-1:2013 Table 415
    e->server->fn = fn_test_0x34;

    // sending this request to the server
    const uint8_t REQ[] = {0x34, 0x11, 0x33, 0x60, 0x20, 0x00, 0x00, 0xFF, 0xFF};
    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    // should receive a positive response matching UDS-1:2013 Table 415
    const uint8_t RESP[] = {0x74, 0x20, 0x00, 0x81};
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, RESP, sizeof(RESP));
}

void test_0x38_no_handler(void **state) {
    Env_t *e = *state;
    uint8_t buf[8] = {0};

    // When no handler function is installed
    e->server->fn = NULL;

    // sending this request to the server
    const uint8_t ADDFILE_REQUEST[] = {0x38, 0x01, 0x00, 0x12, 0x2F, 0x64, 0x61, 0x74, 0x61, 0x2F,
                                       0x74, 0x65, 0x73, 0x74, 0x66, 0x69, 0x6C, 0x65, 0x2E, 0x7A,
                                       0x69, 0x70, 0x00, 0x03, 0x11, 0x22, 0x33, 0x00, 0x11, 0x22};
    UDSTpSend(e->client_tp, ADDFILE_REQUEST, sizeof(ADDFILE_REQUEST), NULL);

    // should return a kServiceNotSupported response
    const uint8_t RESP[] = {0x7F, 0x38, 0x11};
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, RESP, sizeof(RESP));
}

int fn_test_0x38_addfile(UDSServer_t *srv, UDSEvent_t ev, void *arg) {
    TEST_INT_EQUAL(ev, UDS_EVT_RequestFileTransfer);
    UDSRequestFileTransferArgs_t *r = (UDSRequestFileTransferArgs_t *)arg;
    TEST_INT_EQUAL(0x01, r->modeOfOperation);
    TEST_INT_EQUAL(18, r->filePathLen);
    TEST_MEMORY_EQUAL((void *)"/data/testfile.zip", r->filePath, r->filePathLen);
    TEST_INT_EQUAL(0x00, r->dataFormatIdentifier);
    TEST_INT_EQUAL(0x112233, r->fileSizeUnCompressed);
    TEST_INT_EQUAL(0x001122, r->fileSizeCompressed);
    r->maxNumberOfBlockLength = 0x0081;
    return UDS_PositiveResponse;
}

void test_0x38_addfile(void **state) {
    Env_t *e = *state;
    uint8_t buf[8] = {0};

    // When a handler is installed that implements UDS-1:2013 Table 435
    e->server->fn = fn_test_0x38_addfile;

    // sending this request to the server
    const uint8_t ADDFILE_REQUEST[] = {0x38, 0x01, 0x00, 0x12, 0x2F, 0x64, 0x61, 0x74, 0x61, 0x2F,
                                       0x74, 0x65, 0x73, 0x74, 0x66, 0x69, 0x6C, 0x65, 0x2E, 0x7A,
                                       0x69, 0x70, 0x00, 0x03, 0x11, 0x22, 0x33, 0x00, 0x11, 0x22};
    UDSTpSend(e->client_tp, ADDFILE_REQUEST, sizeof(ADDFILE_REQUEST), NULL);

    // should receive a positive response matching UDS-1:2013 Table 435
    const uint8_t RESP[] = {0x78, 0x01, 0x02, 0x00, 0x81};
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, RESP, sizeof(RESP));
}

int fn_test_0x38_delfile(UDSServer_t *srv, UDSEvent_t ev, void *arg) {
    TEST_INT_EQUAL(ev, UDS_EVT_RequestFileTransfer);
    UDSRequestFileTransferArgs_t *r = (UDSRequestFileTransferArgs_t *)arg;
    TEST_INT_EQUAL(0x02, r->modeOfOperation);
    TEST_INT_EQUAL(18, r->filePathLen);
    TEST_MEMORY_EQUAL((void *)"/data/testfile.zip", r->filePath, r->filePathLen);
    return UDS_PositiveResponse;
}

void test_0x38_delfile(void **state) {
    Env_t *e = *state;
    uint8_t buf[8] = {0};

    // When a handler is installed that implements UDS-1:2013 Table 435
    e->server->fn = fn_test_0x38_delfile;

    // sending this request to the server
    const uint8_t DELFILE_REQUEST[] = {0x38, 0x02, 0x00, 0x12, 0x2F, 0x64, 0x61, 0x74,
                                       0x61, 0x2F, 0x74, 0x65, 0x73, 0x74, 0x66, 0x69,
                                       0x6C, 0x65, 0x2E, 0x7A, 0x69, 0x70};
    UDSTpSend(e->client_tp, DELFILE_REQUEST, sizeof(DELFILE_REQUEST), NULL);

    // should receive a positive response matching UDS-1:2013 Table 435
    const uint8_t RESP[] = {0x78, 0x02};
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, RESP, sizeof(RESP));
}

int fn_test_0x3e_suppress_positive_response(UDSServer_t *srv, UDSEvent_t ev, void *arg) {
    return UDS_PositiveResponse;
}

void test_0x3e_suppress_positive_response(void **state) {
    Env_t *e = *state;
    uint8_t buf[8] = {0};
    e->server->fn = fn_test_0x3e_suppress_positive_response;

    // When the suppressPositiveResponse bit is set
    const uint8_t REQ[] = {0x3E, 0x80};
    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    // there should be no response even after running for a long time
    EnvRunMillis(e, 10000);
    TEST_INT_EQUAL(UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL), 0);
}

UDSErr_t fn_test_0x85_control_dtc_setting(UDSServer_t *srv, UDSEvent_t ev, void *arg) {
    TEST_INT_EQUAL(ev, UDS_EVT_ControlDTCSetting);

    UDSControlDTCSettingArgs_t *args = arg;

    switch (args->type) {
    case 0x01:
        return UDS_PositiveResponse;
    case 0x02:
        TEST_INT_EQUAL(args->len, 2)
        uint8_t expected_data[] = {0xF1, 0xF2};
        TEST_MEMORY_EQUAL(args->data, expected_data, sizeof(expected_data))
        return UDS_PositiveResponse;
    case 0x40:
        return UDS_NRC_ConditionsNotCorrect;
    }

    return UDS_NRC_GeneralProgrammingFailure;
}

// ISO14229-1 2020 10.8.5.2 Example #2 - ControlDTCSetting ( DTCSettingType = on)
void test_0x85_control_dtc_setting(void **state) {
    Env_t *e = *state;
    uint8_t buf[20] = {0};

    e->server->fn = fn_test_0x85_control_dtc_setting;
    e->server->fn_data = NULL;

    /* Request per ISO14229-1 2020 Table 135 */
    const uint8_t REQ[] = {
        0x85, /* SID */
        0x01, /* SubFunction */
    };

    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    /* Response per ISO14229-1 2020 Table 136 */
    const uint8_t EXPECTED_RESP[] = {
        0xC5, /* Response SID */
        0x01, /* SubFunction */
    };

    /* the client transport should receive a response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP, sizeof(EXPECTED_RESP));
}

// ISO14229-1 2020 10.8.5.1 Example #1 - ControlDTCSetting (DTCSettingType = off)
// Here additional DTCSettingControlOptionRecord are added to the sample to test
// the correct propagation of them
void test_0x85_control_dtc_setting_with_control_data(void **state) {
    Env_t *e = *state;
    uint8_t buf[20] = {0};

    e->server->fn = fn_test_0x85_control_dtc_setting;
    e->server->fn_data = NULL;

    const uint8_t REQ[] = {
        0x85, /* SID */
        0x02, /* SubFunction */
        /* Custom Record not present in the sample */
        0xF1, /* DTCSettingControlOptionRecord#1 */
        0xF2, /* DTCSettingControlOptionRecord#2 */
    };

    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    const uint8_t EXPECTED_RESP[] = {
        0xC5, /* Response SID */
        0x02, /* SubFunction */
    };

    /* the client transport should receive a response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP, sizeof(EXPECTED_RESP));
}

void test_0x85_control_dtc_setting_request_too_short(void **state) {
    Env_t *e = *state;
    uint8_t buf[20] = {0};

    e->server->fn = fn_test_0x85_control_dtc_setting;
    e->server->fn_data = NULL;

    const uint8_t REQ[] = {
        0x85, /* SID */
              /* Missing required SubFunction */
    };

    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    const uint8_t EXPECTED_RESP[] = {
        0x7F, /* Response SID */
        0x85, /* Original Request SID */
        0x13, /* NRC: IncorrectMessageLengthOrInvalidFormat */
    };

    /* the client transport should receive a response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP, sizeof(EXPECTED_RESP));
}

void test_0x85_control_dtc_setting_request_returns_negative_response(void **state) {
    Env_t *e = *state;
    uint8_t buf[20] = {0};

    e->server->fn = fn_test_0x85_control_dtc_setting;
    e->server->fn_data = NULL;

    const uint8_t REQ[] = {
        0x85, /* SID */
        0x40, /* SubFunction */
    };

    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    const uint8_t EXPECTED_RESP[] = {
        0x7F, /* Response SID */
        0x85, /* Original Request SID */
        0x22, /* NRC: ConditionsNotCorrect */
    };

    /* the client transport should receive a response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP, sizeof(EXPECTED_RESP));
}

UDSErr_t fn_test_0x87_link_ctrl(UDSServer_t *srv, UDSEvent_t ev, void *arg) {
    TEST_INT_EQUAL(ev, UDS_EVT_LinkControl);

    UDSLinkCtrlArgs_t *args = arg;

    switch (args->type) {
    case UDS_LEV_LCTP_VMTWFP:
        TEST_INT_EQUAL(args->len, 0x01);
        TEST_INT_EQUAL(*(uint8_t *)args->data, 0x05);
        return UDS_PositiveResponse;
    case UDS_LEV_LCTP_VMTWSP:
        TEST_INT_EQUAL(args->len, 0x03);
        uint8_t expected_data[] = {0x02, 0x49, 0xF0};
        TEST_MEMORY_EQUAL(args->data, expected_data, sizeof(expected_data));
        return UDS_PositiveResponse;
    case UDS_LEV_LCTP_TM:
        return UDS_PositiveResponse;
    case 0x40: /* Custom vehicle manufacturer specific */
        return UDS_NRC_ConditionsNotCorrect;
    }

    return UDS_NRC_SubFunctionNotSupported;
}

// ISO14229-1 2020 10.10.5.1 Example #1 - Transition baudrate to fixed baudrate
// (PC baudrate 115,2 kBit/s)
void test_0x87_link_ctrl_sub_0x01(void **state) {
    Env_t *e = *state;
    uint8_t buf[20] = {0};

    e->server->fn = fn_test_0x87_link_ctrl;
    e->server->fn_data = NULL;

    /* Request per ISO14229-1 2020 Table 176 */
    const uint8_t REQ[] = {
        0x87, /* SID */
        0x01, /* LinkControlType */
        0x05, /* LinkControlModeIdentifier */
    };

    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    /* Response per ISO14229-1 2020 Table 177 */
    const uint8_t EXPECTED_RESP[] = {
        0xC7, /* Response SID */
        0x01, /* LinkControlType */
    };

    /* the client transport should receive a response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP, sizeof(EXPECTED_RESP));

    /* Request per ISO14229-1 2020 Table 178 - Without the suppress response bit */
    const uint8_t REQ_TRANSITION[] = {
        0x87, /* SID */
        0x03, /* LinkControlType */
    };

    UDSTpSend(e->client_tp, REQ_TRANSITION, sizeof(REQ_TRANSITION), NULL);

    const uint8_t EXPECTED_RESP_TRANSITION[] = {
        0xC7, /* Response SID */
        0x03, /* LinkControlType */
    };

    /* the client transport should receive a response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP_TRANSITION, sizeof(EXPECTED_RESP_TRANSITION));
}

// ISO14229-1 2020 10.10.5.2 Example #2 - Transition baudrate to specific baudrate (150 kBit/s)
void test_0x87_link_ctrl_sub_0x02(void **state) {
    Env_t *e = *state;
    uint8_t buf[20] = {0};

    e->server->fn = fn_test_0x87_link_ctrl;
    e->server->fn_data = NULL;

    /* Request per ISO14229-1 2020 Table 179 */
    const uint8_t REQ[] = {
        0x87, /* SID */
        0x02, /* LinkControlType */
        0x02, /* LinkRecord#1 */
        0x49, /* LinkRecord#2 */
        0xF0, /* LinkRecord#3 */
    };

    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    /* Response per ISO14229-1 2020 Table 180 */
    const uint8_t EXPECTED_RESP[] = {
        0xC7, /* Response SID */
        0x02, /* LinkControlType */
    };

    /* the client transport should receive a response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP, sizeof(EXPECTED_RESP));

    /* Request per ISO14229-1 2020 Table 181 - Supressing response */
    const uint8_t REQ_TRANSITION[] = {
        0x87, /* SID */
        0x83, /* LinkControlType */
    };

    UDSTpSend(e->client_tp, REQ_TRANSITION, sizeof(REQ_TRANSITION), NULL);

    /* even after running for a long time, but not long enough to timeout */
    EnvRunMillis(e, 1000);

    /* there should be no response from the server */
    int len = UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL);
    TEST_INT_EQUAL(len, 0);
}

void test_0x87_link_ctrl_negative_response(void **state) {
    Env_t *e = *state;
    uint8_t buf[20] = {0};

    e->server->fn = fn_test_0x87_link_ctrl;
    e->server->fn_data = NULL;

    const uint8_t REQ[] = {
        0x87, /* SID */
        0x40, /* custom type to provoke error */
    };

    UDSTpSend(e->client_tp, REQ, sizeof(REQ), NULL);

    const uint8_t EXPECTED_RESP[] = {
        0x7F, /* Negative Response SID */
        0x87, /* Original SID */
        0x22, /* NRC: ConditionsNotCorrect */
    };

    /* the client transport should receive a response within client_p2 ms */
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);
    TEST_MEMORY_EQUAL(buf, EXPECTED_RESP, sizeof(EXPECTED_RESP));
}

int fn_0x27_noop(UDSServer_t *srv, UDSEvent_t ev, void *arg) {
    switch (ev) {
    case UDS_EVT_SecAccessRequestSeed: {
        UDSSecAccessRequestSeedArgs_t *r = (UDSSecAccessRequestSeedArgs_t *)arg;
        const uint8_t seed[] = {0x36, 0x57};
        return r->copySeed(srv, seed, sizeof(seed));
    }
    case UDS_EVT_SecAccessValidateKey: {
        return UDS_PositiveResponse;
    }
    default:
        break;
    }
    return UDS_PositiveResponse;
}

void test_security_level_resets_on_session_timeout(void **state) {
    Env_t *e = *state;
    uint8_t buf[8] = {0};

    // When a server handler function is installed
    e->server->fn = fn_0x27_noop;

    // and the anti-brute-force timeout has expired
    EnvRunMillis(e, UDS_SERVER_0x27_BRUTE_FORCE_MITIGATION_BOOT_DELAY_MS + 10);

    // and a seed request is sent to the server
    const uint8_t SEED_REQUEST[] = {0x27, 0x01};
    UDSTpSend(e->client_tp, SEED_REQUEST, sizeof(SEED_REQUEST), NULL);

    // the server should respond with a seed within p2 ms
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);

    // and the server security level should still be 0
    TEST_INT_EQUAL(e->server->securityLevel, 0);

    // When an unlock request is sent to the server
    const uint8_t UNLOCK_REQUEST[] = {0x27, 0x02, 0xC9, 0xA9};
    UDSTpSend(e->client_tp, UNLOCK_REQUEST, sizeof(UNLOCK_REQUEST), NULL);

    // the server should respond with a positive response within p2 ms
    EXPECT_WITHIN_MS(e, UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL) > 0,
                     UDS_CLIENT_DEFAULT_P2_MS);

    // and the server security level should now be 1
    TEST_INT_EQUAL(e->server->securityLevel, 1);

    // Switch to programming session (to allow timeout)
    const uint8_t SESSION_REQUEST[] = {0x10, 0x02};
    UDSTpSend(e->client_tp, SESSION_REQUEST, sizeof(SESSION_REQUEST), NULL);
    UDSTpRecv(e->client_tp, buf, sizeof(buf), NULL);

    // Wait for session timeout
    EnvRunMillis(e, e->server->s3_ms + 10);

    // Should now be in default session
    TEST_INT_EQUAL(e->server->sessionType, UDS_LEV_DS_DS);

    // Security level should be reset to locked
    TEST_INT_EQUAL(e->server->securityLevel, 0);
}

void test_badness(void **state) { TEST_INT_EQUAL(UDS_ERR_INVALID_ARG, UDSServerInit(NULL)); }

int main(int ac, char **av) {
    if (ac > 1) {
        cmocka_set_test_filter(av[1]);
    }
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_default_session_does_not_timeout, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_programming_session_times_out, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x10_no_fn_results_in_negative_resp, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x10_no_fn_results_in_negative_resp_functional, Setup,
                                        Teardown),
        cmocka_unit_test_setup_teardown(test_0x10_suppress_pos_resp, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x11_no_send_after_ECU_reset, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x14_positive_response, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x14_incorrect_request_length, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x14_negative_response, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x19_sub_0x01, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x19_sub_0x02, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x19_sub_0x02_no_matching_dtc, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x19_sub_0x03, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x19_sub_0x03_no_snapshots, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x19_sub_0x04, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x19_sub_0x04_no_records, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x19_sub_0x05, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x19_sub_0x05_no_data, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x19_sub_0x06, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x19_sub_0x06_no_data, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x19_sub_0x07, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x19_sub_0x08, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x19_sub_0x08_no_dtc, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x19_sub_0x09, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x19_sub_0x09_no_dtc, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x19_sub_0x0A, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x19_sub_0x0B, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x19_sub_0x0B_no_info, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x19_sub_0x0C, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x19_sub_0x0C_no_info, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x19_sub_0x0D, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x19_sub_0x0D_no_info, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x19_sub_0x0E, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x19_sub_0x0E_no_info, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x19_sub_0x14, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x19_sub_0x15, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x19_sub_0x15_no_data, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x19_sub_0x16, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x19_sub_0x16_no_record, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x19_sub_0x17, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x19_sub_0x17_no_matching_dtc, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x19_sub_0x18, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x19_sub_0x18_no_record, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x19_sub_0x19, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x19_sub_0x19_no_data, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x19_sub_0x1A, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x19_sub_0x1A_no_data, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x19_sub_0x42, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x19_sub_0x42_no_record, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x19_sub_0x55, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x19_sub_0x55_no_record, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x19_sub_0x56, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x19_sub_0x56_no_record, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x19_subfunc_not_suported, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x19_invalid_req_len, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x19_invalid_subFunction, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x19_shrink_default_response_len, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x19_malformed_responses, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x22, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x22_nonexistent, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x22_misuse, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x23, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x27_level_is_zero_at_init, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x27_unlock, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x27_brute_force_prevention_1, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x27_brute_force_prevention_2, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x28_comm_ctrl_example1, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x28_comm_ctrl_example2, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x28_comm_ctrl_invalid_request, Setup, Teardown),
        cmocka_unit_test_setup_teardown(
            test_0x28_comm_ctrl_forced_malformed_response_in_event_handler, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x29_auth_de_auth, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x29_req_too_short, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x29_auth_verify_certificate_unidirectional, Setup,
                                        Teardown),
        cmocka_unit_test_setup_teardown(test_0x29_auth_verify_certificate_bidirectional, Setup,
                                        Teardown),
        cmocka_unit_test_setup_teardown(test_0x29_auth_proof_of_ownership, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x29_auth_transmit_cert, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x29_auth_req_challenge_for_auth, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x29_auth_verify_pown_unidirectional, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x29_auth_verify_pown_bidirectional, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x29_auth_config, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x29_auth_timeout_on_session_timeout, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x29_auth_timeout_on_session_switch_to_default, Setup,
                                        Teardown),
        cmocka_unit_test_setup_teardown(test_0x29_auth_example_1, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x29_auth_example_2, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x29_auth_example_3, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x29_auth_example_4, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x29_auth_example_5, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x29_auth_example_6, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x29_auth_example_7, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x2C_request_too_short, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x2C_sub_0x01, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x2C_sub_0x01_request_too_short, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x2C_sub_0x01_negative_response, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x2C_sub_0x02, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x2C_sub_0x02_request_malformed, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x2C_sub_0x02_request_too_short, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x2C_sub_0x02_negative_response, Setup, Teardown),
        cmocka_unit_test_setup_teardown(
            test_0x2C_sub_0x02_zero_address_and_length_format_identifier, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x2C_sub_0x03, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x2C_sub_0x03_clear_all, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x2C_sub_0x03_negative_response, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x2F_example, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x2F_incorrect_request_length, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x2F_negative_response, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x31_RCRRP, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x34_no_handler, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x34, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x38_no_handler, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x38_addfile, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x38_delfile, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x3D_example_1, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x3D_example_2, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x3D_example_3, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x3e_suppress_positive_response, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x85_control_dtc_setting, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x85_control_dtc_setting_with_control_data, Setup,
                                        Teardown),
        cmocka_unit_test_setup_teardown(test_0x85_control_dtc_setting_request_too_short, Setup,
                                        Teardown),
        cmocka_unit_test_setup_teardown(
            test_0x85_control_dtc_setting_request_returns_negative_response, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x87_link_ctrl_sub_0x01, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x87_link_ctrl_sub_0x02, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x87_link_ctrl_negative_response, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_security_level_resets_on_session_timeout, Setup,
                                        Teardown),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}