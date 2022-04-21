#ifndef ISO14229_H
#define ISO14229_H

#include <stdbool.h>
#include <stdint.h>
#include "isotp-c/isotp.h"

#define ARRAY_SZ(X) (sizeof((X)) / sizeof((X)[0]))

// ISO-14229-1:2013 Table 2
#define ISO14229_MAX_DIAGNOSTIC_SERVICES 0x7F

#define ISO14229_RESPONSE_SID_OF(request_sid) (request_sid + 0x40)
#define ISO14229_REQUEST_SID_OF(response_sid) (response_sid - 0x40)

enum Iso14229DiagnosticServiceId {
    // ISO 14229-1 service requests
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
    kSID_INPUT_CONTROL_BY_IDENTIFIER = 0x2F,
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
    // ...
};

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
 * @addtogroup diagnosticSessionControl_0x10
 */
typedef struct {
    uint8_t diagSessionType;
} __attribute__((packed)) DiagnosticSessionControlRequest;
/**
 * @addtogroup diagnosticSessionControl_0x10
 */
typedef struct {
    uint8_t diagSessionType;
    uint16_t P2;
    uint16_t P2star;
} __attribute__((packed)) DiagnosticSessionControlResponse;

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
 * @addtogroup ecuReset_0x11
 */
typedef struct {
    uint8_t resetType;
} __attribute__((packed)) ECUResetRequest;

/**
 * @addtogroup ecuReset_0x11
 */
typedef struct {
    uint8_t resetType;
    uint8_t powerDownTime;
} __attribute__((packed)) ECUResetResponse;

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
 * @addtogroup securityAccess_0x27
 */
typedef struct {
    uint8_t subFunction;
    uint8_t securityAccessDataRecord[];
} SecurityAccessRequest;

/**
 * @addtogroup securityAccess_0x27
 */
typedef struct {
    uint8_t securityAccessType;
    uint8_t securitySeed[];
} __attribute__((packed)) SecurityAccessResponse;

/**
 * @addtogroup communicationControl_0x28
 */
enum Iso14229CommunicationControlType {
    kEnableRxAndTx = 0,
    kEnableRxAndDisableTx = 1,
    kDisableRxAndEnableTx = 2,
    kDisableRxAndTx = 3,
};

enum Iso14229CommunicationType {
    kNormalCommunicationMessages = 0x1,
    kNetworkManagementCommunicationMessages = 0x2,
    kNetworkManagementCommunicationMessagesAndNormalCommunicationMessages = 0x3,
};

/**
 * @addtogroup communicationControl_0x28
 */
typedef struct {
    uint8_t controlType;
    uint8_t communicationType;
    uint16_t nodeIdentificationNumber;
} __attribute__((packed)) CommunicationControlRequest;

/**
 * @addtogroup communicationControl_0x28
 */
typedef struct {
    uint8_t controlType;
} __attribute__((packed)) CommunicationControlResponse;

/**
 * @addtogroup readDataByIdentifier_0x22
 */
typedef struct {
} ReadDataByIdentifierRequest;

/**
 * @addtogroup readDataByIdentifier_0x22
 */
typedef struct {
} __attribute__((packed)) ReadDataByIdentifierResponse;

/**
 * @addtogroup writeDataByIdentifier_0x2E
 */
typedef struct {
    uint16_t dataIdentifier;
    uint8_t dataRecord[];
} WriteDataByIdentifierRequest;

/**
 * @addtogroup writeDataByIdentifier_0x2E
 */
typedef struct {
    uint16_t dataId;
} __attribute__((packed)) WriteDataByIdentifierResponse;

/**
 * @addtogroup routineControl_0x31
 */
typedef struct {
    uint8_t routineControlType;
    uint16_t routineIdentifier;
    uint8_t routineControlOptionRecord[];
} __attribute__((packed)) RoutineControlRequest;

/**
 * @addtogroup routineControl_0x31
 */
typedef struct {
    uint8_t routineControlType;
    uint16_t routineIdentifier;
    uint8_t routineStatusRecord[];
} __attribute__((packed)) RoutineControlResponse;

/**
 * @addtogroup routineControl_0x31
 */
enum RoutineControlType {
    kStartRoutine = 1,
    kStopRoutine = 2,
    kRequestRoutineResults = 3,
};

/**
 * @addtogroup requestDownload_0x34
 */
typedef struct {
    uint8_t dataFormatIdentifier;
    uint8_t addressAndLengthFormatIdentifier;
    uint32_t memoryAddress;
    uint32_t memorySize;
} __attribute__((packed)) RequestDownloadRequest;
/**
 * @addtogroup requestDownload_0x34
 */
typedef struct {
    uint8_t lengthFormatIdentifier;
    uint16_t maxNumberOfBlockLength;
} __attribute__((packed)) RequestDownloadResponse;

/**
 * @addtogroup transferData_0x36
 */
typedef struct {
    uint8_t blockSequenceCounter;
    uint8_t data[];
} __attribute__((packed)) TransferDataRequest;

/**
 * @addtogroup transferData_0x36
 */
typedef struct {
    uint8_t blockSequenceCounter;
} __attribute__((packed)) TransferDataResponse;

/**
 * @addtogroup requestTransferExit_0x37
 */
typedef struct {
    // uint8_t transferResponseParameterRecord[]; // error: flexible array
    // member in a struct with no named members
} __attribute__((packed)) RequestTransferExitResponse;

/**
 * @addtogroup testerPresent_0x3e
 */
typedef struct {
    uint8_t zeroSubFunction;
} TesterPresentRequest;

/**
 * @addtogroup testerPresent_0x3e
 */
typedef struct {
    uint8_t zeroSubFunction;
} __attribute__((packed)) TesterPresentResponse;

enum DTCSettingType {
    kDTCSettingON = 0x01,
    kDTCSettingOFF = 0x02,
};

/**
 * @addtogroup controlDTCSetting_0x85
 */
typedef struct {
    uint8_t dtcSettingType;
    uint8_t dtcSettingControlOptionRecord[];
} ControlDtcSettingRequest;

/**
 * @addtogroup controlDTCSetting_0x85
 */
typedef struct {
    uint8_t dtcSettingType;
} __attribute__((packed)) ControlDtcSettingResponse;

struct Iso14229NegativeResponse {
    uint8_t negResponseSid;
    uint8_t requestSid;
    uint8_t responseCode;
};

union Iso14229AllResponseTypes {
    DiagnosticSessionControlResponse diagnosticSessionControl;
    ECUResetResponse ecuReset;
    SecurityAccessResponse securityAccess;
    CommunicationControlResponse communicationControl;
    ReadDataByIdentifierResponse readDataByIdentifier;
    WriteDataByIdentifierResponse writeDataByIdentifier;
    RoutineControlResponse routineControl;
    RequestDownloadResponse requestDownload;
    TransferDataResponse transferData;
    RequestTransferExitResponse requestTransferExit;
    TesterPresentResponse testerPresent;
    ControlDtcSettingResponse controlDtcSetting;
};

struct Iso14229PositiveResponse {
    uint8_t serviceId;
    union Iso14229AllResponseTypes type;
};

#ifdef ISO14229CLIENT_H
#define CLIENT_CONST const
#else
#define CLIENT_CONST
#endif

struct Iso14229Response {
    union {
        CLIENT_CONST uint8_t *raw;
        CLIENT_CONST struct Iso14229PositiveResponse *positive;
        CLIENT_CONST struct Iso14229NegativeResponse *negative;
    } as;
    uint16_t buffer_size;
    uint16_t len;
};

// ========================================================================
//                              Helper functions
// ========================================================================

static inline bool responseIsNegative(const struct Iso14229Response *resp) {
    return 0x7F == resp->as.negative->negResponseSid;
}

/**
 * @brief host to network short
 *
 * @param hostshort
 * @return uint16_t
 */
static inline uint16_t Iso14229htons(uint16_t hostshort) {
    return ((hostshort & 0xff) << 8) | ((hostshort & 0xff00) >> 8);
}

/**
 * @brief network to host short
 *
 * @param hostshort
 * @return uint16_t
 */
static inline uint16_t Iso14229ntohs(uint16_t networkshort) { return Iso14229htons(networkshort); }

static inline uint32_t Iso14229htonl(uint32_t hostlong) {
    return (((hostlong & 0xff) << 24) | ((hostlong & 0xff00) << 8) | ((hostlong & 0xff0000) >> 8) |
            ((hostlong & 0xff000000) >> 24));
}

static inline uint32_t Iso14229ntohl(uint32_t networklong) { return Iso14229htonl(networklong); }

/* returns true if `a` is after `b` */
static inline bool Iso14229TimeAfter(uint32_t a, uint32_t b) {
    return ((int32_t)((int32_t)(b) - (int32_t)(a)) < 0);
}

/*
provide a debug function with -DISO14229USERDEBUG=printf when compiling this
library
*/
#ifndef ISO14229USERDEBUG
#define ISO14229USERDEBUG(fmt, ...) ((void)fmt)
#endif

#endif
