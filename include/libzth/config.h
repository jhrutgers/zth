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
	static bool const Debug =
#	ifndef NDEBUG
		true;
#	else
		false;
#	endif

	static bool const EnableAssert =
#	ifndef NDEBUG
		Debug;
#	else
		false;
#	endif

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

	static size_t const DefaultFiberStackSize = 0x20000;
	static bool const EnableStackGuard = Debug;
	static bool const EnableStackWaterMark = Debug;
	static bool const ContextSignals = false;
	constexpr static double MinTimeslice_s()
	{
		return 1e-4;
	}
	static int const TimesliceOverrunFactorReportThreshold = 4;
	static bool const CheckTimesliceOverrun = Debug;
	static bool const NamedSynchronizer = SupportDebugPrint && Print_sync > 0;

	static size_t const PerfEventBufferSize = 128;
	static size_t const PerfEventBufferThresholdToTriggerVCDWrite = PerfEventBufferSize / 2;
	static size_t const PerfVCDFileBuffer = 0x1000;

	static bool const EnablePerfEvent = true;
	static bool const DoPerfEvent = false;
	static bool const PerfSyscall = true;

	static bool const NamedFsm = Debug || (EnableDebugPrint && Print_fsm > 0);

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
