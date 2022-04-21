#include <errno.h>
#include <error.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "../iso14229server.h"

#define SRV_PHYS_RECV_ID 0x7A0 // server listens for physical (1:1) messages on this CAN ID
#define SRV_FUNC_RECV_ID 0x7A1 // server listens for functional (1:n) messages on this CAN ID
#define SRV_SEND_ID 0x7A8      // server responds on this CAN ID
#define ISOTP_BUFSIZE 256

int g_sockfd;               // CAN socket FD
bool g_should_exit = false; // flag for shutting down
int userSendCAN(const uint32_t arbitration_id, const uint8_t *data, const uint8_t size);
static Iso14229Server *uds_ptr = NULL;

/**
 * @brief poll for CAN messages
 * @return 0 if message is present, -1 otherwise
 */
int CANRxPoll(uint32_t *arb_id, uint8_t *data, uint8_t *size) {
    struct can_frame frame = {0};

    int nbytes = read(g_sockfd, &frame, sizeof(struct can_frame));

    if (nbytes < 0) {
        if (EAGAIN == errno || EWOULDBLOCK == errno) {
            return -1;
        } else {
            perror("Read err");
            exit(-1);
        }
    }

    *arb_id = frame.can_id;
    *size = frame.can_dlc;
    memmove(data, frame.data, *size);
    return 0;
}

/**
 * @brief close file descriptor on SIGINT
 * @param signum
 */
void teardown(int signum) {
    if (close(g_sockfd) < 0) {
        perror("failed to close socket");
        exit(-1);
    }
    g_should_exit = true;
}

/**
 * @brief millisecond sleep
 * @param tms time in milliseconds
 */
int msleep(long tms) {
    struct timespec ts;
    int ret;

    if (tms < 0) {
        errno = EINVAL;
        return -1;
    }

    ts.tv_sec = tms / 1000;
    ts.tv_nsec = (tms % 1000) * 1000000;

    do {
        ret = nanosleep(&ts, &ts);
    } while (ret && errno == EINTR);

    return ret;
}

struct sigaction action;
struct sockaddr_can addr;
struct ifreq ifr;
struct stat fd_stat;
FILE *fd;

void setupSocket(int ac, char **av) {
    memset(&action, 0, sizeof(action));
    action.sa_handler = teardown;
    sigaction(SIGINT, &action, NULL);

    if ((g_sockfd = socket(PF_CAN, SOCK_RAW | SOCK_NONBLOCK, CAN_RAW)) < 0) {
        perror("Socket");
        exit(-1);
    }

    if (ac < 2) {
        printf("usage: %s [socketCAN link]\n", av[0]);
        exit(-1);
    }

    strcpy(ifr.ifr_name, av[1]);
    ioctl(g_sockfd, SIOCGIFINDEX, &ifr);

    memset(&addr, 0, sizeof(addr));
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    if (bind(g_sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("Bind");
        exit(-1);
    }

    // Try sending a message. This will fail if the network is down.
    userSendCAN(0x111, (uint8_t[4]){1, 2, 3, 4}, 4);

    printf("listening on %s\n", av[1]);
}

// =====================================
// STEP 1: implement the hooks
// =====================================
/**
 * @brief iso14229.h required function
 * Implement this with the functions available on your host platform
 */
int userSendCAN(const uint32_t arbitration_id, const uint8_t *data, const uint8_t size) {
    struct can_frame frame = {0};

    frame.can_id = arbitration_id;
    frame.can_dlc = size;
    memmove(frame.data, data, size);

    if (write(g_sockfd, &frame, sizeof(struct can_frame)) != sizeof(struct can_frame)) {
        perror("Write err");
        exit(-1);
    }
    return 0;
}

/**
 * @brief iso14229.h required function
 * Implement this with the functions available on your host platform
 */
uint32_t userGetms() {
    struct timeval te;
    gettimeofday(&te, NULL);                                         // get current time
    long long milliseconds = te.tv_sec * 1000LL + te.tv_usec / 1000; // calculate milliseconds
    return milliseconds;
}

/**
 * @brief iso14229.h required function
 * Implement this with the functions available on your host platform
 */
void userDebug(const char *fmt, ...) {}

/**
 * @brief Schedule an ECU reset
 */
enum Iso14229ResponseCode ecuResetHandler(const struct Iso14229ServerStatus *status,
                                          uint8_t resetType) {
    printf("reset scheduled!\n");
    return kPositiveResponse;
}

void sessionTimeoutHandler() { printf("session timed out\n"); }

// =====================================
// STEP 2: initialize the server
// =====================================

int main(int ac, char **av) {
    // setup the linux CAN socket. This will vary depending on your platform. see example/server.c
    setupSocket(ac, av);

    uint8_t isotpPhysRecvBuf[ISOTP_BUFSIZE];
    uint8_t isotpPhysSendBuf[ISOTP_BUFSIZE];
    uint8_t isotpFuncRecvBuf[ISOTP_BUFSIZE];
    uint8_t isotpFuncSendBuf[ISOTP_BUFSIZE];
    uint8_t udsSendBuf[ISOTP_BUFSIZE];
    uint8_t udsRecvBuf[ISOTP_BUFSIZE];

    IsoTpLink isotpPhysLink;
    IsoTpLink isotpFuncLink;
    Iso14229Server uds;
    uds_ptr = &uds;

    const Iso14229ServerConfig cfg = {
        .phys_recv_id = SRV_PHYS_RECV_ID,
        .func_recv_id = SRV_FUNC_RECV_ID,
        .send_id = SRV_SEND_ID,
        .phys_link = &isotpPhysLink,
        .func_link = &isotpFuncLink,
        .receive_buffer = udsRecvBuf,
        .receive_buf_size = sizeof(udsRecvBuf),
        .send_buffer = udsSendBuf,
        .send_buf_size = sizeof(udsSendBuf),
        .userRDBIHandler = NULL,
        .userWDBIHandler = NULL,
        .userECUResetHandler = ecuResetHandler,
        .userSessionTimeoutHandler = sessionTimeoutHandler,
        .userGetms = userGetms,
        .p2_ms = 50,
        .p2_star_ms = 2000,
        .s3_ms = 5000,
    };

    Iso14229Server srv;

    /* initialize the ISO-TP links */
    isotp_init_link(&isotpPhysLink, SRV_SEND_ID, isotpPhysSendBuf, sizeof(isotpPhysSendBuf),
                    isotpPhysRecvBuf, sizeof(isotpPhysRecvBuf), userGetms, userSendCAN, userDebug);
    isotp_init_link(&isotpFuncLink, SRV_SEND_ID, isotpFuncSendBuf, sizeof(isotpFuncSendBuf),
                    isotpFuncRecvBuf, sizeof(isotpFuncRecvBuf), userGetms, userSendCAN, userDebug);

    Iso14229ServerInit(&srv, &cfg);
    iso14229ServerEnableService(&srv, kSID_ECU_RESET);

    // =====================================
    // STEP 3: poll the server
    // =====================================

    while (!g_should_exit) {
        uint32_t arb_id;
        uint8_t data[8];
        uint8_t size;

        Iso14229ServerPoll(&srv);
        if (0 == CANRxPoll(&arb_id, data, &size)) {
            iso14229ServerReceiveCAN(&srv, arb_id, data, size);
        }
        msleep(10);
    }
}