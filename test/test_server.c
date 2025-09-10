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
    int *call_count = (int *)srv->fn_data;
    TEST_INT_EQUAL(UDS_EVT_SessionTimeout, ev);
    (*call_count)++;
    return UDS_OK;
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

// Container to provide data and length to the 0x19 handler function
typedef struct {
    void *data;
    size_t len;
    char *test_identifier;
} Test0x19FnData_t;

int fn_test_0x19(UDSServer_t *srv, UDSEvent_t ev, void *arg) {
    TEST_INT_EQUAL(ev, UDS_EVT_ReadDTCInformation);
    UDSRDTCIArgs_t *r = (UDSRDTCIArgs_t *)arg;
    Test0x19FnData_t *fnData = (Test0x19FnData_t *)srv->fn_data;

    switch (r->type) {
    case 0x01: /* reportNumberOfDTCByStatusMask */
        TEST_INT_EQUAL(r->numOfDTCByStatusMaskArgs.mask, 0x8);
        break;
    case 0x02: /* reportDTCByStatusMask */
        if (strcmp("test_0x19_Sub_2", fnData->test_identifier) == 0) {
            TEST_INT_EQUAL(r->dtcStatusByMaskArgs.mask, 0x84);
        } else if (strcmp("test_0x19_Sub_2_no_matching_dtc", fnData->test_identifier) == 0) {
            TEST_INT_EQUAL(r->dtcStatusByMaskArgs.mask, 0x01);
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
        TEST_INT_EQUAL(r->dtcSnapshotRecordbyDTCNumArgs.dtc, 0x00123456);
        TEST_INT_EQUAL(r->dtcSnapshotRecordbyDTCNumArgs.snapshotNum, 0x02);
        break;
    case 0x05: /* reportDTCStoredDataByRecordNumber */
        TEST_INT_EQUAL(r->dtcStoredDataByRecordNumArgs.recordNum, 0x02);
        break;
    case 0x06: /* reportDTCExtDataRecordByDTCNumber */
        TEST_INT_EQUAL(r->dtcExtDtaRecordByDTCNumArgs.dtc, 0x00123456);
        TEST_INT_EQUAL(r->dtcExtDtaRecordByDTCNumArgs.extDataRecNum, 0xFF);
        break;
    case 0x07: /* reportNumberOfDTCBySeverityMaskRecord */
    case 0x08: /* reportDTCBySeverityMaskRecord */
        TEST_INT_EQUAL(r->numOfDTCBySeverityMaskArgs.severityMask, 0xC0);
        TEST_INT_EQUAL(r->numOfDTCBySeverityMaskArgs.statusMask, 0x01);
        break;
    case 0x09: /* reportDTCBySeverityMaskRecord */
        TEST_INT_EQUAL(r->severityInfoOfDTCArgs.dtc, 0x00080511);
        break;
    case 0x16: /* reportDTCExtDataRecordByNumber */
        TEST_INT_EQUAL(r->dtcExtDataRecordByRecordNumArgs.recordNum, 0x05);
        break;
    case 0x17: /* reportUserDefMemoryDTCByStatusMask */
        if (strcmp("test_0x19_Sub_0x17", fnData->test_identifier) == 0) {
            TEST_INT_EQUAL(r->userDefMemoryDTCByStatusMaskArgs.mask, 0x84);
        } else if (strcmp("test_0x19_Sub_0x17_no_matching_dtc", fnData->test_identifier) == 0) {
            TEST_INT_EQUAL(r->userDefMemoryDTCByStatusMaskArgs.mask, 0x01);
        }

        TEST_INT_EQUAL(r->userDefMemoryDTCByStatusMaskArgs.memory, 0x18);
        break;

    case 0x18: /* reportUserDefMemoryDTCSnapshotRecordByDTCNumber */
        TEST_INT_EQUAL(r->userDefMemDTCSnapshotRecordByDTCNumArgs.dtc, 0x00123456);
        TEST_INT_EQUAL(r->userDefMemDTCSnapshotRecordByDTCNumArgs.snapshotNum, 0x06);
        TEST_INT_EQUAL(r->userDefMemDTCSnapshotRecordByDTCNumArgs.memory, 0x18);
        break;
    case 0x19: /* reportUserDefMemoryDTCExtDataRecordByDTCNumber */
        TEST_INT_EQUAL(r->userDefMemDTCExtDataRecordByDTCNumArgs.dtc, 0x00123456);
        TEST_INT_EQUAL(r->userDefMemDTCExtDataRecordByDTCNumArgs.extDataRecNum, 0xFF);
        TEST_INT_EQUAL(r->userDefMemDTCExtDataRecordByDTCNumArgs.memory, 0x18);
        break;
    case 0x1A: /* reportDTCExtendedDataRecordIdentification */
        TEST_INT_EQUAL(r->dtcExtDataRecordIdArgs.recordNum, 0x91);
        break;
    case 0x42: /* reportWWHOBDDTCByMaskRecord */
        TEST_INT_EQUAL(r->wwhobdDTCByMaskArgs.functionalGroup, 0x33);
        TEST_INT_EQUAL(r->wwhobdDTCByMaskArgs.statusMask, 0x08);
        TEST_INT_EQUAL(r->wwhobdDTCByMaskArgs.severityMask, 0xFF);
        break;
    case 0x55: /* reportWWHOBDDTCWithPermanentStatus */
        TEST_INT_EQUAL(r->wwhobdDTCByMaskArgs.functionalGroup, 0x33);
        break;
    case 0x56: /* reportDTCInformationByDTCReadinessGroupIdentifier */
        TEST_INT_EQUAL(r->dtcInfoByDTCReadinessGroupIdArgs.functionalGroup, 0x33);
        TEST_INT_EQUAL(r->dtcInfoByDTCReadinessGroupIdArgs.readinessGroup, 0x01);
        break;

    default:
        return UDS_NRC_ConditionsNotCorrect;
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

// ISO14229-1 2020 12.3.5.3 Example #2 - ReadDTCInformation, SubFunction = reportDTCByStatusMask,
// matching DTCs returned
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

// ISO14229-1 2020 12.3.5.4 Example #3 - ReadDTCInformation, SubFunction = reportDTCByStatusMask, no
// matching DTCs returned
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

    uint8_t ResponseData[] = {};
    Test0x19FnData_t fn_data = {.data = ResponseData, .len = sizeof(ResponseData)};

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

// ISO14229-1 2020 12.3.5.12 Example #11 – ReadDTCInformation - SubFunction = reportSupportedDTCs
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
void test_0x19_sub_0x14(void **state) {
    Env_t *e = *state;
    uint8_t buf[512] = {0};

    uint8_t ResponseData[] = {0x12, 0x34, 0x56, 0x24, 0x60};
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
        0x24, /* DTCAndStatusRecord [status of DTC] */
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

    uint8_t ResponseData[9] = {
        0x07F, 0x0A, 0x9B, 0x17, 0x24, 0x08, 0x05, 0x11, 0x2F,
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

    uint8_t ResponseData[9] = {
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

    uint8_t ResponseData[] = {};
    Test0x19FnData_t fn_data = {.data = ResponseData, .len = sizeof(ResponseData)};

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

    // When a server handler function is installed that does not handle the UDS_EVT_ReadDataByIdent
    // event
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

    // When a server handler function is installed and the anti-brute-force timeout has not expired
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
        cmocka_unit_test_setup_teardown(test_0x22, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x22_nonexistent, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x22_misuse, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x23, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x27_level_is_zero_at_init, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x27_unlock, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x27_brute_force_prevention_1, Setup, Teardown),
        cmocka_unit_test_setup_teardown(test_0x27_brute_force_prevention_2, Setup, Teardown),
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
        cmocka_unit_test_setup_teardown(test_security_level_resets_on_session_timeout, Setup,
                                        Teardown),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}