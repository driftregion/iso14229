#pragma once

#if !defined(__cplusplus) && ((!defined(__STDC_VERSION__)) || (__STDC_VERSION__ < 201112L))
#ifndef static_assert
/* cppcheck-suppress [misra-c2012-19.2,misra-c2012-20.4] Patch static_assert for pre-C11 toolchains
 */
#define static_assert(expr, msg)
#else // #ifndef static_assert
#warning "using static_assert of unknown provenance; Please check that the signature is correct"
#endif // #ifndef static_assert
#endif

#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
