#include "src/iso14229.h"
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    UDSSDU_t sdu_info;
    ssize_t msg_len;
    int srv_retval;
    uint8_t msg[UDS_TP_MTU];
} StuffToFuzz_t;


int main() {
    FILE *fp = fopen("corpus/test_fuzz_server", "w");
    StuffToFuzz_t fuzz_buf = {
        .sdu_info = {
            .A_Mtype = UDS_A_MTYPE_DIAG,
            .A_SA = 0x7E0,
            .A_TA = 0x7E8,
            .A_TA_Type = UDS_A_TA_TYPE_PHYSICAL,
            .A_AE = 0x7DF,
        },
        .msg_len = 3,
        .srv_retval = 0,
        .msg = {0x02, 0x10, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    };
    fwrite(&fuzz_buf, sizeof(fuzz_buf), 1, fp);
    fclose(fp);
}