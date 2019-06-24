#include <libzth/macros.h>
#include <libzth/time.h>

#ifdef ZTH_OS_MAC
#  include <mach/mach_time.h>
#endif

#ifdef ZTH_OS_MAC

static mach_timebase_info_data_t clock_info;
static uint64_t mach_clock_start;

static void clock_global_init() {
	mach_timebase_info(&clock_info);
	mach_clock_start = mach_absolute_time();
}
INIT_CALL(clock_global_init)

int clock_gettime(int clk_id, struct timespec* res) {
	if(unlikely(!res))
		return EFAULT;

	zth_assert(clk_id == CLOCK_MONOTONIC);
	uint64_t c = (mach_absolute_time() - mach_clock_start);
	uint64_t chigh = (c >> 32) * clock_info.numer;
	uint64_t chighrem = ((chigh % clock_info.denom) << 32) / clock_info.denom;
	chigh /= clock_info.denom;
	uint64_t clow = (c & (((uint64_t)1 << 32) - 1)) * clock_info.numer / clock_info.denom;
	clow += chighrem;
	uint64_t ns = (chigh << 32) + clow; // 64-bit ns gives us more than 500 y before wrap-around.

	// Split in sec + nsec
	res->tv_nsec = (long)(ns % zth::TimeInterval::BILLION);
	res->tv_sec = (time_t)(ns / zth::TimeInterval::BILLION);
	return 0;
}

int clock_nanosleep(int clk_id, int flags, struct timespec const* request, struct timespec* remain) {
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
		useconds_t us = sec * 1000000;
		us += (request->tv_nsec - now.tv_nsec) / 1000;
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
// When using gcc and MinGW, clock_nanosleep() only accepts CLOCK_REALTIME, but we use CLOCK_MONOTONIC.
int clock_nanosleep(clockid_t clock_id, int flags, const struct timespec *request, struct timespec *remain) {
	if(unlikely(!request))
		return EFAULT;
	if(unlikely(clk_id != CLOCK_MONOTONIC || flags != TIMER_ABSTIME))
		return EINVAL;
	
	struct timespec t;
	int res = clock_gettime(CLOCK_MONOTONIC, &t);
	if(unlikely(res))
		return res;

	if(t.tv_sec < request->tv_sec)
		return 0;
	if(t.tv_sec == request->tv_sec && t.tv_nsec < request->tv_nsec)
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

namespace zth {
	Timestamp const startTime(Timestamp::now());
}

