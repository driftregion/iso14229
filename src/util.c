#include "sys.h"
#include "config.h"
#include "util.h"
#include "uds.h"

#if UDS_CUSTOM_MILLIS
#else
uint32_t UDSMillis(void) {
#if UDS_SYS == UDS_SYS_UNIX
    struct timeval te;
    gettimeofday(&te, NULL); // cppcheck-suppress misra-c2012-21.6
    long long milliseconds = (te.tv_sec * 1000LL) + (te.tv_usec / 1000);
    return milliseconds;
#elif UDS_SYS == UDS_SYS_WINDOWS
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    long long milliseconds = ts.tv_sec * 1000LL + ts.tv_nsec / 1000000;
    return milliseconds;
#elif UDS_SYS == UDS_SYS_ARDUINO
    return millis();
#elif UDS_SYS == UDS_SYS_ESP32
    return esp_timer_get_time() / 1000;
#else
#error "UDSMillis() undefined!"
#endif
}
#endif

/**
 * @brief Check if a security level is reserved per ISO14229-1:2020 Table 42
 *
 * @param securityLevel
 * @return true
 * @return false
 */
bool UDSSecurityAccessLevelIsReserved(uint8_t subFunction) {
    uint8_t securityLevel = subFunction & 0b00111111u;
    if (0u == securityLevel) {
        return true;
    }
    if ((securityLevel >= 0x43u) && (securityLevel <= 0x5Eu)) {
        return true;
    }
    if (securityLevel == 0x7Fu) {
        return true;
    }
    return false;
}

const char *UDSErrToStr(UDSErr_t err) {
    switch (err) {
    case UDS_OK:
        return "UDS_OK";
    case UDS_FAIL:
        return "UDS_FAIL";
    case UDS_NRC_GeneralReject:
        return "UDS_NRC_GeneralReject";
    case UDS_NRC_ServiceNotSupported:
        return "UDS_NRC_ServiceNotSupported";
    case UDS_NRC_SubFunctionNotSupported:
        return "UDS_NRC_SubFunctionNotSupported";
    case UDS_NRC_IncorrectMessageLengthOrInvalidFormat:
        return "UDS_NRC_IncorrectMessageLengthOrInvalidFormat";
    case UDS_NRC_ResponseTooLong:
        return "UDS_NRC_ResponseTooLong";
    case UDS_NRC_BusyRepeatRequest:
        return "UDS_NRC_BusyRepeatRequest";
    case UDS_NRC_ConditionsNotCorrect:
        return "UDS_NRC_ConditionsNotCorrect";
    case UDS_NRC_RequestSequenceError:
        return "UDS_NRC_RequestSequenceError";
    case UDS_NRC_NoResponseFromSubnetComponent:
        return "UDS_NRC_NoResponseFromSubnetComponent";
    case UDS_NRC_FailurePreventsExecutionOfRequestedAction:
        return "UDS_NRC_FailurePreventsExecutionOfRequestedAction";
    case UDS_NRC_RequestOutOfRange:
        return "UDS_NRC_RequestOutOfRange";
    case UDS_NRC_SecurityAccessDenied:
        return "UDS_NRC_SecurityAccessDenied";
    case UDS_NRC_AuthenticationRequired:
        return "UDS_NRC_AuthenticationRequired";
    case UDS_NRC_InvalidKey:
        return "UDS_NRC_InvalidKey";
    case UDS_NRC_ExceedNumberOfAttempts:
        return "UDS_NRC_ExceedNumberOfAttempts";
    case UDS_NRC_RequiredTimeDelayNotExpired:
        return "UDS_NRC_RequiredTimeDelayNotExpired";
    case UDS_NRC_SecureDataTransmissionRequired:
        return "UDS_NRC_SecureDataTransmissionRequired";
    case UDS_NRC_SecureDataTransmissionNotAllowed:
        return "UDS_NRC_SecureDataTransmissionNotAllowed";
    case UDS_NRC_SecureDataVerificationFailed:
        return "UDS_NRC_SecureDataVerificationFailed";
    case UDS_NRC_CertficateVerificationFailedInvalidTimePeriod:
        return "UDS_NRC_CertficateVerificationFailedInvalidTimePeriod";
    case UDS_NRC_CertficateVerificationFailedInvalidSignature:
        return "UDS_NRC_CertficateVerificationFailedInvalidSignature";
    case UDS_NRC_CertficateVerificationFailedInvalidChainOfTrust:
        return "UDS_NRC_CertficateVerificationFailedInvalidChainOfTrust";
    case UDS_NRC_CertficateVerificationFailedInvalidType:
        return "UDS_NRC_CertficateVerificationFailedInvalidType";
    case UDS_NRC_CertficateVerificationFailedInvalidFormat:
        return "UDS_NRC_CertficateVerificationFailedInvalidFormat";
    case UDS_NRC_CertficateVerificationFailedInvalidContent:
        return "UDS_NRC_CertficateVerificationFailedInvalidContent";
    case UDS_NRC_CertficateVerificationFailedInvalidScope:
        return "UDS_NRC_CertficateVerificationFailedInvalidScope";
    case UDS_NRC_CertficateVerificationFailedInvalidCertificate:
        return "UDS_NRC_CertficateVerificationFailedInvalidCertificate";
    case UDS_NRC_OwnershipVerificationFailed:
        return "UDS_NRC_OwnershipVerificationFailed";
    case UDS_NRC_ChallengeCalculationFailed:
        return "UDS_NRC_ChallengeCalculationFailed";
    case UDS_NRC_SettingAccessRightsFailed:
        return "UDS_NRC_SettingAccessRightsFailed";
    case UDS_NRC_SessionKeyCreationOrDerivationFailed:
        return "UDS_NRC_SessionKeyCreationOrDerivationFailed";
    case UDS_NRC_ConfigurationDataUsageFailed:
        return "UDS_NRC_ConfigurationDataUsageFailed";
    case UDS_NRC_DeAuthenticationFailed:
        return "UDS_NRC_DeAuthenticationFailed";
    case UDS_NRC_UploadDownloadNotAccepted:
        return "UDS_NRC_UploadDownloadNotAccepted";
    case UDS_NRC_TransferDataSuspended:
        return "UDS_NRC_TransferDataSuspended";
    case UDS_NRC_GeneralProgrammingFailure:
        return "UDS_NRC_GeneralProgrammingFailure";
    case UDS_NRC_WrongBlockSequenceCounter:
        return "UDS_NRC_WrongBlockSequenceCounter";
    case UDS_NRC_RequestCorrectlyReceived_ResponsePending:
        return "UDS_NRC_RequestCorrectlyReceived_ResponsePending";
    case UDS_NRC_SubFunctionNotSupportedInActiveSession:
        return "UDS_NRC_SubFunctionNotSupportedInActiveSession";
    case UDS_NRC_ServiceNotSupportedInActiveSession:
        return "UDS_NRC_ServiceNotSupportedInActiveSession";
    case UDS_NRC_RpmTooHigh:
        return "UDS_NRC_RpmTooHigh";
    case UDS_NRC_RpmTooLow:
        return "UDS_NRC_RpmTooLow";
    case UDS_NRC_EngineIsRunning:
        return "UDS_NRC_EngineIsRunning";
    case UDS_NRC_EngineIsNotRunning:
        return "UDS_NRC_EngineIsNotRunning";
    case UDS_NRC_EngineRunTimeTooLow:
        return "UDS_NRC_EngineRunTimeTooLow";
    case UDS_NRC_TemperatureTooHigh:
        return "UDS_NRC_TemperatureTooHigh";
    case UDS_NRC_TemperatureTooLow:
        return "UDS_NRC_TemperatureTooLow";
    case UDS_NRC_VehicleSpeedTooHigh:
        return "UDS_NRC_VehicleSpeedTooHigh";
    case UDS_NRC_VehicleSpeedTooLow:
        return "UDS_NRC_VehicleSpeedTooLow";
    case UDS_NRC_ThrottlePedalTooHigh:
        return "UDS_NRC_ThrottlePedalTooHigh";
    case UDS_NRC_ThrottlePedalTooLow:
        return "UDS_NRC_ThrottlePedalTooLow";
    case UDS_NRC_TransmissionRangeNotInNeutral:
        return "UDS_NRC_TransmissionRangeNotInNeutral";
    case UDS_NRC_TransmissionRangeNotInGear:
        return "UDS_NRC_TransmissionRangeNotInGear";
    case UDS_NRC_BrakeSwitchNotClosed:
        return "UDS_NRC_BrakeSwitchNotClosed";
    case UDS_NRC_ShifterLeverNotInPark:
        return "UDS_NRC_ShifterLeverNotInPark";
    case UDS_NRC_TorqueConverterClutchLocked:
        return "UDS_NRC_TorqueConverterClutchLocked";
    case UDS_NRC_VoltageTooHigh:
        return "UDS_NRC_VoltageTooHigh";
    case UDS_NRC_VoltageTooLow:
        return "UDS_NRC_VoltageTooLow";
    case UDS_NRC_ResourceTemporarilyNotAvailable:
        return "UDS_NRC_ResourceTemporarilyNotAvailable";
    case UDS_ERR_TIMEOUT:
        return "UDS_ERR_TIMEOUT";
    case UDS_ERR_DID_MISMATCH:
        return "UDS_ERR_DID_MISMATCH";
    case UDS_ERR_SID_MISMATCH:
        return "UDS_ERR_SID_MISMATCH";
    case UDS_ERR_SUBFUNCTION_MISMATCH:
        return "UDS_ERR_SUBFUNCTION_MISMATCH";
    case UDS_ERR_TPORT:
        return "UDS_ERR_TPORT";
    case UDS_ERR_RESP_TOO_SHORT:
        return "UDS_ERR_RESP_TOO_SHORT";
    case UDS_ERR_BUFSIZ:
        return "UDS_ERR_BUFSIZ";
    case UDS_ERR_INVALID_ARG:
        return "UDS_ERR_INVALID_ARG";
    case UDS_ERR_BUSY:
        return "UDS_ERR_BUSY";
    case UDS_ERR_MISUSE:
        return "UDS_ERR_MISUSE";
    default:
        return "unknown";
    }
}

const char *UDSEventToStr(UDSEvent_t evt) {

    switch (evt) {
    case UDS_EVT_Custom:
        return "UDS_EVT_Custom";
    case UDS_EVT_Err:
        return "UDS_EVT_Err";
    case UDS_EVT_DiagSessCtrl:
        return "UDS_EVT_DiagSessCtrl";
    case UDS_EVT_EcuReset:
        return "UDS_EVT_EcuReset";
    case UDS_EVT_ReadDataByIdent:
        return "UDS_EVT_ReadDataByIdent";
    case UDS_EVT_ReadMemByAddr:
        return "UDS_EVT_ReadMemByAddr";
    case UDS_EVT_CommCtrl:
        return "UDS_EVT_CommCtrl";
    case UDS_EVT_SecAccessRequestSeed:
        return "UDS_EVT_SecAccessRequestSeed";
    case UDS_EVT_SecAccessValidateKey:
        return "UDS_EVT_SecAccessValidateKey";
    case UDS_EVT_WriteDataByIdent:
        return "UDS_EVT_WriteDataByIdent";
    case UDS_EVT_RoutineCtrl:
        return "UDS_EVT_RoutineCtrl";
    case UDS_EVT_RequestDownload:
        return "UDS_EVT_RequestDownload";
    case UDS_EVT_RequestUpload:
        return "UDS_EVT_RequestUpload";
    case UDS_EVT_TransferData:
        return "UDS_EVT_TransferData";
    case UDS_EVT_RequestTransferExit:
        return "UDS_EVT_RequestTransferExit";
    case UDS_EVT_SessionTimeout:
        return "UDS_EVT_SessionTimeout";
    case UDS_EVT_DoScheduledReset:
        return "UDS_EVT_DoScheduledReset";
    case UDS_EVT_RequestFileTransfer:
        return "UDS_EVT_RequestFileTransfer";
    case UDS_EVT_Poll:
        return "UDS_EVT_Poll";
    case UDS_EVT_SendComplete:
        return "UDS_EVT_SendComplete";
    case UDS_EVT_ResponseReceived:
        return "UDS_EVT_ResponseReceived";
    case UDS_EVT_Idle:
        return "UDS_EVT_Idle";
    case UDS_EVT_MAX:
        return "UDS_EVT_MAX";
    default:
        return "unknown";
    }
}
