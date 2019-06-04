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

#define INIT_CALL(f)	struct f##__init { f##__init() { f(); } }; static f##__init f##__init_;

// Make a FOREACH macro
#define FOREACH_1(WHAT, X)       WHAT(X)
#define FOREACH_2(WHAT, X, ...)  WHAT(X)FOREACH_1(WHAT, __VA_ARGS__)
#define FOREACH_3(WHAT, X, ...)  WHAT(X)FOREACH_2(WHAT, __VA_ARGS__)
#define FOREACH_4(WHAT, X, ...)  WHAT(X)FOREACH_3(WHAT, __VA_ARGS__)
#define FOREACH_5(WHAT, X, ...)  WHAT(X)FOREACH_4(WHAT, __VA_ARGS__)
#define FOREACH_6(WHAT, X, ...)  WHAT(X)FOREACH_5(WHAT, __VA_ARGS__)
#define FOREACH_7(WHAT, X, ...)  WHAT(X)FOREACH_6(WHAT, __VA_ARGS__)
#define FOREACH_8(WHAT, X, ...)  WHAT(X)FOREACH_7(WHAT, __VA_ARGS__)
#define FOREACH_9(WHAT, X, ...)  WHAT(X)FOREACH_8(WHAT, __VA_ARGS__)
#define FOREACH_10(WHAT, X, ...) WHAT(X)FOREACH_9(WHAT, __VA_ARGS__)
//... repeat as needed

#define GET_MACRO(_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,NAME,...) NAME
#define FOR_EACH(action,...) \
    GET_MACRO(__VA_ARGS__,FOREACH_10,FOREACH_9,FOREACH_8,FOREACH_7,FOREACH_6,FOREACH_5,FOREACH_4,FOREACH_3,FOREACH_2,FOREACH_1)(action,__VA_ARGS__)

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
#include <memory>

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

#if __cplusplus < 201103L
#  define zth_auto_ptr std::auto_ptr
#else
#  define zth_auto_ptr std::unique_ptr
#endif
}

#endif // __cplusplus
#endif // __ZTH_UTIL_H
