/*
 * SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#include <libzth/indirection.h>

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
zth_indirection_t zth_indirection = {};

#if ZTH_SHARED_LIB
#  define ZTH_INDIRECT_CHECKV(f, ...)                                      \
	  static bool recursive = false;                                   \
                                                                           \
	  zth_assert(zth_indirection.f##_check);                           \
                                                                           \
	  if(recursive) {                                                  \
		  zth_indirection.f = zth_indirection.f##_check = nullptr; \
		  f(__VA_ARGS__);                                          \
	  } else {                                                         \
		  recursive = true;                                        \
		  zth_indirection.f##_check(__VA_ARGS__);                  \
		  zth_indirection.f = zth_indirection.f##_check;           \
	  }

#  define ZTH_INDIRECT_CHECK(f, ret, ...)                                  \
	  static bool recursive = false;                                   \
                                                                           \
	  zth_assert(zth_indirection.f##_check);                           \
                                                                           \
	  if(recursive) {                                                  \
		  zth_indirection.f = zth_indirection.f##_check = nullptr; \
		  return f(__VA_ARGS__);                                   \
	  } else {                                                         \
		  recursive = true;                                        \
		  ret res = zth_indirection.f##_check(__VA_ARGS__);        \
		  zth_indirection.f = zth_indirection.f##_check;           \
		  return res;                                              \
	  }

void zth_logv_indirect_check(char const* fmt, va_list arg)
{
	ZTH_INDIRECT_CHECKV(zth_logv, fmt, arg)
}

int zth_main_fiber_indirect_check(int argc, char** argv)
{
	ZTH_INDIRECT_CHECK(main_fiber, int, argc, argv)
}

void zth_assert_handler_indirect_check(char const* file, int line, char const* expr)
{
	ZTH_INDIRECT_CHECKV(zth_assert_handler, file, line, expr)
}

int zth_postdeinit_indirect_check()
{
	ZTH_INDIRECT_CHECK(zth_postdeinit, int)
}

void zth_preinit_indirect_check()
{
	ZTH_INDIRECT_CHECKV(zth_preinit)
}

void zth_terminate_indirect_check()
{
	ZTH_INDIRECT_CHECKV(zth_terminate)
}
#endif // ZTH_SHARED_LIB

/*!
 * \brief Set the function pointer for zth_logv.
 * \ingroup zth_api_indirection
 */
void zth_logv_indirect(zth_logv_t* func)
{
	zth_indirection.zth_logv = func;
}

/*!
 * \brief Set the function pointer for main_fiber.
 * \ingroup zth_api_indirection
 */
void zth_main_fiber_indirect(zth_main_fiber_t* func)
{
	zth_indirection.main_fiber = func;
}

/*!
 * \brief Set the function pointer for zth_assert_handler.
 * \ingroup zth_api_indirection
 */
void zth_assert_handler_indirect(zth_assert_handler_t* func)
{
	zth_indirection.zth_assert_handler = func;
}

/*!
 * \brief Set the function pointer for zth_postdeinit.
 * \ingroup zth_api_indirection
 */
void zth_postdeinit_indirect(zth_postdeinit_t* func)
{
	zth_indirection.zth_postdeinit = func;
}

/*!
 * \brief Set the function pointer for zth_preinit.
 * \ingroup zth_api_indirection
 */
void zth_preinit_indirect(zth_preinit_t* func)
{
	zth_indirection.zth_preinit = func;
}

/*!
 * \brief Set the function pointer for zth_terminate.
 * \ingroup zth_api_indirection
 */
void zth_terminate_indirect(zth_terminate_t* func)
{
	zth_indirection.zth_terminate = func;
}

/*!
 * \brief Set all function indirection pointers.
 * \param indirection the struct with function pointers to set. If \c nullptr, all pointers
 * are reset. \ingroup zth_api_indirection
 */
void zth_indirect(zth_indirection_t const* indirection)
{
	if(indirection)
		zth_indirection = *indirection;
	else
		zth_indirection = {};
}
