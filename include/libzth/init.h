#ifndef ZTH_INIT_H
#define ZTH_INIT_H
/*
 * Zth (libzth), a cooperative userspace multitasking library.
 * Copyright (C) 2019-2021  Jochem Rutgers
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <libzth/macros.h>

#include <stdlib.h>

struct zth_init_entry {
#ifdef __cplusplus
	zth_init_entry(void (*f)(void), zth_init_entry const* next)
		: f(f)
		, next(next)
	{}
#endif
	void (*f)(void);
	struct zth_init_entry const* next;
};

extern struct zth_init_entry const* zth_init_head;
extern struct zth_init_entry* zth_init_tail;
EXTERN_C ZTH_EXPORT void zth_init();
EXTERN_C ZTH_EXPORT void zth_preinit();
EXTERN_C ZTH_EXPORT int zth_postdeinit();

#ifdef __cplusplus
#	ifndef ZTH_INIT_CALL
/*!
 * \brief Mark the given function \c f to be invoked during static initialization.
 */
#		define ZTH_INIT_CALL_(f, ...)                              \
			struct f##__init : public zth_init_entry {          \
				f##__init()                                 \
					: zth_init_entry(&exec, nullptr)    \
				{                                           \
					if(zth_init_tail)                   \
						zth_init_tail->next = this; \
					zth_init_tail = this;               \
					if(!zth_init_head)                  \
						zth_init_head = this;       \
				}                                           \
				static void exec(){__VA_ARGS__};            \
			};
#		define ZTH_INIT_CALL(f)        \
			ZTH_INIT_CALL_(f, f();) \
			static f##__init const f##__init_;
#	endif

#	ifndef ZTH_DEINIT_CALL
#		ifdef ZTH_OS_BAREMETAL
#			define ZTH_DEINIT_CALL( \
				...) // Don't bother doing any cleanup during shutdown.
#		else
/*!
 * \brief Mark the given function \c f to be invoked at exit.
 */
#			define ZTH_DEINIT_CALL(f)            \
				ZTH_INIT_CALL_(f, atexit(f);) \
				static f##__deinit f##__deinit_;
#		endif
#	endif

#endif // __cplusplus
#endif // ZTH_INIT_H
