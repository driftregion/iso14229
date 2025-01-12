#include "util.h"
#include "uds.h"

#if UDS_CUSTOM_MILLIS
#else
uint32_t UDSMillis(void) {
#if UDS_SYS == UDS_SYS_UNIX
    struct timeval te;
    gettimeofday(&te, NULL);
    long long milliseconds = te.tv_sec * 1000LL + te.tv_usec / 1000;
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

bool UDSSecurityAccessLevelIsReserved(uint8_t securityLevel) {
    securityLevel &= 0x3f;
    return (0 == securityLevel || (0x43 <= securityLevel && securityLevel >= 0x5E) ||
            0x7F == securityLevel);
}

const char *UDSErrToStr(UDSErr_t err) {
#define MAKE_CASE(x)                                                                               \
    case x:                                                                                        \
        return #x;

    switch (err) {
        MAKE_CASE(UDS_FAIL)
        MAKE_CASE(UDS_OK)
        MAKE_CASE(UDS_NRC_GeneralReject)
        MAKE_CASE(UDS_NRC_ServiceNotSupported)
        MAKE_CASE(UDS_NRC_SubFunctionNotSupported)
        MAKE_CASE(UDS_NRC_IncorrectMessageLengthOrInvalidFormat)
        MAKE_CASE(UDS_NRC_ResponseTooLong)
        MAKE_CASE(UDS_NRC_BusyRepeatRequest)
        MAKE_CASE(UDS_NRC_ConditionsNotCorrect)
        MAKE_CASE(UDS_NRC_RequestSequenceError)
        MAKE_CASE(UDS_NRC_NoResponseFromSubnetComponent)
        MAKE_CASE(UDS_NRC_FailurePreventsExecutionOfRequestedAction)
        MAKE_CASE(UDS_NRC_RequestOutOfRange)
        MAKE_CASE(UDS_NRC_SecurityAccessDenied)
        MAKE_CASE(UDS_NRC_InvalidKey)
        MAKE_CASE(UDS_NRC_ExceedNumberOfAttempts)
        MAKE_CASE(UDS_NRC_RequiredTimeDelayNotExpired)
        MAKE_CASE(UDS_NRC_UploadDownloadNotAccepted)
        MAKE_CASE(UDS_NRC_TransferDataSuspended)
        MAKE_CASE(UDS_NRC_GeneralProgrammingFailure)
        MAKE_CASE(UDS_NRC_WrongBlockSequenceCounter)
        MAKE_CASE(UDS_NRC_RequestCorrectlyReceived_ResponsePending)
        MAKE_CASE(UDS_NRC_SubFunctionNotSupportedInActiveSession)
        MAKE_CASE(UDS_NRC_ServiceNotSupportedInActiveSession)
        MAKE_CASE(UDS_NRC_RpmTooHigh)
        MAKE_CASE(UDS_NRC_RpmTooLow)
        MAKE_CASE(UDS_NRC_EngineIsRunning)
        MAKE_CASE(UDS_NRC_EngineIsNotRunning)
        MAKE_CASE(UDS_NRC_EngineRunTimeTooLow)
        MAKE_CASE(UDS_NRC_TemperatureTooHigh)
        MAKE_CASE(UDS_NRC_TemperatureTooLow)
        MAKE_CASE(UDS_NRC_VehicleSpeedTooHigh)
        MAKE_CASE(UDS_NRC_VehicleSpeedTooLow)
        MAKE_CASE(UDS_NRC_ThrottlePedalTooHigh)
        MAKE_CASE(UDS_NRC_ThrottlePedalTooLow)
        MAKE_CASE(UDS_NRC_TransmissionRangeNotInNeutral)
        MAKE_CASE(UDS_NRC_TransmissionRangeNotInGear)
        MAKE_CASE(UDS_NRC_BrakeSwitchNotClosed)
        MAKE_CASE(UDS_NRC_ShifterLeverNotInPark)
        MAKE_CASE(UDS_NRC_TorqueConverterClutchLocked)
        MAKE_CASE(UDS_NRC_VoltageTooHigh)
        MAKE_CASE(UDS_NRC_VoltageTooLow)
        MAKE_CASE(UDS_ERR_TIMEOUT)
        MAKE_CASE(UDS_ERR_DID_MISMATCH)
        MAKE_CASE(UDS_ERR_SID_MISMATCH)
        MAKE_CASE(UDS_ERR_SUBFUNCTION_MISMATCH)
        MAKE_CASE(UDS_ERR_TPORT)
        MAKE_CASE(UDS_ERR_RESP_TOO_SHORT)
        MAKE_CASE(UDS_ERR_BUFSIZ)
        MAKE_CASE(UDS_ERR_INVALID_ARG)
        MAKE_CASE(UDS_ERR_BUSY)
    default:
        return "unknown";
    }
#undef MAKE_CASE
}

const char *UDSEvtToStr(UDSEvent_t evt) {
#define MAKE_CASE(x)                                                                               \
    case x:                                                                                        \
        return #x;

    switch (evt) {
        MAKE_CASE(UDS_EVT_Err)
        MAKE_CASE(UDS_EVT_DiagSessCtrl)
        MAKE_CASE(UDS_EVT_EcuReset)
        MAKE_CASE(UDS_EVT_ReadDataByIdent)
        MAKE_CASE(UDS_EVT_ReadMemByAddr)
        MAKE_CASE(UDS_EVT_CommCtrl)
        MAKE_CASE(UDS_EVT_SecAccessRequestSeed)
        MAKE_CASE(UDS_EVT_SecAccessValidateKey)
        MAKE_CASE(UDS_EVT_WriteDataByIdent)
        MAKE_CASE(UDS_EVT_RoutineCtrl)
        MAKE_CASE(UDS_EVT_RequestDownload)
        MAKE_CASE(UDS_EVT_RequestUpload)
        MAKE_CASE(UDS_EVT_TransferData)
        MAKE_CASE(UDS_EVT_RequestTransferExit)
        MAKE_CASE(UDS_EVT_SessionTimeout)
        MAKE_CASE(UDS_EVT_DoScheduledReset)
        MAKE_CASE(UDS_EVT_RequestFileTransfer)

        MAKE_CASE(UDS_EVT_Poll)
        MAKE_CASE(UDS_EVT_SendComplete)
        MAKE_CASE(UDS_EVT_ResponseReceived)
        MAKE_CASE(UDS_EVT_Idle)

    default:
        return "unknown";
    }
#undef MAKE_CASE
}
