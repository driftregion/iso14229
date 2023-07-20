#include "../iso14229.h"
#include "../isotp-c/isotp.h"
#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <errno.h>

static int sockfd = 0;
static bool HasSetup = false;

static void SetupOnce() {
    if (HasSetup) {
        return;
    }
    UDS_DBG_PRINT("setting up CAN\n");
    struct sockaddr_can addr;
    struct ifreq ifr;
    if ((sockfd = socket(PF_CAN, SOCK_RAW | SOCK_NONBLOCK, CAN_RAW)) < 0) {
        perror("socket");
        exit(-1);
    }
    strcpy(ifr.ifr_name, "vcan0");
    ioctl(sockfd, SIOCGIFINDEX, &ifr);
    memset(&addr, 0, sizeof(addr));
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;
    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        exit(-1);
    }
    HasSetup = true;
}

uint32_t isotp_user_get_us() { return UDSMillis() * 1000; }

void isotp_user_debug(const char *message, ...) {}

int isotp_user_send_can(const uint32_t arbitration_id, const uint8_t *data, const uint8_t size) {
    SetupOnce();
    struct can_frame frame = {0};
    frame.can_id = arbitration_id;
    frame.can_dlc = size;
    memmove(frame.data, data, size);
    if (write(sockfd, &frame, sizeof(struct can_frame)) != sizeof(struct can_frame)) {
        perror("Write err");
        return ISOTP_RET_ERROR;
    }
    return ISOTP_RET_OK;
}

static void printhex(const uint8_t *addr, int len) {
    for (int i = 0; i < len; i++) {
        printf("%02x,", addr[i]);
    }
    printf("\n");
}

void SocketCANRecv(UDSTpIsoTpC_t *tp, int phys_recv_id, int func_recv_id) {
    assert(tp);
    SetupOnce();
    struct can_frame frame = {0};
    int nbytes = 0;
    for (;;) {
        nbytes = read(sockfd, &frame, sizeof(struct can_frame));
        if (nbytes < 0) {
            if (EAGAIN == errno || EWOULDBLOCK == errno) {
                break;
            } else {
                perror("read");
            }
        } else if (nbytes == 0) {
            break;
        } else {
            if (frame.can_id == phys_recv_id) {
                UDS_DBG_PRINT("phys recvd can\n");
                UDS_DBG_PRINTHEX(frame.data, frame.can_dlc);
                isotp_on_can_message(&tp->phys_link, frame.data, frame.can_dlc);
            } else if (frame.can_id == func_recv_id) {
                UDS_DBG_PRINT("func recvd can\n");
                UDS_DBG_PRINTHEX(frame.data, frame.can_dlc);
                isotp_on_can_message(&tp->func_link, frame.data, frame.can_dlc);
            }
        }
    }
}