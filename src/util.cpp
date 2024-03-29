/*
 * Zth (libzth), a cooperative userspace multitasking library.
 * Copyright (C) 2019-2022  Jochem Rutgers
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <libzth/macros.h>
#include <libzth/allocator.h>
#include <libzth/init.h>
#include <libzth/perf.h>
#include <libzth/util.h>
#include <libzth/version.h>

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>

#ifdef ZTH_OS_WINDOWS
#	include <windows.h>
#endif

using namespace std;

namespace zth {

/*!
 * \brief Prints a banner line with version and configuration information.
 * \ingroup zth_api_cpp_util
 */
char const* banner() noexcept
{
	// clang-format off
	return "Zth " ZTH_VERSION
#ifdef __GNUC__
		" g++-" ZTH_STRINGIFY(__GNUC__) "." ZTH_STRINGIFY(__GNUC_MINOR__)
			"." ZTH_STRINGIFY(__GNUC_PATCHLEVEL__)
#endif
#if __cplusplus < 201103L
		" C++98"
#elif __cplusplus == 201103L
		" C++11"
#elif __cplusplus == 201402L
		" C++14"
#elif __cplusplus == 201703L
		" C++17"
#else
		" C++" ZTH_STRINGIFY(__cplusplus)
#endif
#ifdef ZTH_OS_LINUX
		" linux"
#endif
#ifdef ZTH_OS_MAC
		" mac"
#endif
#ifdef ZTH_OS_BAREMETAL
		" baremetal"
		" newlib" _NEWLIB_VERSION
#endif
#ifdef ZTH_ARCH_X86_64
		" x86_64"
#endif
#ifdef ZTH_ARCH_X86
		" x86"
#endif
#ifdef ZTH_ARCH_ARM
		" arm"
#  ifdef __ARM_ARCH_PROFILE
#    if __ARM_ARCH_PROFILE == 'A'
		"A"
#    elif __ARM_ARCH_PROFILE == 'R'
		"R"
#    elif __ARM_ARCH_PROFILE == 'M'
		"M"
#    elif __ARM_ARCH_PROFILE == 'S'
		"S"
#    endif
#  endif
#  ifdef __ARM_ARCH
		"v" ZTH_STRINGIFY(__ARM_ARCH)
#  endif
#endif
#ifdef ZTH_ARM_USE_PSP
		" psp"
#endif
#ifdef _DEBUG
		" debug"
#endif
#ifdef ZTH_DRAFT_API
		" draft"
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
#ifdef ZTH_CONTEXT_SJLJ
		" sjlj"
#endif
#ifdef ZTH_CONTEXT_WINFIBER
		" winfiber"
#endif
#ifdef ZTH_HAVE_ZMQ
		" zmq"
#endif
#ifdef ZTH_ENABLE_ASAN
		" asan"
#endif
#ifdef ZTH_ENABLE_LSAN
		" lsan"
#endif
#ifdef ZTH_ENABLE_UBSAN
		" ubsan"
#endif
		;
	// clang-format on
}

/*!
 * \brief Aborts the process after printing the given printf() formatted message.
 * \ingroup zth_api_cpp_util
 */
void abort(char const* fmt, ...) noexcept
{
	va_list args;
	va_start(args, fmt);
	abortv(fmt, args);
	va_end(args);
}

/*!
 * \copybrief zth::abort()
 * \ingroup zth_api_cpp_util
 */
void abortv(char const* fmt, va_list args) noexcept
{
	static bool recurse = false;

	if(!recurse) {
		recurse = true;
		log("\n%s  *** Zth ABORT:  ", zth::Config::EnableColorLog ? "\x1b[41;1;37;1m" : "");
		logv(fmt, args);
		log("  ***  %s\n\n", zth::Config::EnableColorLog ? "\x1b[0m" : "");

		Backtrace().print();
	}

	::abort();
}

#ifndef ZTH_OS_BAREMETAL
static void log_init()
{
#	ifdef ZTH_OS_WINDOWS
	// Windows does not support line buffering.
	// If set anyway, this implies full buffering.
	// Only do that when we expect much debug output.
	if(zth_config(EnableDebugPrint)) {
#	endif
		setvbuf(stdout, nullptr, _IOLBF, 4096);
		setvbuf(stderr, nullptr, _IOLBF, 4096);
#	ifdef ZTH_OS_WINDOWS
	}
#	endif
}
ZTH_INIT_CALL(log_init)
#endif

/*!
 * \brief Returns if the system supports ANSI colors.
 */
bool log_supports_ansi_colors() noexcept
{
#ifdef ZTH_OS_WINDOWS
	static int support = 0;
	if(likely(support))
		return support > 0;

	// Win10 supports ANSI, but it should be enabled.  This can be done to
	// set the Console Mode to ENABLE_VIRTUAL_TERMINAL_PROCESSING (0x0004).
	// However, the application is not compiled for Win10 specifically, so
	// we cannot ask if we are running on Win10, and this macro has not
	// been defined. Therefore, we just try to set the flag by its number,
	// and check if it succeeds. If so, we assume cmd will accept ANSI
	// colors.
	AttachConsole(ATTACH_PARENT_PROCESS);

	DWORD mode = 0;
	if(!GetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), &mode))
		goto nosupport;
	if(!SetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), mode | 0x0004))
		goto nosupport;

	// Succeeded. We have support. Enable stderr too.
	if(GetConsoleMode(GetStdHandle(STD_ERROR_HANDLE), &mode))
		SetConsoleMode(GetStdHandle(STD_ERROR_HANDLE), mode | 0x0004);

	support = 1;
	return true;

nosupport:
	support = -1;
	return false;
#else  // !ZTH_OS_WINDOWS
	return true;
#endif // !ZTH_OS_WINDOWS
}

/*!
 * \brief Logs a given printf()-like formatted string using an ANSI color code.
 * \details #zth_logv() is used for the actual logging.
 * \ingroup zth_api_cpp_util
 */
void log_colorv(int color, char const* fmt, va_list args)
{
	static bool do_color =
		Config::EnableColorLog && log_supports_ansi_colors() && isatty(fileno(stdout));

	if(do_color && color > 0)
		log("\x1b[%d%sm", (color % 8) + 30, color >= 8 ? ";1" : "");

	logv(fmt, args);

	if(do_color && color > 0)
		log("\x1b[0m");
}

/*!
 * \brief Format like \c vsprintf(), but save the result in an \c zth::string.
 */
string formatv(char const* fmt, va_list args)
{
	int const maxstack = (int)sizeof(void*) * 8;

	char sbuf[maxstack];
	char* hbuf = nullptr;
	size_t hbuf_size = 0;
	char* buf = sbuf;

	va_list args2;
	va_copy(args2, args);
	// NOLINTNEXTLINE
	int c = vsnprintf(sbuf, maxstack, fmt, args);

	if(c >= maxstack) {
		hbuf_size = (size_t)c + 1u;
		hbuf = allocate_noexcept<char>(hbuf_size);

		if(unlikely(!hbuf)) {
			c = 0;
		} else {
			// NOLINTNEXTLINE
			int c2 = vsprintf(hbuf, fmt, args2);
			zth_assert(c2 <= c);
			c = c2;
			buf = hbuf;
		}
	}

	va_end(args2);

	string res(buf, (size_t)(c < 0 ? 0 : c));
	deallocate(hbuf, hbuf_size);
	return res;
}

} // namespace zth

/*!
 * \copydoc zth::abort()
 * \ingroup zth_api_c_util
 */
void zth_abort(char const* fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	zth::abortv(fmt, va);
	va_end(va);
}

#ifdef ZTH_OS_BAREMETAL
__attribute__((weak)) int asprintf(char** strp, const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	int res = vasprintf(strp, fmt, args);
	va_end(args);
	return res;
}

__attribute__((weak)) int vasprintf(char** strp, const char* fmt, va_list ap)
{
	va_list ap_dup;
	va_copy(ap_dup, ap);
	int len = vsnprintf(nullptr, 0, fmt, ap_dup);
	va_end(ap_dup);

	if(strp && (*strp = (char*)malloc(len + 1)))
		vsnprintf(*strp, len + 1, fmt, ap);

	// cppcheck-suppress memleak
	return len;
}
#endif
