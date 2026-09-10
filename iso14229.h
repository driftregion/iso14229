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


#ifdef UDS_LINES
#line 1 "src/version.h"
#endif
#define UDS_LIB_VERSION "0.10.0"


#ifdef UDS_LINES
#line 1 "src/sys.h"
#endif


/**
 * @defgroup uds_sys_ valid values of UDS_SYS
 * @brief iso14229 host system selection
 * @see UDS_SYS
 * @{
 */
#define UDS_SYS_CUSTOM 0 /**< bare metal or unsupported targets */
#define UDS_SYS_UNIX 1
#define UDS_SYS_WINDOWS 2
#define UDS_SYS_ARDUINO 3
#define UDS_SYS_ESP32 4
#define UDS_SYS_ZEPHYR 5
/** @} */

#if !defined(UDS_SYS)

#if defined(__ZEPHYR__) // native_sim links w/host libc which also defines __unix__
#define UDS_SYS UDS_SYS_ZEPHYR
#elif defined(__unix__) || defined(__APPLE__)
#define UDS_SYS UDS_SYS_UNIX
#elif defined(_WIN32)
#define UDS_SYS UDS_SYS_WINDOWS
#elif defined(ARDUINO)
#define UDS_SYS UDS_SYS_ARDUINO
#elif defined(ESP_PLATFORM)
#define UDS_SYS UDS_SYS_ESP32
#else
#warning                                                                                           \
    "UDS_SYS was not detected, defaulting to UDS_SYS_CUSTOM. Remove this warning by defining UDS_SYS=UDS_SYS_CUSTOM in your build configuration"
#define UDS_SYS UDS_SYS_CUSTOM
#endif

#endif

#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#if UDS_SYS == UDS_SYS_CUSTOM
#define UDS_CUSTOM_MILLIS
#endif // UDS_SYS == UDS_SYS_CUSTOM

#if UDS_SYS == UDS_SYS_UNIX
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#endif // if UDS_SYS == UDS_SYS_UNIX

#if UDS_SYS == UDS_SYS_WINDOWS
#include <stdlib.h>
#include <time.h>
#ifdef _MSC_VER
#define strncasecmp _strnicmp
#define strcasecmp _stricmp
#endif // ifdef _MSC_VER
#endif // if UDS_SYS == UDS_SYS_WINDOWS

#if UDS_SYS == UDS_SYS_ARDUINO
#include <Arduino.h>
#define UDS_TP_ISOTP_C
#endif // if UDS_SYS == UDS_SYS_ARDUINO

#if UDS_SYS == UDS_SYS_ESP32
#include <esp_timer.h>
#define UDS_TP_ISOTP_C
#endif // if UDS_SYS == UDS_SYS_ESP32

#if UDS_SYS == UDS_SYS_ZEPHYR
#include <zephyr/kernel.h>
#define UDS_TP_ISOTP_C
#endif // if UDS_SYS == UDS_SYS_ZEPHYR


#ifdef UDS_LINES
#line 1 "src/config.h"
#endif


/**
 * @def UDS_SYS
 * @brief Selects the host system iso14229 is compiled for.
 * @see uds_sys_ for the list of valid values
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


#ifdef UDS_LINES
#line 1 "src/uds.h"
#endif


/** @file */

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

/**
 * @brief Error Codes, including NRCs defined by the standard.
 * @see UDSErrToStr
 */
typedef enum {
    UDS_FAIL = -1, // General error
    UDS_OK = 0,    // Success

    // Negative Response Codes (NRCs) as defined in ISO14229-1:2020 Table A.1 - Negative Response
    // Code (NRC) definition and values
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
    UDS_ERR_SUBFUNCTION_MISMATCH, // The response SubFunction does not match the request SubFunction
    UDS_ERR_RESP_TOO_SHORT,       // The response is too short
    UDS_ERR_BUFSIZ,               // The buffer is not large enough
    UDS_ERR_INVALID_ARG,          // The function has been called with invalid arguments
    UDS_ERR_BUSY,                 // The client is busy and cannot process the request
    UDS_ERR_MISUSE,               // The library is used incorrectly

    UDS_ERR_TPORT = 0x200, // Transport error
} UDSErr_t;

/**
 * @defgroup uds_lev_ds_ Diagnostic Session Levels
 * @brief ISO14229-1:2020 Table 25
 * @see UDSSendDiagSessCtrl UDS_EVT_DiagSessCtrl
 * @{
 */
#define UDS_LEV_DS_DS 1    ///< Default Session
#define UDS_LEV_DS_PRGS 2  ///< Programming Session
#define UDS_LEV_DS_EXTDS 3 ///< Extended Diagnostic Session
#define UDS_LEV_DS_SSDS 4  ///< Safety System Diagnostic Session
/** @} */

/**
 * @defgroup uds_lev_rt_ Reset Types
 * @brief ISO14229-1:2020 Table 34
 * @see UDSSendECUReset UDS_EVT_ECUReset
 * @{
 */
#define UDS_LEV_RT_HR 1      ///< Hard Reset
#define UDS_LEV_RT_KOFFONR 2 ///< Key Off On Reset
#define UDS_LEV_RT_SR 3      ///< Soft Reset
#define UDS_LEV_RT_ERPSD 4   ///< Enable Rapid Power Shut Down
#define UDS_LEV_RT_DRPSD 5   ///< Disable Rapid Power Shut Down
/** @} */

/**
 * @defgroup uds_lev_ctrlp_ Communication Control Levels
 * @brief ISO14229-1:2020 Table 54
 * @see UDSSendCommCtrl UDS_EVT_CommCtrl
 * @{
 */
#define UDS_LEV_CTRLTP_ERXTX 0  ///< EnableRxAndTx
#define UDS_LEV_CTRLTP_ERXDTX 1 ///< EnableRxAndDisableTx
#define UDS_LEV_CTRLTP_DRXETX 2 ///< DisableRxAndEnableTx
#define UDS_LEV_CTRLTP_DRXTX 3  ///< DisableRxAndTx
/** @} */

/**
 * @defgroup uds_ctp_ Communication Types
 * @brief ISO14229-1:2020 Table B.1
 * @see UDSSendCommCtrl UDS_EVT_CommCtrl
 * @{
 */
#define UDS_CTP_NCM 1   ///< NormalCommunicationMessages
#define UDS_CTP_NWMCM 2 ///< NetworkManagementCommunicationMessages
#define UDS_CTP_NWMCM_NCM                                                                          \
    3 ///< NetworkManagementCommunicationMessagesAndNormalCommunicationMessages
/** @} */

/**
 * @defgroup uds_lev_rctp_ Routine Control Levels
 * @brief ISO14229-1:2020 Table 426
 * @see UDSSendRoutineCtrl UDS_EVT_RoutineCtrl
 * @{
 */
#define UDS_LEV_RCTP_STR 1  ///< StartRoutine
#define UDS_LEV_RCTP_STPR 2 ///< StopRoutine
#define UDS_LEV_RCTP_RRR 3  ///< RequestRoutineResults
/** @} */

/**
 * @defgroup uds_moop_ Mode of Operation for RequestFileTransfer
 * @brief ISO14229-1:2020 Table G.1
 * @see UDSSendRequestFileTransfer UDS_EVT_RequestFileTransfer
 * @{
 */
#define UDS_MOOP_ADDFILE 1  ///< AddFile
#define UDS_MOOP_DELFILE 2  ///< DeleteFile
#define UDS_MOOP_REPLFILE 3 ///< ReplaceFile
#define UDS_MOOP_RDFILE 4   ///< ReadFile
#define UDS_MOOP_RDDIR 5    ///< ReadDirectory
#define UDS_MOOP_RSFILE 6   ///< ResumeFile
/** @} */

/**
 * @defgroup uds_lev_dtcstp_ Diagnostic Trouble Code Control Level
 * @brief ISO14229-1:2020 Table 128
 * @see UDSSendControlDTCSetting UDS_EVT_ControlDTCSetting
 * @{
 */
#define UDS_LEV_DTCSTP_ON 1  ///< Resume updating DTCs
#define UDS_LEV_DTCSTP_OFF 2 ///< Stop updating DTCs
/** @} */

/**
 * @defgroup uds_lev_lctp_ Link Control Level
 * @brief ISO14229-1:2020 Table 171
 * @see UDSSendLinkControl UDS_EVT_LinkControl
 * @{
 */
#define UDS_LEV_LCTP_VMTWFP 1 ///< VerifyModeTransitionWithFixedParameter
#define UDS_LEV_LCTP_VMTWSP 2 ///< VerifyModeTransitionWithSpecificParameter
#define UDS_LEV_LCTP_TM 3     ///< TransitionMode
/** @} */

/// ISO-14229-1:2013 Table 2
#define UDS_MAX_DIAGNOSTIC_SERVICES 0x7F

#define UDS_RESPONSE_SID_OF(request_sid)                                                           \
    ((request_sid) + 0x40) ///< Convert request SID to response SID
#define UDS_REQUEST_SID_OF(response_sid)                                                           \
    ((response_sid) - 0x40) ///< Convert response SID to request SID

/// \cond DOXYGEN_SHOULD_SKIP_THIS
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
#define UDS_0X38_REQ_BASE_LEN 5U
#define UDS_0X38_RESP_BASE_LEN 2U
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
/// \endcond


#ifdef UDS_LINES
#line 1 "src/tp.h"
#endif





#if defined UDS_TP_ISOTP_C_SOCKETCAN
#ifndef UDS_TP_ISOTP_C
#define UDS_TP_ISOTP_C
#endif
#endif

/** private: transport message type
 * @defgroup uds_a_mtype
 */
#define UDS_A_MTYPE_DIAG 0
#define UDS_A_MTYPE_REMOTE_DIAG 1
#define UDS_A_MTYPE_SECURE_DIAG 2
#define UDS_A_MTYPE_SECURE_REMOTE_DIAG 3

typedef uint8_t UDS_A_Mtype_t; ///< private: oneof @ref uds_a_mtype

/** private: transport transmission type
 * @defgroup uds_a_ta_type
 */
#define UDS_A_TA_TYPE_PHYSICAL 0   // unicast (1:1)
#define UDS_A_TA_TYPE_FUNCTIONAL 1 // multicast

typedef uint8_t UDS_A_TA_Type_t; ///< private: oneof @ref uds_a_ta_type

/**
 * @brief Service data unit (SDU)
 * @details Service data unit (SDU): data interface between the application layer and the
 * transport layer
 */
typedef struct {
    UDS_A_Mtype_t A_Mtype;     /**< message type (diagnostic, remote diagnostic, secure diagnostic,
                                  secure remote diagnostic) */
    uint32_t A_SA;             /**< application source address */
    uint32_t A_TA;             /**< application target address */
    UDS_A_TA_Type_t A_TA_Type; /**< application target address type (physical or functional) */
    uint32_t A_AE;             /**< application layer remote address */
} UDSSDU_t;

#define UDS_TP_NOOP_ADDR (0xFFFFFFFF) ///< flags A_SA / A_TA as unused

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
     * @return UDS_OK if successful
     */
    UDSErr_t (*send)(struct UDSTp *hdl, const uint8_t *buf, const size_t len, const UDSSDU_t *info);

    /**
     * @brief Receive data from the transport
     * @param hdl: transport handle
     * @param buf: receive buffer
     * @param bufsiz: size of receive buffer
     * @param recvlen: number of bytes actually received
     * @param info: pointer to SDU info to be updated by transport implementation. May be NULL. If
     * non-NULL, the transport implementation must populate it with valid values.
     * @return UDS_OK if successful
     */
    UDSErr_t (*recv)(struct UDSTp *hdl, uint8_t *buf, size_t bufsiz, size_t *recvlen,
                     UDSSDU_t *info);

    /**
     * @brief Poll the transport layer.
     * @param hdl: pointer to transport handle
     * @note
     */
    UDSErr_t (*poll)(struct UDSTp *hdl);

    /**
     * @brief status flag (read-only)
     */
    struct {
        unsigned is_sending : 1; // set when data transmission starts in send(); cleared when done.
    } status;
} UDSTp_t;

UDSErr_t UDSTpSend(UDSTp_t *hdl, const uint8_t *buf, const size_t len,
                   const UDSSDU_t *info); ///< Send to transport
UDSErr_t UDSTpRecv(UDSTp_t *hdl, uint8_t *buf, const size_t bufsiz, size_t *recvlen,
                   UDSSDU_t *info); ///< Receive from transport
UDSErr_t UDSTpPoll(UDSTp_t *hdl);   ///< call this at <5ms intervals


#ifdef UDS_LINES
#line 1 "src/util.h"
#endif






/**
 * @def UDS_ASSERT(x)
 * @brief define this during library development.
 * It is a no-op by default for library users.
 * API misuse is expected to be covered by runtime checks, not by UDS_ASSERT
 */
#ifndef UDS_ASSERT
#define UDS_ASSERT(x)
#endif

/**
 * @brief Check whether one timestamp is after another, correctly handling wrap-around
 * @param a: timestamp to check
 * @param b: reference timestamp
 * @return true if `a` is after `b`
 */
static inline bool UDSTimeAfter(uint32_t a, uint32_t b) { return (int32_t)(a - b) > 0; }

/**
 * @brief Get time in milliseconds
 * @return current time in milliseconds
 * @note implementers must ensure the return value is monotonically increasing between
 * calls. The value must never go backwards.
 * Wrap-around (overflow back to 0) is expected; this is handled by UDSTimeAfter.
 */
uint32_t UDSMillis(void);

const char *UDSErrToStr(UDSErr_t err);
const char *UDSEventToStr(UDSEvent_t evt);


#ifdef UDS_LINES
#line 1 "src/log.h"
#endif


/**
 * @brief logging for bring-up and unit tests.
 * This interface was copied from ESP-IDF.
 */






/**
 * @defgroup uds_log_level_ valid values for UDS_LOG_LEVEL
 * @brief configures logging verbosity
 * @{
 */
#define UDS_LOG_NONE 0    /**< No log output */
#define UDS_LOG_ERROR 1   /**< Log errors only */
#define UDS_LOG_WARN 2    /**< Log warnings and errors */
#define UDS_LOG_INFO 3    /**< Log info, warnings, and errors */
#define UDS_LOG_DEBUG 4   /**< Log debug, info, warnings, and errors */
#define UDS_LOG_VERBOSE 5 /**< Log verbose, debug, info, warnings, and errors */
/** @} */

typedef int UDS_LogLevel_t; ///< one of @ref uds_log_level_

/**
 * @def UDS_LOG_LEVEL
 * @brief sets the logging level
 * @see uds_log_level_ for valid values
 */
#ifndef UDS_LOG_LEVEL
#define UDS_LOG_LEVEL UDS_LOG_NONE
#endif

/// \cond DOXYGEN_SHOULD_SKIP_THIS
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

static_assert(UDS_LOG_LEVEL == UDS_LOG_NONE || UDS_LOG_LEVEL == UDS_LOG_ERROR ||
                  UDS_LOG_LEVEL == UDS_LOG_WARN || UDS_LOG_LEVEL == UDS_LOG_INFO ||
                  UDS_LOG_LEVEL == UDS_LOG_DEBUG || UDS_LOG_LEVEL == UDS_LOG_VERBOSE,
              "unknown log level");

#if UDS_LOG_LEVEL >= UDS_LOG_ERROR && UDS_LOG_LEVEL != UDS_LOG_NONE
#define UDS_LOGE(tag, format, ...)                                                                 \
    UDS_LogWrite(UDS_LOG_ERROR, tag, UDS_LOG_FORMAT(E, format), UDSMillis(), tag, ##__VA_ARGS__)
#else
#define UDS_LOGE(tag, format, ...) UDS_LogDummy(tag, format, ##__VA_ARGS__)
#endif

#if UDS_LOG_LEVEL >= UDS_LOG_WARN && UDS_LOG_LEVEL != UDS_LOG_NONE
#define UDS_LOGW(tag, format, ...)                                                                 \
    UDS_LogWrite(UDS_LOG_WARN, tag, UDS_LOG_FORMAT(W, format), UDSMillis(), tag, ##__VA_ARGS__)
#else
#define UDS_LOGW(tag, format, ...) UDS_LogDummy(tag, format, ##__VA_ARGS__)
#endif

#if UDS_LOG_LEVEL >= UDS_LOG_INFO && UDS_LOG_LEVEL != UDS_LOG_NONE
#define UDS_LOGI(tag, format, ...)                                                                 \
    UDS_LogWrite(UDS_LOG_INFO, tag, UDS_LOG_FORMAT(I, format), UDSMillis(), tag, ##__VA_ARGS__)
#else
#define UDS_LOGI(tag, format, ...) UDS_LogDummy(tag, format, ##__VA_ARGS__)
#endif

#if UDS_LOG_LEVEL >= UDS_LOG_DEBUG && UDS_LOG_LEVEL != UDS_LOG_NONE
#define UDS_LOGD(tag, format, ...)                                                                 \
    UDS_LogWrite(UDS_LOG_DEBUG, tag, UDS_LOG_FORMAT(D, format), UDSMillis(), tag, ##__VA_ARGS__)
#else
#define UDS_LOGD(tag, format, ...) UDS_LogDummy(tag, format, ##__VA_ARGS__)
#endif

#if UDS_LOG_LEVEL >= UDS_LOG_VERBOSE
#define UDS_LOGV(tag, format, ...)                                                                 \
    UDS_LogWrite(UDS_LOG_VERBOSE, tag, UDS_LOG_FORMAT(V, format), UDSMillis(), tag, ##__VA_ARGS__)
#define UDS_LOG_SDU(tag, buffer, buff_len, info)                                                   \
    UDS_LogSDUInternal(UDS_LOG_DEBUG, tag, buffer, buff_len, info)
#else
#define UDS_LOGV(tag, format, ...) UDS_LogDummy(tag, format, ##__VA_ARGS__)
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
void UDS_LogSDUInternal(UDS_LogLevel_t level, const char *tag, const uint8_t *buffer, size_t buflen,
                        const UDSSDU_t *info);
#endif

// Dummy function that consumes arguments but does nothing
static inline void UDS_LogDummy(const char *tag, const char *format, ...) {
    (void)tag;
    (void)format;
}
static inline void UDS_LogSDUDummy(const char *tag, const uint8_t *buffer, size_t buflen,
                                   const UDSSDU_t *info) {
    (void)tag;
    (void)buffer;
    (void)buflen;
    (void)info;
}
/// \endcond


#ifdef UDS_LINES
#line 1 "src/client.h"
#endif







#define UDS_SUPPRESS_POS_RESP 0x1  ///< set the suppress positive response bit
#define UDS_FUNCTIONAL 0x2         ///< send the request as a functional request
#define UDS_IGNORE_SRV_TIMINGS 0x8 ///< ignore the server-provided p2 and p2_star

/**
 * @brief UDS client structure
 */
typedef struct UDSClient {
    uint16_t p2_ms;      /**< p2 timeout in milliseconds */
    uint32_t p2_star_ms; /**< p2* timeout in milliseconds (for 0x78 response) */
    UDSTp_t *tp;         /**< transport layer handle */

    uint32_t p2_timer; /**< p2 timer value */
    uint8_t state;     /**< client request state, @see client_request_states */

    uint8_t options;                        /**< current request options */
    uint8_t defaultOptions;                 /**< default options for all requests */
    uint8_t _options_copy;                  /**< copy of options at the time a request is made */
    uint8_t cfg_data_format_identifier;     /**< 0x38 RequestFileTransfer dataFormatIdentifier */
    uint8_t cfg_file_size_parameter_length; /**< 0x38 RequestFileTransfer fileSizeParameterLength */

    int (*fn)(struct UDSClient *client, UDSEvent_t evt, void *ev_data); /**< callback function */
    void *fn_data; /**< user-specified function data */

    size_t recv_size;                           /**< size of received data */
    size_t send_size;                           /**< size of data to send */
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
    uint32_t maxBlockLength; /**< server's maximum block length for TransferData requests */
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

UDSErr_t UDSClientInit(UDSClient_t *client); ///< Call this once
UDSErr_t UDSClientPoll(UDSClient_t *client); ///< Call at <5ms intervals
UDSErr_t UDSSendBytes(UDSClient_t *client, const uint8_t *data,
                      uint16_t size); ///< Send user-defined bytes to a UDS server
UDSErr_t UDSSendECUReset(UDSClient_t *client, uint8_t type);     ///< Request ECUReset
UDSErr_t UDSSendDiagSessCtrl(UDSClient_t *client, uint8_t mode); ///< Change the diagnostic session
UDSErr_t UDSSendSecurityAccess(UDSClient_t *client, uint8_t level, uint8_t *data,
                               uint16_t size); ///< Get Security Access
UDSErr_t UDSSendCommCtrl(UDSClient_t *client, uint8_t ctrl,
                         uint8_t comm); ///< Change communication settings
UDSErr_t UDSSendRDBI(UDSClient_t *client, const uint16_t *didList,
                     const uint16_t numDataIdentifiers); ///< Read Data By Identifier
UDSErr_t UDSSendWDBI(UDSClient_t *client, uint16_t dataIdentifier, const uint8_t *data,
                     uint16_t size);                ///< Write Data By Identifier
UDSErr_t UDSSendTesterPresent(UDSClient_t *client); ///< What's up?
UDSErr_t UDSSendRoutineCtrl(UDSClient_t *client, uint8_t type, uint16_t routineIdentifier,
                            const uint8_t *data, uint16_t size); ///< Request to Twiddle Routines

UDSErr_t UDSSendRequestDownload(UDSClient_t *client, uint8_t dataFormatIdentifier,
                                uint8_t addressAndLengthFormatIdentifier, size_t memoryAddress,
                                size_t memorySize); ///< Request to Download via TransferData

UDSErr_t UDSSendRequestUpload(UDSClient_t *client, uint8_t dataFormatIdentifier,
                              uint8_t addressAndLengthFormatIdentifier, size_t memoryAddress,
                              size_t memorySize); ///< Request to Upload via TransferData
UDSErr_t UDSSendTransferData(UDSClient_t *client, uint8_t blockSequenceCounter,
                             const uint16_t blockLength, const uint8_t *data,
                             uint16_t size); ///< Transfer Data to/from a buffer
UDSErr_t UDSSendTransferDataStream(UDSClient_t *client, uint8_t blockSequenceCounter,
                                   const uint16_t blockLength,
                                   FILE *fd); ///< Transfer Data to/from a file
UDSErr_t
UDSSendRequestTransferExit(UDSClient_t *client); ///< Call this when finished with TransferData

UDSErr_t UDSSendRequestFileTransfer(
    UDSClient_t *client, uint8_t mode, const char *filePath, size_t fileSizeUncompressed,
    size_t fileSizeCompressed); ///< filesystem-based frontend to TransferData
UDSErr_t UDSCtrlDTCSetting(UDSClient_t *client, uint8_t dtcSettingType,
                           uint8_t *dtcSettingControlOptionRecord,
                           uint16_t len); ///< control DTC setting
UDSErr_t UDSUnpackRDBIResponse(UDSClient_t *client, UDSRDBIVar_t *vars,
                               uint16_t numVars); ///< Parse server's response to RDBI
UDSErr_t UDSUnpackSecurityAccessResponse(
    const UDSClient_t *client,
    struct SecurityAccessResponse *resp); ///< Parse server's response to SecurityAccess
UDSErr_t UDSUnpackRequestDownloadResponse(
    const UDSClient_t *client,
    struct RequestDownloadResponse *resp); ///< Parse server's response to RequestDownload
UDSErr_t UDSUnpackRoutineControlResponse(
    const UDSClient_t *client,
    struct RoutineControlResponse *resp); ///< Parse server's response to RoutineControl


#ifdef UDS_LINES
#line 1 "src/server.h"
#endif







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
    const uint8_t type;  /**< oneof @ref uds_lev_ds_ */
    uint16_t p2_ms;      /**< optional: p2 timing override */
    uint32_t p2_star_ms; /**< optional: p2* timing override */
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
    const uint32_t groupOfDTC;     /**< lower 3 bytes describe the groupOfDTC */
    const bool hasMemorySelection; /**< `true` when a memory selection byte is present */
    const uint8_t memorySelection; /**<  memorySelection byte (optional) */
} UDSCDIArgs_t;

/**
 * @brief Read DTC information arguments
 */
typedef struct {
    const uint8_t type; /**< invoked subfunction */
    uint8_t (*copy)(UDSServer_t *srv, const void *src,
                    uint16_t count); /**< function for copying data */

    union {
        struct {
            uint8_t mask;           /**< DTC status mask */
        } numOfDTCByStatusMaskArgs, /**< args for number of DTCs by status mask */
            dtcStatusByMaskArgs;    /**< args for DTCs by status mask */
        struct {
            uint32_t dtc;                /**< DTC Mask Record */
            uint8_t snapshotNum;         /**< DTC Snaphot Record Number */
            uint8_t memory;              /**< Memory Selection (only used when type == 0x18) */
        } dtcSnapshotRecordbyDTCNumArgs, /**< args for DTC snapshot record by DTC number */
            userDefMemDTCSnapshotRecordByDTCNumArgs; /**< args for user-defined-memory DTC snapshot
                                                         record by DTC number */
        struct {
            uint8_t recordNum;               /**< DTC Data Record Number */
        } dtcStoredDataByRecordNumArgs,      /**< args for DTC stored data by record number */
            dtcExtDataRecordByRecordNumArgs, /**< args for DTC extended data record by record number
                                              */
            dtcExtDataRecordIdArgs;          /**< args for supported DTC extended data record ID */
        struct {
            uint32_t dtc;              /**< DTC Mask Record */
            uint8_t extDataRecNum;     /**< DTC Extended Data Record Number */
            uint8_t memory;            /**< Memory Selection (only used when type == 0x19) */
        } dtcExtDtaRecordByDTCNumArgs, /**< args for DTC extended data record by DTC number */
            userDefMemDTCExtDataRecordByDTCNumArgs; /**< args for user-defined-memory DTC extended
                                                        data record by DTC number */
        struct {
            uint8_t
                functionalGroup;  /**< Functional Group Identifier (only used when type == 0x42) */
            uint8_t severityMask; /**< DTC Severity Mask */
            uint8_t statusMask;   /**< DTC Status Mask */
        } numOfDTCBySeverityMaskArgs, /**< args for number of DTCs by severity mask */
            dtcBySeverityMaskArgs,    /**< args for DTCs by severity mask */
            wwhobdDTCByMaskArgs;      /**< args for WWH-OBD DTCs by mask */
        struct {
            uint32_t dtc;        /**< DTC Mask Record */
        } severityInfoOfDTCArgs; /**< args for severity information of a DTC */
        struct {
            uint8_t mask;                   /**< DTC status mask */
            uint8_t memory;                 /**< Memory Selection */
        } userDefMemoryDTCByStatusMaskArgs; /**< args for user-defined-memory DTCs by status mask */
        struct {
            uint8_t functionalGroup; /**< Functional Group Identifier */
            uint8_t
                readinessGroup; /**< DTC Readiness Group Identifier (only used when type == 0x56) */
        } wwhobdDTCWithPermStatusArgs,        /**< args for WWH-OBD DTCs with permanent status */
            dtcInfoByDTCReadinessGroupIdArgs; /**< args for DTCs by readiness group */
    } subFuncArgs; /**< subfunction-specific arguments, selected by \ref type */
} UDSRDTCIArgs_t;

/**
 * @brief Read data by identifier arguments
 */
typedef struct {
    const uint16_t dataId; /**< RDBI Data Identifier */
    uint8_t (*copy)(UDSServer_t *srv, const void *src,
                    uint16_t count); /**< function for copying data */
} UDSRDBIArgs_t;

/**
 * @brief Read memory by address arguments
 */
typedef struct {
    const void *memAddr;  /**< requested server memory address */
    const size_t memSize; /**< requested size */
    uint8_t (*copy)(UDSServer_t *srv, const void *src,
                    uint16_t count); /**< function for copying data to response */
} UDSReadMemByAddrArgs_t;

/**
 * @brief Communication control arguments
 */
typedef struct {
    uint8_t ctrlType; /**< ControlType */
    uint8_t commType; /**< CommunicationType */
    uint16_t nodeId;  /**< NodeIdentificationNumber (only used when ctrlType is 0x04 or 0x05) */
} UDSCommCtrlArgs_t;

/**
 * @brief Security access request seed arguments
 */
typedef struct {
    const uint8_t level;             /**< requested security level */
    const uint8_t *const dataRecord; /**< pointer to request data */
    const uint16_t len;              /**< size of request data */
    uint8_t (*copySeed)(UDSServer_t *srv, const void *src,
                        uint16_t len); /**< function for copying data */
} UDSSecAccessRequestSeedArgs_t;

/**
 * @brief Security access validate key arguments
 */
typedef struct {
    const uint8_t level;      /**< security level to be validated */
    const uint8_t *const key; /**< key sent by client */
    const uint16_t len;       /**< length of key */
} UDSSecAccessValidateKeyArgs_t;

/**
 * @brief Write data by identifier arguments
 */
typedef struct {
    const uint16_t dataId;     /**< WDBI Data Identifier */
    const uint8_t *const data; /**< pointer to data */
    const uint16_t len;        /**< length of data */
} UDSWDBIArgs_t;

/**
 * @brief Write memory by address arguments
 */
typedef struct {
    const void *memAddr;       /**< pointer to memory address */
    const size_t memSize;      /**< size of memory */
    const uint8_t *const data; /**< pointer to data */
} UDSWriteMemByAddrArgs_t;

/**
 * @brief Dynamically define data identifier arguments
 */
typedef struct {
    const uint8_t type;     /**< invoked subfunction */
    bool allDataIds;        /**< is true when request is for all data identifiers (only relevant for
                              subFunc 0x03) */
    uint16_t dynamicDataId; /**< dynamicallyDefinedDataIdentifier */

    union {
        struct {
            uint16_t sourceDataId; /**< source DataIdentifier */
            uint8_t position;      /**< position in source data record */
            uint8_t size;          /**< number of bytes to be copied */
        } defineById; /**< args when defining from an existing source data identifier */
        struct {
            void *memAddr;    /**< memory address to read from */
            size_t memSize;   /**< number of bytes to read */
        } defineByMemAddress; /**< args when defining from a memory address */
    } subFuncArgs;            /**< subfunction-specific arguments, selected by \ref type */
} UDSDDDIArgs_t;

/**
 * @brief Input/output control by identifier arguments
 */
typedef struct {
    const uint16_t dataId;              /**< Data Identifier */
    const uint8_t ioCtrlParam;          /**< inputOutputControlParameter */
    const void *const ctrlStateAndMask; /**< controlState bytes and controlMask (optional) */
    const size_t ctrlStateAndMaskLen;   /**< number of bytes in `ctrlStateAndMask` */
    uint8_t (*copy)(UDSServer_t *srv, const void *src,
                    uint16_t count); /**< function for copying data */
} UDSIOCtrlArgs_t;

/**
 * @brief Routine control arguments
 */
typedef struct {
    const uint8_t ctrlType;      /**< routineControlType */
    const uint16_t id;           /**< routineIdentifier */
    const uint8_t *optionRecord; /**< optional data */
    const uint16_t len;          /**< length of optional data */
    uint8_t (*copyStatusRecord)(UDSServer_t *srv, const void *src,
                                uint16_t len); /**< function for copying response data */
} UDSRoutineCtrlArgs_t;

/**
 * @brief Request download arguments
 */
typedef struct {
    const void *addr;                   /**< requested address */
    const size_t size;                  /**< requested download size */
    const uint8_t dataFormatIdentifier; /**< optional specifier for format of data */
    uint16_t maxNumberOfBlockLength;    /**< optional response: inform client how many data bytes to
                                           send in each    `TransferData` request */
} UDSRequestDownloadArgs_t;

/**
 * @brief Request upload arguments
 */
typedef struct {
    const void *addr;                   /**< requested address */
    const size_t size;                  /**< requested download size */
    const uint8_t dataFormatIdentifier; /**< optional specifier for format of data */
    uint16_t maxNumberOfBlockLength;    /**< optional response: inform client how many data bytes to
                                           send in each    `TransferData` request */
} UDSRequestUploadArgs_t;

/**
 * @brief Transfer data arguments
 */
typedef struct {
    const uint8_t *const data; /**< transfer data */
    const uint16_t len;        /**< transfer data length */
    const uint16_t maxRespLen; /**< don't send more than this many bytes with copyResponse */
    uint8_t (*copyResponse)(
        UDSServer_t *srv, const void *src,
        uint16_t len); /**< function for copying transfer data response data (optional) */
} UDSTransferDataArgs_t;

/**
 * @brief Request transfer exit arguments
 */
typedef struct {
    const uint8_t *const data; /**< request data */
    const uint16_t len;        /**< request data length */
    uint8_t (*copyResponse)(UDSServer_t *srv, const void *src,
                            uint16_t len); /**< function for copying response data (optional) */
} UDSRequestTransferExitArgs_t;

/**
 * @brief Request file transfer arguments
 */
typedef struct {
    /**
     * request:
     * @see @ref uds_moop_ "modeOfOperation values"
     */
    const uint8_t modeOfOperation;
    const uint16_t filePathLen; /**< request: data length. */
    const uint8_t *filePath;    /**< request: file path or directory name (ReadDirectory). */
    const uint8_t dataFormatIdentifier; /**< request: specifier for format of data (does not apply
                                           to DeleteFile or ReadDir) */

    // if MOOP is AddFile, ReplaceFile, or ResumeFile, these fields are **requests**.
    // if MOOP is ReadFile or ReadDirectory, these fields are **responses** -- the server must set
    // them. if MOOP is DelFile, these fields are unused.
    size_t fileSizeUnCompressed; /**< file size or directory info len (ReadDirectory) */
    size_t fileSizeCompressed;   /**< compressed filesize (ReadFile), otherwise zero. */

    uint16_t maxNumberOfBlockLength; /**< response: Defaults to UDS_TP_MTU. Informs client how many
                                        data bytes to send in each `TransferData` request. (unused
                                        by DelFile). */
    size_t filePosition; /**< response: byte position to resume from after suspended download
                            (ResumeFile), otherwise zero. */
} UDSRequestFileTransferArgs_t;

/**
 * @brief Control DTC setting arguments
 */
typedef struct {
    uint8_t type; /**< invoked subfunction */
    size_t len;   /**< length of data */
    void *data;   /**< DTCSettingControlOptionRecord */
} UDSControlDTCSettingArgs_t;

/**
 * @brief Link control arguments
 */
typedef struct {
    const uint8_t type; /**< invoked subfunction */
    /* purposefully left generic to allow vehicle- and supplier specific data of different sizes */
    const size_t len; /**< length of data */
    const void *data; /**< data used in the subfunction. E.g. on SubFunction 0x01 this is the
                         linkControlModelIdentifier, on SubFunction 0x02 this is the linkRecord */
} UDSLinkCtrlArgs_t;

/**
 * @brief Custom service arguments
 */
typedef struct {
    const uint16_t sid;          /**< serviceIdentifier */
    const uint8_t *optionRecord; /**< optional data */
    const uint16_t len;          /**< length of optional data */
    uint8_t (*copyResponse)(UDSServer_t *srv, const void *src,
                            uint16_t len); /**< function for copying response data (optional) */
} UDSCustomArgs_t;

UDSErr_t UDSServerInit(UDSServer_t *srv); ///< call this once
void UDSServerPoll(UDSServer_t *srv);     ///< Call this at <5ms intervals

#if defined(UDS_TP_ISOTP_C)
/// \cond DOXYGEN_SHOULD_SKIP_THIS

#define ISO_TP_USER_SEND_CAN_ARG 1
#define ISO_TP_NO_FORMATTED_ERRORS 1

#ifdef UDS_LINES
#line 1 "src/tp/isotp-c/isotp_config.h"
#endif
////////////////////////////////////////////////////////////////////////
//                  ___ ___  ___ _____ ___      ___                   //
//                 |_ _/ __|/ _ \_   _| _ \___ / __|                  //
//                  | |\__ \ (_) || | |  _/___| (__                   //
//                 |___|___/\___/ |_| |_|      \___|                  //
//                                                                    //
//                      ___ ___  _  _ ___ ___ ___                     //
//                     / __/ _ \| \| | __|_ _/ __|                    //
//                    | (_| (_) | .` | _| | | (_ |                    //
//                     \___\___/|_|\_|_| |___\___|                    //
//                                                                    //
////////////////////////////////////////////////////////////////////////

#ifndef ISOTPC_CONFIG_H
#define ISOTPC_CONFIG_H

/**
 * @file isotp_config.h
 * @brief Compile-time transport configuration and defaults.
 *
 * Prefer the corresponding CMake or Make settings when using a supplied build
 * system. Direct builds must define ABI-affecting options identically while
 * compiling the library and every consumer.
 */

/** @defgroup isotp_config Compile-time configuration
 * @brief Macros controlling frame sizes, timing, optional APIs, and platform integration.
 * @{ */

/** The maximum amount of data bytes a single CAN frame may carry (CAN_DL).
 * Classical CAN is limited to 8 bytes; CAN FD additionally allows frames of
 * 12, 16, 20, 24, 32, 48 and 64 bytes.
 *
 * Set this to one of the CAN FD lengths to enable CAN FD support. This
 * increases the size of the internal frame buffers accordingly, so leave it at
 * 8 on platforms without CAN FD.
 */
#ifndef ISO_TP_MAX_CAN_FRAME_SIZE
    #define ISO_TP_MAX_CAN_FRAME_SIZE 8
#endif

/** The CAN_DL (TX_DL) used by a freshly initialised link.
 * This may be reduced per link at runtime using isotp_set_tx_dl(), e.g. when a
 * peer only supports Classical CAN frame lengths.
 */
#ifndef ISO_TP_DEFAULT_TX_DL
    #define ISO_TP_DEFAULT_TX_DL ISO_TP_MAX_CAN_FRAME_SIZE
#endif

/** Flow Control block size advertised by the receiver.
 * A value of 0 asks the sender not to wait for further block acknowledgements.
 */
#ifndef ISO_TP_DEFAULT_BLOCK_SIZE
    #define ISO_TP_DEFAULT_BLOCK_SIZE 8
#endif

/** Minimum Consecutive Frame separation requested by the receiver, in microseconds.
 */
#ifndef ISO_TP_DEFAULT_ST_MIN_US
    #define ISO_TP_DEFAULT_ST_MIN_US 0
#endif

/** Maximum number of Flow Control Wait frames accepted during transmission.
 */
#ifndef ISO_TP_MAX_WFT_NUMBER
    #define ISO_TP_MAX_WFT_NUMBER 1
#endif

/** Timeout for required Flow Control or Consecutive Frames, in microseconds.
 * Keep this below half the range of the 32-bit microsecond clock so wrapping
 * deadline comparisons remain unambiguous.
 */
#ifndef ISO_TP_DEFAULT_RESPONSE_TIMEOUT_US
    #define ISO_TP_DEFAULT_RESPONSE_TIMEOUT_US 100000
#endif

/** @def ISO_TP_FRAME_PADDING
 * Pad transmitted Classical CAN frames to TX_DL. CAN FD frames larger than
 * eight bytes are always padded to a legal CAN FD data length.
 */
#ifdef DOXYGEN
    #define ISO_TP_FRAME_PADDING
#endif

/** @def ISO_TP_NO_FORMATTED_ERRORS
 * Omit the two formatted error messages, which are the library's only
 * use of snprintf(). Define this on a target whose libc has no snprintf, or
 * where the 128-byte ISOTP_MAX_ERROR_MSG_SIZE stack buffer is unwelcome. The
 * errors are still reported through isotp_user_debug(), without the values.
 *
 * Measured on Cortex-M4, -Os -DNDEBUG: .text 2235 -> 2091 bytes, and the
 * largest stack frame 160 -> 32 bytes. With this and NDEBUG the object needs
 * nothing from libc but memcpy and memset.
 */
#ifdef DOXYGEN
    #define ISO_TP_NO_FORMATTED_ERRORS
#endif


/** Byte written into unused padded frame positions. */
#ifndef ISO_TP_FRAME_PADDING_VALUE
    #define ISO_TP_FRAME_PADDING_VALUE 0xAA
#endif

/** @def ISO_TP_USER_SEND_CAN_ARG
 * Append the link's user_send_can_arg value to isotp_user_send_can(). This
 * changes the shim signature and the public IsoTpLink layout.
 */
#ifdef DOXYGEN
    #define ISO_TP_USER_SEND_CAN_ARG
#endif

/** @def ISO_TP_USER_SEND_CAN_FLAGS
 * Add a frame-flags argument to isotp_user_send_can(), telling the driver whether a frame has to be
 * transmitted as a CAN FD frame. Enable this if the driver cannot derive the
 * frame format from the frame length. When combined with
 * ISO_TP_USER_SEND_CAN_ARG, the flags argument comes first.
 */
#ifdef DOXYGEN
    #define ISO_TP_USER_SEND_CAN_FLAGS
#endif

/** @def ISO_TP_CAN_FD_USE_BRS
 * Add ISOTP_CAN_FRAME_FLAG_BRS to CAN FD transmissions. This has an effect only
 * with ISO_TP_USER_SEND_CAN_FLAGS.
 */
#ifdef DOXYGEN
    #define ISO_TP_CAN_FD_USE_BRS
#endif

/** @def ISO_TP_TRANSMIT_COMPLETE_CALLBACK
 * Add the transmit callback type, registration API, and link state.
 */
#ifdef DOXYGEN
    #define ISO_TP_TRANSMIT_COMPLETE_CALLBACK
#endif

/** @def ISO_TP_RECEIVE_COMPLETE_CALLBACK
 * Add the receive callback type, registration API, and link state.
 */
#ifdef DOXYGEN
    #define ISO_TP_RECEIVE_COMPLETE_CALLBACK
#endif

/** @def ISO_TP_ENABLE_STREAMING
 * Add isotp_receive_streaming() and link state for receiving messages larger
 * than the receive buffer in application-consumable chunks.
 */
#ifdef DOXYGEN
    #define ISO_TP_ENABLE_STREAMING
#endif

/** @} */

#endif // ISOTPC_CONFIG_H

#ifdef UDS_LINES
#line 1 "src/tp/isotp-c/isotp_defines.h"
#endif
#ifndef ISOTPC_USER_DEFINITIONS_H
#define ISOTPC_USER_DEFINITIONS_H

/**
 * @file isotp_defines.h
 * @brief Public result codes, CAN frame constants, and callback types.
 */

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

    #define ISOTP_PACKED_STRUCT(content) typedef struct __attribute__((packed)) content
#endif

/**************************************************************
 * OS specific defines
 *************************************************************/
#ifdef _MSC_VER
    #define ISOTP_PACKED_STRUCT(content) __pragma(pack(push, 1)) typedef struct content __pragma(pack(pop))

    #define snprintf _snprintf

    #include <windows.h>
    #define ISOTP_BYTE_ORDER_LITTLE_ENDIAN
    #define __builtin_bswap8 _byteswap_uint8
    #define __builtin_bswap16 _byteswap_uint16
    #define __builtin_bswap32 _byteswap_uint32
    #define __builtin_bswap64 _byteswap_uint64
#endif

#define LE32TOH(le) ((uint32_t)(((le) << 24) | (((le) & 0x0000FF00) << 8) | (((le) & 0x00FF0000) >> 8) | ((le) >> 24)))

/**************************************************************
 * CAN frame length (CAN_DL) defines
 *************************************************************/

/** @defgroup isotp_can CAN frame constants
 * @brief Values used to configure CAN and CAN FD transmission.
 * @{ */

/** Number of payload bytes in a full Classical CAN frame. */
#define ISOTP_CAN_DL_CLASSIC 8

#if (ISO_TP_MAX_CAN_FRAME_SIZE != 8) && (ISO_TP_MAX_CAN_FRAME_SIZE != 12) && (ISO_TP_MAX_CAN_FRAME_SIZE != 16) && (ISO_TP_MAX_CAN_FRAME_SIZE != 20) && \
    (ISO_TP_MAX_CAN_FRAME_SIZE != 24) && (ISO_TP_MAX_CAN_FRAME_SIZE != 32) && (ISO_TP_MAX_CAN_FRAME_SIZE != 48) && (ISO_TP_MAX_CAN_FRAME_SIZE != 64)
    #error "ISO_TP_MAX_CAN_FRAME_SIZE must be one of 8, 12, 16, 20, 24, 32, 48, 64"
#endif

#if (ISO_TP_DEFAULT_TX_DL > ISO_TP_MAX_CAN_FRAME_SIZE) || (ISO_TP_DEFAULT_TX_DL < ISOTP_CAN_DL_CLASSIC)
    #error "ISO_TP_DEFAULT_TX_DL must be at least 8 and must not exceed ISO_TP_MAX_CAN_FRAME_SIZE"
#endif

/** Transmit a Classical CAN frame; no optional frame properties are set. */
#define ISOTP_CAN_FRAME_FLAG_NONE 0x00
/** Transmit a CAN FD frame. */
#define ISOTP_CAN_FRAME_FLAG_FD 0x01
/** Enable CAN FD bit-rate switching for the data phase. */
#define ISOTP_CAN_FRAME_FLAG_BRS 0x02

/**
 * Largest ISO-TP payload that fits in one frame for a given CAN_DL.
 *
 * CAN FD data lengths use the two-byte SF_DL escape header; Classical CAN uses
 * a one-byte header.
 */
#define ISOTP_SF_MAX_PAYLOAD(can_dl) ((can_dl) > ISOTP_CAN_DL_CLASSIC ? (uint32_t)((can_dl) - 2u) : (uint32_t)((can_dl) - 1u))

/** @} */

/**************************************************************
 * Result codes
 *************************************************************/
/** @defgroup isotp_results Return values
 * @brief Status values returned by the transport API and platform send shim.
 * @{ */

/** Operation succeeded or a frame was accepted by the CAN driver. */
#define ISOTP_RET_OK 0
/** Invalid argument, incompatible receive mode, or permanent driver failure. */
#define ISOTP_RET_ERROR -1
/** A segmented transmission is already active. */
#define ISOTP_RET_INPROGRESS -2
/** A payload exceeds the configured link buffer. */
#define ISOTP_RET_OVERFLOW -3
/** A Consecutive Frame used an unexpected sequence number. */
#define ISOTP_RET_WRONG_SN -4
/** No completed message or streaming chunk is available. */
#define ISOTP_RET_NO_DATA -5
/** A protocol response was not received before its deadline. */
#define ISOTP_RET_TIMEOUT -6
/** A CAN frame or declared ISO-TP payload length is invalid. */
#define ISOTP_RET_LENGTH -7
/** Temporary CAN-driver backpressure, or a streaming destination that is too small. */
#define ISOTP_RET_NOSPACE -8

/** @} */

/** @cond ISOTP_INTERNAL */

/* return logic true if 'a' is after 'b' */
#define IsoTpTimeAfter(a, b) ((int32_t)((int32_t)(b) - (int32_t)(a)) < 0)

/*  invalid bs */
#define ISOTP_INVALID_BS 0xFFFF

/* Define the maximum amount of characters allowed in an error message. This fixes code which would otherwise break on Microsoft's dumb platform. */
#define ISOTP_MAX_ERROR_MSG_SIZE 128

/** @endcond */

/** @defgroup isotp_status Link status and protocol results
 * @brief Observable state for asynchronous send and receive operations.
 * @{ */

/** Current segmented-transmission state stored in IsoTpLink::send_status. */
typedef enum {
    ISOTP_SEND_STATUS_IDLE,       /**< No segmented transmission is active. */
    ISOTP_SEND_STATUS_INPROGRESS, /**< Consecutive Frames or Flow Control are pending. */
    ISOTP_SEND_STATUS_ERROR,      /**< Transmission stopped; inspect IsoTpLink::send_protocol_result. */
} IsoTpSendStatusTypes;

/** Current reassembly state stored in IsoTpLink::receive_status. */
typedef enum {
    ISOTP_RECEIVE_STATUS_IDLE,       /**< No message is being reassembled. */
    ISOTP_RECEIVE_STATUS_INPROGRESS, /**< Consecutive Frames are expected. */
    ISOTP_RECEIVE_STATUS_FULL,       /**< A complete message or streaming chunk is available. */
} IsoTpReceiveStatusTypes;

/** @} */

/** @cond ISOTP_INTERNAL */

/* can fram defination */
#if defined(ISOTP_BYTE_ORDER_LITTLE_ENDIAN)
typedef struct {
    uint8_t reserve_1 : 4;
    uint8_t type      : 4;
    uint8_t reserve_2[ISO_TP_MAX_CAN_FRAME_SIZE - 1];
} IsoTpPciType;

typedef struct {
    uint8_t SF_DL : 4;
    uint8_t type  : 4;
    uint8_t data[ISO_TP_MAX_CAN_FRAME_SIZE - 1];
} IsoTpSingleFrame;

typedef struct {
    uint8_t set_to_zero : 4;
    uint8_t type        : 4;
    uint8_t SF_DL;
    uint8_t data[ISO_TP_MAX_CAN_FRAME_SIZE - 2];
} IsoTpSingleFrameEscape;

typedef struct {
    uint8_t FF_DL_high : 4;
    uint8_t type       : 4;
    uint8_t FF_DL_low;
    uint8_t data[ISO_TP_MAX_CAN_FRAME_SIZE - 2];
} IsoTpFirstFrameShort;

ISOTP_PACKED_STRUCT({
    uint8_t  set_to_zero_high : 4;
    uint8_t  type             : 4;
    uint8_t  set_to_zero_low;
    uint32_t FF_DL;
    uint8_t  data[ISO_TP_MAX_CAN_FRAME_SIZE - 6];
} IsoTpFirstFrameLong);

typedef struct {
    uint8_t SN   : 4;
    uint8_t type : 4;
    uint8_t data[ISO_TP_MAX_CAN_FRAME_SIZE - 1];
} IsoTpConsecutiveFrame;

typedef struct {
    uint8_t FS   : 4;
    uint8_t type : 4;
    uint8_t BS;
    uint8_t STmin;
    uint8_t reserve[ISO_TP_MAX_CAN_FRAME_SIZE - 3];
} IsoTpFlowControl;

#else

typedef struct {
    uint8_t type      : 4;
    uint8_t reserve_1 : 4;
    uint8_t reserve_2[ISO_TP_MAX_CAN_FRAME_SIZE - 1];
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
    uint8_t type  : 4;
    uint8_t SF_DL : 4;
    uint8_t data[ISO_TP_MAX_CAN_FRAME_SIZE - 1];
} IsoTpSingleFrame;

/*
 * single frame using the SF_DL escape sequence (CAN FD only, CAN_DL > 8)
 * +-------------------------+-----------------------+-----+
 * | byte #0                 | byte #1               | ... |
 * +-------------------------+-----------+-----------+-----+
 * | nibble #0   | nibble #1 | nibble #2 | nibble #3 | ... |
 * +-------------+-----------+-----------+-----------+-----+
 * | PCIType = 0 | unused=0  | SF_DL                 | ... |
 * +-------------+-----------+-----------------------+-----+
 */
typedef struct {
    uint8_t type        : 4;
    uint8_t set_to_zero : 4;
    uint8_t SF_DL;
    uint8_t data[ISO_TP_MAX_CAN_FRAME_SIZE - 2];
} IsoTpSingleFrameEscape;

/*
 * first frame short
 * +-------------------------+-----------------------+-----+
 * | byte #0                 | byte #1               | ... |
 * +-------------------------+-----------+-----------+-----+
 * | nibble #0   | nibble #1 | nibble #2 | nibble #3 | ... |
 * +-------------+-----------+-----------+-----------+-----+
 * | PCIType = 1 | FF_DL                             | ... |
 * +-------------+-----------+-----------------------+-----+
 */
typedef struct {
    uint8_t type       : 4;
    uint8_t FF_DL_high : 4;
    uint8_t FF_DL_low;
    uint8_t data[ISO_TP_MAX_CAN_FRAME_SIZE - 2];
} IsoTpFirstFrameShort;

/*
 * first frame long
 * +-------------------------+-----------------------+---------+---------+---------+---------+
 * | byte #0                 | byte #1               | byte #2 | byte #3 | byte #4 | byte #5 |
 * +-------------------------+-----------+-----------+---------+---------+---------+---------+
 * | nibble #0   | nibble #1 | nibble #2 | nibble #3 | ...                                   |
 * +-------------+-----------+-----------+-----------+---------------------------------------+
 * | PCIType = 1 | unused=0  | escape sequence = 0   | FF_DL                                 |
 * +-------------+-----------+-----------------------+---------------------------------------+
 */
ISOTP_PACKED_STRUCT({
    uint8_t  type             : 4;
    uint8_t  set_to_zero_high : 4;
    uint8_t  set_to_zero_low;
    uint32_t FF_DL;
    uint8_t  data[ISO_TP_MAX_CAN_FRAME_SIZE - 6];
} IsoTpFirstFrameLong);

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
    uint8_t SN   : 4;
    uint8_t data[ISO_TP_MAX_CAN_FRAME_SIZE - 1];
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
    uint8_t FS   : 4;
    uint8_t BS;
    uint8_t STmin;
    uint8_t reserve[ISO_TP_MAX_CAN_FRAME_SIZE - 3];
} IsoTpFlowControl;

#endif

typedef struct {
        uint8_t ptr[ISO_TP_MAX_CAN_FRAME_SIZE];
} IsoTpDataArray;

typedef struct {
    union {
        IsoTpPciType           common;
        IsoTpSingleFrame       single_frame;
        IsoTpSingleFrameEscape single_frame_escape;
        IsoTpFirstFrameShort   first_frame_short;
        IsoTpFirstFrameLong    first_frame_long;
        IsoTpConsecutiveFrame  consecutive_frame;
        IsoTpFlowControl       flow_control;
        IsoTpDataArray         data_array;
    } as;
} IsoTpCanMessage;

/** @endcond */

/**************************************************************
 * Callback types
 *************************************************************/

#ifdef ISO_TP_TRANSMIT_COMPLETE_CALLBACK
/**
 * @brief Called after a complete payload has been transmitted successfully.
 *
 * @param link Link that completed transmission. Cast to IsoTpLink* when needed.
 * @param tx_size Size of the completed ISO-TP payload in bytes.
 * @param user_arg Value registered with isotp_set_tx_done_cb().
 */
typedef void (*isotp_tx_done_cb)(void* link, uint32_t tx_size, void* user_arg);
#endif

#ifdef ISO_TP_RECEIVE_COMPLETE_CALLBACK
/**
 * @brief Called after a complete payload has been received successfully.
 *
 * @param link Link that received the payload. Cast to IsoTpLink* when needed.
 * @param data Link-owned payload, valid only for the duration of the callback.
 * @param size Payload size in bytes.
 * @param user_arg Value registered with isotp_set_rx_done_cb().
 */
typedef void (*isotp_rx_done_cb)(void* link, const uint8_t* data, uint32_t size, void* user_arg);
#endif

/** @cond ISOTP_INTERNAL */
/* Protocol Control Information (PCI) types. */
typedef enum {
    ISOTP_PCI_TYPE_SINGLE             = 0x0,
    ISOTP_PCI_TYPE_FIRST_FRAME        = 0x1,
    TSOTP_PCI_TYPE_CONSECUTIVE_FRAME  = 0x2,
    ISOTP_PCI_TYPE_FLOW_CONTROL_FRAME = 0x3,

    ISOTP_PCI_TYPE_CONSECUTIVE_FRAME  = 0x2, // Typo fix; but keep broken value for backwards-compat.
} IsoTpProtocolControlInformation;

/* Private: Protocol Control Information (PCI) flow control identifiers.
 */
typedef enum { PCI_FLOW_STATUS_CONTINUE = 0x0, PCI_FLOW_STATUS_WAIT = 0x1, PCI_FLOW_STATUS_OVERFLOW = 0x2 } IsoTpFlowStatus;

/** @endcond */

/** @addtogroup isotp_status
 * @{ */
/** Protocol operation completed without an error. */
#define ISOTP_PROTOCOL_RESULT_OK 0
/** Reserved result for an N_As transmission timeout; not currently produced. */
#define ISOTP_PROTOCOL_RESULT_TIMEOUT_A -1
/** Timeout waiting for a Flow Control frame. */
#define ISOTP_PROTOCOL_RESULT_TIMEOUT_BS -2
/** Timeout waiting for a Consecutive Frame. */
#define ISOTP_PROTOCOL_RESULT_TIMEOUT_CR -3
/** An incoming Consecutive Frame had the wrong sequence number. */
#define ISOTP_PROTOCOL_RESULT_WRONG_SN -4
/** Reserved result for invalid Flow Control status; not currently produced. */
#define ISOTP_PROTOCOL_RESULT_INVALID_FS -5
/** A protocol data unit arrived in an incompatible link state. */
#define ISOTP_PROTOCOL_RESULT_UNEXP_PDU -6
/** The peer sent more Flow Control Wait frames than ISO_TP_MAX_WFT_NUMBER. */
#define ISOTP_PROTOCOL_RESULT_WFT_OVRN -7
/** The sender reported overflow, or an incoming message exceeded the receive buffer. */
#define ISOTP_PROTOCOL_RESULT_BUFFER_OVFLW -8
/** Reserved generic protocol error; not currently produced. */
#define ISOTP_PROTOCOL_RESULT_ERROR -9

/** @} */

#endif // ISOTPC_USER_DEFINITIONS_H

#ifdef UDS_LINES
#line 1 "src/tp/isotp-c/isotp_user.h"
#endif
////////////////////////////////////////////////////////////////////////
//                  ___ ___  ___ _____ ___      ___                   //
//                 |_ _/ __|/ _ \_   _| _ \___ / __|                  //
//                  | |\__ \ (_) || | |  _/___| (__                   //
//                 |___|___/\___/ |_| |_|      \___|                  //
//                                                                    //
////////////////////////////////////////////////////////////////////////

#ifndef ISOTPC_USER_H
#define ISOTPC_USER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file isotp_user.h
 * @brief Application-provided platform hooks.
 */

/** @defgroup isotp_platform Platform integration
 * @brief Functions that every application supplies to connect ISO-TP to its driver and clock.
 * @{ */

/**
 * @brief Receive a diagnostic message from the library.
 *
 * The application may implement this as a no-op. Calls can occur from any
 * transport API that detects an error. The library does not require the
 * message to be retained after this function returns.
 *
 * @param[in] message Diagnostic string or printf-style format.
 * @param[in] ... Optional format arguments.
 */
void isotp_user_debug(const char* message, ...);

/**
 * @brief Submit one CAN or CAN FD frame to the application's driver.
 *
 * The implementation must consume or copy @p data before returning. It may be
 * called synchronously from isotp_send(), isotp_on_can_message(),
 * isotp_receive_streaming(), or isotp_poll().
 *
 * @param[in] arbitration_id CAN identifier to transmit.
 * @param[in] data Frame payload, valid only for the duration of this call.
 * @param[in] size Frame payload length. It never exceeds
 *                 ISO_TP_MAX_CAN_FRAME_SIZE. Lengths above 8 require CAN FD.
 * @param[in] flags Present only with ISO_TP_USER_SEND_CAN_FLAGS. A bitwise OR
 *                  of ISOTP_CAN_FRAME_FLAG_FD and ISOTP_CAN_FRAME_FLAG_BRS;
 *                  short frames on a link with TX_DL above 8 still carry the
 *                  FD flag.
 * @param[in] arg Present only with ISO_TP_USER_SEND_CAN_ARG. This is the link's
 *                user_send_can_arg value.
 * @retval ISOTP_RET_OK The driver accepted the frame.
 * @retval ISOTP_RET_NOSPACE The driver is temporarily full and a polled
 *                           Consecutive Frame should be retried.
 * @retval ISOTP_RET_ERROR The frame could not be submitted.
 */
int isotp_user_send_can(const uint32_t arbitration_id, const uint8_t* data, const uint8_t size
#ifdef ISO_TP_USER_SEND_CAN_FLAGS
                        , const uint8_t flags
#endif
#ifdef ISO_TP_USER_SEND_CAN_ARG
                        , void* arg
#endif
);

/**
 * @brief Return the current 32-bit monotonic time in microseconds.
 *
 * The value must advance independently of call frequency. Natural wraparound
 * at UINT32_MAX is supported.
 *
 * @return Current platform tick in microseconds.
 */
uint32_t isotp_user_get_us(void);

/** @} */

#ifdef __cplusplus
}
#endif

#endif // ISOTPC_USER_H

#ifdef UDS_LINES
#line 1 "src/tp/isotp-c/isotp.h"
#endif
////////////////////////////////////////////////////////////////////////
//                  ___ ___  ___ _____ ___      ___                   //
//                 |_ _/ __|/ _ \_   _| _ \___ / __|                  //
//                  | |\__ \ (_) || | |  _/___| (__                   //
//                 |___|___/\___/ |_| |_|      \___|                  //
//                                                                    //
////////////////////////////////////////////////////////////////////////

#ifndef ISOTPC_H
#define ISOTPC_H

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#ifdef __cplusplus
    #include <stdint.h>

extern "C" {
#endif





/**
 * @file isotp.h
 * @brief Public ISO-TP link and transport API.
 */

/**
 * @defgroup isotp_api Transport API
 * @brief Functions for creating, driving, sending through, and receiving from an ISO-TP link.
 * @{
 */

/**
 * @brief State for one independent, full-duplex ISO-TP conversation.
 *
 * Allocate one link for each conversation that can have independent send or
 * receive state. Initialise it with isotp_init_link() before use and keep the
 * object and its buffers alive for the complete lifetime of the link.
 *
 * Calls that access the same link must be serialised. Applications may observe
 * the documented status/result members and set user_send_can_arg when enabled;
 * all other members are implementation state.
 */
typedef struct IsoTpLink {
    /** @cond ISOTP_INTERNAL */
    /* sender parameters */
    uint32_t            send_arbitration_id;
    uint8_t             tx_dl;

    uint8_t*            send_buffer;
    uint32_t            send_buf_size;
    uint32_t            send_size;
    uint32_t            send_offset;

    uint8_t             send_sn;
    uint32_t            send_bs_remain;
    uint32_t            send_st_min_us;
    uint8_t             send_wtf_count;
    uint32_t            send_timer_st;
    uint32_t            send_timer_bs;
    /** @endcond */
    /** Result of the current or most recent segmented transmission. */
    int32_t             send_protocol_result;
    /** Current IsoTpSendStatusTypes value. */
    uint8_t             send_status;
    /** @cond ISOTP_INTERNAL */

    /* receiver parameters */
    uint32_t            receive_arbitration_id;
    uint8_t             rx_dl;

    uint8_t*            receive_buffer;
    uint32_t            receive_buf_size;
    uint32_t            receive_size;
    uint32_t            receive_offset;

    uint8_t             receive_sn;
    uint8_t             receive_bs_count;
    uint32_t            receive_timer_cr;
    /** @endcond */
    /** Result of the current or most recent receive operation. */
    int                 receive_protocol_result;
    /** Current IsoTpReceiveStatusTypes value. Prefer isotp_receive() to consume completed data. */
    uint8_t             receive_status;
    /** @cond ISOTP_INTERNAL */

#ifdef ISO_TP_ENABLE_STREAMING
    uint32_t            receive_stream_size;
    uint8_t             receive_streaming;
    uint8_t             receive_stream_carry_size;
    uint8_t             receive_stream_carry[ISO_TP_MAX_CAN_FRAME_SIZE - 1];
#endif
    /** @endcond */

#if defined(ISO_TP_USER_SEND_CAN_ARG)
    /**
     * Application value passed to isotp_user_send_can() for this link.
     *
     * Set this after isotp_init_link(), which initially clears it to NULL.
     * A CAN controller or driver context pointer is a typical value.
     */
    void*               user_send_can_arg;
#endif

    /** @cond ISOTP_INTERNAL */
#ifdef ISO_TP_TRANSMIT_COMPLETE_CALLBACK
    isotp_tx_done_cb    tx_done_cb;
    void*               tx_done_cb_arg;
#endif

#ifdef ISO_TP_RECEIVE_COMPLETE_CALLBACK
    isotp_rx_done_cb    rx_done_cb;
    void*               rx_done_cb_arg;
#endif
    /** @endcond */
} IsoTpLink;

/**
 * @brief Initialise an ISO-TP link and its caller-owned buffers.
 *
 * The complete link object is cleared. Its transmit identifier is set to
 * @p sendid, TX_DL is set to ISO_TP_DEFAULT_TX_DL, and receive and transmit
 * state become idle.
 *
 * @param[out] link Link object to initialise. Must not be NULL.
 * @param[in] sendid CAN arbitration identifier used by isotp_send() and by
 *                   Flow Control responses.
 * @param[in,out] sendbuf Persistent buffer into which outgoing payloads are
 *                        copied. Must not be NULL when @p sendbufsize is nonzero.
 * @param[in] sendbufsize Capacity of @p sendbuf and therefore the maximum
 *                        payload accepted for transmission.
 * @param[in,out] recvbuf Persistent reassembly or streaming buffer. Must not be
 *                        NULL when @p recvbufsize is nonzero.
 * @param[in] recvbufsize Capacity of @p recvbuf. Without streaming, incoming
 *                        messages larger than this are rejected.
 *
 * @pre The link is not being used by another call.
 * @see isotp_set_tx_dl()
 */
void isotp_init_link(IsoTpLink* link, uint32_t sendid, uint8_t* sendbuf, uint32_t sendbufsize, uint8_t* recvbuf, uint32_t recvbufsize);

/**
 * @brief Set the CAN frame data length used to transmit on a link.
 *
 * A Classical CAN link uses 8. CAN FD links may use 12, 16, 20, 24, 32, 48,
 * or 64, subject to the compiled ISO_TP_MAX_CAN_FRAME_SIZE.
 *
 * @param[in,out] link Initialised link, or NULL.
 * @param[in] tx_dl Desired transmit data length.
 * @retval ISOTP_RET_OK The value was applied.
 * @retval ISOTP_RET_ERROR The link is NULL, the value is not a legal CAN data
 *                         length, or it exceeds ISO_TP_MAX_CAN_FRAME_SIZE.
 * @retval ISOTP_RET_INPROGRESS A segmented transmission is active; its TX_DL
 *                              cannot be changed.
 *
 * @note RX_DL is learned independently from each incoming First Frame.
 * @example isotp_example_can_fd.c
 */
int isotp_set_tx_dl(IsoTpLink* link, uint8_t tx_dl);

/**
 * @brief Return the effective transmit data length for a link.
 *
 * @param[in] link Initialised link, or NULL.
 * @return The configured TX_DL, with 8 used as a defensive fallback for a
 *         zero-valued field; returns 0 when @p link is NULL.
 */
uint8_t isotp_get_tx_dl(const IsoTpLink* link);

/**
 * @brief Clear a link's state and callback registrations.
 *
 * No memory is freed because the library owns no allocation. The caller retains
 * ownership of the link and both buffers. Passing NULL has no effect.
 *
 * @param[in,out] link Link to clear, or NULL.
 */
void isotp_destroy_link(IsoTpLink* link);

/**
 * @brief Advance segmented transmission and protocol timeouts.
 *
 * Call this regularly even when completion callbacks are enabled. The required
 * frequency depends on the configured separation time and response timeout.
 * Single-frame transmission and incoming-frame parsing happen synchronously in
 * their respective API calls.
 *
 * @param[in,out] link Initialised link. Must not be NULL.
 * @example isotp_example_polling.c
 */
void isotp_poll(IsoTpLink* link);

/**
 * @brief Process one CAN frame already routed to this link.
 *
 * The application must filter arbitration identifiers before calling this
 * function. Frames shorter than two bytes or longer than
 * ISO_TP_MAX_CAN_FRAME_SIZE are ignored. Valid frames update receive or
 * transmit flow-control state and may synchronously send a Flow Control frame.
 * A registered receive callback may run before this function returns.
 *
 * @param[in,out] link Initialised link. Must not be NULL.
 * @param[in] data Frame payload, valid for at least @p len bytes. Must not be NULL.
 * @param[in] len CAN payload length in bytes.
 */
void isotp_on_can_message(IsoTpLink* link, const uint8_t* data, uint8_t len);

/**
 * @brief Start transmitting a payload with the link's configured CAN identifier.
 *
 * The payload is copied into the link's send buffer. A Single Frame is sent
 * synchronously. For a segmented message, only the First Frame is sent here;
 * isotp_poll() sends the remaining Consecutive Frames after Flow Control.
 *
 * @param[in,out] link Initialised link. Must not be NULL.
 * @param[in] payload Payload to copy. Must be valid for at least @p size bytes.
 * @param[in] size Payload size. It must not exceed the link's send-buffer capacity.
 * @retval ISOTP_RET_OK The Single Frame or First Frame was accepted by the driver.
 * @retval ISOTP_RET_OVERFLOW The payload exceeds the link's send buffer.
 * @retval ISOTP_RET_INPROGRESS Another segmented transmission is active.
 * @return Any other value returned by isotp_user_send_can().
 *
 * @warning ISOTP_RET_OK does not mean a segmented transmission is complete.
 *          Keep polling until the completion callback runs or send_status is
 *          no longer ISOTP_SEND_STATUS_INPROGRESS, then inspect
 *          send_protocol_result.
 */
int isotp_send(IsoTpLink* link, const uint8_t payload[], uint32_t size);

/**
 * @brief Start transmitting with a one-time CAN identifier override.
 *
 * Behaviour and return values are the same as isotp_send(), except @p id is used
 * instead of the identifier stored in the link. This is commonly used for a
 * functional-addressing request. ISO-TP functional requests must fit in a
 * Single Frame; the library does not enforce that addressing rule.
 *
 * @param[in,out] link Initialised link, or NULL.
 * @param[in] id CAN arbitration identifier for this transmission.
 * @param[in] payload Payload to copy. Must be valid for at least @p size bytes.
 * @param[in] size Payload size.
 * @retval ISOTP_RET_ERROR The link is NULL.
 * @retval ISOTP_RET_OK The Single Frame or First Frame was accepted by the driver.
 * @retval ISOTP_RET_OVERFLOW The payload exceeds the link's send buffer.
 * @retval ISOTP_RET_INPROGRESS Another segmented transmission is active.
 * @return Any other value returned by isotp_user_send_can().
 */
int isotp_send_with_id(IsoTpLink* link, uint32_t id, const uint8_t payload[], uint32_t size);

/**
 * @brief Copy and consume one completed, non-streaming message.
 *
 * At most @p payload_size bytes are copied. The completed message is released
 * even when the destination is too small, so any uncopied remainder is lost.
 *
 * @param[in,out] link Initialised link. Must not be NULL.
 * @param[out] payload Destination buffer. Must not be NULL.
 * @param[in] payload_size Capacity of @p payload.
 * @param[out] out_size Number of bytes copied. Must not be NULL.
 * @retval ISOTP_RET_OK A completed message was copied and consumed.
 * @retval ISOTP_RET_NO_DATA No complete message is available.
 * @retval ISOTP_RET_ERROR Streaming reception is active, or a receive callback
 *                         is registered in a build that supports callbacks.
 */
int isotp_receive(IsoTpLink* link, uint8_t* payload, const uint32_t payload_size, uint32_t* out_size);

#ifdef ISO_TP_ENABLE_STREAMING
/**
 * @brief Copy and consume the next available chunk of an incoming message.
 *
 * This function supports both oversized streaming messages and messages that
 * fit in the normal receive buffer. Consuming an intermediate chunk emits a
 * Continue Flow Control frame when more wire data is needed.
 *
 * @param[in,out] link Initialised link, or NULL.
 * @param[out] payload Destination for the complete available chunk, or NULL.
 * @param[in] payload_size Capacity of @p payload.
 * @param[out] out_size Number of bytes copied, or NULL.
 * @param[out] is_complete Set to true when this chunk ends the message, or NULL.
 * @retval ISOTP_RET_OK A chunk was copied.
 * @retval ISOTP_RET_NO_DATA No chunk is currently available.
 * @retval ISOTP_RET_NOSPACE The destination cannot hold the available chunk;
 *                           the chunk remains available.
 * @retval ISOTP_RET_ERROR Any required pointer is NULL.
 * @example isotp_example_streaming.c
 */
int isotp_receive_streaming(IsoTpLink* link, uint8_t* payload, const uint32_t payload_size, uint32_t* out_size, bool* is_complete);
#endif

#ifdef ISO_TP_TRANSMIT_COMPLETE_CALLBACK
/**
 * @brief Register or clear the successful-transmission callback.
 *
 * A Single Frame invokes the callback synchronously from isotp_send() or
 * isotp_send_with_id(). A segmented transmission invokes it from isotp_poll()
 * after the final Consecutive Frame is accepted.
 *
 * @param[in,out] link Initialised link. Passing NULL has no effect.
 * @param[in] cb Callback to register, or NULL to disable notification.
 * @param[in] arg Application value passed to @p cb.
 */
void isotp_set_tx_done_cb(IsoTpLink* link, isotp_tx_done_cb cb, void* arg);
#endif

#ifdef ISO_TP_RECEIVE_COMPLETE_CALLBACK
/**
 * @brief Register or clear the completed-receive callback.
 *
 * The callback runs synchronously from isotp_on_can_message(). Its payload
 * pointer refers to the link's receive buffer and is valid only until the
 * callback returns. Registering it disables delivery through isotp_receive().
 * Oversized streaming messages are still delivered through
 * isotp_receive_streaming().
 *
 * @param[in,out] link Initialised link. Passing NULL has no effect.
 * @param[in] cb Callback to register, or NULL to restore polling delivery.
 * @param[in] arg Application value passed to @p cb.
 * @example isotp_example_callbacks.c
 */
void isotp_set_rx_done_cb(IsoTpLink* link, isotp_rx_done_cb cb, void* arg);
#endif

/** @} */

#ifdef __cplusplus
}
#endif

#endif // ISOTPC_H

/// \endcond
#endif // if defined(UDS_TP_ISOTP_C)

#ifdef UDS_LINES
#line 1 "src/tp/isotp_c.h"
#endif

#if defined(UDS_TP_ISOTP_C)







/**
 * @brief isotp-c implementation of \ref UDSTp_t
 */
typedef struct {
    /// \cond DOXYGEN_SHOULD_SKIP_THIS
    UDSTp_t hdl;
    IsoTpLink phys_link;
    IsoTpLink func_link;
    uint8_t send_buf[UDS_ISOTP_MTU];
    uint8_t recv_buf[UDS_ISOTP_MTU];
    uint8_t func_send_buf[8];
    uint8_t func_recv_buf[8];
    uint32_t phys_sa, phys_ta;
    uint32_t func_sa, func_ta;
    /// \endcond
} UDSTpISOTpC_t;

/**
 * @brief Initialize isotp-c transport for \ref UDSServer_t
 * @param tp \ref UDSTpISOTpC_t instance.
 * @param source_addr Server listens for physical transmissions on this address.
 * @param target_addr Server sends responses to this address.
 * @param source_addr_func Server listens for functional transmissions on this address.
 */
UDSErr_t UDSServerTpISOTpCInit(UDSTpISOTpC_t *tp, uint32_t source_addr, uint32_t target_addr,
                               uint32_t source_addr_func);

/**
 * @brief Initialize isotp-c transport for \ref UDSClient_t
 * @param tp \ref UDSTpISOTpC_t instance.
 * @param source_addr Client listens for responses at this address.
 * @param target_addr Client sends physical requests to this address.
 * @param target_addr_func Client sends functional transmissions to this address.
 */
UDSErr_t UDSClientTpISOTpCInit(UDSTpISOTpC_t *tp, uint32_t source_addr, uint32_t target_addr,
                               uint32_t target_addr_func);

// Internal API
UDSErr_t UDSTpISOTpCInit(UDSTpISOTpC_t *tp, uint32_t sa, uint32_t ta, uint32_t sa_func,
                         uint32_t ta_func);
UDSErr_t UDSTpISOTpCPoll(UDSTp_t *tp);

#endif


#ifdef UDS_LINES
#line 1 "src/tp/isotp_c_socketcan.h"
#endif


#if defined(UDS_TP_ISOTP_C_SOCKETCAN)




/**
 * @brief isotp-c over SocketCAN implementation of \ref UDSTp_t
 */
typedef struct {
    /// \cond DOXYGEN_SHOULD_SKIP_THIS
    UDSTpISOTpC_t hdl2;
    int fd;
    char tag[16];
    /// \endcond
} UDSTpISOTpCSocketCAN_t;

/**
 * @brief Initialize isotp-c over SocketCAN transport for \ref UDSServer_t
 * @param tp \ref UDSTpISOTpSocketCAN_t instance.
 * @param source_addr Server listens for physical transmissions on this address.
 * @param target_addr Server sends responses to this address.
 * @param source_addr_func Server listens for functional transmissions on this address.
 */
UDSErr_t UDSServerTpISOTpCSocketCANInit(UDSTpISOTpCSocketCAN_t *tp, const char *ifname,
                                        uint32_t source_addr, uint32_t target_addr,
                                        uint32_t source_addr_func);

/**
 * @brief Initialize isotp-c over SocketCAN transport for \ref UDSClient_t
 * @param tp \ref UDSTpISOTpC_t instance.
 * @param source_addr Client listens for responses at this address.
 * @param target_addr Client sends physical requests to this address.
 * @param target_addr_func Client sends functional transmissions to this address.
 */
UDSErr_t UDSClientTpISOTpCSocketCANInit(UDSTpISOTpCSocketCAN_t *tp, const char *ifname,
                                        uint32_t source_addr, uint32_t target_addr,
                                        uint32_t target_addr_func);

void UDSTpISOTpCSocketCANDeinit(UDSTpISOTpCSocketCAN_t *tp); ///< release socket

#endif


#ifdef UDS_LINES
#line 1 "src/tp/isotp_sock.h"
#endif
#if defined(UDS_TP_ISOTP_SOCK)





/**
 * @brief linux ISO-TP socket implementation of \ref UDSTp_t
 */
typedef struct {
    /// \cond DOXYGEN_SHOULD_SKIP_THIS
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
    /// \endcond
} UDSTpIsoTpSock_t;

UDSErr_t UDSServerTpIsoTpSockInit(UDSTpIsoTpSock_t *tp, const char *ifname, uint32_t source_addr,
                                  uint32_t target_addr,
                                  uint32_t source_addr_func); ///< for UDSServer_t
UDSErr_t UDSClientTpIsoTpSockInit(UDSTpIsoTpSock_t *tp, const char *ifname, uint32_t source_addr,
                                  uint32_t target_addr,
                                  uint32_t target_addr_func); ///< for UDSClient_t
void UDSTpIsoTpSockDeinit(UDSTpIsoTpSock_t *tp);              ///< release sockets

#endif


#ifdef UDS_LINES
#line 1 "src/tp/isotp_mock.h"
#endif
#if defined(UDS_TP_ISOTP_MOCK)



/// \cond INTERNAL_INTERFACE




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

/// \endcond INTERNAL_INTERFACE

#endif


#ifdef __cplusplus
}
#endif

#endif
