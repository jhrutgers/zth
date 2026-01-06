/*
 * SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#include <libzth/macros.h>

#include <libzth/init.h>
#include <libzth/time.h>
#include <libzth/util.h>
#include <cerrno>

#ifdef ZTH_OS_MAC
#  include <mach/mach_time.h>
#endif

#ifdef ZTH_OS_MAC

#  ifdef ZTH_CUSTOM_CLOCK_GETTIME
static mach_timebase_info_data_t clock_info;
static uint64_t mach_clock_start;

static void clock_global_init()
{
	mach_timebase_info(&clock_info);
	mach_clock_start = mach_absolute_time();
}
ZTH_INIT_CALL(clock_global_init)

int clock_gettime(int clk_id, struct timespec* res)
{
	if(unlikely(!res))
		return EFAULT;

	zth_assert(clk_id == CLOCK_MONOTONIC);
	uint64_t c = (mach_absolute_time() - mach_clock_start);
	uint64_t ns;
	if(unlikely(clock_info.numer != clock_info.denom)) // For all recent Mac OSX versions,
							   // mach_absolute_time() is in ns.
	{
		uint64_t chigh = (c >> 32U) * clock_info.numer;
		uint64_t chighrem = ((chigh % clock_info.denom) << 32U) / clock_info.denom;
		chigh /= clock_info.denom;
		uint64_t clow =
			(c & (((uint64_t)1 << 32U) - 1)) * clock_info.numer / clock_info.denom;
		clow += chighrem;
		ns = (chigh << 32U)
		     + clow; // 64-bit ns gives us more than 500 y before wrap-around.
	} else {
		ns = c;
	}

	// Split in sec + nsec
	res->tv_nsec = (long)(ns % zth::TimeInterval::BILLION);
	res->tv_sec = (time_t)(ns / zth::TimeInterval::BILLION);
	return 0;
}
#  endif // ZTH_CUSTOM_CLOCK_GETTIME

int clock_nanosleep(int clk_id, int flags, struct timespec const* request, struct timespec* remain)
{
	if(unlikely(!request))
		return EFAULT;
	if(unlikely(clk_id != CLOCK_MONOTONIC || flags != TIMER_ABSTIME))
		return EINVAL;

	struct timespec now;
	int res = clock_gettime(CLOCK_MONOTONIC, &now);
	if(unlikely(res))
		return res;

	if(now.tv_sec > request->tv_sec)
		// No need to sleep.
		return 0;

	time_t sec = request->tv_sec - now.tv_sec;
	if(sec == 0 && request->tv_nsec <= now.tv_nsec) {
		// No need to sleep.
		return 0;
	} else if(sec < std::numeric_limits<useconds_t>::max() / 1000000 - 1) {
		useconds_t us = (useconds_t)(sec * 1000000);
		us += (useconds_t)((request->tv_nsec - now.tv_nsec) / 1000);
		if(likely(!usleep(us)))
			return 0;
	} else {
		// Overflow, sleep (almost) infinitely.
		if(likely(!usleep(std::numeric_limits<useconds_t>::max())))
			return 0;
	}

	if(remain) {
		struct timespec intr;
		if((res = clock_gettime(CLOCK_MONOTONIC, &intr)))
			return res;

		remain->tv_sec = intr.tv_sec - now.tv_sec;
		remain->tv_nsec = intr.tv_nsec - now.tv_nsec;
		if(remain->tv_nsec < 0) {
			remain->tv_nsec += 1000000000L;
			remain->tv_sec--;
		}
	}

	return EINTR;
}
#endif

#ifdef ZTH_OS_WINDOWS
// When using gcc and MinGW, clock_nanosleep() only accepts CLOCK_REALTIME, but we use
// CLOCK_MONOTONIC.
int clock_nanosleep(
	clockid_t clock_id, int flags, const struct timespec* request, struct timespec* remain)
{
	if(unlikely(!request))
		return EFAULT;
	if(unlikely(clock_id != CLOCK_MONOTONIC || flags != TIMER_ABSTIME))
		return EINVAL;

	struct timespec t;
	int res = clock_gettime(CLOCK_MONOTONIC, &t);
	if(unlikely(res))
		return res;

	if(t.tv_sec > request->tv_sec)
		return 0;
	if(t.tv_sec == request->tv_sec && t.tv_nsec > request->tv_nsec)
		return 0;

	t.tv_sec = request->tv_sec - t.tv_sec;
	t.tv_nsec = request->tv_nsec - t.tv_nsec;
	if(t.tv_nsec < 0) {
		t.tv_nsec += 1000000000L;
		t.tv_sec--;
	}

	return nanosleep(&t, remain) ? errno : 0;
}
#endif

#ifdef ZTH_OS_BAREMETAL
// Clock functions are typically not provided by the std library.

// As there is no OS, just do polling for the clock. Please provide a more accurate one, when
// appropriate.
__attribute__((weak)) int clock_nanosleep(
	int clk_id, int flags, struct timespec const* request, struct timespec* UNUSED_PAR(remain))
{
	if(unlikely(!request))
		return EFAULT;
	if(unlikely(clk_id != CLOCK_MONOTONIC || flags != TIMER_ABSTIME))
		return EINVAL;

	while(true) {
		struct timespec now;
		int res = clock_gettime(CLOCK_MONOTONIC, &now);
		if(unlikely(res))
			return res;
		else if(now.tv_sec > request->tv_sec)
			return 0;
		else if(now.tv_sec == request->tv_sec && now.tv_nsec >= request->tv_nsec)
			return 0;
		// else just keep polling...
	}
}
#endif

#ifdef ZTH_OS_MAC
namespace zth {
zth::Timestamp startTime;
}

static void startTimeInit()
{
	zth::startTime = zth::Timestamp::now();
}
ZTH_INIT_CALL(startTimeInit)
#else  // !ZTH_OS_MAC
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
zth::Timestamp startTime_;

static void startTimeInit()
{
	startTime_ = zth::Timestamp::now();
}
ZTH_INIT_CALL(startTimeInit)

// Let the const zth::startTime alias to the non-const startTime_.
namespace zth {
extern Timestamp const __attribute__((alias("startTime_"))) startTime;
}
#endif // ZTH_OS_MAC
