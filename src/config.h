#pragma once

/** ISO-TP Maximum Transmissiable Unit (ISO-15764-2-2004 section 5.3.3) */
#define UDS_ISOTP_MTU (4095)

#ifndef UDS_TP_MTU
#define UDS_TP_MTU UDS_ISOTP_MTU
#endif

#ifndef UDS_SERVER_SEND_BUF_SIZE
#define UDS_SERVER_SEND_BUF_SIZE (UDS_TP_MTU)
#endif

#ifndef UDS_SERVER_RECV_BUF_SIZE
#define UDS_SERVER_RECV_BUF_SIZE (UDS_TP_MTU)
#endif

#ifndef UDS_CLIENT_SEND_BUF_SIZE
#define UDS_CLIENT_SEND_BUF_SIZE (UDS_TP_MTU)
#endif

#ifndef UDS_CLIENT_RECV_BUF_SIZE
#define UDS_CLIENT_RECV_BUF_SIZE (UDS_TP_MTU)
#endif

#ifndef UDS_CLIENT_DEFAULT_P2_MS
#define UDS_CLIENT_DEFAULT_P2_MS (150U)
#endif

#ifndef UDS_CLIENT_DEFAULT_P2_STAR_MS
#define UDS_CLIENT_DEFAULT_P2_STAR_MS (1500U)
#endif

// Default value from ISO14229-2 2013 Table 5: 2000 ms
#ifndef UDS_CLIENT_DEFAULT_S3_MS
#define UDS_CLIENT_DEFAULT_S3_MS (2000)
#endif

static_assert(UDS_CLIENT_DEFAULT_P2_STAR_MS > UDS_CLIENT_DEFAULT_P2_MS, "");

// Default value from ISO14229-2 2013 Table 4: 50 ms
#ifndef UDS_SERVER_DEFAULT_P2_MS
#define UDS_SERVER_DEFAULT_P2_MS (50)
#endif

// Default value from ISO14229-2 2013 Table 4: 5000 ms
#ifndef UDS_SERVER_DEFAULT_P2_STAR_MS
#define UDS_SERVER_DEFAULT_P2_STAR_MS (5000)
#endif

// Default value from ISO14229-2 2013 Table 5: 5000 -0/+200 ms
#ifndef UDS_SERVER_DEFAULT_S3_MS
#define UDS_SERVER_DEFAULT_S3_MS (5100)
#endif

static_assert((0 < UDS_SERVER_DEFAULT_P2_MS) &&
                  (UDS_SERVER_DEFAULT_P2_MS < UDS_SERVER_DEFAULT_P2_STAR_MS) &&
                  (UDS_SERVER_DEFAULT_P2_STAR_MS < UDS_SERVER_DEFAULT_S3_MS),
              "");

// Duration between the server sending a positive response to an ECU reset request and the emission
// of a UDS_EVT_DoScheduledReset event. This should be set to a duration adequate for the server
// transport layer to finish responding to the ECU reset request.
#ifndef UDS_SERVER_DEFAULT_POWER_DOWN_TIME_MS
#define UDS_SERVER_DEFAULT_POWER_DOWN_TIME_MS (60)
#endif

#if (UDS_SERVER_DEFAULT_POWER_DOWN_TIME_MS < UDS_SERVER_DEFAULT_P2_MS)
#error "The server shall have adequate time to respond before reset"
#endif

// Amount of time to wait after boot before accepting 0x27 requests.
#ifndef UDS_SERVER_0x27_BRUTE_FORCE_MITIGATION_BOOT_DELAY_MS
#define UDS_SERVER_0x27_BRUTE_FORCE_MITIGATION_BOOT_DELAY_MS (1000)
#endif

// Amount of time to wait after an authentication failure before accepting another 0x27 request.
#ifndef UDS_SERVER_0x27_BRUTE_FORCE_MITIGATION_AUTH_FAIL_DELAY_MS
#define UDS_SERVER_0x27_BRUTE_FORCE_MITIGATION_AUTH_FAIL_DELAY_MS (1000)
#endif

#ifndef UDS_SERVER_DEFAULT_XFER_DATA_MAX_BLOCKLENGTH
/*! ISO14229-1:2013 Table 396. This parameter is used by the requestDownload positive response
message to inform the client how many data bytes (maxNumberOfBlockLength) to include in each
TransferData request message from the client. */
#define UDS_SERVER_DEFAULT_XFER_DATA_MAX_BLOCKLENGTH (UDS_TP_MTU)
#endif

#ifndef UDS_CUSTOM_MILLIS
#define UDS_CUSTOM_MILLIS 0
#endif
