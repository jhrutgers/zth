#ifndef ZTH_CONFIG_H
#define ZTH_CONFIG_H
/*
 * Zth (libzth), a cooperative userspace multitasking library.
 * Copyright (C) 2019-2021  Jochem Rutgers
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

/*!
 * \defgroup zth_api_cpp_config config
 * \ingroup zth_api_cpp
 */

#include <libzth/macros.h>

#ifdef __cplusplus
#	include <cstddef>

#	if __cplusplus >= 201103L
#		include <cstdint>
#	else
#		include <stdint.h>
#	endif

#	include <memory>

namespace zth {

struct Env {
	enum { EnableDebugPrint, DoPerfEvent, PerfSyscall, CheckTimesliceOverrun };
};

bool config(int env /* one of Env::* */, bool whenUnset);

/*!
 * \brief Checks if the given zth::Config field is enabled.
 * \details This function checks both the set zth::Config value and the environment.
 * \param name the field name within zth::Config (without zth::Config prefix)
 * \return a bool, indicating if the field is enabled
 * \ingroup zth_api_cpp_config
 * \hideinitializer
 */
#	define zth_config(name) (::zth::config(::zth::Env::name, ::zth::Config::name))

struct DefaultConfig {
	/*! \brief This is a debug build when set to \c true. */
	static bool const Debug =
#	ifndef NDEBUG
		true;
#	else
		false;
#	endif

	/*! \brief When \c true, enable #zth_assert(). */
	static bool const EnableAssert =
#	ifndef NDEBUG
		Debug;
#	else
		false;
#	endif

	/*!
	 * \brief Show failing expression in case of a failed assert.
	 * \details Disable to reduce binary size.
	 */
	static bool const EnableFullAssert =
#	ifdef ZTH_OS_BAREMETAL
		// Assume we are a bit short on memory.
		false;
#	else
		EnableAssert;
#	endif

	/*! \brief Add (Worker) thread support when \c true. */
	static bool const EnableThreads =
#	if ZTH_THREADS
		true;
#	else
		false;
#	endif

	/*!
	 * \brief Add support to enable debug output prints.
	 *
	 * The output is only actually printed when #EnableDebugPrint is \c true.
	 */
	static bool const SupportDebugPrint = Debug;

	/*!
	 * \brief Actually do print the debug output.
	 *
	 * Needs #SupportDebugPrint to be \c true.  Can be overridden by
	 * \c ZTH_CONFIG_ENABLE_DEBUG_PRINT environment variable.
	 */
	static bool const EnableDebugPrint =
#	ifdef ZTH_CONFIG_ENABLE_DEBUG_PRINT
		ZTH_CONFIG_ENABLE_DEBUG_PRINT;
#	else
		false;
#	endif

	/*! \brief Enable colored output. */
	static bool const EnableColorLog = true;

	/*!
	 * \brief ANSI color used by #zth_dbg().
	 * Printing this category is disabled when set to 0.
	 */
	static int const Print_banner = 12; // bright blue
	static int const Print_worker = 5;  // magenta
	static int const Print_waiter = 1;  // red
	static int const Print_io = 9;	    // bright red
	static int const Print_perf = 6;    // cyan
	static int const Print_fiber = 10;  // bright green
	static int const Print_context = 2; // green
	static int const Print_sync = 11;   // bright yellow
	static int const Print_list = 8;    // bright black
	static int const Print_zmq = 9;	    // bright red
	static int const Print_fsm = 14;    // bright cyan
	static int const Print_thread = 3;  // yellow

	/*! \brief Default fiber stack size in bytes. */
	static size_t const DefaultFiberStackSize = 0x20000;
	/*! \brief When \c true, enable stack guards. */
	static bool const EnableStackGuard = Debug;
	/*! \brief When \c true, enable stack watermark to detect maximum stack usage. */
	static bool const EnableStackWaterMark = Debug;
	/*! \brief Take POSIX signal into account when doing a context switch. */
	static bool const ContextSignals = false;
	/*! \brief Minimum time slice before zth::yield() actually yields. */
	constexpr static double MinTimeslice_s()
	{
		return 1e-4;
	}

	/*! \brief Print an overrun reported when MinTimeslice_s() is exceeded by this factor. */
	static int const TimesliceOverrunFactorReportThreshold = 4;
	/*! \brief Check time slice overrun at every context switch. */
	static bool const CheckTimesliceOverrun = Debug;
	/*! \brief Save names for all #zth::Synchronizer instances. */
	static bool const NamedSynchronizer = SupportDebugPrint && Print_sync > 0;

	/*! \brief Buffer size for perf events. */
	static size_t const PerfEventBufferSize = 128;
	/*! \brief Threshold when to force writing out VCD buffer. */
	static size_t const PerfEventBufferThresholdToTriggerVCDWrite = PerfEventBufferSize / 2;
	/*! \brief VCD file buffer in bytes. */
	static size_t const PerfVCDFileBuffer = 0x1000;

	/*! \brief Enable (but not necessarily record) perf. */
	static bool const EnablePerfEvent = true;
	/*! \brief Record and output perf events. */
	static bool const DoPerfEvent = false;
	/*! \brief Also record syscalls by perf. */
	static bool const PerfSyscall = true;

	/*! \brief Use named FSM guards/actions. */
	static bool const NamedFsm = Debug || (EnableDebugPrint && Print_fsm > 0);

	/*! \brief Enable ZeroMQ support. */
	static bool const UseZMQ =
#	ifdef ZTH_HAVE_LIBZMQ
		true;
#	else
		false;
#	endif

	/*!
	 * \brief Allocator type.
	 *
	 * \c Config::Allocator<int>::type is an allocator for ints.
	 * \c Config::Allocator<int> behaves like std::allocator_traits<Alloc>
	 *
	 */
	template <typename T>
	struct Allocator {
		typedef std::allocator<T> type;
	};
};
} // namespace zth
#endif // __cplusplus

#include "zth_config.h"

#endif // ZTH_CONFIG_H
