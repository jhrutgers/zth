#define _GNU_SOURCE
#include <zth>

#include <cstdlib>
#include <cstdio>
#include <cstdarg>
#include <unistd.h>

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

static bool config(char const* name, bool whenUnset) {
	char const* e = getenv(name);
	if(!e || !*e)
		return whenUnset;
	else if(strcmp(e, "0") == 0)
		// Explicitly set to disabled
		return false;
	else
		// Any other value is true
		return true;
}

bool config(int env, bool whenUnset) {
	switch(env) {
	case Env::EnableDebugPrint:			{ static bool const e = config("ZTH_CONFIG_ENABLE_DEBUG_PRINT", whenUnset); return e; }
	case Env::DoPerfEvent:				{ static bool const e = config("ZTH_CONFIG_DO_PERF_EVENT", whenUnset); return e; }
	default:
		return whenUnset;
	}
}

void zth_abort(char const* msg, ...)
{
	zth_log("\n%s  *** Zth ABORT:  ", zth::Config::EnableColorDebugPrint ? "\x1b[41;1;37;1m" : "");

	va_list va;
	va_start(va, msg);
	zth_logv(msg, va);
	va_end(va);

	zth_log("  ***  %s\n\n", zth::Config::EnableColorDebugPrint ? "\x1b[0m" : "");

	Backtrace().print();
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
#ifdef ZTH_OS_WINDOWS
	// Windows does not support line buffering.
	// If set anyway, this implies full buffering.
	// Only do that when we expect much debug output.
	if(zth_config(EnableDebugPrint)) {
#endif
		setvbuf(stdout, NULL, _IOLBF, 4096);
		setvbuf(stderr, NULL, _IOLBF, 4096);
#ifdef ZTH_OS_WINDOWS
	}
#endif
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

