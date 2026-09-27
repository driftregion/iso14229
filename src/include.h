#pragma once

#if !defined(__cplusplus) && ((!defined(__STDC_VERSION__)) || (__STDC_VERSION__ < 201112L))
#ifdef static_assert
#undef static_assert
#endif
#define static_assert(expr, msg)
#define _Static_assert(expr, msg)
#endif

#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
