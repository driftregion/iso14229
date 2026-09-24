#pragma once
// non user-facing config

#if UDS_SYS == UDS_SYS_CUSTOM
#ifndef UDS_CUSTOM_MILLIS
#define UDS_CUSTOM_MILLIS
#endif // #ifndef UDS_CUSTOM_MILLIS
#endif // UDS_SYS == UDS_SYS_CUSTOM

#if UDS_SYS == UDS_SYS_WINDOWS
#ifdef _MSC_VER
#define strncasecmp _strnicmp
#define strcasecmp _stricmp
#endif // ifdef _MSC_VER
#endif // if UDS_SYS == UDS_SYS_WINDOWS

#if defined(UDS_TP_ISOTP_C_SOCKETCAN)
#ifndef UDS_TP_ISOTP_C
#define UDS_TP_ISOTP_C
#endif // #ifndef (UDS_TP_ISOTP_C)
#endif // #defined(UDS_TP_ISOTP_C_SOCKETCAN)

#if defined(UDS_TP_ISOTP_C_SOCKETCAN)
#ifndef UDS_TP_ISOTP_C
#error "UDS_TP_ISOTP_C must be defined to use UDS_TP_ISOTP_C_SOCKETCAN"
#endif // #ifndef UDS_TP_ISOTP_C
#endif // defined(UDS_TP_ISOTP_C_SOCKETCAN)

#if defined(UDS_TP_ISOTP_C)
#ifdef ISO_TP_USER_SEND_CAN_ARG
#error "this flag is set by iso14229"
#endif // #ifdef ISO_TP_USER_SEND_CAN_ARG
#define ISO_TP_USER_SEND_CAN_ARG 1

#ifdef ISO_TP_NO_FORMATTED_ERRORS
#error "this flag is set by iso14229"
#endif
#define ISO_TP_NO_FORMATTED_ERRORS 1
#endif // defined(UDS_TP_ISOTP_C)
