#include "test/env.h"
#include <stdio.h>

static UDSClient_t client;
static UDSTpHandle_t *srv_tp = NULL;

// int main() {
//     UDSClient_t client;

//     const uint8_t GOOD[] = {0x51, 0x01};
//     const uint8_t BAD_SID[] = {0x50, 0x01};
//     const uint8_t TOO_SHORT[] = {0x51};
//     const uint8_t BAD_SUBFUNC[] = {0x51, 0x02};
//     const uint8_t NEG[] = {0x7F, 0x11, 0x10};

// #define CASE(d1, opt, err)                                                                         \
//     {                                                                                              \
//         .tag = "client options:" #opt ", server resp: " #d1 ", expected_err: " #err, .resp = d1,   \
//         .resp_len = sizeof(d1), .options = opt, .expected_err = err                                \
//     }

//     struct {
//         const char *tag;
//         const uint8_t *resp;
//         size_t resp_len;
//         uint8_t options;
//         UDSErr_t expected_err;
//     } p[] = {
//         CASE(GOOD, 0, UDS_OK),
//         CASE(BAD_SID, 0, UDS_ERR_SID_MISMATCH),
//         CASE(TOO_SHORT, 0, UDS_ERR_RESP_TOO_SHORT),
//         CASE(BAD_SUBFUNC, 0, UDS_ERR_SUBFUNCTION_MISMATCH),
//         CASE(NEG, 0, UDS_OK),
//         CASE(NEG, UDS_NEG_RESP_IS_ERR, UDS_ERR_NEG_RESP),
//         CASE(GOOD, UDS_NEG_RESP_IS_ERR, UDS_OK),
//         CASE(GOOD, UDS_SUPPRESS_POS_RESP, UDS_OK), // Should this case pass?
//     };

//     for (size_t i = 0; i < sizeof(p) / sizeof(p[0]); i++) {
//         ENV_CLIENT_INIT(client);
//         UDSTpHandle_t *srv_tp = ENV_TpNew("server");

//         // when the client sends a ECU reset request with these options
//         client.options = p[i].options;
//         UDSSendECUReset(&client, kHardReset);

//         // and the server responds with this message
//         UDSTpSend(srv_tp, p[i].resp, p[i].resp_len, NULL);

//         // then the client should receive a response with this error code
//         ENV_RunMillis(50);
//         TEST_INT_EQUAL(client.state, kRequestStateIdle);
//         TEST_INT_EQUAL(client.err, p[i].expected_err);

//         ENV_TpFree(srv_tp);
//         ENV_TpFree(client.tp);
//     }
// }

typedef struct EventLog {
    UDSEvent_t evt;
    void *ev_data;
    unsigned time;
} EventLog_t;
#define MAX_NUM_EVENTS 500
unsigned EventCount = 0;
EventLog_t Events[MAX_NUM_EVENTS] = {0};

int FnLogEvents(UDSClient_t *client, UDSEvent_t evt, void *ev_data){
    assert(EventCount < MAX_NUM_EVENTS);
    Events[EventCount].evt = evt;
    switch (evt) {
        case UDS_EVT_Err:
            Events[EventCount].ev_data = malloc(sizeof(UDSErr_t));
            memmove(Events[EventCount].ev_data, ev_data, sizeof(UDSErr_t));
            break;
        default:
            Events[EventCount].ev_data = NULL;
            break;
    }
    Events[EventCount].time = UDSMillis();
    EventCount++;
    return 0;
}

bool EventLogContains(UDSEvent_t evt) {
    for (unsigned i = 0; i < EventCount; i++) {
        if (Events[i].evt == evt) {
            return true;
        }
    }
    return false;
}

EventLog_t *GetFirstEventOfType(UDSEvent_t evt) {
    for (unsigned i = 0; i < EventCount; i++) {
        if (Events[i].evt == evt) {
            return &Events[i];
        }
    }
    return NULL;
}

unsigned NumEventsOfType(UDSEvent_t evt) {
    unsigned cnt = 0;
    for (unsigned i = 0; i < EventCount; i++) {
        if (Events[i].evt == evt) {
            cnt++;
        }
    }
    return cnt;
}

void PrintEventLog() {
    printf("logged %d client events\n", EventCount);
    printf("%3s, %6s, %3s, %s \n", "idx","time", "num", "summary");
    for (unsigned i = 0; i < EventCount; i++) {
        char extra[32] = {0};
        if (Events[i].evt == UDS_EVT_Err) {
            snprintf(extra, sizeof(extra), ": [%s]", UDSErrToStr(*(int*)Events[i].ev_data));
        } 
        printf("% 3d, % 6d, % 3d, %s%s\n", i, Events[i].time, Events[i].evt, UDSEvtToStr(Events[i].evt), extra);
    }
    fflush(stdout);
}

void TestGood(void **state) {
    // When the client sends a request
    UDSSendECUReset(&client, kHardReset);

    // and the server responds positively
    const uint8_t GOOD[] = {0x51, 0x01};
    UDSTpSend(srv_tp, GOOD, sizeof(GOOD), NULL);

    // the client should emit a response event within p2
    EXPECT_WITHIN_MS(EventLogContains(UDS_EVT_ResponseReceived), 50);

    // and there should be no errors
    assert_int_equal(NumEventsOfType(UDS_EVT_Err), 0);
}


void TestSIDMismatch(void **state) {
    // When the client sends a request
    UDSSendECUReset(&client, kHardReset);

    // and the server responds with the wrong SID
    const uint8_t BAD_SID[] = {0x50, 0x01};
    UDSTpSend(srv_tp, BAD_SID, sizeof(BAD_SID), NULL);

    // the client should emit an error event within p2
    EXPECT_WITHIN_MS(EventLogContains(UDS_EVT_Err), 50);

    // this can be made into a macro
    EventLog_t *ev = GetFirstEventOfType(UDS_EVT_Err);
    assert_non_null(ev);
    UDSErr_t *err = (UDSErr_t*)ev->ev_data;
    assert_non_null(err);
    printf("error type: %d (%s)\n", *err, UDSErrToStr(*err));
    assert_int_equal(*err, UDS_ERR_SID_MISMATCH);
}

void TestShortResponse(void **state) {
    // When the client sends a request
    UDSSendECUReset(&client, kHardReset);

    // and the server responds with a response that is too short
    const uint8_t TOO_SHORT[] = {0x51};
    UDSTpSend(srv_tp, TOO_SHORT, sizeof(TOO_SHORT), NULL);

    // the client should emit an error event within p2
    EXPECT_WITHIN_MS(EventLogContains(UDS_EVT_Err), 50);
}

void TestBadSubFunc(void **state) {
    // When the client sends a request
    UDSSendECUReset(&client, kHardReset);

    // and the server responds with an inconsistent subfunction
    const uint8_t BAD_SUBFUNC[] = {0x51, 0x02};
    UDSTpSend(srv_tp, BAD_SUBFUNC, sizeof(BAD_SUBFUNC), NULL);

    // the client should emit an error event within p2
    EXPECT_WITHIN_MS(EventLogContains(UDS_EVT_Err), 50);
}


void TestNegResp(void **state) {
    // When the client sends a request
    UDSSendECUReset(&client, kHardReset);

    // and the server responds negatively
    const uint8_t NEG[] = {0x7F, 0x11, 0x10};
    UDSTpSend(srv_tp, NEG, sizeof(NEG), NULL);

    // the client should emit an error event within p2
    EXPECT_WITHIN_MS(EventLogContains(UDS_EVT_Err), 50);
}



//     const uint8_t TOO_SHORT[] = {0x51};
//     const uint8_t BAD_SUBFUNC[] = {0x51, 0x02};
//     const uint8_t NEG[] = {0x7F, 0x11, 0x10};


int setup(void **state) {
    ENV_CLIENT_INIT(client);
    srv_tp = ENV_TpNew("server");
    client.fn = FnLogEvents;
    return 0;
}

int teardown(void **state) {
    PrintEventLog();
    ENV_TpFree(srv_tp);
    ENV_TpFree(client.tp);
    EventCount = 0;
    return 0;
}


int main() {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(TestGood, setup, teardown),
        cmocka_unit_test_setup_teardown(TestSIDMismatch, setup, teardown),
        cmocka_unit_test_setup_teardown(TestShortResponse, setup, teardown),
        cmocka_unit_test_setup_teardown(TestNegResp, setup, teardown),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}

