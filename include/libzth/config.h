#ifndef __ZTH_CONFIG_H
#define __ZTH_CONFIG_H

#include <libzth/macros.h>

#ifdef __cplusplus
#include <stddef.h>
#include <stdint.h>

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
		static bool const EnableColorDebugPrint =
#ifdef _WIN32
			// Default Windows cmd prompt does not support colors.
			false;
#else
			true;
#endif

		static int const Print_banner = 12;
		static int const Print_worker = 8;
		static int const Print_fiber = 9;
		static int const Print_context = 10;
		static int const Print_list = 11;
		static int const Print_waiter = 13;
		static int const Print_sync = 14;
		static int const Print_perf = 15;

		static size_t const DefaultFiberStackSize = 0x20000;
		static bool const EnableStackGuard = Debug;
		static bool const ContextSignals = false;
		static double MinTimeslice_s() { return 1e-4; }

		static size_t const PerfEventBufferSize = 128;
		static size_t const PerfEventBufferThresholdToTriggerVCDWrite = PerfEventBufferSize / 2;
		static size_t const PerfVCDFileBuffer = 0x1000;

		static bool const PerfVCD = true;
		static bool const PerfTrackFiberState = PerfVCD;
	};
} // namespace
#endif // __cplusplus

#include "zth_config.h"

#endif // __ZTH_CONFIG_H
