#ifndef ZTH_CONFIG_H
#define ZTH_CONFIG_H
/*
 * SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
 *
 * SPDX-License-Identifier: MPL-2.0
 */

/*!
 * \defgroup zth_api_cpp_config config
 * \ingroup zth_api_cpp
 */

#include <libzth/macros.h>

#ifdef __cplusplus
#  include <cstddef>
#  include <sys/time.h>

#  if __cplusplus >= 201103L
#    include <cstdint>
#  else
#    include <stdint.h>
#  endif

#  include <memory>

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
#  define zth_config(name) (::zth::config(::zth::Env::name, ::zth::Config::name))

#  if __cplusplus < 201103L
#    define ZTH_CONSTEXPR_RETURN(type, ...) \
	    type x = {__VA_ARGS__};         \
	    return x;
#  else // C++11 and up
#    define ZTH_CONSTEXPR_RETURN(type, ...) return {__VA_ARGS__};
#  endif // C++11 and up

struct DefaultConfig {
	/*! \brief This is a debug build when set to \c true. */
	static bool const Debug =
#  ifndef NDEBUG
		true;
#  else
		false;
#  endif

	/*! \brief When \c true, enable #zth_assert(). */
	static bool const EnableAssert =
#  ifndef NDEBUG
		Debug;
#  else
		false;
#  endif

	/*!
	 * \brief Show failing expression in case of a failed assert.
	 * \details Disable to reduce binary size.
	 */
	static bool const EnableFullAssert =
#  ifdef ZTH_OS_BAREMETAL
		// Assume we are a bit short on memory.
		false;
#  else
		EnableAssert;
#  endif

	/*! \brief Add (Worker) thread support when \c true. */
	static bool const EnableThreads =
#  if ZTH_THREADS
		true;
#  else
		false;
#  endif

	/*!
	 * \brief Actually do print the debug output.
	 *
	 * Needs #SupportDebugPrint to be \c true.  Can be overridden by
	 * \c ZTH_CONFIG_ENABLE_DEBUG_PRINT environment variable.
	 */
	static bool const EnableDebugPrint =
#  ifdef ZTH_CONFIG_ENABLE_DEBUG_PRINT
		ZTH_CONFIG_ENABLE_DEBUG_PRINT;
#  else
		false;
#  endif

	/*!
	 * \brief Add support to enable debug output prints.
	 *
	 * The output is only actually printed when #EnableDebugPrint is \c true.
	 */
	static bool const SupportDebugPrint =
#  ifdef ZTH_OS_BAREMETAL
		// Without OS, there is no environment to enable debugging when
		// it is not enabled right away.
		Debug && EnableDebugPrint;
#  else
		Debug;
#  endif

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
	constexpr static struct timespec MinTimeslice()
	{
		ZTH_CONSTEXPR_RETURN(struct timespec, 0, 100000)
	}
	/*! \brief Print an overrun reported when this timeslice is exceeded. */
	constexpr static struct timespec TimesliceOverrunReportThreshold()
	{
		ZTH_CONSTEXPR_RETURN(struct timespec, 0, 10000000)
	}

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

	/*! \brief Record and output perf events. */
	static bool const DoPerfEvent = false;
	/*! \brief Enable (but not necessarily record) perf. */
	static bool const EnablePerfEvent =
#  ifdef ZTH_OS_BAREMETAL
		// No environment, so only enable when we are actually saving it.
		DoPerfEvent;
#  else
		true;
#  endif
	/*! \brief Also record syscalls by perf. */
	static bool const PerfSyscall = true;

	/*! \brief Use named FSM guards/actions. */
	static bool const NamedFsm = Debug || (EnableDebugPrint && Print_fsm > 0);

	/*! \brief Enable ZeroMQ support. */
	static bool const UseZMQ =
#  ifdef ZTH_HAVE_LIBZMQ
		true;
#  else
		false;
#  endif

	/*! \brief Use limited formatting specifiers. */
	static bool const UseLimitedFormatSpecifiers =
#  if defined(ZTH_FORMAT_LIMITED) && ZTH_FORMAT_LIMITED
		true;
#  else
		false;
#  endif

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

#undef ZTH_CONSTEXPR_RETURN
#endif // ZTH_CONFIG_H
