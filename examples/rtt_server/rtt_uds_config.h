/**
 * @file rtt_uds_config.h
 * @brief Default configurations for the UDS library on RT-Thread.
 * @details This file provides default configurations for the UDS (Unified Diagnostic Services)
 *          library tailored for the RT-Thread operating system. These values can be
 *          overridden by defining them in the project's `rtconfig.h` or through the
 *          Kconfig system. This configuration is for the UDS library available at
 *          https://github.com/driftregion/iso14229.
 * @author wdfk-prog ()
 * @version 1.0
 * @date 2025-11-17
 * 
 * @copyright Copyright (c) 2025  
 * 
 * @note :
 * @par Change Log:
 * Date       Version Author      Description
 * 2025-11-17 1.0     wdfk-prog   first version
 */
#ifndef __RTT_UDS_CONFIG_H__
#define __RTT_UDS_CONFIG_H__

#include <rtthread.h>
#include <rtdevice.h>

/**
 * @brief Specifies the system type for the UDS library.
 * @details This macro is automatically defined to 5 (UDS_SYS_RTT) when the RT-Thread
 *          priority macro `RT_THREAD_PRIORITY_MAX` is detected, indicating that the
 *          operating system is RT-Thread. It allows the UDS library to adapt its
 *          internal workings for RT-Thread.
 */
#ifndef UDS_SYS
#ifdef RT_THREAD_PRIORITY_MAX
#define UDS_SYS 5 // UDS_SYS_RTT
#endif
#endif

/* ------------------- Logging Configuration ------------------- */

/**
 * @def UDS_LOG_LEVEL
 * @brief Defines the log level for the UDS library.
 * @details Sets the verbosity of the log output. The levels typically range from
 *          0 (Error) to 5 (Verbose). The default level is 3 (Info).
 */
#ifndef UDS_LOG_LEVEL
#define UDS_LOG_LEVEL 3
#endif

/**
 * @def UDS_RTTHREAD_ULOG_ENABLED
 * @brief Enables the use of RT-Thread's ULOG component.
 * @details If `RT_USING_ULOG` is defined in the project (meaning the ULOG component
 *          is available), this macro will also be defined to integrate the UDS
 *          library's logging with ULOG.
 */
#if defined(RT_USING_ULOG) && !defined(UDS_RTTHREAD_ULOG_ENABLED)
#define UDS_RTTHREAD_ULOG_ENABLED
#endif

#ifndef UDS_RTTHREAD_ULOG_ENABLED
/**
 * @def UDS_RTTHREAD_LOG_BUFFER_SIZE
 * @brief Log buffer size when not using ULOG.
 * @details This is only needed when ULOG is not used. It defines the size of the
 *          character buffer for formatting log messages.
 */
#ifndef UDS_RTTHREAD_LOG_BUFFER_SIZE
#define UDS_RTTHREAD_LOG_BUFFER_SIZE 256
#endif

/**
 * @def UDS_CONFIG_LOG_COLORS
 * @brief Enables or disables colored log output.
 * @details This is only applicable when ULOG is not used. Set to 1 to enable
 *          ANSI color codes in the log output for better readability.
 */
#ifndef UDS_CONFIG_LOG_COLORS
#define UDS_CONFIG_LOG_COLORS 1
#endif
#endif

/** 
 * @brief Size of the event dispatch table.
 * @details Corresponds to UDS_EVT_MAX + 1 to allow O(1) array indexing 
 *          based on the UDSEvent_t enum value.
 */
#ifndef UDS_RTT_EVENT_TABLE_SIZE
#define UDS_RTT_EVENT_TABLE_SIZE  (UDS_EVT_MAX + 1)
#endif

#endif /* __RTT_UDS_CONFIG_H__ */