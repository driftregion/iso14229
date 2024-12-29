#include "iso14229.h"
#include "env.h"


int server_fn(UDSServer_t *srv, UDSEvent_t evt, void *arg) {
    switch (evt) {
        case UDS_EVT_EcuReset:
            return UDS_NRC_GeneralReject;
        case UDS_EVT_DoScheduledReset:
            printf("server: Simulated ECU reset\n");
            srv->ecuResetScheduled = 0;
            break;
        default:
            printf("server: unhandled event %s (%d)\n", UDSEvtToStr(evt), evt);
            break;
    }
    return UDS_OK;
}


typedef enum {
    STEP_0_Send,
    STEP_0_Recv,
    STEP_DONE,
} SeqStep_t;

typedef struct {
    SeqStep_t step;
    UDSErr_t err;
    int ecu_reset_retry_counter;
} SeqState_t;

int client_fn(UDSClient_t *client, UDSEvent_t evt, void *ev_data) {
    SeqState_t *ss = (SeqState_t*)client->fn_data;
    switch (ss->step) {
        case STEP_0_Send:
            ss->err = UDSSendECUReset(client, kHardReset);
            if (ss->err == UDS_OK) {
                ss->step = STEP_0_Recv;
            } else {
                ss->step = STEP_DONE;
            }
            break;
        case STEP_0_Recv:
            if (evt == UDS_EVT_ResponseReceived) {
                printf("client: exiting on step %d with error: %s\n", ss->step, UDSErrToStr(ss->err));
                ss->step = STEP_DONE;
            }
            else if (evt == UDS_EVT_Err) {
                UDSErr_t err = *((UDSErr_t*)ev_data);
                if (err == UDS_NRC_GeneralReject && ss->ecu_reset_retry_counter < 3) {
                    ss->ecu_reset_retry_counter++;
                    ss->step = STEP_0_Send;
                    printf("client: retrying ECU reset (%d)\n", ss->ecu_reset_retry_counter);
                } else {
                    printf("client: exiting on step %d with error: %s\n", ss->step, UDSErrToStr(err));
                    ss->step = STEP_DONE;
                }
            }
            break;
        case STEP_DONE:
            break;
        default:
            break;
    }
    return UDS_OK;
}

int main() {
    UDSClient_t client; 
    UDSServer_t server;

    UDSClientInit(&client);
    client.tp = TPMockNew("client", TPMOCK_DEFAULT_CLIENT_ARGS);
    client.fn = client_fn;
    SeqState_t seq_state = {0};
    client.fn_data = &seq_state;

    UDSServerInit(&server);
    server.tp = TPMockNew("server", TPMOCK_DEFAULT_SERVER_ARGS);
    server.fn = server_fn;

    TPMockLogToStdout();

    for (int i = 0; i < 1000; i++) {
        ENV_RunMillis(1);
        UDSClientPoll(&client);
        UDSServerPoll(&server);
    }
}