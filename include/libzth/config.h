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
#    ifdef ZTH_CONFIG_DEBUG
		ZTH_CONFIG_DEBUG;
#    else
		true;
#    endif
#  else
		false;
#  endif

	/*! \brief When \c true, enable #zth_assert(). */
	static bool const EnableAssert =
#  ifndef NDEBUG
		Debug
#    ifdef ZTH_CONFIG_ENABLE_ASSERT
		&& ZTH_CONFIG_ENABLE_ASSERT
#    endif
		;
#  else
		false;
#  endif

	/*!
	 * \brief Show failing expression in case of a failed assert.
	 * \details Disable to reduce binary size.
	 */
	static bool const EnableFullAssert =
#  ifdef ZTH_CONFIG_ENABLE_FULL_ASSERT
		EnableAssert && ZTH_CONFIG_ENABLE_FULL_ASSERT;
#  elif defined(ZTH_OS_BAREMETAL)
		// Assume we are a bit short on memory.
		false;
#  else
			EnableAssert;
#  endif

	/*!
	 * \brief Enable backtrace support.
	 */
	static bool const EnableBacktrace =
#  ifdef ZTH_CONFIG_ENABLE_BACKTRACE
		ZTH_CONFIG_ENABLE_BACKTRACE;
#  else
		true;
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
#  ifdef ZTH_CONFIG_SUPPORT_DEBUG_PRINT
		Debug && ZTH_CONFIG_SUPPORT_DEBUG_PRINT;
#  elif defined(ZTH_OS_BAREMETAL)
		// Without OS, there is no environment to enable debugging when
		// it is not enabled right away.
		Debug && EnableDebugPrint;
#  else
			Debug;
#  endif

	/*! \brief Enable colored output. */
	static bool const EnableColorLog =
#  ifdef ZTH_CONFIG_ENABLE_COLOR_LOG
		ZTH_CONFIG_ENABLE_COLOR_LOG;
#  else
		true;
#  endif

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
	static int const Print_coro = 13;   // bright magenta

	/*! \brief Default fiber stack size in bytes. */
	static size_t const DefaultFiberStackSize =
#  ifdef ZTH_CONFIG_DEFAULT_FIBER_STACK_SIZE
		ZTH_CONFIG_DEFAULT_FIBER_STACK_SIZE;
#  elif defined(ZTH_OS_BAREMETAL)
		0x2000;
#  else
		0x20000;
#  endif

	/*! \brief When \c true, enable stack guards. */
	static bool const EnableStackGuard =
#  ifdef ZTH_CONFIG_ENABLE_STACK_GUARD
		ZTH_CONFIG_ENABLE_STACK_GUARD;
#  else
		Debug;
#  endif

	/*! \brief When \c true, enable stack watermark to detect maximum stack usage. */
	static bool const EnableStackWaterMark =
#  ifdef ZTH_CONFIG_ENABLE_STACK_WATER_MARK
		ZTH_CONFIG_ENABLE_STACK_WATER_MARK;
#  else
		Debug;
#  endif

	/*! \brief Take POSIX signal into account when doing a context switch. */
	static bool const ContextSignals =
#  if defined(ZTH_CONFIG_CONTEXT_SIGNALS) && !defined(ZTH_OS_BAREMETAL)
		ZTH_CONFIG_CONTEXT_SIGNALS;
#  else
		false;
#  endif

	/*! \brief Minimum time slice before zth::yield() actually yields. */
	constexpr static struct timespec MinTimeslice()
	{
#  ifdef ZTH_CONFIG_MIN_TIMESLICE
#    define ZTH_CONFIG_MIN_TIMESLICE_ ZTH_CONFIG_MIN_TIMESLICE
#  else
#    define ZTH_CONFIG_MIN_TIMESLICE_ 100000
#  endif
		ZTH_CONSTEXPR_RETURN(struct timespec, 0, ZTH_CONFIG_MIN_TIMESLICE_)
#  undef ZTH_CONFIG_MIN_TIMESLICE_
	}
	/*! \brief Print an overrun reported when this timeslice is exceeded. */
	constexpr static struct timespec TimesliceOverrunReportThreshold()
	{
#  ifdef ZTH_CONFIG_TIMESLICE_OVERRUN_REPORT_THRESHOLD
#    define ZTH_CONFIG_TIMESLICE_OVERRUN_REPORT_THRESHOLD_ \
	    ZTH_CONFIG_TIMESLICE_OVERRUN_REPORT_THRESHOLD
#  else
#    define ZTH_CONFIG_TIMESLICE_OVERRUN_REPORT_THRESHOLD_ 10000000
#  endif
		ZTH_CONSTEXPR_RETURN(
			struct timespec, 0, ZTH_CONFIG_TIMESLICE_OVERRUN_REPORT_THRESHOLD_)
#  undef ZTH_CONFIG_TIMESLICE_OVERRUN_REPORT_THRESHOLD_
	}

	/*! \brief Check time slice overrun at every context switch. */
	static bool const CheckTimesliceOverrun =
#  ifdef ZTH_CONFIG_CHECK_TIMESLICE_OVERRUN
		ZTH_CONFIG_CHECK_TIMESLICE_OVERRUN;
#  else
		Debug;
#  endif

	/*! \brief Save names for all #zth::Synchronizer instances. */
	static bool const NamedSynchronizer = SupportDebugPrint && Print_sync > 0;

	/*! \brief Buffer size for perf events. */
	static size_t const PerfEventBufferSize =
#  ifdef ZTH_CONFIG_PERF_EVENT_BUFFER_SIZE
		ZTH_CONFIG_PERF_EVENT_BUFFER_SIZE;
#  elif defined(ZTH_OS_BAREMETAL)
		0x400;
#  else
		0x4000;
#  endif

	/*! \brief Minimum remaining space before perf event collection is stopped. */
	static size_t const PerfEventBufferSpare =
		PerfEventBufferSize > 64 ? PerfEventBufferSize / 2 : 32;

	/*!
	 * \brief Record and output perf events to file automatically.
	 *
	 * Default to \c false, but can be overridden by \c ZTH_CONFIG_DO_PERF_EVENT environment
	 * variable.
	 *
	 * This only works for targets with a filesystem. The file can be overridden using
	 * \c ZTH_PERF_FILE environment variable. On targets without filesystem, call #perf_dump()
	 * manually.
	 */
	static bool const DoPerfEvent = false;

	/*! \brief Enable (but not necessarily record) perf. */
	static bool const EnablePerfEvent =
#  ifdef ZTH_CONFIG_ENABLE_PERF_EVENT
		ZTH_CONFIG_ENABLE_PERF_EVENT;
#  elif defined(ZTH_OS_BAREMETAL)
		false;
#  else
		true;
#  endif
	/*! \brief Also record syscalls by perf. */
	static bool const PerfSyscall =
#  ifdef ZTH_CONFIG_PERF_SYSCALL
		ZTH_CONFIG_PERF_SYSCALL;
#  else
		true;
#  endif

	/*! \brief Use named FSM guards/actions. */
	static bool const NamedFsm = Debug || (SupportDebugPrint && Print_fsm > 0);

	/*! \brief Use named objects. */
	static bool const NamedObjects =
		SupportDebugPrint || EnablePerfEvent || NamedSynchronizer || CheckTimesliceOverrun;

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

	/*! \brief Indicate if exceptions are supported. */
	static bool const EnableExceptions =
#  if defined(__cpp_exceptions) && !defined(ZTH_DISABLE_EXCEPTIONS)
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

#ifdef __cplusplus
namespace zth {

struct Check {
	enum {
		Config_Debug,
		Config_EnableAssert,
		Config_EnableFullAssert,
		Config_EnableThreads,
		Config_SupportDebugPrint,
		Config_EnableColorLog,
		Config_DefaultFiberStackSize,
		Config_EnableStackGuard,
		Config_EnableStackWaterMark,
		Config_ContextSignals,
		Config_CheckTimesliceOverrun,
		Config_PerfEventBufferSize,
		Config_EnablePerfEvent,
		Config_PerfSyscall,
		Config_UseZMQ,
		Config_UseLimitedFormatSpecifiers,
		Config_EnableExceptions,
	};
};

void checkConfig(int check /* one if Check::Config_... */, size_t value);

static inline void checkConfig()
{
	static bool checked;
	if(checked)
		return;

#  define ZTH_CHECK(x) checkConfig(Check::Config_##x, (size_t)Config::x);

	ZTH_CHECK(Debug)
	ZTH_CHECK(EnableAssert)
	ZTH_CHECK(EnableFullAssert)
	ZTH_CHECK(EnableThreads)
	ZTH_CHECK(SupportDebugPrint)
	ZTH_CHECK(EnableColorLog)
	ZTH_CHECK(DefaultFiberStackSize)
	ZTH_CHECK(EnableStackGuard)
	ZTH_CHECK(EnableStackWaterMark)
	ZTH_CHECK(ContextSignals)
	ZTH_CHECK(CheckTimesliceOverrun)
	ZTH_CHECK(PerfEventBufferSize)
	ZTH_CHECK(EnablePerfEvent)
	ZTH_CHECK(PerfSyscall)
	ZTH_CHECK(UseZMQ)
	ZTH_CHECK(UseLimitedFormatSpecifiers)
	ZTH_CHECK(EnableExceptions)

#  undef ZTH_CHECK

#  ifdef __cpp_exceptions
	static_assert(Config::EnableExceptions);
#  else
	static_assert(!Config::EnableExceptions);
#  endif

	checked = true;
}

} // namespace zth
#endif // __cplusplus

#endif // ZTH_CONFIG_H
