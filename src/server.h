#pragma once

#include "sys.h"
#include "tp.h"
#include "uds.h"
#include "config.h"

/**
 * @brief Server request context
 */
typedef struct {
    uint8_t recv_buf[UDS_SERVER_RECV_BUF_SIZE];
    uint8_t send_buf[UDS_SERVER_SEND_BUF_SIZE];
    size_t recv_len;
    size_t send_len;
    size_t send_buf_size;
    UDSSDU_t info;
} UDSReq_t;

typedef struct UDSServer {
    UDSTp_t *tp;
    UDSErr_t (*fn)(struct UDSServer *srv, UDSEvent_t event, void *arg);
    void *fn_data; // user-specified function data

    /**
     * @brief Server time constants (milliseconds)
     */
    uint16_t p2_ms;      // Default P2_server_max timing supported by the server for
                         // the activated diagnostic session.
    uint32_t p2_star_ms; // Enhanced (NRC 0x78) P2_server_max supported by the
                         // server for the activated diagnostic session.
    uint16_t s3_ms;      // Session timeout

    uint8_t ecuResetScheduled;            // nonzero indicates that an ECUReset has been scheduled
    uint32_t ecuResetTimer;               // for delaying resetting until a response
                                          // has been sent to the client
    uint32_t p2_timer;                    // for rate limiting server responses
    uint32_t s3_session_timeout_timer;    // indicates that diagnostic session has timed out
    uint32_t sec_access_auth_fail_timer;  // brute-force hardening: rate limit security access
                                          // requests
    uint32_t sec_access_boot_delay_timer; // brute-force hardening: restrict security access until
                                          // timer expires

    /**
     * @brief UDS-1-2013: Table 407 - 0x36 TransferData Supported negative
     * response codes requires that the server keep track of whether the
     * transfer is active
     */
    bool xferIsActive;
    // UDS-1-2013: 14.4.2.3, Table 404: The blockSequenceCounter parameter
    // value starts at 0x01
    uint8_t xferBlockSequenceCounter;
    size_t xferTotalBytes;  // total transfer size in bytes requested by the client
    size_t xferByteCounter; // total number of bytes transferred
    size_t xferBlockLength; // block length (convenience for the TransferData API)

    uint8_t sessionType;   // diagnostic session type (0x10)
    uint8_t securityLevel; // SecurityAccess (0x27) level

    bool RCRRP;             // set to true when user fn returns 0x78 and false otherwise
    bool requestInProgress; // set to true when a request has been processed but the response has
                            // not yet been sent

    // UDS-1 2013 defines the following conditions under which the server does not
    // process incoming requests:
    // - not ready to receive (Table A.1 0x78)
    // - not accepting request messages and not sending responses (9.3.1)
    //
    // when this variable is set to true, incoming ISO-TP data will not be processed.
    bool notReadyToReceive;

    UDSReq_t r;
} UDSServer_t;

typedef struct {
    const uint8_t type;  /*! requested diagnostic session type */
    uint16_t p2_ms;      /*! optional: p2 timing override */
    uint32_t p2_star_ms; /*! optional: p2* timing override */
} UDSDiagSessCtrlArgs_t;

typedef struct {
    const uint8_t type; /**< \~chinese 客户端请求的复位类型 \~english reset type requested by client
                           (uint8_t) */
    uint32_t powerDownTimeMillis; /**< when this much time has elapsed after a UDS_PositiveResponse,
                                     a UDS_EVT_DoScheduledReset will be issued */
} UDSECUResetArgs_t;

typedef struct {
    const uint16_t dataId; /*! RDBI Data Identifier */
    uint8_t (*copy)(UDSServer_t *srv, const void *src,
                    uint16_t count); /*! function for copying data */
} UDSRDBIArgs_t;

typedef struct {
    const void *memAddr;
    const size_t memSize;
    uint8_t (*copy)(UDSServer_t *srv, const void *src,
                    uint16_t count); /*! function for copying data */
} UDSReadMemByAddrArgs_t;

typedef struct {
    uint8_t ctrlType; /* uint8_t */
    uint8_t commType; /* uint8_t */
} UDSCommCtrlArgs_t;

typedef struct {
    const uint8_t level;             /*! requested security level */
    const uint8_t *const dataRecord; /*! pointer to request data */
    const uint16_t len;              /*! size of request data */
    uint8_t (*copySeed)(UDSServer_t *srv, const void *src,
                        uint16_t len); /*! function for copying data */
} UDSSecAccessRequestSeedArgs_t;

typedef struct {
    const uint8_t level;      /*! security level to be validated */
    const uint8_t *const key; /*! key sent by client */
    const uint16_t len;       /*! length of key */
} UDSSecAccessValidateKeyArgs_t;

typedef struct {
    const uint16_t dataId;     /*! WDBI Data Identifier */
    const uint8_t *const data; /*! pointer to data */
    const uint16_t len;        /*! length of data */
} UDSWDBIArgs_t;

typedef struct {
    const uint8_t ctrlType;      /*! routineControlType */
    const uint16_t id;           /*! routineIdentifier */
    const uint8_t *optionRecord; /*! optional data */
    const uint16_t len;          /*! length of optional data */
    uint8_t (*copyStatusRecord)(UDSServer_t *srv, const void *src,
                                uint16_t len); /*! function for copying response data */
} UDSRoutineCtrlArgs_t;

typedef struct {
    const void *addr;                   /*! requested address */
    const size_t size;                  /*! requested download size */
    const uint8_t dataFormatIdentifier; /*! optional specifier for format of data */
    uint16_t maxNumberOfBlockLength;    /*! optional response: inform client how many data bytes to
                                           send in each    `TransferData` request */
} UDSRequestDownloadArgs_t;

typedef struct {
    const void *addr;                   /*! requested address */
    const size_t size;                  /*! requested download size */
    const uint8_t dataFormatIdentifier; /*! optional specifier for format of data */
    uint16_t maxNumberOfBlockLength;    /*! optional response: inform client how many data bytes to
                                           send in each    `TransferData` request */
} UDSRequestUploadArgs_t;

typedef struct {
    const uint8_t *const data; /*! transfer data */
    const uint16_t len;        /*! transfer data length */
    const uint16_t maxRespLen; /*! don't send more than this many bytes with copyResponse */
    uint8_t (*copyResponse)(
        UDSServer_t *srv, const void *src,
        uint16_t len); /*! function for copying transfer data response data (optional) */
} UDSTransferDataArgs_t;

typedef struct {
    const uint8_t *const data; /*! request data */
    const uint16_t len;        /*! request data length */
    uint8_t (*copyResponse)(UDSServer_t *srv, const void *src,
                            uint16_t len); /*! function for copying response data (optional) */
} UDSRequestTransferExitArgs_t;

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

typedef struct {
    const uint16_t sid;          /*! serviceIdentifier */
    const uint8_t *optionRecord; /*! optional data */
    const uint16_t len;          /*! length of optional data */
    uint8_t (*copyResponse)(UDSServer_t *srv, const void *src,
                            uint16_t len); /*! function for copying response data (optional) */
} UDSCustomArgs_t;

UDSErr_t UDSServerInit(UDSServer_t *srv);
void UDSServerPoll(UDSServer_t *srv);
