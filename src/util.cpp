#include <zth>

#include <cstdlib>
#include <cstdio>
#include <cstdarg>

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

} // namespace

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
struct MachTimebaseInfo {
	MachTimebaseInfo()
	{
		mach_timebase_info(&info);
	}

	mach_timebase_info_data_t info;
};

static MachTimebaseInfo machTimebaseInfo;

int clock_gettime(int clk_id, struct timespec* res)
{
	if(unlikely(!res))
		return EINVAL;

	zth_assert(clk_id == CLOCK_MONOTONIC);
	uint64_t c = mach_absolute_time() * machTimebaseInfo.info.numer / machTimebaseInfo.info.denom;

	res->tv_sec = c / 1000000000l;
	res->tv_nsec = c % 1000000000l;
	return 0;
}
#endif
