#ifndef __ZTH_UTIL_H
#define __ZTH_UTIL_H

#include <libzth/macros.h>

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
#    define unlikely(expr) __builtin_expect(!!(expr), 0)
#  else
#    define unlikely(expr) (expr)
#  endif
#endif

#include <assert.h>
#if __cplusplus < 201103L && !defined(static_assert)
#  define static_assert(expr, msg)	typedef int static_assert_[(expr) ? 1 : -1]
#endif

#define INIT_CALL(f)	struct f##__init { f##__init() { f(); } }; static f##__init f##__init_;

// Make a FOREACH macro
#define FOREACH_0(WHAT)
#define FOREACH_1(WHAT, X)			WHAT(X)
#define FOREACH_2(WHAT, X, ...)		WHAT(X)FOREACH_1(WHAT, __VA_ARGS__)
#define FOREACH_3(WHAT, X, ...)		WHAT(X)FOREACH_2(WHAT, __VA_ARGS__)
#define FOREACH_4(WHAT, X, ...)		WHAT(X)FOREACH_3(WHAT, __VA_ARGS__)
#define FOREACH_5(WHAT, X, ...)		WHAT(X)FOREACH_4(WHAT, __VA_ARGS__)
#define FOREACH_6(WHAT, X, ...)		WHAT(X)FOREACH_5(WHAT, __VA_ARGS__)
#define FOREACH_7(WHAT, X, ...)		WHAT(X)FOREACH_6(WHAT, __VA_ARGS__)
#define FOREACH_8(WHAT, X, ...)		WHAT(X)FOREACH_7(WHAT, __VA_ARGS__)
#define FOREACH_9(WHAT, X, ...)		WHAT(X)FOREACH_8(WHAT, __VA_ARGS__)
#define FOREACH_10(WHAT, X, ...)	WHAT(X)FOREACH_9(WHAT, __VA_ARGS__)
//... repeat as needed

#define _GET_MACRO(_0,_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,NAME,...) NAME
#define FOREACH(action,...) \
    _GET_MACRO(0,##__VA_ARGS__,FOREACH_10,FOREACH_9,FOREACH_8,FOREACH_7,FOREACH_6,FOREACH_5,FOREACH_4,FOREACH_3,FOREACH_2,FOREACH_1,FOREACH_0)(action,##__VA_ARGS__)

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif
void zth_color_log(int color, char const* fmt, ...) __attribute__((format(ZTH_ATTR_PRINTF, 2, 3)));
void zth_log(char const* fmt, ...) __attribute__((format(ZTH_ATTR_PRINTF, 1, 2)));
void zth_logv(char const* fmt, va_list arg) __attribute__((weak));
#ifdef __cplusplus
} // extern "C"
#endif

#ifdef __cplusplus
#include <string>
#include <pthread.h>
#include <memory>

#ifdef ZTH_HAVE_PTHREAD
#  include <pthread.h>
#else
#  include <sys/types.h>
#  include <unistd.h>
#endif

#define zth_dbg(group, msg, a...) \
	do { \
		if(::zth::Config::EnableDebugPrint && ::zth::Config::Print_##group != 0) { \
			if(::zth::Config::EnableColorDebugPrint) \
				zth_color_log(::zth::Config::Print_##group, " > zth::" ZTH_STRINGIFY(group) ": " msg "\n", ##a); \
			else \
				zth_log(" > zth::" ZTH_STRINGIFY(group) ": " msg "\n", ##a); \
		} \
	} while(0)

#ifndef NDEBUG
	#define zth_assert(expr) do { if(unlikely(::zth::Config::EnableAssert && !(expr))) \
		::zth::zth_abort("assertion failed at " __FILE__ ":" ZTH_STRINGIFY(__LINE__) ": " ZTH_STRINGIFY(expr)); } while(false)
#else
	#define zth_assert(...)
#endif

namespace zth {
	char const* banner();
    void zth_abort(char const* msg, ...) __attribute__((format(ZTH_ATTR_PRINTF, 1, 2), noreturn));
#ifdef ZTH_HAVE_PTHREAD
	std::string threadId(pthread_t p = pthread_self());
#else
	std::string threadId(pid_t p = getpid());
#endif

	std::string format(char const* fmt, ...) __attribute__((format(ZTH_ATTR_PRINTF, 1, 2)));
}

#endif // __cplusplus
#endif // __ZTH_UTIL_H
