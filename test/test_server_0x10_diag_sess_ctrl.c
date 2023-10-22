

#include "iso14229.h"
#include "test/env.h"

uint8_t fn(UDSServer_t *srv, UDSServerEvent_t ev, const void *arg) {
    printf("holy shit\n");
    return kServiceNotSupported;
}

int main() {
    UDSServer_t srv;
    UDSTpHandle_t *tp = ENV_TpNew(); 
    UDSServerInit(&srv, &(UDSServerConfig_t){
        .fn = fn,
        .tp = tp,
    });

    const uint8_t REQ[] = {0x10, 0x02};

    UDSSDU_t req = {
        .A_Mtype = UDS_A_MTYPE_DIAG,
        .A_SA = 0x7E0,
        .A_TA = 0x7E8,
        .A_TA_Type = UDS_A_TA_TYPE_PHYSICAL,
        .A_Length = sizeof(REQ),
        .A_Data = REQ,
    };
    
    ENV_Send(&req);


    UDSSDU_t resp;
    ENV_EXPECT_MSG_WITHIN_MILLIS(&resp, 50);
    const uint8_t EXP_RESP[] = {0x7f, 0x10, 0x11};
    ASSERT_BYTES_EQUAL(resp.A_Data, EXP_RESP, sizeof(EXP_RESP));
}

void testServer0x10DiagSessCtrlIsDisabledByDefault() {
    TEST_SETUP(CLIENT_ONLY);
    const uint8_t REQ[] = {0x10, 0x02};
    SEND_TO_SERVER(REQ, kTpAddrTypePhysical);
    const uint8_t RESP[] = {0x7f, 0x10, 0x11};
    EXPECT_RESPONSE_WITHIN_MILLIS(RESP, kTpAddrTypePhysical, 50);
    TEST_TEARDOWN();
}
