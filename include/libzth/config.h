#ifndef __ZTH_CONFIG_H
#define __ZTH_CONFIG_H

#ifdef _DEBUG
#  undef NDEBUG
#endif
#if !defined(_DEBUG) && !defined(NDEBUG)
#  define _DEBUG
#endif

#ifdef __GNUC__
	// This is gcc
#else
#  error Unsupported compiler. Please use gcc.
#endif

#if defined(__x86_64__)
#  define ZTH_ARCH_X86_64 1
#elif defined(__i386__)
#  define ZTH_ARCH_X86 1
#else
#  error Unsupported hardware platform.
#endif

#if defined(__linux__)
#  define ZTH_OS_LINUX 1
#elif defined(__APPLE__)
#  include "TargetConditionals.h"
#  ifdef TARGET_OS_MAC
#    define ZTH_OS_MAC 1
#  else
#    error Unsupported Apple platform.
#  endif
#else
#  error Unsupported OS.
#endif

#ifdef __cplusplus
namespace zth {
	struct DefaultConfig {
		static const bool Debug = 
#ifndef NDEBUG
			true;
#else
			false;
#endif

		static const bool EnableAssert = 
#ifndef NDEBUG
			Debug;
#else
			false;
#endif

		static const bool EnableDebugPrint = Debug;
		static const bool EnableColorDebugPrint = true;
		static int const Print_banner = 12;
	};
} // namespace
#endif // __cplusplus

#include "zth_config.h"

#endif // __ZTH_CONFIG_H
