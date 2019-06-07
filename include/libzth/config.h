#ifndef __ZTH_CONFIG_H
#define __ZTH_CONFIG_H

#ifdef _DEBUG
#  undef NDEBUG
#endif
#if !defined(_DEBUG) && !defined(NDEBUG)
#  define _DEBUG
#endif

#ifndef ZTH_THREADS
#  define ZTH_THREADS 1
#endif

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
#else
#  error Unsupported compiler. Please use gcc.
#endif

#ifndef ZTH_TLS_DECLARE
#  undef ZTH_THREADS
#  define ZTH_THREAD 0
#  define ZTH_TLS_DECLARE(type,var)			extern type var;
#  define ZTH_TLS_DEFINE(type,var,init)		type var = init;
#  define ZTH_TLS_STATIC(type,var,init)		static type var = init;
#  define ZTH_TLS_SET(var,value)			var = value
#  define ZTH_TLS_GET(var)					var
#endif

#if defined(__x86_64__)
#  define ZTH_ARCH_X86_64 1
#elif defined(__i386__)
#  define ZTH_ARCH_X86 1
#else
#  error Unsupported hardware platform.
#endif

#if defined(_WIN32) || defined(__CYGWIN__)
#  define ZTH_OS_WINDOWS
#  define _WANT_IO_C99_FORMATS 1
#  define __USE_MINGW_ANSI_STDIO 1
#  define __STDC_FORMAT_MACROS
#  if defined(UNICODE) || defined(_UNICODE)
#    error Do not use UNICODE. Use ANSI with UTF-8 instead.
#  endif
#elif defined(__linux__)
#  define ZTH_OS_LINUX 1
#  define ZTH_HAVE_VALGRIND
#elif defined(__APPLE__)
#  include "TargetConditionals.h"
#  ifdef TARGET_OS_MAC
#    define ZTH_OS_MAC 1
#  else
#    error Unsupported Apple platform.
#  endif
#  define ZTH_HAVE_VALGRIND
#else
#  error Unsupported OS.
#endif

#if defined(_DEBUG) && defined(ZTH_HAVE_VALGRIND)
#  define ZTH_USE_VALGRIND
#endif

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




#ifdef __cplusplus
#include <cstddef>
#include <cstdint>

namespace zth {
	struct DefaultConfig {
		static bool const Debug = 
#ifndef NDEBUG
			true;
#else
			false;
#endif

		static bool const EnableAssert = 
#ifndef NDEBUG
			Debug;
#else
			false;
#endif
		
		static bool const EnableThreads =
#if ZTH_THREADS
			true;
#else
			false;
#endif

		static bool const EnableDebugPrint = Debug;
		static bool const EnableColorDebugPrint = true;
		static int const Print_banner = 12;
		static int const Print_worker = 8;
		static int const Print_fiber = 9;
		static int const Print_context = 10;
		static int const Print_list = 11;
		static int const Print_waiter = 13;
		static int const Print_sync = 14;

		static size_t const DefaultFiberStackSize = 0x20000;
		static bool const EnableStackGuard = Debug;
		static bool const ContextSignals = false;
		static double MinTimeslice_s() { return 1e-4; }
	};
} // namespace
#endif // __cplusplus

#include "zth_config.h"

#endif // __ZTH_CONFIG_H
