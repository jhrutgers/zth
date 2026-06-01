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

#include <libzth/util.h>

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

typedef void(zth_logv_t)(char const* fmt, va_list arg);

typedef struct {
	zth_logv_t* zth_logv;
} zth_indirection_t;

extern zth_indirection_t zth_indirection;

ZTH_EXPORT void zth_logv_indirect(zth_logv_t* func);

ZTH_EXPORT void zth_indirect(zth_indirection_t const* indirection);

/*!
 * \brief Set all function indirection pointers automatically.
 *
 * Call this function early from your application. It tries to auto-detect functions that are
 * defined in your application, and should override the Zth's versions.  This also works on shared
 * libraries, without weak pointer support.
 */
static __attribute__((gnu_inline)) inline void zth_indirect_auto()
{
#if ZTH_SHARED_LIB
	if(!zth_indirection.zth_logv)
		zth_indirection.zth_logv = zth_logv;
#endif
}

#define ZTH_INDIRECT_CALLV(func, ...)                       \
	do { /* NOLINT(cppcoreguidelines-avoid-do-while) */ \
		zth_assert(zth_indirection.func);           \
		zth_indirection.func(__VA_ARGS__);          \
	} while(0)
#define ZTH_INDIRECT_CALL(func, ...) \
	(zth_assert(zth_indirection.func), zth_indirection.func(__VA_ARGS__))
#define ZTH_INDIRECT(func) (zth_indirection.func && zth_indirection.func != func)

#define ZTH_INDIRECT_PROLOGUE(func, ...) \
	if(ZTH_INDIRECT(func))           \
		return ZTH_INDIRECT_CALL(func, __VA_ARGS__);
#define ZTH_INDIRECT_PROLOGUEV(func, ...)              \
	if(ZTH_INDIRECT(func)) {                       \
		ZTH_INDIRECT_CALLV(func, __VA_ARGS__); \
		return;                                \
	}

#ifdef __cplusplus
} // extern "C"
#endif // __cplusplus
#endif // ZTH_INDIRECTION_H
