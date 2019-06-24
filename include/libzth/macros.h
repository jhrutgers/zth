#ifndef __ZTH_MACROS_H
#define __ZTH_MACROS_H



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




//////////////////////////////////////////////////
// Compiler
//

#ifdef __GNUC__
// This is gcc
#  ifdef __cplusplus
#    if __cplusplus < 201103L
#      define decltype(x) typeof(x)
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
#  define __STDC_FORMAT_MACROS
#  if defined(UNICODE) || defined(_UNICODE)
#    error Do not use UNICODE. Use ANSI with UTF-8 instead.
#  endif
#  ifdef __CYGWIN__
#    define ZTH_HAVE_PTHREAD
#  endif
#  define ZTH_HAVE_WINSOCK
#elif defined(__linux__)
#  define ZTH_OS_LINUX 1
#  define ZTH_HAVE_VALGRIND
#  define ZTH_HAVE_PTHREAD
#  define ZTH_HAVE_LIBUNWIND
#elif defined(__APPLE__)
#  include "TargetConditionals.h"
#  ifdef TARGET_OS_MAC
#    define ZTH_OS_MAC 1
#  else
#    error Unsupported Apple platform.
#  endif
#  define ZTH_HAVE_VALGRIND
#  define ZTH_HAVE_PTHREAD
#  define ZTH_HAVE_LIBUNWIND
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
#  define ZTH_CONTEXT_SJLJ
//#  define ZTH_CONTEXT_UCONTEXT
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
