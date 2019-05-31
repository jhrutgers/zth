#ifndef __ZTH_UTIL_H
#define __ZTH_UTIL_H

#define ZTH_STRINGIFY_(x) #x
#define ZTH_STRINGIFY(x) ZTH_STRINGIFY_(x)

#ifndef likely
#  ifdef __GNUC__
#    define likely(expr) __builtin_expect(!!(expr), 1)
#  else
#    define likely(expr) (expr)
#  endif
#endif
#ifndef unlikely
#  ifdef __GNUC__
#    define unlikely(expr) __builtin_expect((expr), 0)
#  else
#    define unlikely(expr) (expr)
#  endif
#endif

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif
void zth_log(char const* fmt, ...) __attribute__((format(printf, 1, 2)));
void zth_logv(char const* fmt, va_list arg) __attribute__((weak));
#ifdef __cplusplus
} // extern "C"
#endif

#ifdef __cplusplus
#include <string>
#include <pthread.h>

#define zth_dbg(group, msg, a...) \
	do { \
		if(::zth::Config::EnableDebugPrint && ::zth::Config::Print_##group != 0) { \
			if(::zth::Config::EnableColorDebugPrint) \
				zth_log("\x1b[%d%sm > zth::" ZTH_STRINGIFY(group) ": " msg "\x1b[0m\n", \
					(::zth::Config::Print_##group % 8) + 30, \
					::zth::Config::Print_##group >= 8 ? ";1" : "", ##a); \
			else \
				zth_log(" > zth::" ZTH_STRINGIFY(group) ": " msg "\n", ##a); \
		} \
	} while(0)

#ifndef NDEBUG
	#define zth_assert(expr) do { if(unlikely(::zth::Config::EnableAssert && !(expr))) \
		::zth::zth_abort("assertion failed at " __FILE__ ":" ZTH_STRINGIFY(__LINE__) ": " ZTH_STRINGIFY(expr)); } while(false)
#else
	#define zth_assert(...)  ((void)0)
#endif

namespace zth {
	char const* banner();
    void zth_abort(char const* msg, ...) __attribute__((format(printf, 1, 2), noreturn));
	std::string pthreadId(pthread_t p = pthread_self());
	std::string format(char const* fmt, ...) __attribute__((format(printf, 1, 2)));
}

#endif // __cplusplus
#endif // __ZTH_UTIL_H
