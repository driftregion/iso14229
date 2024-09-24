#pragma once

enum UDSServerEvent {
    UDS_SRV_EVT_DiagSessCtrl,         // UDSDiagSessCtrlArgs_t *
    UDS_SRV_EVT_EcuReset,             // UDSECUResetArgs_t *
    UDS_SRV_EVT_ReadDataByIdent,      // UDSRDBIArgs_t *
    UDS_SRV_EVT_ReadMemByAddr,        // UDSReadMemByAddrArgs_t *
    UDS_SRV_EVT_CommCtrl,             // UDSCommCtrlArgs_t *
    UDS_SRV_EVT_SecAccessRequestSeed, // UDSSecAccessRequestSeedArgs_t *
    UDS_SRV_EVT_SecAccessValidateKey, // UDSSecAccessValidateKeyArgs_t *
    UDS_SRV_EVT_WriteDataByIdent,     // UDSWDBIArgs_t *
    UDS_SRV_EVT_RoutineCtrl,          // UDSRoutineCtrlArgs_t*
    UDS_SRV_EVT_RequestDownload,      // UDSRequestDownloadArgs_t*
    UDS_SRV_EVT_RequestUpload,        // UDSRequestUploadArgs_t *
    UDS_SRV_EVT_TransferData,         // UDSTransferDataArgs_t *
    UDS_SRV_EVT_RequestTransferExit,  // UDSRequestTransferExitArgs_t *
    UDS_SRV_EVT_SessionTimeout,       // NULL
    UDS_SRV_EVT_DoScheduledReset,     // enum UDSEcuResetType *
    UDS_SRV_EVT_CUSTOM,               // UDSCustomArgs_t *
    UDS_SRV_EVT_Err,                  // UDSErr_t *
    UDS_EVT_IDLE,
    UDS_EVT_RESP_RECV,
};

typedef int UDSServerEvent_t;
typedef UDSServerEvent_t UDSEvent_t;

typedef enum {
    UDS_ERR = -1,                 // 通用错误
    UDS_OK = 0,                   // 成功
    UDS_ERR_TIMEOUT,              // 请求超时
    UDS_ERR_NEG_RESP,             // 否定响应
    UDS_ERR_DID_MISMATCH,         // 响应DID对不上期待的DID
    UDS_ERR_SID_MISMATCH,         // 请求和响应SID对不上
    UDS_ERR_SUBFUNCTION_MISMATCH, // 请求和响应SubFunction对不上
    UDS_ERR_TPORT,                // 传输层错误
    UDS_ERR_FILE_IO,              // 文件IO错误
    UDS_ERR_RESP_TOO_SHORT,       // 响应太短
    UDS_ERR_BUFSIZ,               // 缓冲器不够大
    UDS_ERR_INVALID_ARG,          // 参数不对、没发
    UDS_ERR_BUSY,                 // 正在忙、没发
} UDSErr_t;

typedef enum {
    UDSSeqStateDone = 0,
    UDSSeqStateRunning = 1,
    UDSSeqStateGotoNext = 2,
} UDSSeqState_t;

enum UDSDiagnosticSessionType {
    kDefaultSession = 0x01,
    kProgrammingSession = 0x02,
    kExtendedDiagnostic = 0x03,
    kSafetySystemDiagnostic = 0x04,
};

enum {
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
enum UDSECUResetType {
    kHardReset = 1,
    kKeyOffOnReset = 2,
    kSoftReset = 3,
    kEnableRapidPowerShutDown = 4,
    kDisableRapidPowerShutDown = 5,
};

typedef uint8_t UDSECUReset_t;

/**
 * @addtogroup securityAccess_0x27
 */
enum UDSSecurityAccessType {
    kRequestSeed = 0x01,
    kSendKey = 0x02,
};

/**
 * @addtogroup communicationControl_0x28
 */
enum UDSCommunicationControlType {
    kEnableRxAndTx = 0,
    kEnableRxAndDisableTx = 1,
    kDisableRxAndEnableTx = 2,
    kDisableRxAndTx = 3,
};

/**
 * @addtogroup communicationControl_0x28
 */
enum UDSCommunicationType {
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

// ISO-14229-1:2013 Table 2
#define UDS_MAX_DIAGNOSTIC_SERVICES 0x7F

#define UDS_RESPONSE_SID_OF(request_sid) (request_sid + 0x40)
#define UDS_REQUEST_SID_OF(response_sid) (response_sid - 0x40)

#define UDS_NEG_RESP_LEN 3U
#define UDS_0X10_REQ_LEN 2U
#define UDS_0X10_RESP_LEN 6U
#define UDS_0X11_REQ_MIN_LEN 2U
#define UDS_0X11_RESP_BASE_LEN 2U
#define UDS_0X23_REQ_MIN_LEN 4U
#define UDS_0X23_RESP_BASE_LEN 1U
#define UDS_0X22_RESP_BASE_LEN 1U
#define UDS_0X27_REQ_BASE_LEN 2U
#define UDS_0X27_RESP_BASE_LEN 2U
#define UDS_0X28_REQ_BASE_LEN 3U
#define UDS_0X28_RESP_LEN 2U
#define UDS_0X2E_REQ_BASE_LEN 3U
#define UDS_0X2E_REQ_MIN_LEN 4U
#define UDS_0X2E_RESP_LEN 3U
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
#define UDS_0X3E_REQ_MIN_LEN 2U
#define UDS_0X3E_REQ_MAX_LEN 2U
#define UDS_0X3E_RESP_LEN 2U
#define UDS_0X85_REQ_BASE_LEN 2U
#define UDS_0X85_RESP_LEN 2U

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
};
