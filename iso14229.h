#ifndef ISO14229_H
#define ISO14229_H

/**
 * @file iso14229.h
 * @brief ISO14229-1 (UDS) library
 * @copyright Copyright (c) Nick Kirkby
 * @see https://github.com/driftregion/iso14229
 */

#ifdef __cplusplus
extern "C" {
#endif

#define UDS_LIB_VERSION "0.9.0"

#define UDS_SYS_CUSTOM 0
#define UDS_SYS_UNIX 1
#define UDS_SYS_WINDOWS 2
#define UDS_SYS_ARDUINO 3
#define UDS_SYS_ESP32 4

#if !defined(UDS_SYS)

#if defined(__unix__) || defined(__APPLE__)
#define UDS_SYS UDS_SYS_UNIX
#elif defined(_WIN32)
#define UDS_SYS UDS_SYS_WINDOWS
#elif defined(ARDUINO)
#define UDS_SYS UDS_SYS_ARDUINO
#elif defined(ESP_PLATFORM)
#define UDS_SYS UDS_SYS_ESP32
#else
#define UDS_SYS UDS_SYS_CUSTOM
#endif

#endif

#if UDS_SYS == UDS_SYS_ARDUINO

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>
#include <Arduino.h>

#ifndef UDS_TP_ISOTP_C
#define UDS_TP_ISOTP_C
#endif

#endif

#if UDS_SYS == UDS_SYS_UNIX

#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>

#endif

#if UDS_SYS == UDS_SYS_WINDOWS

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <time.h>
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;

#ifdef _MSC_VER
#define strncasecmp _strnicmp
#define strcasecmp _stricmp
#endif

#endif

#if UDS_SYS == UDS_SYS_ESP32

#include <string.h>
#include <inttypes.h>
#include <esp_timer.h>

#define UDS_TP_ISOTP_C 1

#endif

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

#if defined UDS_TP_ISOTP_C_SOCKETCAN
#ifndef UDS_TP_ISOTP_C
#define UDS_TP_ISOTP_C
#endif
#endif

enum UDSTpStatusFlags {
    UDS_TP_IDLE = 0x0000,
    UDS_TP_SEND_IN_PROGRESS = 0x0001,
    UDS_TP_RECV_COMPLETE = 0x0002,
    UDS_TP_ERR = 0x0004,
};

typedef uint32_t UDSTpStatus_t;

typedef enum {
    UDS_A_MTYPE_DIAG = 0,
    UDS_A_MTYPE_REMOTE_DIAG,
    UDS_A_MTYPE_SECURE_DIAG,
    UDS_A_MTYPE_SECURE_REMOTE_DIAG,
} UDS_A_Mtype_t;

typedef enum {
    UDS_A_TA_TYPE_PHYSICAL = 0, // unicast (1:1)
    UDS_A_TA_TYPE_FUNCTIONAL,   // multicast
} UDS_A_TA_Type_t;

typedef uint8_t UDSTpAddr_t;

/**
 * @brief Service data unit (SDU)
 * @details data interface between the application layer and the transport layer
 */
typedef struct {
    UDS_A_Mtype_t A_Mtype;     /**< message type (diagnostic, remote diagnostic, secure diagnostic,
                                  secure remote diagnostic) */
    uint32_t A_SA;             /**< application source address */
    uint32_t A_TA;             /**< application target address */
    UDS_A_TA_Type_t A_TA_Type; /**< application target address type (physical or functional) */
    uint32_t A_AE;             /**< application layer remote address */
} UDSSDU_t;

#define UDS_TP_NOOP_ADDR (0xFFFFFFFF)

/**
 * @brief UDS Transport layer
 * @note implementers should embed this struct at offset zero in their own transport layer handle
 */
typedef struct UDSTp {
    /**
     * @brief Send data to the transport
     * @param hdl: pointer to transport handle
     * @param buf: a pointer to the data to send
     * @param len: length of data to send
     * @param info: pointer to SDU info (may be NULL). If NULL, implementation should send with
     * physical addressing
     */
    ssize_t (*send)(struct UDSTp *hdl, uint8_t *buf, size_t len, UDSSDU_t *info);

    /**
     * @brief Receive data from the transport
     * @param hdl: transport handle
     * @param buf: receive buffer
     * @param bufsize: size of the receive buffer
     * @param info: pointer to SDU info to be updated by transport implementation. May be NULL. If
     * non-NULL, the transport implementation must populate it with valid values.
     */
    ssize_t (*recv)(struct UDSTp *hdl, uint8_t *buf, size_t bufsize, UDSSDU_t *info);

    /**
     * @brief Poll the transport layer.
     * @param hdl: pointer to transport handle
     * @note the transport layer user is responsible for calling this function periodically
     * @note threaded implementations like linux isotp sockets don't need to do anything here.
     * @return UDS_TP_IDLE if idle, otherwise UDS_TP_SEND_IN_PROGRESS or UDS_TP_RECV_COMPLETE
     */
    UDSTpStatus_t (*poll)(struct UDSTp *hdl);
} UDSTp_t;

ssize_t UDSTpSend(UDSTp_t *hdl, const uint8_t *buf, ssize_t len, UDSSDU_t *info);
ssize_t UDSTpRecv(UDSTp_t *hdl, uint8_t *buf, size_t bufsize, UDSSDU_t *info);
UDSTpStatus_t UDSTpPoll(UDSTp_t *hdl);

/**
 * @enum UDSEvent_t
 * @brief UDS events
 *
 * Events are passed to the server or client callback function along with
 * a pointer to the associated argument structure.
 */
typedef enum UDSEvent {
    UDS_EVT_Err, /**< Common event. Argument type: UDSErr_t * */

    UDS_EVT_DiagSessCtrl,         /**< Server evt 0x10, argtype: UDSDiagSessCtrlArgs_t * */
    UDS_EVT_EcuReset,             /**< Server evt 0x11, argtype: UDSECUResetArgs_t * */
    UDS_EVT_ClearDiagnosticInfo,  /**< Server evt 0x14, argtype: UDSCDIArgs_t * */
    UDS_EVT_ReadDTCInformation,   /**< Server evt 0x19, argtype: UDSRDTCIArgs_t * */
    UDS_EVT_ReadDataByIdent,      /**< Server evt 0x22, argtype: UDSRDBIArgs_t * */
    UDS_EVT_ReadMemByAddr,        /**< Server evt 0x23, argtype: UDSReadMemByAddrArgs_t * */
    UDS_EVT_CommCtrl,             /**< Server evt 0x28, argtype: UDSCommCtrlArgs_t * */
    UDS_EVT_SecAccessRequestSeed, /**< Server evt 0x27, argtype: UDSSecAccessRequestSeedArgs_t * */
    UDS_EVT_SecAccessValidateKey, /**< Server evt 0x27, argtype: UDSSecAccessValidateKeyArgs_t * */
    UDS_EVT_Auth,                 /**< Server evt 0x29, argtype: UDSAuthArgs_t * */
    UDS_EVT_AuthTimeout,          /**< Server evt 0x29, argtype: NULL */
    UDS_EVT_WriteDataByIdent,     /**< Server evt 0x2E, argtype: UDSWDBIArgs_t * */
    UDS_EVT_WriteMemByAddr,       /**< Server evt 0x3D, argtype: UDSWriteMemByAddrArgs_t * */
    UDS_EVT_DynamicDefineDataId,  /**< Server evt 0x2C, argtype: UDSDDDIArgs_t * */
    UDS_EVT_IOControl,            /**< Server evt 0x2F, argtype: UDSIOCtrlArgs_t * */
    UDS_EVT_RoutineCtrl,          /**< Server evt 0x31, argtype: UDSRoutineCtrlArgs_t * */
    UDS_EVT_RequestDownload,      /**< Server evt 0x34, argtype: UDSRequestDownloadArgs_t * */
    UDS_EVT_RequestUpload,        /**< Server evt 0x35, argtype: UDSRequestUploadArgs_t * */
    UDS_EVT_TransferData,         /**< Server evt 0x36, argtype: UDSTransferDataArgs_t * */
    UDS_EVT_RequestTransferExit,  /**< Server evt 0x37, argtype: UDSRequestTransferExitArgs_t * */
    UDS_EVT_SessionTimeout,       /**< Server evt 0x38, argtype: NULL */
    UDS_EVT_DoScheduledReset,     /**< Server evt 0x39, argtype: uint8_t * */
    UDS_EVT_RequestFileTransfer,  /**< Server evt 0x38, argtype: UDSRequestFileTransferArgs_t * */
    UDS_EVT_ControlDTCSetting,    /**< Server evt 0x85, argtype: UDSControlDTCSettingArgs_t * */
    UDS_EVT_LinkControl,          /**< Server evt 0x87, argtype: UDSLinkCtrlArgs_t * */
    UDS_EVT_Custom,               /**< Server evt other, argtype: UDSCustomArgs_t * */

    UDS_EVT_Poll,             /**< Client evt: Poll. Argument type: NULL */
    UDS_EVT_SendComplete,     /**< Client evt: Send complete. Argument type: NULL */
    UDS_EVT_ResponseReceived, /**< Client evt: Response received. Argument type: NULL */
    UDS_EVT_Idle,             /**< Client evt: Idle. Argument type: NULL */

    UDS_EVT_MAX, /**< Unused sentinel value */
} UDSEvent_t;

    typedef enum {
        UDS_FAIL = -1, // General error
        UDS_OK = 0,    // Success

        // Negative Response Codes (NRCs) as defined in ISO14229-1:2020 Table A.1 - Negative
        // Response Code (NRC) definition and values
        UDS_PositiveResponse = 0,
        // 0x01 to 0x0F are reserved by ISO14229-1:2020
        UDS_NRC_GeneralReject = 0x10,
        UDS_NRC_ServiceNotSupported = 0x11,
        UDS_NRC_SubFunctionNotSupported = 0x12,
        UDS_NRC_IncorrectMessageLengthOrInvalidFormat = 0x13,
        UDS_NRC_ResponseTooLong = 0x14,
        // 0x15 to 0x20 are reserved by ISO14229-1:2020
        UDS_NRC_BusyRepeatRequest = 0x21,
        UDS_NRC_ConditionsNotCorrect = 0x22,
        UDS_NRC_RequestSequenceError = 0x24,
        UDS_NRC_NoResponseFromSubnetComponent = 0x25,
        UDS_NRC_FailurePreventsExecutionOfRequestedAction = 0x26,
        // 0x27 to 0x30 are reserved by ISO14229-1:2020
        UDS_NRC_RequestOutOfRange = 0x31,
        // 0x32 is reserved by ISO14229-1:2020
        UDS_NRC_SecurityAccessDenied = 0x33,
        UDS_NRC_AuthenticationRequired = 0x34,
        UDS_NRC_InvalidKey = 0x35,
        UDS_NRC_ExceedNumberOfAttempts = 0x36,
        UDS_NRC_RequiredTimeDelayNotExpired = 0x37,
        UDS_NRC_SecureDataTransmissionRequired = 0x38,
        UDS_NRC_SecureDataTransmissionNotAllowed = 0x39,
        UDS_NRC_SecureDataVerificationFailed = 0x3A,
        // 0x3B to 0x4F are reserved by ISO14229-1:2020
        UDS_NRC_CertficateVerificationFailedInvalidTimePeriod = 0x50,
        UDS_NRC_CertficateVerificationFailedInvalidSignature = 0x51,
        UDS_NRC_CertficateVerificationFailedInvalidChainOfTrust = 0x52,
        UDS_NRC_CertficateVerificationFailedInvalidType = 0x53,
        UDS_NRC_CertficateVerificationFailedInvalidFormat = 0x54,
        UDS_NRC_CertficateVerificationFailedInvalidContent = 0x55,
        UDS_NRC_CertficateVerificationFailedInvalidScope = 0x56,
        UDS_NRC_CertficateVerificationFailedInvalidCertificate = 0x57,
        UDS_NRC_OwnershipVerificationFailed = 0x58,
        UDS_NRC_ChallengeCalculationFailed = 0x59,
        UDS_NRC_SettingAccessRightsFailed = 0x5A,
        UDS_NRC_SessionKeyCreationOrDerivationFailed = 0x5B,
        UDS_NRC_ConfigurationDataUsageFailed = 0x5C,
        UDS_NRC_DeAuthenticationFailed = 0x5D,
        // 0x5E to 0x6F are reserved by ISO14229-1:2020
        UDS_NRC_UploadDownloadNotAccepted = 0x70,
        UDS_NRC_TransferDataSuspended = 0x71,
        UDS_NRC_GeneralProgrammingFailure = 0x72,
        UDS_NRC_WrongBlockSequenceCounter = 0x73,
        // 0x74 to 0x77 are reserved by ISO14229-1:2020
        UDS_NRC_RequestCorrectlyReceived_ResponsePending = 0x78,
        // 0x79 to 0x7D are reserved by ISO14229-1:2020
        UDS_NRC_SubFunctionNotSupportedInActiveSession = 0x7E,
        UDS_NRC_ServiceNotSupportedInActiveSession = 0x7F,
        // 0x80 is reserved by ISO14229-1:2020
        UDS_NRC_RpmTooHigh = 0x81,
        UDS_NRC_RpmTooLow = 0x82,
        UDS_NRC_EngineIsRunning = 0x83,
        UDS_NRC_EngineIsNotRunning = 0x84,
        UDS_NRC_EngineRunTimeTooLow = 0x85,
        UDS_NRC_TemperatureTooHigh = 0x86,
        UDS_NRC_TemperatureTooLow = 0x87,
        UDS_NRC_VehicleSpeedTooHigh = 0x88,
        UDS_NRC_VehicleSpeedTooLow = 0x89,
        UDS_NRC_ThrottlePedalTooHigh = 0x8A,
        UDS_NRC_ThrottlePedalTooLow = 0x8B,
        UDS_NRC_TransmissionRangeNotInNeutral = 0x8C,
        UDS_NRC_TransmissionRangeNotInGear = 0x8D,
        // 0x8E is reserved by ISO14229-1:2020
        UDS_NRC_BrakeSwitchNotClosed = 0x8F,
        UDS_NRC_ShifterLeverNotInPark = 0x90,
        UDS_NRC_TorqueConverterClutchLocked = 0x91,
        UDS_NRC_VoltageTooHigh = 0x92,
        UDS_NRC_VoltageTooLow = 0x93,
        UDS_NRC_ResourceTemporarilyNotAvailable = 0x94,

        /* 0x95 to 0xEF are reservedForSpecificConditionsNotCorrect */
        /* 0xF0 to 0xFE are vehicleManufacturerSpecificConditionsNotCorrect */
        /* 0xFF is ISOSAEReserved */

        // The following values are not defined in ISO14229-1:2020
        UDS_ERR_TIMEOUT = 0x100,      // A request has timed out
        UDS_ERR_DID_MISMATCH,         // The response DID does not match the request DID
        UDS_ERR_SID_MISMATCH,         // The response SID does not match the request SID
        UDS_ERR_SUBFUNCTION_MISMATCH, // The response SubFunction does not match the request
                                      // SubFunction
        UDS_ERR_TPORT,          // Transport error. Check the transport layer for more information
        UDS_ERR_RESP_TOO_SHORT, // The response is too short
        UDS_ERR_BUFSIZ,         // The buffer is not large enough
        UDS_ERR_INVALID_ARG,    // The function has been called with invalid arguments
        UDS_ERR_BUSY,           // The client is busy and cannot process the request
        UDS_ERR_MISUSE,         // The library is used incorrectly
    } UDSErr_t;

// ISO14229-1:2020 Table 25
// UDS Level Diagnostic Session
#define UDS_LEV_DS_DS 1    // Default Session
#define UDS_LEV_DS_PRGS 2  // Programming Session
#define UDS_LEV_DS_EXTDS 3 // Extended Diagnostic Session
#define UDS_LEV_DS_SSDS 4  // Safety System Diagnostic Session

/**
 * @brief 0x11 ECU Reset SubFunction = [resetType]
 * ISO14229-1:2020 Table 34
 * UDS Level Reset Type
 */
#define UDS_LEV_RT_HR 1      // Hard Reset
#define UDS_LEV_RT_KOFFONR 2 // Key Off On Reset
#define UDS_LEV_RT_SR 3      // Soft Reset
#define UDS_LEV_RT_ERPSD 4   // Enable Rapid Power Shut Down
#define UDS_LEV_RT_DRPSD 5   // Disable Rapid Power Shut Down

/**
 * @brief 0x28 Communication Control SubFunction = [controlType]
 * ISO14229-1:2020 Table 54
 * UDS Level Control Type
 */
#define UDS_LEV_CTRLTP_ERXTX 0  // EnableRxAndTx
#define UDS_LEV_CTRLTP_ERXDTX 1 // EnableRxAndDisableTx
#define UDS_LEV_CTRLTP_DRXETX 2 // DisableRxAndEnableTx
#define UDS_LEV_CTRLTP_DRXTX 3  // DisableRxAndTx

/**
 * @brief 0x28 Communication Control communicationType
 * ISO14229-1:2020 Table B.1
 */
#define UDS_CTP_NCM 1       // NormalCommunicationMessages
#define UDS_CTP_NWMCM 2     // NetworkManagementCommunicationMessages
#define UDS_CTP_NWMCM_NCM 3 // NetworkManagementCommunicationMessagesAndNormalCommunicationMessages

/**
 * @brief 0x29 Authentication SubFunction
 * ISO14229-1:2020 Table 74
 */
#define UDS_LEV_AT_DA 0     // deAuthenticate
#define UDS_LEV_AT_VCU 1    // verifyCertificateUnidirectional
#define UDS_LEV_AT_VCB 2    // verifyCertificateBidirectional
#define UDS_LEV_AT_POWN 3   // proofOfOwnership
#define UDS_LEV_AT_TC 4     // transmitCertificate
#define UDS_LEV_AT_RCFA 5   // requestChallengeForAuthentication
#define UDS_LEV_AT_VPOWNU 6 //  verifyProofOfOwnershipUnidirectional
#define UDS_LEV_AT_VPOWNB 7 //  verifyProofOfOwnershipBidirectional
#define UDS_LEV_AT_AC 8     //  authenticationConfiguration

/**
 * @brief 0x29 Authentication - Authentication Return Parameter
 * ISO14229-1:2020 Table B.5
 */
#define UDS_AT_RA 0x00      // RequestAccepted
#define UDS_AT_GR 0x01      // GeneralReject
#define UDS_AT_ACAPCE 0x02  // authenticationConfiguration APCE
#define UDS_AT_ACACRAC 0x03 // AuthenticationConfiguration ACR with asymmetric cryptography
#define UDS_AT_ACACRSC 0x04 // AuthenticationConfiguration ACR with symmetric cryptography
#define UDS_AT_DAS 0x10     // DeAuthentication successful
#define UDS_AT_CVOVN 0x11   // CertificateVerified, OwnershipVerificationNecessary
#define UDS_AT_OVAC 0x12    // OwnershipVerified, AuthenticationComplete
#define UDS_AT_CV 0x13      // CertificateVerified

/**
 * @brief 0x31 RoutineControl SubFunction = [routineControlType]
 * ISO14229-1:2020 Table 426
 */
#define UDS_LEV_RCTP_STR 1  // StartRoutine
#define UDS_LEV_RCTP_STPR 2 // StopRoutine
#define UDS_LEV_RCTP_RRR 3  // RequestRoutineResults

/**
 * @brief modeOfOperation parameter used in 0x38 RequestFileTransfer
 * ISO14229-1:2020 Table G.1
 */
#define UDS_MOOP_ADDFILE 1  // AddFile
#define UDS_MOOP_DELFILE 2  // DeleteFile
#define UDS_MOOP_REPLFILE 3 // ReplaceFile
#define UDS_MOOP_RDFILE 4   // ReadFile
#define UDS_MOOP_RDDIR 5    // ReadDirectory
#define UDS_MOOP_RSFILE 6   // ResumeFile

/**
 * @brief 0x85 ControlDTCSetting SubFunction = [dtcSettingType]
 * ISO14229-1:2020 Table 128
 */
#define UDS_LEV_DTCSTP_ON 1
#define UDS_LEV_DTCSTP_OFF 2

/**
 * @brief 0x87 LinkControl SubFunction = [linkControlType]
 * ISO14229-1:2020 Table 171
 */
#define UDS_LEV_LCTP_VMTWFP 1 // VerifyModeTransitionWithFixedParameter
#define UDS_LEV_LCTP_VMTWSP 2 // VerifyModeTransitionWithSpecificParameter
#define UDS_LEV_LCTP_TM 3     // TransitionMode

// ISO-14229-1:2013 Table 2
#define UDS_MAX_DIAGNOSTIC_SERVICES 0x7F

#define UDS_RESPONSE_SID_OF(request_sid) ((request_sid) + 0x40)
#define UDS_REQUEST_SID_OF(response_sid) ((response_sid) - 0x40)

#define UDS_NEG_RESP_LEN 3U
#define UDS_0X10_REQ_LEN 2U
#define UDS_0X10_RESP_LEN 6U
#define UDS_0X11_REQ_MIN_LEN 2U
#define UDS_0X11_RESP_BASE_LEN 2U
#define UDS_0X14_REQ_MIN_LEN 4U
#define UDS_0X14_RESP_BASE_LEN 1U
#define UDS_0X19_REQ_MIN_LEN 2U
#define UDS_0X19_RESP_BASE_LEN 2U
#define UDS_0X23_REQ_MIN_LEN 4U
#define UDS_0X23_RESP_BASE_LEN 1U
#define UDS_0X22_RESP_BASE_LEN 1U
#define UDS_0X27_REQ_BASE_LEN 2U
#define UDS_0X27_RESP_BASE_LEN 2U
#define UDS_0X28_REQ_BASE_LEN 3U
#define UDS_0X28_RESP_LEN 2U
#define UDS_0X29_REQ_MIN_LEN 2U
#define UDS_0X29_RESP_BASE_LEN 3U
#define UDS_0X2C_REQ_MIN_LEN 2U
#define UDS_0X2C_RESP_BASE_LEN 2U
#define UDS_0X2E_REQ_BASE_LEN 3U
#define UDS_0X2E_REQ_MIN_LEN 4U
#define UDS_0X2E_RESP_LEN 3U
#define UDS_0X2F_REQ_MIN_LEN 4U
#define UDS_0X2F_RESP_BASE_LEN 4U
#define UDS_0X31_REQ_MIN_LEN 4U
#define UDS_0X31_RESP_MIN_LEN 4U
#define UDS_0X34_REQ_BASE_LEN 3U
#define UDS_0X34_RESP_BASE_LEN 2U
#define UDS_0X35_REQ_BASE_LEN 3U
#define UDS_0X35_RESP_BASE_LEN 2U
#define UDS_0X36_REQ_BASE_LEN 2U
#define UDS_0X36_RESP_BASE_LEN 2U
#define UDS_0X37_REQ_BASE_LEN 1U
#define UDS_0X37_RESP_BASE_LEN 1U
#define UDS_0X38_REQ_BASE_LEN 9U
#define UDS_0X38_RESP_BASE_LEN 3U
#define UDS_0X3D_REQ_MIN_LEN 5U
#define UDS_0X3D_RESP_BASE_LEN 2U
#define UDS_0X3E_REQ_MIN_LEN 2U
#define UDS_0X3E_REQ_MAX_LEN 2U
#define UDS_0X3E_RESP_LEN 2U
#define UDS_0X85_REQ_BASE_LEN 2U
#define UDS_0X85_RESP_LEN 2U
#define UDS_0X87_REQ_BASE_LEN 2U
#define UDS_0X87_RESP_LEN 2U

enum UDSDiagnosticServiceId {
    kSID_DIAGNOSTIC_SESSION_CONTROL = 0x10,
    kSID_ECU_RESET = 0x11,
    kSID_CLEAR_DIAGNOSTIC_INFORMATION = 0x14,
    kSID_READ_DTC_INFORMATION = 0x19,
    kSID_READ_DATA_BY_IDENTIFIER = 0x22,
    kSID_READ_MEMORY_BY_ADDRESS = 0x23,
    kSID_READ_SCALING_DATA_BY_IDENTIFIER = 0x24,
    kSID_SECURITY_ACCESS = 0x27,
    kSID_COMMUNICATION_CONTROL = 0x28,
    kSID_AUTHENTICATION = 0x29,
    kSID_READ_PERIODIC_DATA_BY_IDENTIFIER = 0x2A,
    kSID_DYNAMICALLY_DEFINE_DATA_IDENTIFIER = 0x2C,
    kSID_WRITE_DATA_BY_IDENTIFIER = 0x2E,
    kSID_IO_CONTROL_BY_IDENTIFIER = 0x2F,
    kSID_ROUTINE_CONTROL = 0x31,
    kSID_REQUEST_DOWNLOAD = 0x34,
    kSID_REQUEST_UPLOAD = 0x35,
    kSID_TRANSFER_DATA = 0x36,
    kSID_REQUEST_TRANSFER_EXIT = 0x37,
    kSID_REQUEST_FILE_TRANSFER = 0x38,
    kSID_WRITE_MEMORY_BY_ADDRESS = 0x3D,
    kSID_TESTER_PRESENT = 0x3E,
    kSID_ACCESS_TIMING_PARAMETER = 0x83,
    kSID_SECURED_DATA_TRANSMISSION = 0x84,
    kSID_CONTROL_DTC_SETTING = 0x85,
    kSID_RESPONSE_ON_EVENT = 0x86,
    kSID_LINK_CONTROL = 0x87,
};

#ifndef UDS_ASSERT
#include <assert.h>
#define UDS_ASSERT(x) assert(x)
#endif

/* returns true if `a` is after `b` */
static inline bool UDSTimeAfter(uint32_t a, uint32_t b) { return (int32_t)(a - b) > 0; }

/**
 * @brief Get time in milliseconds
 * @return current time in milliseconds
 */
uint32_t UDSMillis(void);

bool UDSSecurityAccessLevelIsReserved(uint8_t securityLevel);
bool UDSErrIsNRC(UDSErr_t err);

const char *UDSErrToStr(UDSErr_t err);
const char *UDSEventToStr(UDSEvent_t evt);

/**
 * @brief logging for bring-up and unit tests.
 * This interface was copied from ESP-IDF.
 */

#define UDS_LOG_NONE 0    // No log output
#define UDS_LOG_ERROR 1   // Log errors only
#define UDS_LOG_WARN 2    // Log warnings and errors
#define UDS_LOG_INFO 3    // Log info, warnings, and errors
#define UDS_LOG_DEBUG 4   // Log debug, info, warnings, and errors
#define UDS_LOG_VERBOSE 5 // Log verbose, debug, info, warnings, and errors

typedef int UDS_LogLevel_t;

#ifndef UDS_LOG_LEVEL
#define UDS_LOG_LEVEL UDS_LOG_NONE
#endif

#if UDS_CONFIG_LOG_COLORS
#define UDS_LOG_COLOR_BLACK "30"
#define UDS_LOG_COLOR_RED "31"
#define UDS_LOG_COLOR_GREEN "32"
#define UDS_LOG_COLOR_BROWN "33"
#define UDS_LOG_COLOR_BLUE "34"
#define UDS_LOG_COLOR_PURPLE "35"
#define UDS_LOG_COLOR_CYAN "36"
#define LOG_COLOR(COLOR) "\033[0;" COLOR "m"
#define LOG_BOLD(COLOR) "\033[1;" COLOR "m"
#define UDS_LOG_RESET_COLOR "\033[0m"
#define UDS_LOG_COLOR_E LOG_COLOR(UDS_LOG_COLOR_RED)
#define UDS_LOG_COLOR_W LOG_COLOR(UDS_LOG_COLOR_BROWN)
#define UDS_LOG_COLOR_I LOG_COLOR(UDS_LOG_COLOR_GREEN)
#define UDS_LOG_COLOR_D
#define UDS_LOG_COLOR_V
#else // UDS_CONFIG_LOG_COLORS
#define UDS_LOG_COLOR_E
#define UDS_LOG_COLOR_W
#define UDS_LOG_COLOR_I
#define UDS_LOG_COLOR_D
#define UDS_LOG_COLOR_V
#define UDS_LOG_RESET_COLOR
#endif // UDS_CONFIG_LOG_COLORS

#define UDS_LOG_FORMAT(letter, format)                                                             \
    UDS_LOG_COLOR_##letter #letter " (%" PRIu32 ") %s: " format UDS_LOG_RESET_COLOR "\n"

#if (UDS_LOG_LEVEL >= UDS_LOG_ERROR) && (UDS_LOG_LEVEL > UDS_LOG_NONE)
#define UDS_LOGE(tag, format, ...)                                                                 \
    UDS_LogWrite(UDS_LOG_ERROR, tag, UDS_LOG_FORMAT(E, format), UDSMillis(), tag, ##__VA_ARGS__)
#else
#define UDS_LOGE(tag, format, ...) UDS_LogDummy(tag, format, ##__VA_ARGS__)
#endif

#if UDS_LOG_LEVEL >= UDS_LOG_WARN && UDS_LOG_LEVEL > UDS_LOG_NONE
#define UDS_LOGW(tag, format, ...)                                                                 \
    UDS_LogWrite(UDS_LOG_WARN, tag, UDS_LOG_FORMAT(W, format), UDSMillis(), tag, ##__VA_ARGS__)
#else
#define UDS_LOGW(tag, format, ...) UDS_LogDummy(tag, format, ##__VA_ARGS__)
#endif

#if UDS_LOG_LEVEL >= UDS_LOG_INFO && UDS_LOG_LEVEL > UDS_LOG_NONE
#define UDS_LOGI(tag, format, ...)                                                                 \
    UDS_LogWrite(UDS_LOG_INFO, tag, UDS_LOG_FORMAT(I, format), UDSMillis(), tag, ##__VA_ARGS__)
#else
#define UDS_LOGI(tag, format, ...) UDS_LogDummy(tag, format, ##__VA_ARGS__)
#endif

#if UDS_LOG_LEVEL >= UDS_LOG_DEBUG && UDS_LOG_LEVEL > UDS_LOG_NONE
#define UDS_LOGD(tag, format, ...)                                                                 \
    UDS_LogWrite(UDS_LOG_DEBUG, tag, UDS_LOG_FORMAT(D, format), UDSMillis(), tag, ##__VA_ARGS__)
#else
#define UDS_LOGD(tag, format, ...) UDS_LogDummy(tag, format, ##__VA_ARGS__)
#endif

#if UDS_LOG_LEVEL >= UDS_LOG_VERBOSE && UDS_LOG_LEVEL > UDS_LOG_NONE
#define UDS_LOGV(tag, format, ...)                                                                 \
    UDS_LogWrite(UDS_LOG_VERBOSE, tag, UDS_LOG_FORMAT(V, format), UDSMillis(), tag, ##__VA_ARGS__)
#else
#define UDS_LOGV(tag, format, ...) UDS_LogDummy(tag, format, ##__VA_ARGS__)
#endif

#if UDS_LOG_LEVEL >= UDS_LOG_DEBUG && UDS_LOG_LEVEL > UDS_LOG_NONE
#define UDS_LOG_SDU(tag, buffer, buff_len, info)                                                   \
    UDS_LogSDUInternal(UDS_LOG_DEBUG, tag, buffer, buff_len, info)
#else
#define UDS_LOG_SDU(tag, buffer, buff_len, info) UDS_LogSDUDummy(tag, buffer, buff_len, info)
#endif

#if defined(__GNUC__) || defined(__clang__)
#define UDS_PRINTF_FORMAT(fmt_index, first_arg)                                                    \
    __attribute__((format(printf, fmt_index, first_arg)))
#else
#define UDS_PRINTF_FORMAT(fmt_index, first_arg)
#endif

#if UDS_LOG_LEVEL > UDS_LOG_NONE
void UDS_LogWrite(UDS_LogLevel_t level, const char *tag, const char *format, ...)
    UDS_PRINTF_FORMAT(3, 4);
void UDS_LogSDUInternal(UDS_LogLevel_t level, const char *tag, const uint8_t *buffer,
                        size_t buff_len, UDSSDU_t *info);
#endif

// Dummy function that consumes arguments but does nothing
static inline void UDS_LogDummy(const char *tag, const char *format, ...) {
    (void)tag;
    (void)format;
}
static inline void UDS_LogSDUDummy(const char *tag, const uint8_t *buffer, size_t buff_len,
                                   void *info) {
    (void)tag;
    (void)buffer;
    (void)buff_len;
    (void)info;
}

#define UDS_SUPPRESS_POS_RESP 0x1  // set the suppress positive response bit
#define UDS_FUNCTIONAL 0x2         // send the request as a functional request
#define UDS_IGNORE_SRV_TIMINGS 0x8 // ignore the server-provided p2 and p2_star

/**
 * @brief UDS client structure
 */
typedef struct UDSClient {
    uint16_t p2_ms;      /**< p2 timeout in milliseconds */
    uint32_t p2_star_ms; /**< p2* timeout in milliseconds (for 0x78 response) */
    UDSTp_t *tp;         /**< transport layer handle */

    uint32_t p2_timer; /**< p2 timer value */
    uint8_t state;     /**< client request state */

    uint8_t options;        /**< current request options */
    uint8_t defaultOptions; /**< default options for all requests */
    uint8_t _options_copy;  /**< copy of options at the time a request is made */

    int (*fn)(struct UDSClient *client, UDSEvent_t evt, void *ev_data); /**< callback function */
    void *fn_data; /**< user-specified function data */

    uint16_t recv_size;                         /**< size of received data */
    uint16_t send_size;                         /**< size of data to send */
    uint8_t recv_buf[UDS_CLIENT_RECV_BUF_SIZE]; /**< receive buffer */
    uint8_t send_buf[UDS_CLIENT_SEND_BUF_SIZE]; /**< send buffer */
} UDSClient_t;

/**
 * @brief Security access response structure
 */
struct SecurityAccessResponse {
    uint8_t securityAccessType;  /**< security access type (subfunction) */
    const uint8_t *securitySeed; /**< pointer to security seed data */
    uint16_t securitySeedLength; /**< length of security seed */
};

/**
 * @brief Request download response structure
 */
struct RequestDownloadResponse {
    size_t maxNumberOfBlockLength; /**< maximum number of block length */
};

/**
 * @brief Routine control response structure
 */
struct RoutineControlResponse {
    uint8_t routineControlType;         /**< routine control type (subfunction) */
    uint16_t routineIdentifier;         /**< routine identifier */
    const uint8_t *routineStatusRecord; /**< pointer to routine status record */
    uint16_t routineStatusRecordLength; /**< length of routine status record */
};

/**
 * @brief Read data by identifier variable structure
 */
typedef struct {
    uint16_t did;                                            /**< data identifier */
    uint16_t len;                                            /**< data length */
    void *data;                                              /**< pointer to data buffer */
    void *(*UnpackFn)(void *dst, const void *src, size_t n); /**< optional unpack function */
} UDSRDBIVar_t;

UDSErr_t UDSClientInit(UDSClient_t *client);
UDSErr_t UDSClientPoll(UDSClient_t *client);
UDSErr_t UDSSendBytes(UDSClient_t *client, const uint8_t *data, uint16_t size);
UDSErr_t UDSSendECUReset(UDSClient_t *client, uint8_t type);
UDSErr_t UDSSendDiagSessCtrl(UDSClient_t *client, uint8_t mode);
UDSErr_t UDSSendSecurityAccess(UDSClient_t *client, uint8_t level, uint8_t *data, uint16_t size);
UDSErr_t UDSSendCommCtrl(UDSClient_t *client, uint8_t ctrl, uint8_t comm);
UDSErr_t UDSSendRDBI(UDSClient_t *client, const uint16_t *didList,
                     const uint16_t numDataIdentifiers);
UDSErr_t UDSSendWDBI(UDSClient_t *client, uint16_t dataIdentifier, const uint8_t *data,
                     uint16_t size);
UDSErr_t UDSSendTesterPresent(UDSClient_t *client);
UDSErr_t UDSSendRoutineCtrl(UDSClient_t *client, uint8_t type, uint16_t routineIdentifier,
                            const uint8_t *data, uint16_t size);

UDSErr_t UDSSendRequestDownload(UDSClient_t *client, uint8_t dataFormatIdentifier,
                                uint8_t addressAndLengthFormatIdentifier, size_t memoryAddress,
                                size_t memorySize);

UDSErr_t UDSSendRequestUpload(UDSClient_t *client, uint8_t dataFormatIdentifier,
                              uint8_t addressAndLengthFormatIdentifier, size_t memoryAddress,
                              size_t memorySize);
UDSErr_t UDSSendTransferData(UDSClient_t *client, uint8_t blockSequenceCounter,
                             const uint16_t blockLength, const uint8_t *data, uint16_t size);
UDSErr_t UDSSendTransferDataStream(UDSClient_t *client, uint8_t blockSequenceCounter,
                                   const uint16_t blockLength, FILE *fd);
UDSErr_t UDSSendRequestTransferExit(UDSClient_t *client);

UDSErr_t UDSSendRequestFileTransfer(UDSClient_t *client, uint8_t mode, const char *filePath,
                                    uint8_t dataFormatIdentifier, uint8_t fileSizeParameterLength,
                                    size_t fileSizeUncompressed, size_t fileSizeCompressed);

UDSErr_t UDSCtrlDTCSetting(UDSClient_t *client, uint8_t dtcSettingType,
                           uint8_t *dtcSettingControlOptionRecord, uint16_t len);
UDSErr_t UDSUnpackRDBIResponse(UDSClient_t *client, UDSRDBIVar_t *vars, uint16_t numVars);
UDSErr_t UDSUnpackSecurityAccessResponse(const UDSClient_t *client,
                                         struct SecurityAccessResponse *resp);
UDSErr_t UDSUnpackRequestDownloadResponse(const UDSClient_t *client,
                                          struct RequestDownloadResponse *resp);
UDSErr_t UDSUnpackRoutineControlResponse(const UDSClient_t *client,
                                         struct RoutineControlResponse *resp);

UDSErr_t UDSConfigDownload(UDSClient_t *client, uint8_t dataFormatIdentifier,
                           uint8_t addressAndLengthFormatIdentifier, size_t memoryAddress,
                           size_t memorySize, FILE *fd);

/**
 * @brief Server request context
 */
typedef struct {
    uint8_t recv_buf[UDS_SERVER_RECV_BUF_SIZE]; /**< receive buffer */
    uint8_t send_buf[UDS_SERVER_SEND_BUF_SIZE]; /**< send buffer */
    size_t recv_len;                            /**< received data length */
    size_t send_len;                            /**< send data length */
    size_t send_buf_size;                       /**< send buffer size */
    UDSSDU_t info;                              /**< service data unit information */
} UDSReq_t;

/**
 * @brief UDS server structure
 */
typedef struct UDSServer {
    UDSTp_t *tp; /**< transport layer handle */
    UDSErr_t (*fn)(struct UDSServer *srv, UDSEvent_t event, void *arg); /**< callback function */
    void *fn_data; /**< user-specified function data */

    /**
     * @brief Server time constants (milliseconds)
     */
    uint16_t p2_ms;      /**< Default P2_server_max timing supported by the server */
    uint32_t p2_star_ms; /**< Enhanced (NRC 0x78) P2_server_max supported by the server */
    uint16_t s3_ms;      /**< Session timeout */

    uint8_t ecuResetScheduled;         /**< nonzero indicates that an ECUReset has been scheduled */
    uint32_t ecuResetTimer;            /**< for delaying resetting until a response has been sent */
    uint32_t p2_timer;                 /**< for rate limiting server responses */
    uint32_t s3_session_timeout_timer; /**< indicates that diagnostic session has timed out */
    uint32_t sec_access_auth_fail_timer;  /**< brute-force hardening: rate limit security access */
    uint32_t sec_access_boot_delay_timer; /**< brute-force hardening: restrict security access until
                                             timer expires */

    /**
     * @brief UDS-1-2013: Table 407 - 0x36 TransferData Supported negative
     * response codes requires that the server keep track of whether the
     * transfer is active
     */
    bool xferIsActive;                /**< transfer is active */
    uint8_t xferBlockSequenceCounter; /**< UDS-1-2013: 14.4.2.3, Table 404: block sequence counter
                                         starts at 0x01 */
    size_t xferTotalBytes;            /**< total transfer size in bytes requested by the client */
    size_t xferByteCounter;           /**< total number of bytes transferred */
    size_t xferBlockLength;           /**< block length (convenience for the TransferData API) */

    uint8_t sessionType;   /**< diagnostic session type (0x10) */
    uint8_t securityLevel; /**< SecurityAccess (0x27) level */

    bool RCRRP;             /**< set to true when user fn returns 0x78 and false otherwise */
    bool requestInProgress; /**< set to true when a request has been processed but the response has
                               not yet been sent */

    /**
     * @brief UDS-1 2013 defines the following conditions under which the server does not
     * process incoming requests:
     * - not ready to receive (Table A.1 0x78)
     * - not accepting request messages and not sending responses (9.3.1)
     *
     * when this variable is set to true, incoming ISO-TP data will not be processed.
     */
    bool notReadyToReceive; /**< incoming ISO-TP data will not be processed */

    UDSReq_t r; /**< request context */
} UDSServer_t;

/**
 * @brief Diagnostic session control arguments
 */
typedef struct {
    const uint8_t type;  /*! requested diagnostic session type */
    uint16_t p2_ms;      /*! optional: p2 timing override */
    uint32_t p2_star_ms; /*! optional: p2* timing override */
} UDSDiagSessCtrlArgs_t;

/**
 * @brief ECU reset arguments
 */
typedef struct {
    const uint8_t type;           /**< reset type requested by client */
    uint32_t powerDownTimeMillis; /**< when this much time has elapsed after a UDS_PositiveResponse,
                                     a UDS_EVT_DoScheduledReset will be issued */
} UDSECUResetArgs_t;

/**
 * @brief Clear diagnostic information arguments
 */
typedef struct {
    const uint32_t groupOfDTC;     /*! lower 3 bytes describe the groupOfDTC */
    const bool hasMemorySelection; /*! `true` when a memory selection byte is present */
    const uint8_t memorySelection; /*! memorySelection byte (optional) */
} UDSCDIArgs_t;

/**
 * @brief Read DTC information arguments
 */
typedef struct {
    const uint8_t type; /*! invoked subfunction */
    uint8_t (*copy)(UDSServer_t *srv, const void *src,
                    uint16_t count); /*! function for copying data */

    union {
        struct {
            uint8_t mask; /*! DTC status mask */
        } numOfDTCByStatusMaskArgs, dtcStatusByMaskArgs;
        struct {
            uint32_t dtc;        /*! DTC Mask Record */
            uint8_t snapshotNum; /*! DTC Snaphot Record Number */
            uint8_t memory;      /*! Memory Selection (only used when type == 0x18) */
        } dtcSnapshotRecordbyDTCNumArgs, userDefMemDTCSnapshotRecordByDTCNumArgs;
        struct {
            uint8_t recordNum; /*! DTC Data Record Number */
        } dtcStoredDataByRecordNumArgs, dtcExtDataRecordByRecordNumArgs, dtcExtDataRecordIdArgs;
        struct {
            uint32_t dtc;          /*! DTC Mask Record */
            uint8_t extDataRecNum; /*! DTC Extended Data Record Number */
            uint8_t memory;        /*! Memory Selection (only used when type == 0x19) */
        } dtcExtDtaRecordByDTCNumArgs, userDefMemDTCExtDataRecordByDTCNumArgs;
        struct {
            uint8_t
                functionalGroup;  /*! Functional Group Identifier (only used when type == 0x42) */
            uint8_t severityMask; /*! DTC Severity Mask */
            uint8_t statusMask;   /*! DTC Status Mask */
        } numOfDTCBySeverityMaskArgs, dtcBySeverityMaskArgs, wwhobdDTCByMaskArgs;
        struct {
            uint32_t dtc; /*! DTC Mask Record */
        } severityInfoOfDTCArgs;
        struct {
            uint8_t mask;   /*! DTC status mask */
            uint8_t memory; /*! Memory Selection */
        } userDefMemoryDTCByStatusMaskArgs;
        struct {
            uint8_t functionalGroup; /*! Functional Group Identifier */
            uint8_t
                readinessGroup; /*! DTC Readiness Group Identifier (only used when type == 0x56) */
        } wwhobdDTCWithPermStatusArgs, dtcInfoByDTCReadinessGroupIdArgs;
    } subFuncArgs;
} UDSRDTCIArgs_t;

/**
 * @brief Read data by identifier arguments
 */
typedef struct {
    const uint16_t dataId; /*! RDBI Data Identifier */
    uint8_t (*copy)(UDSServer_t *srv, const void *src,
                    uint16_t count); /*! function for copying data */
} UDSRDBIArgs_t;

/**
 * @brief Read memory by address arguments
 */
typedef struct {
    const void *memAddr;
    const size_t memSize;
    uint8_t (*copy)(UDSServer_t *srv, const void *src,
                    uint16_t count); /*! function for copying data */
} UDSReadMemByAddrArgs_t;

/**
 * @brief Communication control arguments
 */
typedef struct {
    uint8_t ctrlType; /*! ControlType */
    uint8_t commType; /*! CommunicationType */
    uint16_t nodeId;  /*! NodeIdentificationNumber (only used when ctrlType is 0x04 or 0x05) */
} UDSCommCtrlArgs_t;

/**
 * @brief Security access request seed arguments
 */
typedef struct {
    uint8_t type; /*! requested subfunction */

    uint8_t (*set_auth_state)(UDSServer_t *srv,
                              uint8_t state); /*! set the authentication state as lined out in
                                                 ISO14229-1:2020 Table B.5 */

    uint8_t (*copy)(UDSServer_t *srv, const void *src,
                    uint16_t count); /*! function for copying data */

    union {
        struct {
            uint8_t commConf;      /*! CommunicationConfiguration */
            uint16_t certLen;      /*! lengthOfCertificateClient */
            void *cert;            /*! pointer to certificateClient */
            uint16_t challengeLen; /*! lengthOfChallengeClient (may be 0 for unidirectional
                                      verification) */
            void *challenge;       /*! pointer to challengeClient  */
        } verifyCertArgs;          /*! Arguments for unidirectional or bidirectional verification */
        struct {
            uint16_t pownLen;      /*! lengthOfProofOfOwnership */
            void *pown;            /*! pointer to proofOfOwnership */
            uint16_t publicKeyLen; /*! lengthOfPublicKey (may be 0)*/
            void *publicKey;       /*! pointer to publicKey */
        } pownArgs;                /*! ProofOfOwnership*/
        struct {
            uint8_t evalId; /*! certificateEvaluationID */
            uint16_t len;   /*! lengthOfCertificateData */
            void *cert;     /*! pointer to certificateData */
        } transCertArgs;    /*! TransmitCertificate */
        struct {
            uint8_t commConf; /*! CommunicationConfiguration */
            void *algoInd;    /*! pointer to algorithmIndicator (always 16 bytes) */
        } reqChallengeArgs;   /*! RequestChallengeForAuthentication*/
        struct {
            void *algoInd;         /*! pointer to algorithmIndicator (always 16 bytes) */
            uint16_t pownLen;      /*! lengthOfProofOfOwnership */
            void *pown;            /*! pointer to proofOfOwnership */
            uint16_t challengeLen; /*! lengthOfChallengeClient (may be 0 when unidirectional) */
            void *challenge;       /*! pointer to challengeClient */
            uint16_t addParamLen;  /*! lengthOfAdditionalParameter (may be 0) */
            void *addParam;        /*! pointer to additionalParameter */
        } verifyPownArgs; /*! Arguments for unidirectional or bidirectional verification for
                                proof of ownership */
    } subFuncArgs;
} UDSAuthArgs_t;

typedef struct {
    const uint8_t level;             /*! requested security level */
    const uint8_t *const dataRecord; /*! pointer to request data */
    const uint16_t len;              /*! size of request data */
    uint8_t (*copySeed)(UDSServer_t *srv, const void *src,
                        uint16_t len); /*! function for copying data */
} UDSSecAccessRequestSeedArgs_t;

/**
 * @brief Security access validate key arguments
 */
typedef struct {
    const uint8_t level;      /*! security level to be validated */
    const uint8_t *const key; /*! key sent by client */
    const uint16_t len;       /*! length of key */
} UDSSecAccessValidateKeyArgs_t;

/**
 * @brief Write data by identifier arguments
 */
typedef struct {
    const uint16_t dataId;     /*! WDBI Data Identifier */
    const uint8_t *const data; /*! pointer to data */
    const uint16_t len;        /*! length of data */
} UDSWDBIArgs_t;

/**
 * @brief Write memory by address arguments
 */
typedef struct {
    const void *memAddr;       /*! pointer to memory address */
    const size_t memSize;      /*! size of memory */
    const uint8_t *const data; /*! pointer to data */
} UDSWriteMemByAddrArgs_t;

/**
 * @brief Dynamically define data identifier arguments
 */
typedef struct {
    const uint8_t type;     /*! invoked subfunction */
    bool allDataIds;        /*! is true when request is for all data identifiers (only relevant for
                              subFunc 0x03) */
    uint16_t dynamicDataId; /*! dynamicallyDefinedDataIdentifier */

    union {
        struct {
            uint16_t sourceDataId; /*! source DataIdentifier */
            uint8_t position;      /*! position in source data record */
            uint8_t size;          /*! number of bytes to be copied */
        } defineById;
        struct {
            void *memAddr;
            size_t memSize;
        } defineByMemAddress;
    } subFuncArgs;
} UDSDDDIArgs_t;

/**
 * @brief Input/output control by identifier arguments
 */
typedef struct {
    const uint16_t dataId;              /*! Data Identifier */
    const uint8_t ioCtrlParam;          /*! inputOutputControlParameter */
    const void *const ctrlStateAndMask; /*! controlState bytes and controlMask (optional) */
    const size_t ctrlStateAndMaskLen;   /*! number of bytes in `ctrlStateAndMask` */
    uint8_t (*copy)(UDSServer_t *srv, const void *src,
                    uint16_t count); /*! function for copying data */
} UDSIOCtrlArgs_t;

/**
 * @brief Routine control arguments
 */
typedef struct {
    const uint8_t ctrlType;      /*! routineControlType */
    const uint16_t id;           /*! routineIdentifier */
    const uint8_t *optionRecord; /*! optional data */
    const uint16_t len;          /*! length of optional data */
    uint8_t (*copyStatusRecord)(UDSServer_t *srv, const void *src,
                                uint16_t len); /*! function for copying response data */
} UDSRoutineCtrlArgs_t;

/**
 * @brief Request download arguments
 */
typedef struct {
    const void *addr;                   /*! requested address */
    const size_t size;                  /*! requested download size */
    const uint8_t dataFormatIdentifier; /*! optional specifier for format of data */
    uint16_t maxNumberOfBlockLength;    /*! optional response: inform client how many data bytes to
                                           send in each    `TransferData` request */
} UDSRequestDownloadArgs_t;

/**
 * @brief Request upload arguments
 */
typedef struct {
    const void *addr;                   /*! requested address */
    const size_t size;                  /*! requested download size */
    const uint8_t dataFormatIdentifier; /*! optional specifier for format of data */
    uint16_t maxNumberOfBlockLength;    /*! optional response: inform client how many data bytes to
                                           send in each    `TransferData` request */
} UDSRequestUploadArgs_t;

/**
 * @brief Transfer data arguments
 */
typedef struct {
    const uint8_t *const data; /*! transfer data */
    const uint16_t len;        /*! transfer data length */
    const uint16_t maxRespLen; /*! don't send more than this many bytes with copyResponse */
    uint8_t (*copyResponse)(
        UDSServer_t *srv, const void *src,
        uint16_t len); /*! function for copying transfer data response data (optional) */
} UDSTransferDataArgs_t;

/**
 * @brief Request transfer exit arguments
 */
typedef struct {
    const uint8_t *const data; /*! request data */
    const uint16_t len;        /*! request data length */
    uint8_t (*copyResponse)(UDSServer_t *srv, const void *src,
                            uint16_t len); /*! function for copying response data (optional) */
} UDSRequestTransferExitArgs_t;

/**
 * @brief Request file transfer arguments
 */
typedef struct {
    const uint8_t modeOfOperation;      /*! requested specifier for operation mode */
    const uint16_t filePathLen;         /*! request data length */
    const uint8_t *filePath;            /*! requested file path and name */
    const uint8_t dataFormatIdentifier; /*! optional specifier for format of data */
    const size_t fileSizeUnCompressed;  /*! optional file size */
    const size_t fileSizeCompressed;    /*! optional file size */
    uint16_t maxNumberOfBlockLength;    /*! optional response: inform client how many data bytes to
                                           send in each    `TransferData` request */
} UDSRequestFileTransferArgs_t;

/**
 * @brief Control DTC setting arguments
 */
typedef struct {
    uint8_t type; /*! invoked subfunction */
    size_t len;   /*! length of data */
    void *data;   /*! DTCSettingControlOptionRecord */
} UDSControlDTCSettingArgs_t;

/**
 * @brief Link control arguments
 */
typedef struct {
    const uint8_t type; /*! invoked subfunction */
    /* purposefully left generic to allow vehicle- and supplier specific data of different sizes */
    const size_t len; /*! length of data */
    const void *data; /*! data used in the subfunction. E.g. on SubFunction 0x01 this is the
                         linkControlModelIdentifier, on SubFunction 0x02 this is the linkRecord */
} UDSLinkCtrlArgs_t;

/**
 * @brief Custom service arguments
 */
typedef struct {
    const uint16_t sid;          /*! serviceIdentifier */
    const uint8_t *optionRecord; /*! optional data */
    const uint16_t len;          /*! length of optional data */
    uint8_t (*copyResponse)(UDSServer_t *srv, const void *src,
                            uint16_t len); /*! function for copying response data (optional) */
} UDSCustomArgs_t;

UDSErr_t UDSServerInit(UDSServer_t *srv);
void UDSServerPoll(UDSServer_t *srv);

#if defined(UDS_TP_ISOTP_C)
#define ISO_TP_USER_SEND_CAN_ARG 1
#ifndef ISOTPC_CONFIG_H
#define ISOTPC_CONFIG_H

/* Max number of messages the receiver can receive at one time, this value
 * is affected by can driver queue length
 */
#define ISO_TP_DEFAULT_BLOCK_SIZE 8

/* The STmin parameter value specifies the minimum time gap allowed between
 * the transmission of consecutive frame network protocol data units
 */
#define ISO_TP_DEFAULT_ST_MIN_US 0

/* This parameter indicate how many FC N_PDU WTs can be transmitted by the
 * receiver in a row.
 */
#define ISO_TP_MAX_WFT_NUMBER 1

/* Private: The default timeout to use when waiting for a response during a
 * multi-frame send or receive.
 */
#define ISO_TP_DEFAULT_RESPONSE_TIMEOUT_US 100000

/* Private: Determines if by default, padding is added to ISO-TP message frames.
 */
// #define ISO_TP_FRAME_PADDING

/* Private: Value to use when padding frames if enabled by ISO_TP_FRAME_PADDING
 */
#ifndef ISO_TP_FRAME_PADDING_VALUE
#define ISO_TP_FRAME_PADDING_VALUE 0xAA
#endif

/* Private: Determines if by default, an additional argument is present in the
 * definition of isotp_user_send_can.
 */
// #define ISO_TP_USER_SEND_CAN_ARG

#endif // ISOTPC_CONFIG_H

#ifndef ISOTPC_USER_DEFINITIONS_H
#define ISOTPC_USER_DEFINITIONS_H

#include <stdint.h>

/**************************************************************
 * compiler specific defines
 *************************************************************/
#ifdef __GNUC__
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define ISOTP_BYTE_ORDER_LITTLE_ENDIAN
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#else
#error "unsupported byte ordering"
#endif
#endif

/**************************************************************
 * OS specific defines
 *************************************************************/
#ifdef _WIN32
#define snprintf _snprintf
#endif

#ifdef _WIN32
#include <windows.h>
#define ISOTP_BYTE_ORDER_LITTLE_ENDIAN
#define __builtin_bswap8 _byteswap_uint8
#define __builtin_bswap16 _byteswap_uint16
#define __builtin_bswap32 _byteswap_uint32
#define __builtin_bswap64 _byteswap_uint64
#endif

/**************************************************************
 * internal used defines
 *************************************************************/
#define ISOTP_RET_OK 0
#define ISOTP_RET_ERROR -1
#define ISOTP_RET_INPROGRESS -2
#define ISOTP_RET_OVERFLOW -3
#define ISOTP_RET_WRONG_SN -4
#define ISOTP_RET_NO_DATA -5
#define ISOTP_RET_TIMEOUT -6
#define ISOTP_RET_LENGTH -7
#define ISOTP_RET_NOSPACE -8

/* return logic true if 'a' is after 'b' */
#define IsoTpTimeAfter(a, b) ((int32_t)((int32_t)(b) - (int32_t)(a)) < 0)

/*  invalid bs */
#define ISOTP_INVALID_BS 0xFFFF

/* ISOTP sender status */
typedef enum {
    ISOTP_SEND_STATUS_IDLE,
    ISOTP_SEND_STATUS_INPROGRESS,
    ISOTP_SEND_STATUS_ERROR,
} IsoTpSendStatusTypes;

/* ISOTP receiver status */
typedef enum {
    ISOTP_RECEIVE_STATUS_IDLE,
    ISOTP_RECEIVE_STATUS_INPROGRESS,
    ISOTP_RECEIVE_STATUS_FULL,
} IsoTpReceiveStatusTypes;

/* can fram defination */
#if defined(ISOTP_BYTE_ORDER_LITTLE_ENDIAN)
typedef struct {
    uint8_t reserve_1 : 4;
    uint8_t type : 4;
    uint8_t reserve_2[7];
} IsoTpPciType;

typedef struct {
    uint8_t SF_DL : 4;
    uint8_t type : 4;
    uint8_t data[7];
} IsoTpSingleFrame;

typedef struct {
    uint8_t FF_DL_high : 4;
    uint8_t type : 4;
    uint8_t FF_DL_low;
    uint8_t data[6];
} IsoTpFirstFrame;

typedef struct {
    uint8_t SN : 4;
    uint8_t type : 4;
    uint8_t data[7];
} IsoTpConsecutiveFrame;

typedef struct {
    uint8_t FS : 4;
    uint8_t type : 4;
    uint8_t BS;
    uint8_t STmin;
    uint8_t reserve[5];
} IsoTpFlowControl;

#else

typedef struct {
    uint8_t type : 4;
    uint8_t reserve_1 : 4;
    uint8_t reserve_2[7];
} IsoTpPciType;

/*
 * single frame
 * +-------------------------+-----+
 * | byte #0                 | ... |
 * +-------------------------+-----+
 * | nibble #0   | nibble #1 | ... |
 * +-------------+-----------+ ... +
 * | PCIType = 0 | SF_DL     | ... |
 * +-------------+-----------+-----+
 */
typedef struct {
    uint8_t type : 4;
    uint8_t SF_DL : 4;
    uint8_t data[7];
} IsoTpSingleFrame;

/*
 * first frame
 * +-------------------------+-----------------------+-----+
 * | byte #0                 | byte #1               | ... |
 * +-------------------------+-----------+-----------+-----+
 * | nibble #0   | nibble #1 | nibble #2 | nibble #3 | ... |
 * +-------------+-----------+-----------+-----------+-----+
 * | PCIType = 1 | FF_DL                             | ... |
 * +-------------+-----------+-----------------------+-----+
 */
typedef struct {
    uint8_t type : 4;
    uint8_t FF_DL_high : 4;
    uint8_t FF_DL_low;
    uint8_t data[6];
} IsoTpFirstFrame;

/*
 * consecutive frame
 * +-------------------------+-----+
 * | byte #0                 | ... |
 * +-------------------------+-----+
 * | nibble #0   | nibble #1 | ... |
 * +-------------+-----------+ ... +
 * | PCIType = 0 | SN        | ... |
 * +-------------+-----------+-----+
 */
typedef struct {
    uint8_t type : 4;
    uint8_t SN : 4;
    uint8_t data[7];
} IsoTpConsecutiveFrame;

/*
 * flow control frame
 * +-------------------------+-----------------------+-----------------------+-----+
 * | byte #0                 | byte #1               | byte #2               | ... |
 * +-------------------------+-----------+-----------+-----------+-----------+-----+
 * | nibble #0   | nibble #1 | nibble #2 | nibble #3 | nibble #4 | nibble #5 | ... |
 * +-------------+-----------+-----------+-----------+-----------+-----------+-----+
 * | PCIType = 1 | FS        | BS                    | STmin                 | ... |
 * +-------------+-----------+-----------------------+-----------------------+-----+
 */
typedef struct {
    uint8_t type : 4;
    uint8_t FS : 4;
    uint8_t BS;
    uint8_t STmin;
    uint8_t reserve[5];
} IsoTpFlowControl;

#endif

typedef struct {
    uint8_t ptr[8];
} IsoTpDataArray;

typedef struct {
    union {
        IsoTpPciType common;
        IsoTpSingleFrame single_frame;
        IsoTpFirstFrame first_frame;
        IsoTpConsecutiveFrame consecutive_frame;
        IsoTpFlowControl flow_control;
        IsoTpDataArray data_array;
    } as;
} IsoTpCanMessage;

/**************************************************************
 * protocol specific defines
 *************************************************************/

/* Private: Protocol Control Information (PCI) types, for identifying each frame of an ISO-TP
 * message.
 */
typedef enum {
    ISOTP_PCI_TYPE_SINGLE = 0x0,
    ISOTP_PCI_TYPE_FIRST_FRAME = 0x1,
    TSOTP_PCI_TYPE_CONSECUTIVE_FRAME = 0x2,
    ISOTP_PCI_TYPE_FLOW_CONTROL_FRAME = 0x3
} IsoTpProtocolControlInformation;

/* Private: Protocol Control Information (PCI) flow control identifiers.
 */
typedef enum {
    PCI_FLOW_STATUS_CONTINUE = 0x0,
    PCI_FLOW_STATUS_WAIT = 0x1,
    PCI_FLOW_STATUS_OVERFLOW = 0x2
} IsoTpFlowStatus;

/* Private: network layer resault code.
 */
#define ISOTP_PROTOCOL_RESULT_OK 0
#define ISOTP_PROTOCOL_RESULT_TIMEOUT_A -1
#define ISOTP_PROTOCOL_RESULT_TIMEOUT_BS -2
#define ISOTP_PROTOCOL_RESULT_TIMEOUT_CR -3
#define ISOTP_PROTOCOL_RESULT_WRONG_SN -4
#define ISOTP_PROTOCOL_RESULT_INVALID_FS -5
#define ISOTP_PROTOCOL_RESULT_UNEXP_PDU -6
#define ISOTP_PROTOCOL_RESULT_WFT_OVRN -7
#define ISOTP_PROTOCOL_RESULT_BUFFER_OVFLW -8
#define ISOTP_PROTOCOL_RESULT_ERROR -9

#endif // ISOTPC_USER_DEFINITIONS_H
#ifndef ISOTPC_USER_H
#define ISOTPC_USER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief user implemented, print debug message */
void isotp_user_debug(const char *message, ...);

/**
 * @brief user implemented, send can message. should return ISOTP_RET_OK when success.
 *
 * @return may return ISOTP_RET_NOSPACE if the CAN transfer should be retried later
 * or ISOTP_RET_ERROR if transmission couldn't be completed
 */
int isotp_user_send_can(const uint32_t arbitration_id, const uint8_t *data, const uint8_t size
#if ISO_TP_USER_SEND_CAN_ARG
                        ,
                        void *arg
#endif
);

/**
 * @brief user implemented, gets the amount of time passed since the last call in microseconds
 */
uint32_t isotp_user_get_us(void);

#ifdef __cplusplus
}
#endif

#endif // ISOTPC_USER_H

#ifndef ISOTPC_H
#define ISOTPC_H

#include <stdio.h>
#include <string.h>

#ifdef __cplusplus
#include <stdint.h>

extern "C" {
#endif

/**
 * @brief Struct containing the data for linking an application to a CAN instance.
 * The data stored in this struct is used internally and may be used by software programs
 * using this library.
 */
typedef struct IsoTpLink {
    /* sender paramters */
    uint32_t send_arbitration_id; /* used to reply consecutive frame */
    /* message buffer */
    uint8_t *send_buffer;
    uint16_t send_buf_size;
    uint16_t send_size;
    uint16_t send_offset;
    /* multi-frame flags */
    uint8_t send_sn;
    uint16_t send_bs_remain; /* Remaining block size */
    uint32_t send_st_min_us; /* Separation Time between consecutive frames */
    uint8_t send_wtf_count;  /* Maximum number of FC.Wait frame transmissions  */
    uint32_t send_timer_st;  /* Last time send consecutive frame */
    uint32_t send_timer_bs;  /* Time until reception of the next FlowControl N_PDU
                                start at sending FF, CF, receive FC
                                end at receive FC */
    int send_protocol_result;
    uint8_t send_status;
    /* receiver paramters */
    uint32_t receive_arbitration_id;
    /* message buffer */
    uint8_t *receive_buffer;
    uint16_t receive_buf_size;
    uint16_t receive_size;
    uint16_t receive_offset;
    /* multi-frame control */
    uint8_t receive_sn;
    uint8_t receive_bs_count;  /* Maximum number of FC.Wait frame transmissions  */
    uint32_t receive_timer_cr; /* Time until transmission of the next ConsecutiveFrame N_PDU
                                  start at sending FC, receive CF
                                  end at receive FC */
    int receive_protocol_result;
    uint8_t receive_status;

#if defined(ISO_TP_USER_SEND_CAN_ARG)
    void *user_send_can_arg;
#endif
} IsoTpLink;

/**
 * @brief Initialises the ISO-TP library.
 *
 * @param link The @code IsoTpLink @endcode instance used for transceiving data.
 * @param sendid The ID used to send data to other CAN nodes.
 * @param sendbuf A pointer to an area in memory which can be used as a buffer for data to be sent.
 * @param sendbufsize The size of the buffer area.
 * @param recvbuf A pointer to an area in memory which can be used as a buffer for data to be
 * received.
 * @param recvbufsize The size of the buffer area.
 */
void isotp_init_link(IsoTpLink *link, uint32_t sendid, uint8_t *sendbuf, uint16_t sendbufsize,
                     uint8_t *recvbuf, uint16_t recvbufsize);

/**
 * @brief Polling function; call this function periodically to handle timeouts, send consecutive
 * frames, etc.
 *
 * @param link The @code IsoTpLink @endcode instance used.
 */
void isotp_poll(IsoTpLink *link);

/**
 * @brief Handles incoming CAN messages.
 * Determines whether an incoming message is a valid ISO-TP frame or not and handles it accordingly.
 *
 * @param link The @code IsoTpLink @endcode instance used for transceiving data.
 * @param data The data received via CAN.
 * @param len The length of the data received.
 */
void isotp_on_can_message(IsoTpLink *link, const uint8_t *data, uint8_t len);

/**
 * @brief Sends ISO-TP frames via CAN, using the ID set in the initialising function.
 *
 * Single-frame messages will be sent immediately when calling this function.
 * Multi-frame messages will be sent consecutively when calling isotp_poll.
 *
 * @param link The @code IsoTpLink @endcode instance used for transceiving data.
 * @param payload The payload to be sent. (Up to 4095 bytes).
 * @param size The size of the payload to be sent.
 *
 * @return Possible return values:
 *  - @code ISOTP_RET_OVERFLOW @endcode
 *  - @code ISOTP_RET_INPROGRESS @endcode
 *  - @code ISOTP_RET_OK @endcode
 *  - The return value of the user shim function isotp_user_send_can().
 */
int isotp_send(IsoTpLink *link, const uint8_t payload[], uint16_t size);

/**
 * @brief See @link isotp_send @endlink, with the exception that this function is used only for
 * functional addressing.
 */
int isotp_send_with_id(IsoTpLink *link, uint32_t id, const uint8_t payload[], uint16_t size);

/**
 * @brief Receives and parses the received data and copies the parsed data in to the internal
 * buffer.
 * @param link The @link IsoTpLink @endlink instance used to transceive data.
 * @param payload A pointer to an area in memory where the raw data is copied from.
 * @param payload_size The size of the received (raw) CAN data.
 * @param out_size A reference to a variable which will contain the size of the actual (parsed)
 * data.
 *
 * @return Possible return values:
 *      - @link ISOTP_RET_OK @endlink
 *      - @link ISOTP_RET_NO_DATA @endlink
 */
int isotp_receive(IsoTpLink *link, uint8_t *payload, const uint16_t payload_size,
                  uint16_t *out_size);

#ifdef __cplusplus
}
#endif

#endif // ISOTPC_H

#endif

#if defined(UDS_TP_ISOTP_C)

typedef struct {
    UDSTp_t hdl;
    IsoTpLink phys_link;
    IsoTpLink func_link;
    uint8_t send_buf[UDS_ISOTP_MTU];
    uint8_t recv_buf[UDS_ISOTP_MTU];
    uint32_t phys_sa, phys_ta;
    uint32_t func_sa, func_ta;
} UDSISOTpC_t;

typedef struct {
    uint32_t source_addr;
    uint32_t target_addr;
    uint32_t source_addr_func;
    uint32_t target_addr_func;
} UDSISOTpCConfig_t;

UDSErr_t UDSISOTpCInit(UDSISOTpC_t *tp, const UDSISOTpCConfig_t *cfg);

void UDSISOTpCDeinit(UDSISOTpC_t *tp);

#endif

#if defined(UDS_TP_ISOTP_C_SOCKETCAN)

typedef struct {
    UDSTp_t hdl;
    IsoTpLink phys_link;
    IsoTpLink func_link;
    uint8_t send_buf[UDS_ISOTP_MTU];
    uint8_t recv_buf[UDS_ISOTP_MTU];
    int fd;
    uint32_t phys_sa, phys_ta;
    uint32_t func_sa, func_ta;
    char tag[16];
} UDSTpISOTpC_t;

UDSErr_t UDSTpISOTpCInit(UDSTpISOTpC_t *tp, const char *ifname, uint32_t source_addr,
                         uint32_t target_addr, uint32_t source_addr_func,
                         uint32_t target_addr_func);
void UDSTpISOTpCDeinit(UDSTpISOTpC_t *tp);

#endif

#if defined(UDS_TP_ISOTP_SOCK)

typedef struct {
    UDSTp_t hdl;
    uint8_t recv_buf[UDS_ISOTP_MTU];
    uint8_t send_buf[UDS_ISOTP_MTU];
    size_t recv_len;
    UDSSDU_t recv_info;
    int phys_fd;
    int func_fd;
    uint32_t phys_sa, phys_ta;
    uint32_t func_sa, func_ta;
    char tag[16];
} UDSTpIsoTpSock_t;

UDSErr_t UDSTpIsoTpSockInitServer(UDSTpIsoTpSock_t *tp, const char *ifname, uint32_t source_addr,
                                  uint32_t target_addr, uint32_t source_addr_func);
UDSErr_t UDSTpIsoTpSockInitClient(UDSTpIsoTpSock_t *tp, const char *ifname, uint32_t source_addr,
                                  uint32_t target_addr, uint32_t target_addr_func);
void UDSTpIsoTpSockDeinit(UDSTpIsoTpSock_t *tp);

#endif

/**
 * @file isotp_mock.h
 * @brief in-memory ISO15765 (ISO-TP) transport layer implementation for testing
 * @date 2023-10-21
 *
 */
#if defined(UDS_TP_ISOTP_MOCK)

typedef struct ISOTPMock {
    UDSTp_t hdl;
    uint8_t recv_buf[UDS_TP_MTU];
    uint8_t send_buf[UDS_TP_MTU];
    size_t recv_len;
    UDSSDU_t recv_info;
    uint32_t sa_phys;          // source address - physical messages are sent from this address
    uint32_t ta_phys;          // target address - physical messages are sent to this address
    uint32_t sa_func;          // source address - functional messages are sent from this address
    uint32_t ta_func;          // target address - functional messages are sent to this address
    uint32_t send_tx_delay_ms; // simulated delay
    uint32_t send_buf_size;    // simulated size of the send buffer
    char name[32];             // name for logging
} ISOTPMock_t;

typedef struct {
    uint32_t sa_phys; // source address - physical messages are sent from this address
    uint32_t ta_phys; // target address - physical messages are sent to this address
    uint32_t sa_func; // source address - functional messages are sent from this address
    uint32_t ta_func; // target address - functional messages are sent to this address
} ISOTPMockArgs_t;

/**
 * @brief Create a mock transport. It is connected by default to a broadcast network of all other
 * mock transports in the same process.
 * @param name optional name of the transport (can be NULL)
 * @return UDSTp_t*
 */
UDSTp_t *ISOTPMockNew(const char *name, ISOTPMockArgs_t *args);
void ISOTPMockFree(UDSTp_t *tp);

/**
 * @brief write all messages to a file
 * @note uses UDSMillis() to get the current time
 * @param filename log file name (will be overwritten)
 */
void ISOTPMockLogToFile(const char *filename);
void ISOTPMockLogToStdout(void);

/**
 * @brief clear all transports and close the log file
 */
void ISOTPMockReset(void);

#endif

#ifdef __cplusplus
}
#endif

#endif
