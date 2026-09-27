#pragma once

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
    UDS_NRC_UNUSED_ISOSAEReserved = 0xFF,

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
#define UDS_LEV_DS_DS 1U    ///< Default Session
#define UDS_LEV_DS_PRGS 2U  ///< Programming Session
#define UDS_LEV_DS_EXTDS 3U ///< Extended Diagnostic Session
#define UDS_LEV_DS_SSDS 4U  ///< Safety System Diagnostic Session
/** @} */

/**
 * @defgroup uds_lev_rt_ Reset Types
 * @brief ISO14229-1:2020 Table 34
 * @see UDSSendECUReset UDS_EVT_ECUReset
 * @{
 */
#define UDS_LEV_RT_HR 1U      ///< Hard Reset
#define UDS_LEV_RT_KOFFONR 2U ///< Key Off On Reset
#define UDS_LEV_RT_SR 3U      ///< Soft Reset
#define UDS_LEV_RT_ERPSD 4U   ///< Enable Rapid Power Shut Down
#define UDS_LEV_RT_DRPSD 5U   ///< Disable Rapid Power Shut Down
/** @} */

/**
 * @defgroup uds_lev_ctrlp_ Communication Control Levels
 * @brief ISO14229-1:2020 Table 54
 * @see UDSSendCommCtrl UDS_EVT_CommCtrl
 * @{
 */
#define UDS_LEV_CTRLTP_ERXTX 0U  ///< EnableRxAndTx
#define UDS_LEV_CTRLTP_ERXDTX 1U ///< EnableRxAndDisableTx
#define UDS_LEV_CTRLTP_DRXETX 2U ///< DisableRxAndEnableTx
#define UDS_LEV_CTRLTP_DRXTX 3U  ///< DisableRxAndTx
/** @} */

/**
 * @defgroup uds_ctp_ Communication Types
 * @brief ISO14229-1:2020 Table B.1
 * @see UDSSendCommCtrl UDS_EVT_CommCtrl
 * @{
 */
#define UDS_CTP_NCM 1U   ///< NormalCommunicationMessages
#define UDS_CTP_NWMCM 2U ///< NetworkManagementCommunicationMessages
#define UDS_CTP_NWMCM_NCM                                                                          \
    3U ///< NetworkManagementCommunicationMessagesAndNormalCommunicationMessages
/** @} */

/**
 * @defgroup uds_lev_rctp_ Routine Control Levels
 * @brief ISO14229-1:2020 Table 426
 * @see UDSSendRoutineCtrl UDS_EVT_RoutineCtrl
 * @{
 */
#define UDS_LEV_RCTP_STR 1U  ///< StartRoutine
#define UDS_LEV_RCTP_STPR 2U ///< StopRoutine
#define UDS_LEV_RCTP_RRR 3U  ///< RequestRoutineResults
/** @} */

/**
 * @defgroup uds_moop_ Mode of Operation for RequestFileTransfer
 * @brief ISO14229-1:2020 Table G.1
 * @see UDSSendRequestFileTransfer UDS_EVT_RequestFileTransfer
 * @{
 */
#define UDS_MOOP_ADDFILE 1U  ///< AddFile
#define UDS_MOOP_DELFILE 2U  ///< DeleteFile
#define UDS_MOOP_REPLFILE 3U ///< ReplaceFile
#define UDS_MOOP_RDFILE 4U   ///< ReadFile
#define UDS_MOOP_RDDIR 5U    ///< ReadDirectory
#define UDS_MOOP_RSFILE 6U   ///< ResumeFile
/** @} */

/**
 * @defgroup uds_lev_dtcstp_ Diagnostic Trouble Code Control Level
 * @brief ISO14229-1:2020 Table 128
 * @see UDSSendControlDTCSetting UDS_EVT_ControlDTCSetting
 * @{
 */
#define UDS_LEV_DTCSTP_ON 1U  ///< Resume updating DTCs
#define UDS_LEV_DTCSTP_OFF 2U ///< Stop updating DTCs
/** @} */

/**
 * @defgroup uds_lev_lctp_ Link Control Level
 * @brief ISO14229-1:2020 Table 171
 * @see UDSSendLinkControl UDS_EVT_LinkControl
 * @{
 */
#define UDS_LEV_LCTP_VMTWFP 1U ///< VerifyModeTransitionWithFixedParameter
#define UDS_LEV_LCTP_VMTWSP 2U ///< VerifyModeTransitionWithSpecificParameter
#define UDS_LEV_LCTP_TM 3U     ///< TransitionMode
/** @} */
