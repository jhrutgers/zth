#ifndef ZTH_MACROS_H
#define ZTH_MACROS_H
/*
 * SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
 *
 * SPDX-License-Identifier: MPL-2.0
 */



//////////////////////////////////////////////////
// Preamble
//

#ifdef _DEBUG
#  undef NDEBUG
#endif
#if !defined(_DEBUG) && !defined(NDEBUG)
#  define _DEBUG
#endif

#ifndef ZTH_THREADS
#  define ZTH_THREADS 1
#endif

#if !ZTH_THREADS && defined(ZTH_HAVE_LIBZMQ)
#  undef ZTH_THREADS
#  define ZTH_THREADS 1
#endif



//////////////////////////////////////////////////
// Compiler
//

#ifdef __clang_analyzer__
// We don't use clang for compiling, but we do use clang-tidy.
#  define CLANG_TIDY
#endif

#if defined(__GNUC__) || defined(CLANG_TIDY)
// This is gcc
#  ifdef __cplusplus
#    if __cplusplus < 201103L && !defined(decltype)
#      define decltype(x) \
	      __typeof__(x) // Well, not really true when references are
			    // involved...
#    endif
#  endif
#  if ZTH_THREADS
#    define ZTH_TLS_DECLARE(type, var)	    extern __thread type var;
#    define ZTH_TLS_DEFINE(type, var, init) __thread type var = init;
#    define ZTH_TLS_STATIC(type, var, init) static __thread type var = init;
#    define ZTH_TLS_MEMBER(type, var)	    static __thread type var;
#    define ZTH_TLS_SET(var, value)	    var = value
#    define ZTH_TLS_GET(var)		    var
#  endif
#  ifndef _GNU_SOURCE
#    define _GNU_SOURCE
#  endif
#  ifndef CLANG_TIDY
#    define ZTH_ATTR_PRINTF gnu_printf
#  endif
#  ifndef GCC_VERSION
#    define GCC_VERSION (__GNUC__ * 10000L + __GNUC_MINOR__ * 100L + __GNUC_PATCHLEVEL__)
#  endif
#  if GCC_VERSION < 50000
#    pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#  endif
#  if GCC_VERSION >= 70000 && defined(__cplusplus) && __cplusplus < 201703L
#    pragma GCC diagnostic ignored "-Wnoexcept-type"
#  endif
#  if GCC_VERSION >= 130000 && GCC_VERSION < 140000 && defined(__cplusplus)
#    pragma GCC diagnostic ignored "-Wmismatched-new-delete"
#  endif
#  ifndef UNUSED_PAR
#    define UNUSED_PAR(name) name __attribute__((unused))
#  endif
#else
#  error Unsupported compiler. Please use gcc.
#endif

#ifdef CPPCHECK
#  define __attribute__(x)
#endif

#if defined(__cplusplus) && __cplusplus >= 201703L
#  define ZTH_FALLTHROUGH [[fallthrough]];
#elif GCC_VERSION >= 70000L
#  define ZTH_FALLTHROUGH __attribute__((fallthrough));
#else
#  define ZTH_FALLTHROUGH
#endif

#ifndef ZTH_TLS_DECLARE
#  undef ZTH_THREADS
#  define ZTH_THREADS			  0
#  define ZTH_TLS_DECLARE(type, var)	  extern type var;
#  define ZTH_TLS_DEFINE(type, var, init) type var = init;
#  define ZTH_TLS_STATIC(type, var, init) static type var = init;
#  define ZTH_TLS_MEMBER(type, var)	  static type var;
#  define ZTH_TLS_SET(var, value)	  var = value
#  define ZTH_TLS_GET(var)		  var
#endif

#ifndef ZTH_ATTR_PRINTF
#  define ZTH_ATTR_PRINTF printf
#endif

#define __STDC_FORMAT_MACROS

#ifndef EXTERN_C
#  ifdef __cplusplus
#    define EXTERN_C extern "C"
#  else
#    define EXTERN_C
#  endif
#endif

#ifndef ZTH_EXPORT
#  define ZTH_EXPORT __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
#  ifdef ZTH_INLINE_EMIT
#    define ZTH_INLINE
#  else
#    define ZTH_INLINE __attribute__((gnu_inline)) inline
#  endif
#else
#  define ZTH_INLINE __attribute__((gnu_inline)) extern inline
#endif

/*
HOWTO inline:

// Inline private C++-only function:
#ifdef __cplusplus
inline void foo() { baz::bar(); }
#endif

// Inline public C++-only function:
#ifdef __cplusplus
ZTH_EXPORT inline void foo() { baz::bar(); }
#endif

// Inline public C/C++ function:
EXTERN_C ZTH_EXPORT ZTH_INLINE void foo() { bar(); }

// Inline public C/C++ function with C++-only implementation:
#ifdef __cplusplus
EXTERN_C ZTH_EXPORT ZTH_INLINE void foo() { baz::bar(); }
#else
ZTH_EXPORT void foo();
#endif
*/

#ifndef __cplusplus
#  ifndef noexcept
#    define noexcept
#  endif
#elif __cplusplus < 201103L
#  ifndef constexpr
#    define constexpr
#  endif
#  ifndef constexpr14
#    define constexpr14
#  endif
#  ifndef override
#    define override
#  endif
#  ifndef final
#    define final
#  endif
#  ifndef is_default
#    define is_default \
	    {}
#  endif
#  ifndef noexcept
#    define noexcept throw()
#  endif
#  ifndef nullptr
#    define nullptr NULL
#  endif
#  ifndef alignas
#    define alignas(...) __attribute__((aligned(sizeof(void*))))
#  endif
#  ifndef LREF_QUALIFIED
#    define LREF_QUALIFIED
#  endif
#  ifndef inline17
#    define inline17 static
#  endif
#  ifndef static_assert
#    pragma GCC diagnostic ignored "-Wunused-local-typedefs"
#    define ZTH_STATIC_ASSERT_FAILED_(line) zth_static_assert_failed_##line
#    define ZTH_STATIC_ASSERT_FAILED(line)  ZTH_STATIC_ASSERT_FAILED_(line)
#    define static_assert(x, ...)                  \
	    typedef char ZTH_STATIC_ASSERT_FAILED( \
		    __LINE__)[(x) ? 1 : -1] /* NOLINT(clang-diagnostic-vla-cxx-extension) */
#  endif
#else
#  ifndef constexpr14
#    if __cplusplus < 201402L
#      define constexpr14
#    else
#      define constexpr14 constexpr
#    endif
#  endif
#  ifndef is_default
#    define is_default = default;
#  endif
#  ifndef LREF_QUALIFIED
#    define LREF_QUALIFIED &
#  endif
#  ifndef inline17
#    if __cplusplus < 201703L
#      define inline17 static
#    else
#      define inline17 inline
#    endif
#  endif
#endif

#if defined(__cplusplus) && !defined(__cpp_exceptions)
#  define try		 if(true)
#  define catch(...)	 if(false)
#  define zth_throw(...) std::terminate()
#else
#  define zth_throw(...) throw __VA_ARGS__
#endif

#if defined(__SANITIZE_ADDRESS__) && !defined(ZTH_ENABLE_ASAN)
#  define ZTH_ENABLE_ASAN
#endif

#if defined(ZTH_ALLOW_DEPRECATED)
#  define ZTH_DEPRECATED(...)
#elif defined(__cplusplus) && __cplusplus >= 201402L
#  define ZTH_DEPRECATED(...) [[deprecated(__VA_ARGS__)]]
#elif !defined(__cplusplus) && defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
#  define ZTH_DEPRECATED(...) [[deprecated(__VA_ARGS__)]]
#else
#  define ZTH_DEPRECATED(...) __attribute__((deprecated))
#endif



//////////////////////////////////////////////////
// Hardware
//

#if defined(__x86_64__)
#  define ZTH_ARCH_X86_64 1
#elif defined(__i386__)
#  define ZTH_ARCH_X86 1
#elif defined(__aarch64__) && defined(__APPLE__)
#  define ZTH_ARCH_ARM64 1
#elif defined(__arm__)
#  define ZTH_ARCH_ARM 1
#  if defined(__ARM_ARCH) && __ARM_ARCH >= 6 && defined(__ARM_ARCH_PROFILE) \
	  && __ARM_ARCH_PROFILE == 'M'
#    define ZTH_ARM_HAVE_MPU
#  endif
#  define __isb()   __asm__ volatile("isb" ::: "memory")
#  define __dsb()   __asm__ volatile("dsb" ::: "memory")
#  define __dmb()   __asm__ volatile("dmb" ::: "memory")
#  define barrier() __dsb()
#else
#  error Unsupported hardware platform.
#endif

#ifndef barrier
#  if GCC_VERSION < 40802L
#    define barrier() __sync_synchronize()
#  else
#    define barrier() __atomic_thread_fence()
#  endif
#endif



//////////////////////////////////////////////////
// OS
//

#if defined(_WIN32) || defined(__CYGWIN__)
#  define ZTH_OS_WINDOWS	 1
#  define _WANT_IO_C99_FORMATS	 1
#  define __USE_MINGW_ANSI_STDIO 1
#  define WIN32_LEAN_AND_MEAN
#  define NOGDI
#  if defined(UNICODE) || defined(_UNICODE)
#    error Do not use UNICODE. Use ANSI with UTF-8 instead.
#  endif
#  ifdef __CYGWIN__
#    define ZTH_HAVE_PTHREAD
#  endif
// #  define ZTH_HAVE_WINSOCK
#  ifdef ZTH_CONFIG_WRAP_IO
#    error ZTH_CONFIG_WRAP_IO is not supported on Windows.
#  endif
#elif defined(__linux__) && !defined(ZTH_OS_BAREMETAL)
#  define ZTH_OS_LINUX 1
#  define ZTH_OS_POSIX 1
// #  define ZTH_HAVE_VALGRIND
#  define ZTH_HAVE_PTHREAD
// #  define ZTH_HAVE_LIBUNWIND
#  define ZTH_HAVE_POLL
#  define ZTH_HAVE_MMAN
#elif defined(__APPLE__)
#  include <TargetConditionals.h>
#  ifdef TARGET_OS_MAC
#    define ZTH_OS_MAC	 1
#    define ZTH_OS_POSIX 1
#  else
#    error Unsupported Apple platform.
#  endif
#  define ZTH_HAVE_PTHREAD
#  define ZTH_HAVE_POLL
#  define ZTH_HAVE_MMAN
#  include <Availability.h>
#  if __MAC_OS_X_VERSION_MAX_ALLOWED < 1012
// OSX 10.12 includes clock_gettime(). Otherwise, implement it by Zth.
#    define ZTH_CUSTOM_CLOCK_GETTIME
#  endif
#elif defined(ZTH_ARCH_ARM)
// Assume having newlib
#  define ZTH_OS_BAREMETAL 1
#  define ZTH_CUSTOM_CLOCK_GETTIME
#  include <newlib.h>
#  ifndef NEWLIB_VERSION
#    define NEWLIB_VERSION (__NEWLIB__ * 10000L + __NEWLIB_MINOR__ * 100L + __NEWLIB_PATCHLEVEL__)
#  endif
#  ifndef ZTH_FORMAT_LIMITED
#    define ZTH_FORMAT_LIMITED 1
#  endif
#else
#  error Unsupported OS.
#endif

#if defined(_DEBUG) && defined(ZTH_HAVE_VALGRIND)
#  define ZTH_USE_VALGRIND
#endif

#if !ZTH_THREADS
#  undef ZTH_HAVE_PTHREAD
#endif



//////////////////////////////////////////////////
// Context-switch approach
//

// We have the following approaches. If one of them is already defined, it is
// used as preferred one, if compatible.
//
// - ZTH_CONTEXT_SIGALTSTACK
// - ZTH_CONTEXT_SJLJ
// - ZTH_CONTEXT_UCONTEXT
// - ZTH_CONTEXT_WINFIBER

#ifdef ZTH_OS_WINDOWS
#  ifndef ZTH_CONTEXT_WINFIBER
#    define ZTH_CONTEXT_WINFIBER
#  endif
#  undef ZTH_CONTEXT_SIGALTSTACK
#  undef ZTH_CONTEXT_SJLJ
#  undef ZTH_CONTEXT_UCONTEXT
#elif defined(ZTH_OS_BAREMETAL)
// Assume having newlib with setjmp/longjmp fiddling.
#  ifndef ZTH_CONTEXT_SJLJ
#    define ZTH_CONTEXT_SJLJ
#  endif
#  if defined(ZTH_ARCH_ARM) && defined(__ARM_ARCH) && __ARM_ARCH >= 6 \
	  && defined(__ARM_ARCH_PROFILE) && __ARM_ARCH_PROFILE == 'M'
#    define ZTH_ARM_USE_PSP
#    define ZTH_STACK_SWITCH
#  endif
#  undef ZTH_CONTEXT_SIGALTSTACK
#  undef ZTH_CONTEXT_UCONTEXT
#  undef ZTH_CONTEXT_WINFIBER
#elif defined(ZTH_HAVE_VALGRIND)
// Valgrind does not handle sigaltstack very well.
#  ifndef ZTH_CONTEXT_UCONTEXT
#    define ZTH_CONTEXT_UCONTEXT
#  endif
#  undef ZTH_CONTEXT_SIGALTSTACK
#  undef ZTH_CONTEXT_SJLJ
#  undef ZTH_CONTEXT_WINFIBER
#else
// Default approach is ucontext.
#  undef ZTH_CONTEXT_SJLJ
#  undef ZTH_CONTEXT_WINFIBER
#  if defined(ZTH_CONTEXT_UCONTEXT)
#    undef ZTH_CONTEXT_SIGALTSTACK
#  elif !defined(ZTH_CONTEXT_SIGALTSTACK) || defined(ZTH_ENABLE_ASAN)
#    undef ZTH_CONTEXT_SIGALTSTACK
#    define ZTH_CONTEXT_UCONTEXT
#  endif
#endif

#ifdef ZTH_CONTEXT_WINFIBER
#  ifndef WINVER
#    define WINVER 0x0501
#  elif WINVER < 0x0400
#    error WINVER should be at least 0x0400
#  endif
#  ifndef _WIN32_WINNT
#    define _WIN32_WINNT WINVER
#  elif _WIN32_WINNT < 0x0400
#    error _WIN32_WINNT should be at least 0x0400
#  endif
#endif

#endif // ZTH_MACROS_H
