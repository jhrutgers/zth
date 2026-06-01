#ifndef ZTH_INIT_H
#define ZTH_INIT_H
/*
 * SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#include <libzth/macros.h>

#include <stdlib.h>

struct zth_init_entry {
#ifdef __cplusplus
	zth_init_entry(void (*f_)(void), zth_init_entry const* next_) noexcept;
#endif // C++
	void (*f)(void);
	struct zth_init_entry const* next;
};

extern struct zth_init_entry const* zth_init_head;
extern struct zth_init_entry* zth_init_tail;
EXTERN_C ZTH_EXPORT void zth_init();
EXTERN_C ZTH_EXPORT void zth_preinit();
EXTERN_C ZTH_EXPORT int zth_postdeinit();
EXTERN_C ZTH_EXPORT int zth_run(void(fiber)(void*), void* arg);
EXTERN_C ZTH_EXPORT int zth_main(int argc, char** argv);

#ifdef __cplusplus
#  ifndef ZTH_INIT_CALL
/*!
 * \brief Mark the given function \c f to be invoked during \c zth_init() .
 */
#    define ZTH_INIT_CALL_(f, ...)                           \
	    struct f##__init : public zth_init_entry {       \
		    f##__init() noexcept                     \
			    : zth_init_entry(&exec, nullptr) \
		    {}                                       \
		    static void exec() { __VA_ARGS__ };      \
	    };
#    define ZTH_INIT_CALL(f)        \
	    ZTH_INIT_CALL_(f, f();) \
	    static f##__init const f##__init_;
#  endif

#  ifndef ZTH_DEINIT_CALL
#    ifdef ZTH_OS_BAREMETAL
#      define ZTH_DEINIT_CALL(...) // Don't bother doing any cleanup during shutdown.
#    else
/*!
 * \brief Mark the given function \c f to be invoked at exit.
 */
#      define ZTH_DEINIT_CALL(f)            \
	      ZTH_INIT_CALL_(f, atexit(f);) \
	      static f##__init f##__deinit_;
#    endif
#  endif
#endif // __cplusplus

#if defined(ZTH_BUILD_LIB) || !defined(__cplusplus)
#  define ZTH_APP_INIT_CALL(f)
#else
#  if __cplusplus >= 201703L
#    define ZTH_APP_INIT_CALL_(f, ...)       \
	    struct f##__app_init {           \
		    f##__app_init() noexcept \
		    {                        \
			    __VA_ARGS__      \
		    }                        \
	    };                               \
	    inline f##__app_init const f##__app_init_;
#  else
#    define ZTH_APP_INIT_CALL_(f, ...)                 \
	    inline void f##__app_init_once_() noexcept \
	    {                                          \
		    static bool done = false;          \
		    if(!done) {                        \
			    done = true;               \
			    __VA_ARGS__                \
		    }                                  \
	    }                                          \
	    struct f##__app_init {                     \
		    f##__app_init() noexcept           \
		    {                                  \
			    f##__app_init_once_();     \
		    }                                  \
	    };                                         \
	    static f##__app_init const f##__app_init_;
#  endif

#  define ZTH_APP_INIT_CALL(f) ZTH_APP_INIT_CALL_(f, f();)
#endif

#endif // ZTH_INIT_H
