/*
 * wildargs.c
 * Automatic command-line wildcard expansion for
 * environments that aren't based on the Un*x shell.
 *
 * Copyright (C) 2003-2025 Cosmin Truta.
 *
 * Use, modification and distribution are subject
 * to the Boost Software License, Version 1.0.
 * See the accompanying file LICENSE_BSL_1_0.txt
 * or visit https://www.boost.org/LICENSE_1_0.txt
 *
 * SPDX-License-Identifier: BSL-1.0
 */

#if defined(_WIN64) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)

/*
 * Compiler and runtime detection on Windows.
 *
 * NOTE:
 * Some compilers for Windows claim to have a MSC version number
 * without actually being Microsoft C/C++ compilers. To ensure a
 * proper detection of the actual runtime, the MSC version checks
 * should be performed last.
 */
#if defined(__BORLANDC__)
#define WILDARGS_WINDOWS_BORLANDRTL
#elif defined(__MINGW64__) || defined(__MINGW32__)
#define WILDARGS_WINDOWS_MINGW
#elif defined(_MSC_VER) && (_MSC_VER >= 1900)
#define WILDARGS_WINDOWS_VCRUNTIME
#elif defined(_MSC_VER) && (_MSC_VER >= 800)
#define WILDARGS_WINDOWS_MSVCRT
#endif

#endif

#if defined(WILDARGS_WINDOWS_BORLANDRTL)

/*
 * Automatic wildargs expansion for the Borland Runtime Library (RTL).
 * The implementation is inspired from BMP2PNG by MIYASAKA Masaru.
 */
#include <wildargs.h>
typedef void _RTLENTRY (* _RTLENTRY _argv_expand_fn)(char *, _PFN_ADDARG);
typedef void _RTLENTRY (* _RTLENTRY _wargv_expand_fn)(wchar_t *, _PFN_ADDARG);
_argv_expand_fn _argv_expand_ptr = _expand_wild;
_wargv_expand_fn _wargv_expand_ptr = _wexpand_wild;

#elif defined(WILDARGS_WINDOWS_MINGW) || defined(WILDARGS_WINDOWS_MSVCRT)

/*
 * Automatic wildargs expansion for MinGW32, MinGW-w64 and MSVCRT.
 * The implementation is inspired from MinGW32 by Colin Peters.
 */
int _dowildcard = 1;

#elif defined(WILDARGS_WINDOWS_VCRUNTIME)

/*
 * Automatic wildargs expansion for VCRUNTIME, used by the modern
 * versions of Microsoft Visual C++ (from Visual C++ 2015 onwards).
 * The implementation is inspired from the VCRUNTIME source code.
 */
#include <vcruntime_startup.h>
_crt_argv_mode __CRTDECL _get_startup_argv_mode(void)
{ return _crt_argv_expanded_arguments; }

#else

/*
 * Dummy declaration for a guaranteed non-empty translation unit.
 */
#ifndef __cplusplus
typedef int wildargs_dummy_t;
#endif

#endif
