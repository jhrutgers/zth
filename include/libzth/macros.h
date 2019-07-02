#ifndef __ZTH_MACROS_H
#define __ZTH_MACROS_H
/*
 * Zth (libzth), a cooperative userspace multitasking library.
 * Copyright (C) 2019  Jochem Rutgers
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */



//////////////////////////////////////////////////
// Preamble
//

#ifdef _DEBUG
#  undef NDEBUG
#endif
#if !defined(_DEBUG) && !defined(NDEBUG)
#  define _DEBUG
#endif

#ifndef ZTH_THREADS
#  define ZTH_THREADS 1
#endif

#if !ZTH_THREADS && defined(ZTH_HAVE_LIBZMQ)
#  undef ZTH_THREADS
#  define ZTH_THREDS 1
#endif




//////////////////////////////////////////////////
// Compiler
//

#ifdef __GNUC__
// This is gcc
#  ifdef __cplusplus
#    if __cplusplus < 201103L
#      define decltype(x) __typeof__(x) // Well, not really true when references are involved...
#    endif
#  endif
#  if ZTH_THREADS
#    define ZTH_TLS_DECLARE(type,var)			extern __thread type var;
#    define ZTH_TLS_DEFINE(type,var,init)		__thread type var = init;
#    define ZTH_TLS_STATIC(type,var,init)		static __thread type var = init;
#    define ZTH_TLS_SET(var,value)				var = value
#    define ZTH_TLS_GET(var)					var
#  endif
#  ifndef _GNU_SOURCE
#    define _GNU_SOURCE
#  endif
#  define ZTH_ATTR_PRINTF	gnu_printf
#  ifndef GCC_VERSION
#    define GCC_VERSION (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
#  endif
#else
#  error Unsupported compiler. Please use gcc.
#endif

#ifndef ZTH_TLS_DECLARE
#  undef ZTH_THREADS
#  define ZTH_THREAD 0
#  define ZTH_TLS_DECLARE(type,var)				extern type var;
#  define ZTH_TLS_DEFINE(type,var,init)			type var = init;
#  define ZTH_TLS_STATIC(type,var,init)			static type var = init;
#  define ZTH_TLS_SET(var,value)				var = value
#  define ZTH_TLS_GET(var)						var
#endif

#ifndef ZTH_ATTR_PRINTF
#  define ZTH_ATTR_PRINTF	printf
#endif

#define __STDC_FORMAT_MACROS

#ifndef EXTERN_C
#  ifdef __cplusplus
#    define EXTERN_C extern "C"
#  else
#    define EXTERN_C
#  endif
#endif

#ifndef ZTH_EXPORT
#  define ZTH_EXPORT __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
#  ifdef ZTH_EXPORT_INLINE_EMIT
#    define ZTH_EXPORT_INLINE EXTERN_C ZTH_EXPORT
#  else
#    define ZTH_EXPORT_INLINE EXTERN_C ZTH_EXPORT __attribute__((gnu_inline)) inline
#  endif
#  define ZTH_EXPORT_INLINE_CPPONLY ZTH_EXPORT_INLINE
#  define ZTH_EXPORT_INLINE_CPPONLY_IMPL(...) __VA_ARGS__
#else
#  define ZTH_EXPORT_INLINE ZTH_EXPORT __attribute__((gnu_inline)) extern inline
#  define ZTH_EXPORT_INLINE_CPPONLY ZTH_EXPORT extern
#  define ZTH_EXPORT_INLINE_CPPONLY_IMPL(...) ;
#endif




//////////////////////////////////////////////////
// Hardware
//

#if defined(__x86_64__)
#  define ZTH_ARCH_X86_64 1
#elif defined(__i386__)
#  define ZTH_ARCH_X86 1
#else
#  error Unsupported hardware platform.
#endif





//////////////////////////////////////////////////
// OS
//

#if defined(_WIN32) || defined(__CYGWIN__)
#  define ZTH_OS_WINDOWS
#  define _WANT_IO_C99_FORMATS 1
#  define __USE_MINGW_ANSI_STDIO 1
#  if defined(UNICODE) || defined(_UNICODE)
#    error Do not use UNICODE. Use ANSI with UTF-8 instead.
#  endif
#  ifdef __CYGWIN__
#    define ZTH_HAVE_PTHREAD
#  endif
//#  define ZTH_HAVE_WINSOCK
#  ifdef ZTH_CONFIG_WRAP_IO
#    error ZTH_CONFIG_WRAP_IO is not supported on Windows.
#  endif
#elif defined(__linux__)
#  define ZTH_OS_LINUX 1
//#  define ZTH_HAVE_VALGRIND
#  define ZTH_HAVE_PTHREAD
//#  define ZTH_HAVE_LIBUNWIND
#  define ZTH_HAVE_POLL
#elif defined(__APPLE__)
#  include "TargetConditionals.h"
#  ifdef TARGET_OS_MAC
#    define ZTH_OS_MAC 1
#  else
#    error Unsupported Apple platform.
#  endif
//#  define ZTH_HAVE_VALGRIND
#  define ZTH_HAVE_PTHREAD
//#  define ZTH_HAVE_LIBUNWIND
#  define ZTH_HAVE_POLL
#else
#  error Unsupported OS.
#endif

#if defined(_DEBUG) && defined(ZTH_HAVE_VALGRIND)
#  define ZTH_USE_VALGRIND
#endif

#if !ZTH_THREAD
#  undef ZTH_HAVE_PTHREAD
#endif





//////////////////////////////////////////////////
// Context-switch approach
//

#ifdef ZTH_OS_WINDOWS
#  define ZTH_CONTEXT_WINFIBER
#elif defined(ZTH_HAVE_VALGRIND)
// Valgrind does not handle sigaltstack very well.
#  define ZTH_CONTEXT_UCONTEXT
#else
// Default approach.
//#  define ZTH_CONTEXT_SIGALTSTACK
#  define ZTH_CONTEXT_UCONTEXT
#endif

#ifdef ZTH_CONTEXT_WINFIBER
#  if WINVER < 0x0400 || _WIN32_WINNT < 0x0400
#    undef WINVER
#    undef _WIN32_WINNT
#    define WINVER 0x0400
#    define _WIN32_WINNT 0x0400
#  endif
#endif

#endif // __ZTH_MACROS_H
