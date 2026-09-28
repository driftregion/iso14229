#include "include.h"
#include "util.h"
#include "sys.h"
#include "uds.h"

/// Serializes n bytes of val to *dst in big-endian format.
static void PackBE(uint8_t *dst, uint64_t val, size_t n) {
    for (size_t i = 0; i < n; i++) {
        dst[i] = (uint8_t)(val >> (8U * (n - 1U - i)));
    }
}

/**
 * @brief Unpack up to sizeof(size_t) big-endian bytes from src.
 * @param src buffer
 * @param n ranges from 0 to sizeof(size_t) inclusive
 * @return unpacked quantity
 */
static size_t UnpackBEsize(const uint8_t *src, size_t n) {
    UDS_ASSERT(src != NULL);
    UDS_ASSERT(n <= sizeof(size_t));

    size_t val = 0;
    for (size_t i = 0; i < n; i++) {
        val = (val << 8U) | src[i];
    }
    return val;
}

/**
 * @brief Unpack up to sizeof(uintptr_t) big-endian bytes from src.
 * @param src buffer
 * @param n ranges from 0 to sizeof(uintptr_t) inclusive
 * @return unpacked quantity
 */
static uintptr_t UnpackBEuintptr(const uint8_t *src, size_t n) {
    UDS_ASSERT(src != NULL);
    UDS_ASSERT(n <= sizeof(uintptr_t));

    uintptr_t val = 0;
    for (size_t i = 0; i < n; i++) {
        val = (val << 8U) | src[i];
    }
    return val;
}

/**
 * @brief Unpack up to 4 big-endian bytes from src into dst as uint32_t.
 * @param src buffer
 * @param n ranges from 0 to 4 inclusive
 * @return unpacked quantity
 */
static uint32_t UnpackBEu32(const uint8_t *src, size_t n) {
    UDS_ASSERT(src != NULL);
    UDS_ASSERT(n <= sizeof(uint32_t));

    uint32_t val = 0;
    for (size_t i = 0; i < n; i++) {
        val = (val << 8U) | src[i];
    }

    return val;
}

/**
 * @brief Unpack 2 big-endian bytes from src into dst as uint16_t.
 * @param src buffer
 * @param dst pointer to destination
 * @return UDS_OK if successful
 */
static uint16_t UnpackBEu16(const uint8_t *src) {
    UDS_ASSERT(src);
    return (uint16_t)((uint16_t)(src[0] << 8U) | (uint16_t)src[1]);
}

static uint8_t AsResponseSID(uint8_t request_sid) {
    UDS_ASSERT(request_sid <= (UINT8_MAX - 0x40U));
    return request_sid + 0x40U;
}

static uint8_t AsRequestSID(uint8_t response_sid) {
    UDS_ASSERT(response_sid >= 0x40U);
    return response_sid - 0x40U;
}

/**
 * @brief Check whether one timestamp is after another, correctly handling wrap-around
 * @param a: timestamp to check
 * @param b: reference timestamp
 * @return true if `a` is after `b`
 * @note Do not use for durations > 10 days.
 *
 * The upper limit is 2**32 / 2 / 1000 / 60 / 60 / 24 = 24.8 days.
 * TODO: There is likely some buggy behavior after this time has elapsed. Mitigate this.
 *
 */
static bool UDSTimeAfter(uint32_t a, uint32_t b) {
    uint32_t diff = a - b;
    return (diff - 1U) < 0x7FFFFFFFU;
}

/// returns true if a security level is reserved per ISO14229-1:2020 Table 42
// See ISO14229-1:2020 Table 42 — Request message SubFunction parameter definition
static bool UDSSecurityAccessLevelIsReserved(uint8_t subFunction) {
    if (0U == subFunction) {
        return true;
    }
    if (subFunction <= 0x42U) {
        return false;
    }
    if (subFunction <= 0x5EU) {
        return true;
    }
    if (subFunction <= 0x7EU) {
        return false;
    }
    return true;
}

/// returns true if err is defined in ISO14229-1:2020 as an NRC
static bool UDSErrIsNRC(UDSErr_t err) {
    switch (err) {
    case UDS_PositiveResponse:
    case UDS_NRC_GeneralReject:
    case UDS_NRC_ServiceNotSupported:
    case UDS_NRC_SubFunctionNotSupported:
    case UDS_NRC_IncorrectMessageLengthOrInvalidFormat:
    case UDS_NRC_ResponseTooLong:
    case UDS_NRC_BusyRepeatRequest:
    case UDS_NRC_ConditionsNotCorrect:
    case UDS_NRC_RequestSequenceError:
    case UDS_NRC_NoResponseFromSubnetComponent:
    case UDS_NRC_FailurePreventsExecutionOfRequestedAction:
    case UDS_NRC_RequestOutOfRange:
    case UDS_NRC_SecurityAccessDenied:
    case UDS_NRC_AuthenticationRequired:
    case UDS_NRC_InvalidKey:
    case UDS_NRC_ExceedNumberOfAttempts:
    case UDS_NRC_RequiredTimeDelayNotExpired:
    case UDS_NRC_SecureDataTransmissionRequired:
    case UDS_NRC_SecureDataTransmissionNotAllowed:
    case UDS_NRC_SecureDataVerificationFailed:
    case UDS_NRC_CertficateVerificationFailedInvalidTimePeriod:
    case UDS_NRC_CertficateVerificationFailedInvalidSignature:
    case UDS_NRC_CertficateVerificationFailedInvalidChainOfTrust:
    case UDS_NRC_CertficateVerificationFailedInvalidType:
    case UDS_NRC_CertficateVerificationFailedInvalidFormat:
    case UDS_NRC_CertficateVerificationFailedInvalidContent:
    case UDS_NRC_CertficateVerificationFailedInvalidScope:
    case UDS_NRC_CertficateVerificationFailedInvalidCertificate:
    case UDS_NRC_OwnershipVerificationFailed:
    case UDS_NRC_ChallengeCalculationFailed:
    case UDS_NRC_SettingAccessRightsFailed:
    case UDS_NRC_SessionKeyCreationOrDerivationFailed:
    case UDS_NRC_ConfigurationDataUsageFailed:
    case UDS_NRC_DeAuthenticationFailed:
    case UDS_NRC_UploadDownloadNotAccepted:
    case UDS_NRC_TransferDataSuspended:
    case UDS_NRC_GeneralProgrammingFailure:
    case UDS_NRC_WrongBlockSequenceCounter:
    case UDS_NRC_RequestCorrectlyReceived_ResponsePending:
    case UDS_NRC_SubFunctionNotSupportedInActiveSession:
    case UDS_NRC_ServiceNotSupportedInActiveSession:
    case UDS_NRC_RpmTooHigh:
    case UDS_NRC_RpmTooLow:
    case UDS_NRC_EngineIsRunning:
    case UDS_NRC_EngineIsNotRunning:
    case UDS_NRC_EngineRunTimeTooLow:
    case UDS_NRC_TemperatureTooHigh:
    case UDS_NRC_TemperatureTooLow:
    case UDS_NRC_VehicleSpeedTooHigh:
    case UDS_NRC_VehicleSpeedTooLow:
    case UDS_NRC_ThrottlePedalTooHigh:
    case UDS_NRC_ThrottlePedalTooLow:
    case UDS_NRC_TransmissionRangeNotInNeutral:
    case UDS_NRC_TransmissionRangeNotInGear:
    case UDS_NRC_BrakeSwitchNotClosed:
    case UDS_NRC_ShifterLeverNotInPark:
    case UDS_NRC_TorqueConverterClutchLocked:
    case UDS_NRC_VoltageTooHigh:
    case UDS_NRC_VoltageTooLow:
    case UDS_NRC_ResourceTemporarilyNotAvailable:
        return true;
    default:
        return false;
    }
}
