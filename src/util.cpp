#define _GNU_SOURCE
#include <zth>

#include <cstdlib>
#include <cstdio>
#include <cstdarg>
#include <unistd.h>

#ifdef ZTH_OS_MAC
#  include <mach/mach_time.h>
#endif

using namespace std;

namespace zth {

char const* banner() {
	return "Zth " ZTH_VERSION
#ifdef __GNUC__
		" g++-" ZTH_STRINGIFY(__GNUC__) "." ZTH_STRINGIFY(__GNUC_MINOR__) "." ZTH_STRINGIFY(__GNUC_PATCHLEVEL__)
#endif
#ifdef ZTH_OS_LINUX
		" linux"
#endif
#ifdef ZTH_OS_MAC
		" mac"
#endif
#ifdef ZTH_ARCH_X86_64
		" x86_64"
#endif
#ifdef ZTH_ARCH_X86
		" x86"
#endif
#ifdef _DEBUG
		" debug"
#endif
#if ZTH_THREADS
		" threads"
#endif
#ifdef ZTH_CONTEXT_UCONTEXT
		" ucontext"
#endif
#ifdef ZTH_CONTEXT_SIGALTSTACK
		" sigaltstack"
#endif
#ifdef ZTH_CONTEXT_WINFIBER
		" winfiber"
#endif
		;
}

void zth_abort(char const* msg, ...)
{
	zth_log("\n%s  *** Zth ABORT:  ", zth::Config::EnableColorDebugPrint ? "\x1b[41;1;37;1m" : "");

	va_list va;
	va_start(va, msg);
	zth_logv(msg, va);
	va_end(va);

	zth_log("  ***  %s\n\n", zth::Config::EnableColorDebugPrint ? "\x1b[0m" : "");

	abort();
}

template <typename T, size_t S = sizeof(T)> struct as_uint64_t { };
template <typename T> struct as_uint64_t<T,1> { static uint64_t value(T x) { return *reinterpret_cast<uint8_t*>(&x); } };
template <typename T> struct as_uint64_t<T,2> { static uint64_t value(T x) { return *reinterpret_cast<uint16_t*>(&x); } };
template <typename T> struct as_uint64_t<T,4> { static uint64_t value(T x) { return *reinterpret_cast<uint32_t*>(&x); } };
template <typename T> struct as_uint64_t<T,8> { static uint64_t value(T x) { return *reinterpret_cast<uint64_t*>(&x); } };

#ifdef ZTH_HAVE_PTHREAD
std::string threadId(pthread_t p)
#else
std::string threadId(pid_t p)
#endif
{
	char buf[32];
	return snprintf(buf, sizeof(buf), "0x%" PRIx64, as_uint64_t<decltype(p)>::value(p)) > 0 ? buf : "??";
}

std::string format(char const* fmt, ...)
{
	std::string res;
	char* buf = NULL;
	va_list args;
	va_start(args, fmt);
	if(vasprintf(&buf, fmt, args) > 0) {
		res = buf;
		free(buf);
	}
	va_end(args);
	return res;
}

} // namespace

static void zth_log_init() {
	setvbuf(stdout, NULL, _IOLBF, 4096);
	setvbuf(stderr, NULL, _IOLBF, 4096);
}
INIT_CALL(zth_log_init)

void zth_color_log(int color, char const* fmt, ...)
{
#ifdef _WIN32
	bool do_color = false;
#else
	static bool do_color = isatty(fileno(stdout));
#endif

	if(do_color)
		zth_log("\x1b[%d%sm", (color % 8) + 30, color >= 8 ? ";1" : "");

	va_list args;
	va_start(args, fmt);
	zth_logv(fmt, args);
	va_end(args);
	
	if(do_color)
		zth_log("\x1b[0m");
}

void zth_log(char const* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	zth_logv(fmt, args);
	va_end(args);
}

// weak symbol
void zth_logv(char const* fmt, va_list arg)
{
	vprintf(fmt, arg);
}

#ifdef ZTH_OS_MAC

static mach_timebase_info_data_t clock_info;
static uint64_t mach_clock_start;

static void clock_global_init()
{
	mach_timebase_info(&clock_info);
	mach_clock_start = mach_absolute_time();
}
INIT_CALL(clock_global_init)

int clock_gettime(int clk_id, struct timespec* res)
{
	if(unlikely(!res))
		return EFAULT;


	zth_assert(clk_id == CLOCK_MONOTONIC);
	uint64_t c = (mach_absolute_time() - mach_clock_start);
	uint64_t chigh = (c >> 32) * clock_info.numer;
	uint64_t chighrem = ((chigh % clock_info.denom) << 32) / clock_info.denom;
	chigh /= clock_info.denom;
	uint64_t clow = (c & (((uint64_t)1 << 32) - 1)) * clock_info.numer / clock_info.denom;
	clow += chighrem;
	uint64_t ns = chigh + clow; // 64-bit ns gives us more than 500 y before wrap-around.

	// Split in sec + nsec
	res->tv_nsec = (long)(ns % zth::TimeInterval::BILLION);
	res->tv_sec = (time_t)(ns / zth::TimeInterval::BILLION);
	return 0;
}

int clock_nanosleep(int clk_id, int flags, struct timespec const* request, struct timespec* remain)
{
	if(!request)
		return EFAULT;

	zth_assert(clk_id == CLOCK_MONOTONIC);
	zth_assert(flags == TIMER_ABSTIME);

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
		usleep(us);
	} else {
		// Overflow, sleep (almost) infinitely.
		usleep(std::numeric_limits<useconds_t>::max());
	}

	return 0;
}
#endif
