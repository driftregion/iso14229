#pragma once

/**
 * @def UDS_SYS
 * @brief Selects the host system iso14229 is compiled for.
 * @see uds_sys_ for the list of valid values
 */

/**
 * @def UDS_TP_ISOTP_C
 * @brief if defined, build the @ref UDSTpISOTpC_t transport
 */

/**
 * @def UDS_CUSTOM_MILLIS
 * @brief bring your own UDSMillis implementation
 * @details Bring your own UDSMillis implementation. Valid values:
 * - `0` (default): iso14229 provides UDSMillis() for the detected @ref UDS_SYS platform
 * - `1`: the user must provide their own UDSMillis() implementation
 *
 * @see UDSMillis
 */
#ifndef UDS_CUSTOM_MILLIS
#define UDS_CUSTOM_MILLIS 0
#endif

#define UDS_ISOTP_MTU (4095) ///< ISO-TP Maximum Transmission Unit (ISO-15764-2-2004 section 5.3.3)

#ifndef UDS_TP_MTU
/// ISOTP is the only supported tp type, so UDS inherits its MTU
#define UDS_TP_MTU UDS_ISOTP_MTU
#endif

/**
 * @def UDS_SERVER_SEND_BUF_SIZE
 * @brief reduce this at your own risk to save RAM. Fuzz testing is done with the default of @ref
 * UDS_TP_MTU.
 */
#ifndef UDS_SERVER_SEND_BUF_SIZE
#define UDS_SERVER_SEND_BUF_SIZE (UDS_TP_MTU)
#endif

/** @copydoc UDS_SERVER_SEND_BUF_SIZE */
#ifndef UDS_SERVER_RECV_BUF_SIZE
#define UDS_SERVER_RECV_BUF_SIZE (UDS_TP_MTU)
#endif

/** @copydoc UDS_SERVER_SEND_BUF_SIZE */
#ifndef UDS_CLIENT_SEND_BUF_SIZE
#define UDS_CLIENT_SEND_BUF_SIZE (UDS_TP_MTU)
#endif

/** @copydoc UDS_SERVER_SEND_BUF_SIZE */
#ifndef UDS_CLIENT_RECV_BUF_SIZE
#define UDS_CLIENT_RECV_BUF_SIZE (UDS_TP_MTU)
#endif

#ifndef UDS_CLIENT_DEFAULT_P2_MS
#define UDS_CLIENT_DEFAULT_P2_MS (150U) ///< default P2 timeout
#endif

#ifndef UDS_CLIENT_DEFAULT_P2_STAR_MS
#define UDS_CLIENT_DEFAULT_P2_STAR_MS (1500U) ///< default P2* timeout
#endif

static_assert(UDS_CLIENT_DEFAULT_P2_STAR_MS > UDS_CLIENT_DEFAULT_P2_MS, "");

#ifndef UDS_SERVER_DEFAULT_P2_MS
#define UDS_SERVER_DEFAULT_P2_MS (50) ///< default P2 duration
#endif

#ifndef UDS_SERVER_DEFAULT_P2_STAR_MS
#define UDS_SERVER_DEFAULT_P2_STAR_MS (5000) ///< default P2* duration
#endif

#ifndef UDS_SERVER_DEFAULT_S3_MS
#define UDS_SERVER_DEFAULT_S3_MS                                                                   \
    (5100) ///< default S3 duration (ISO14229-2 2013 Table 5: 5000 -0/+200 ms)
#endif

static_assert((0 < UDS_SERVER_DEFAULT_P2_MS) &&
                  (UDS_SERVER_DEFAULT_P2_MS < UDS_SERVER_DEFAULT_P2_STAR_MS) &&
                  (UDS_SERVER_DEFAULT_P2_STAR_MS < UDS_SERVER_DEFAULT_S3_MS),
              "");

/// Duration between the server sending a positive response to an ECU reset request and the emission
/// of a UDS_EVT_DoScheduledReset event. This should be set to a duration adequate for the server
/// transport layer to finish responding to the ECU reset request.
#ifndef UDS_SERVER_DEFAULT_POWER_DOWN_TIME_MS
#define UDS_SERVER_DEFAULT_POWER_DOWN_TIME_MS (60)
#endif

#if (UDS_SERVER_DEFAULT_POWER_DOWN_TIME_MS < UDS_SERVER_DEFAULT_P2_MS)
#error "The server shall have adequate time to respond before reset"
#endif

/// Amount of time to wait after boot before accepting 0x27 requests.
#ifndef UDS_SERVER_0x27_BRUTE_FORCE_MITIGATION_BOOT_DELAY_MS
#define UDS_SERVER_0x27_BRUTE_FORCE_MITIGATION_BOOT_DELAY_MS (1000)
#endif

/// Amount of time to wait after an authentication failure before accepting another 0x27 request.
#ifndef UDS_SERVER_0x27_BRUTE_FORCE_MITIGATION_AUTH_FAIL_DELAY_MS
#define UDS_SERVER_0x27_BRUTE_FORCE_MITIGATION_AUTH_FAIL_DELAY_MS (1000)
#endif

#ifndef UDS_SERVER_DEFAULT_XFER_DATA_MAX_BLOCKLENGTH
/*! ISO14229-1:2013 Table 396. This parameter is used by the requestDownload positive response
message to inform the client how many data bytes (maxNumberOfBlockLength) to include in each
TransferData request message from the client. */
#define UDS_SERVER_DEFAULT_XFER_DATA_MAX_BLOCKLENGTH (UDS_TP_MTU)
#endif
