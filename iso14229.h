#ifndef ISO14229_H
#define ISO14229_H

#include <stdbool.h>
#include <stdint.h>
#include "isotp-c/isotp.h"

/*
provide a debug function with -DISO14229USERDEBUG=printf when compiling this
library
*/
#ifndef ISO14229USERDEBUG
#define ISO14229USERDEBUG(fmt, ...) ((void)fmt)
#endif

#define PRINTHEX(addr, len)                                                                        \
    {                                                                                              \
        for (int i = 0; i < len; i++) {                                                            \
            ISO14229USERDEBUG("%02x,", addr[i]);                                                   \
        }                                                                                          \
        ISO14229USERDEBUG("\n");                                                                   \
    }

#define ARRAY_SZ(X) (sizeof((X)) / sizeof((X)[0]))

// ISO-14229-1:2013 Table 2
#define ISO14229_MAX_DIAGNOSTIC_SERVICES 0x7F

#define ISO14229_RESPONSE_SID_OF(request_sid) (request_sid + 0x40)
#define ISO14229_REQUEST_SID_OF(response_sid) (response_sid - 0x40)

#define ISO14229_NEG_RESP_LEN 3U
#define ISO14229_0X10_RESP_LEN 6U
#define ISO14229_0X11_REQ_MIN_LEN 2U
#define ISO14229_0X11_RESP_BASE_LEN 2U
#define ISO14229_0X22_RESP_BASE_LEN 1U
#define ISO14229_0X27_REQ_BASE_LEN 2U
#define ISO14229_0X27_RESP_BASE_LEN 2U
#define ISO14229_0X28_REQ_BASE_LEN 3U
#define ISO14229_0X28_RESP_LEN 2U
#define ISO14229_0X2E_REQ_BASE_LEN 3U
#define ISO14229_0X2E_REQ_MIN_LEN 4U
#define ISO14229_0X2E_RESP_LEN 3U
#define ISO14229_0X31_REQ_MIN_LEN 4U
#define ISO14229_0X31_RESP_MIN_LEN 4U
#define ISO14229_0X34_REQ_BASE_LEN 3U
#define ISO14229_0X34_RESP_BASE_LEN 2U
#define ISO14229_0X36_REQ_BASE_LEN 2U
#define ISO14229_0X36_RESP_BASE_LEN 2U
#define ISO14229_0X37_RESP_BASE_LEN 1U
#define ISO14229_0X3E_REQ_MIN_LEN 2U
#define ISO14229_0X3E_RESP_LEN 2U
#define ISO14229_0X85_REQ_BASE_LEN 2U
#define ISO14229_0X85_RESP_LEN 2U

#define ISO14229_SID_LIST                                                                          \
    X(DIAGNOSTIC_SESSION_CONTROL, 0x10, iso14229DiagnosticSessionControl)                          \
    X(ECU_RESET, 0x11, iso14229ECUReset)                                                           \
    X(CLEAR_DIAGNOSTIC_INFORMATION, 0x14, NULL)                                                    \
    X(READ_DTC_INFORMATION, 0x19, NULL)                                                            \
    X(READ_DATA_BY_IDENTIFIER, 0x22, iso14229ReadDataByIdentifier)                                 \
    X(READ_MEMORY_BY_ADDRESS, 0x23, NULL)                                                          \
    X(READ_SCALING_DATA_BY_IDENTIFIER, 0x24, NULL)                                                 \
    X(SECURITY_ACCESS, 0x27, iso14229SecurityAccess)                                               \
    X(COMMUNICATION_CONTROL, 0x28, iso14229CommunicationControl)                                   \
    X(READ_PERIODIC_DATA_BY_IDENTIFIER, 0x2A, NULL)                                                \
    X(DYNAMICALLY_DEFINE_DATA_IDENTIFIER, 0x2C, NULL)                                              \
    X(WRITE_DATA_BY_IDENTIFIER, 0x2E, iso14229WriteDataByIdentifier)                               \
    X(INPUT_CONTROL_BY_IDENTIFIER, 0x2F, NULL)                                                     \
    X(ROUTINE_CONTROL, 0x31, iso14229RoutineControl)                                               \
    X(REQUEST_DOWNLOAD, 0x34, iso14229RequestDownload)                                             \
    X(REQUEST_UPLOAD, 0x35, NULL)                                                                  \
    X(TRANSFER_DATA, 0x36, iso14229TransferData)                                                   \
    X(REQUEST_TRANSFER_EXIT, 0x37, iso14229RequestTransferExit)                                    \
    X(REQUEST_FILE_TRANSFER, 0x38, NULL)                                                           \
    X(WRITE_MEMORY_BY_ADDRESS, 0x3D, NULL)                                                         \
    X(TESTER_PRESENT, 0x3E, iso14229TesterPresent)                                                 \
    X(ACCESS_TIMING_PARAMETER, 0x83, NULL)                                                         \
    X(SECURED_DATA_TRANSMISSION, 0x84, NULL)                                                       \
    X(CONTROL_DTC_SETTING, 0x85, iso14229ControlDtcSetting)                                        \
    X(RESPONSE_ON_EVENT, 0x86, NULL)

#define X(str_ident, sid, func) kSID_##str_ident = sid,
enum Iso14229DiagnosticServiceId { ISO14229_SID_LIST };
#undef X

#define X(str_ident, sid, func) k##str_ident##_IDX,
enum Iso14229DiagnosticServiceCallbackIdx {
    ISO14229_SID_LIST kISO14229_SID_NOT_SUPPORTED,
};
#undef X

#define ISO14229_NUM_SERVICES (kISO14229_SID_NOT_SUPPORTED)

enum Iso14229DiagnosticSessionType {
    kDefaultSession = 0x01,
    kProgrammingSession = 0x02,
    kExtendedDiagnostic = 0x03,
    kSafetySystemDiagnostic = 0x04,
    ISO14229_SERVER_USER_DIAGNOSTIC_MODES
};

enum Iso14229ResponseCode {
    kPositiveResponse = 0,
    kGeneralReject = 0x10,
    kServiceNotSupported = 0x11,
    kSubFunctionNotSupported = 0x12,
    kIncorrectMessageLengthOrInvalidFormat = 0x13,
    kResponseTooLong = 0x14,
    kBusyRepeatRequest = 0x21,
    kConditionsNotCorrect = 0x22,
    kRequestSequenceError = 0x24,
    kNoResponseFromSubnetComponent = 0x25,
    kFailurePreventsExecutionOfRequestedAction = 0x26,
    kRequestOutOfRange = 0x31,
    kSecurityAccessDenied = 0x33,
    kInvalidKey = 0x35,
    kExceedNumberOfAttempts = 0x36,
    kRequiredTimeDelayNotExpired = 0x37,
    kUploadDownloadNotAccepted = 0x70,
    kTransferDataSuspended = 0x71,
    kGeneralProgrammingFailure = 0x72,
    kWrongBlockSequenceCounter = 0x73,
    kRequestCorrectlyReceived_ResponsePending = 0x78,
    kSubFunctionNotSupportedInActiveSession = 0x7E,
    kServiceNotSupportedInActiveSession = 0x7F,
    kRpmTooHigh = 0x81,
    kRpmTooLow = 0x82,
    kEngineIsRunning = 0x83,
    kEngineIsNotRunning = 0x84,
    kEngineRunTimeTooLow = 0x85,
    kTemperatureTooHigh = 0x86,
    kTemperatureTooLow = 0x87,
    kVehicleSpeedTooHigh = 0x88,
    kVehicleSpeedTooLow = 0x89,
    kThrottlePedalTooHigh = 0x8A,
    kThrottlePedalTooLow = 0x8B,
    kTransmissionRangeNotInNeutral = 0x8C,
    kTransmissionRangeNotInGear = 0x8D,
    kISOSAEReserved = 0x8E,
    kBrakeSwitchNotClosed = 0x8F,
    kShifterLeverNotInPark = 0x90,
    kTorqueConverterClutchLocked = 0x91,
    kVoltageTooHigh = 0x92,
    kVoltageTooLow = 0x93,
};

/**
 * @brief LEV_RT_
 * @addtogroup ecuReset_0x11
 */
enum Iso14229ECUResetResetType {
    kHardReset = 1,
    kKeyOffOnReset = 2,
    kSoftReset = 3,
    kEnableRapidPowerShutDown = 4,
    kDisableRapidPowerShutDown = 5,
};

/**
 * @addtogroup securityAccess_0x27
 */
enum Iso14229SecurityAccessType {
    kRequestSeed = 0x01,
    kSendKey = 0x02,
};

static inline bool Iso14229SecurityAccessLevelIsReserved(uint8_t securityLevel) {
    securityLevel &= 0x3f;
    return (0 == securityLevel || (0x43 <= securityLevel && securityLevel >= 0x5E) ||
            0x7F == securityLevel);
}

/**
 * @addtogroup communicationControl_0x28
 */
enum Iso14229CommunicationControlType {
    kEnableRxAndTx = 0,
    kEnableRxAndDisableTx = 1,
    kDisableRxAndEnableTx = 2,
    kDisableRxAndTx = 3,
};

/**
 * @addtogroup communicationControl_0x28
 */
enum Iso14229CommunicationType {
    kNormalCommunicationMessages = 0x1,
    kNetworkManagementCommunicationMessages = 0x2,
    kNetworkManagementCommunicationMessagesAndNormalCommunicationMessages = 0x3,
};

/**
 * @addtogroup routineControl_0x31
 */
enum RoutineControlType {
    kStartRoutine = 1,
    kStopRoutine = 2,
    kRequestRoutineResults = 3,
};

/**
 * @addtogroup controlDTCSetting_0x85
 */
enum DTCSettingType {
    kDTCSettingON = 0x01,
    kDTCSettingOFF = 0x02,
};

struct Iso14229NegativeResponse {
    uint8_t negResponseSid; // always 0x7F
    uint8_t requestSid;
    uint8_t responseCode;
};

#ifdef ISO14229CLIENT_H
#define CLIENT_CONST const
#else
#define CLIENT_CONST
#endif

struct Iso14229Response {
    CLIENT_CONST uint8_t *buf;
    uint16_t buffer_size;
    uint16_t len;
};

enum Iso14229CANRxStatus { kCANRxNone = 0, kCANRxSome };

// ========================================================================
//                              Helper functions
// ========================================================================

static inline bool responseIsNegative(const struct Iso14229Response *resp) {
    return 0x7F == resp->buf[0];
}

/* returns true if `a` is after `b` */
static inline bool Iso14229TimeAfter(uint32_t a, uint32_t b) {
    return ((int32_t)((int32_t)(b) - (int32_t)(a)) < 0);
}

#endif
