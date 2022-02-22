
/**
 * @brief maximum allowable number of user defined 0x31 RoutineControl routines
 * The number of routines in theory ranges from [0-0xFFFF]
 */
#ifndef ISO14229_SERVER_MAX_ROUTINES
#define ISO14229_SERVER_MAX_ROUTINES 10
#endif

/**
 * @brief maximum allowable number of download handlers
 *
 */
#ifndef ISO14229_SERVER_MAX_DOWNLOAD_HANDLERS
#define ISO14229_SERVER_MAX_DOWNLOAD_HANDLERS 1
#endif

/*
provide a comma separated list of diagnostic mode enumeration specifiers
*/
#ifndef ISO14229_SERVER_USER_DIAGNOSTIC_MODES
#define ISO14229_SERVER_USER_DIAGNOSTIC_MODES
#endif