#pragma once

#include "sys.h"
#include "tp.h"
#include "uds.h"
#include "config.h"

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
