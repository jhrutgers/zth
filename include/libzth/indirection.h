#ifndef ZTH_INDIRECTION_H
#define ZTH_INDIRECTION_H
/*
 * SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
 *
 * SPDX-License-Identifier: MPL-2.0
 */

/*!
 * \defgroup zth_api_indirection Indirection
 * \brief Indirection support for C API functions.
 *
 * Several low-level functions can be overridden. For a static library, these are defined as weak
 * symbols, allowing the application do define its own version.  For a shared library, this does not
 * work for all targets. So, this interface allows injecting function pointers to be called for the
 * low-level functions.
 *
 * \ingroup zth_api_cpp
 * \ingroup zth_api_c
 */

#include <libzth/macros.h>

#include <libzth/fiber.h>
#include <libzth/init.h>
#include <libzth/util.h>

#if ZTH_SHARED_LIB
#  if defined(ZTH_OS_WINDOWS)
#    include <windows.h>
#  elif defined(ZTH_HAVE_DL)
#    include <dlfcn.h>
#  endif
#endif

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

typedef void(zth_logv_t)(char const* fmt, va_list arg);
typedef int(zth_main_fiber_t)(int argc, char** argv);
typedef void(zth_assert_handler_t)(char const* file, int line, char const* expr);
typedef int(zth_postdeinit_t)();
typedef void(zth_preinit_t)();
typedef void(zth_terminate_t)();

typedef struct {
	zth_logv_t* zth_logv;
	zth_main_fiber_t* main_fiber;
	zth_assert_handler_t* zth_assert_handler;
	zth_postdeinit_t* zth_postdeinit;
	zth_preinit_t* zth_preinit;
	zth_terminate_t* zth_terminate;

#if ZTH_SHARED_LIB
	// For a shared library, we cannot determine at compile time if the application overrides
	// the functions.  Library functions may be wrapped by the compiler. In that case, we check
	// for recursion on the indirection call upon the first time.
	zth_logv_t* zth_logv_check;
	zth_main_fiber_t* main_fiber_check;
	zth_assert_handler_t* zth_assert_handler_check;
	zth_postdeinit_t* zth_postdeinit_check;
	zth_preinit_t* zth_preinit_check;
	zth_terminate_t* zth_terminate_check;
#endif // ZTH_SHARED_LIB
} zth_indirection_t;

extern zth_indirection_t zth_indirection;

ZTH_EXPORT void zth_logv_indirect(zth_logv_t* func);
ZTH_EXPORT void zth_main_fiber_indirect(zth_main_fiber_t* func);
ZTH_EXPORT void zth_assert_handler_indirect(zth_assert_handler_t* func);
ZTH_EXPORT void zth_postdeinit_indirect(zth_postdeinit_t* func);
ZTH_EXPORT void zth_preinit_indirect(zth_preinit_t* func);
ZTH_EXPORT void zth_terminate_indirect(zth_terminate_t* func);

ZTH_EXPORT void zth_indirect(zth_indirection_t const* indirection);

#if ZTH_SHARED_LIB
void zth_logv_indirect_check(char const* fmt, va_list arg);
int zth_main_fiber_indirect_check(int argc, char** argv);
void zth_assert_handler_indirect_check(char const* file, int line, char const* expr);
int zth_postdeinit_indirect_check();
void zth_preinit_indirect_check();
void zth_terminate_indirect_check();

/*!
 * \brief Set all function indirection pointers automatically.
 *
 * Call this function early from your application. It tries to auto-detect functions that are
 * defined in your application, and should override the Zth's versions.  This also works on shared
 * libraries, without weak pointer support.
 */
inline void zth_indirect_auto()
{
	if(!zth_indirection.zth_logv) {
		zth_indirection.zth_logv = zth_logv_indirect_check;
		zth_indirection.zth_logv_check = zth_logv;
	}

	if(!zth_indirection.main_fiber) {
		zth_indirection.main_fiber = zth_main_fiber_indirect_check;
		zth_indirection.main_fiber_check = main_fiber;
	}

	if(!zth_indirection.zth_assert_handler) {
		zth_indirection.zth_assert_handler = zth_assert_handler_indirect_check;
		zth_indirection.zth_assert_handler_check = zth_assert_handler;
	}

	if(!zth_indirection.zth_postdeinit) {
		zth_indirection.zth_postdeinit = zth_postdeinit_indirect_check;
		zth_indirection.zth_postdeinit_check = zth_postdeinit;
	}

	if(!zth_indirection.zth_preinit) {
		zth_indirection.zth_preinit = zth_preinit_indirect_check;
		zth_indirection.zth_preinit_check = zth_preinit;
	}

	if(!zth_indirection.zth_terminate) {
		zth_indirection.zth_terminate = zth_terminate_indirect_check;
		zth_indirection.zth_terminate_check = zth_terminate;
	}
}

ZTH_APP_INIT_CALL(zth_indirect_auto)
#endif // ZTH_SHARED_LIB

#define ZTH_INDIRECT_PROLOGUE(func, ...) \
	if(::zth_indirection.func)       \
		return ::zth_indirection.func(__VA_ARGS__);

#define ZTH_INDIRECT_PROLOGUEV(func, ...)            \
	if(::zth_indirection.func) {                 \
		::zth_indirection.func(__VA_ARGS__); \
		return;                              \
	}

#ifdef __cplusplus
} // extern "C"
#endif // __cplusplus
#endif // ZTH_INDIRECTION_H
