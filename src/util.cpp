/*
 * Zth (libzth), a cooperative userspace multitasking library.
 * Copyright (C) 2019  Jochem Rutgers
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
#include <libzth/util.h>
#include <libzth/version.h>
#include <libzth/perf.h>

#include <cstdlib>
#include <cstdio>
#include <cstdarg>
#include <unistd.h>

using namespace std;

namespace zth {

/*!
 * \brief Prints a banner line with version and configuration information.
 * \ingroup zth_api_cpp
 */
char const* banner() {
	return "Zth " ZTH_VERSION
#ifdef __GNUC__
		" g++-" ZTH_STRINGIFY(__GNUC__) "." ZTH_STRINGIFY(__GNUC_MINOR__) "." ZTH_STRINGIFY(__GNUC_PATCHLEVEL__)
#endif
#ifdef ZTH_OS_LINUX
		" linux"
#endif
#ifdef ZTH_OS_MAC
		" mac"
#endif
#ifdef ZTH_ARCH_X86_64
		" x86_64"
#endif
#ifdef ZTH_ARCH_X86
		" x86"
#endif
#ifdef _DEBUG
		" debug"
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
#ifdef ZTH_CONTEXT_WINFIBER
		" winfiber"
#endif
#ifdef ZTH_HAVE_ZMQ
		" zmq"
#endif
		;
}

/*!
 * \brief Aborts the process after printing the given printf() formatted message.
 * \ingroup zth_api_cpp
 */
void abort(char const* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	abortv(fmt, args);
	va_end(args);
}

/*!
 * \copybrief zth::abort()
 * \ingroup zth_api_cpp
 */
void abortv(char const* fmt, va_list args)
{
	log("\n%s  *** Zth ABORT:  ", zth::Config::EnableColorDebugPrint ? "\x1b[41;1;37;1m" : "");
	logv(fmt, args);
	log("  ***  %s\n\n", zth::Config::EnableColorDebugPrint ? "\x1b[0m" : "");

	Backtrace().print();
	::abort();
}

static void log_init() {
#ifdef ZTH_OS_WINDOWS
	// Windows does not support line buffering.
	// If set anyway, this implies full buffering.
	// Only do that when we expect much debug output.
	if(zth_config(EnableDebugPrint)) {
#endif
		setvbuf(stdout, NULL, _IOLBF, 4096);
		setvbuf(stderr, NULL, _IOLBF, 4096);
#ifdef ZTH_OS_WINDOWS
	}
#endif
}
INIT_CALL(log_init)

/*!
 * \brief Logs a given printf()-like formatted string using an ANSI color code.
 * \details #zth_logv() is used for the actual logging.
 * \ingroup zth_api_cpp
 */
void log_colorv(int color, char const* fmt, va_list args)
{
#ifdef ZTH_OS_WINDOWS
	bool do_color = false;
#else
	static bool do_color = isatty(fileno(stdout));
#endif

	if(do_color)
		log("\x1b[%d%sm", (color % 8) + 30, color >= 8 ? ";1" : "");

	logv(fmt, args);
	
	if(do_color)
		log("\x1b[0m");
}

} // namespace

/*!
 * \copydoc zth::abort()
 * \ingroup zth_api_c
 */
void zth_abort(char const* fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	zth::abortv(fmt, va);
	va_end(va);
}

/*!
 * \brief Prints the given printf()-like formatted string to stdout.
 * \details This is a weak symbol. Override when required.
 * \ingroup zth_api_c
 */
void zth_logv(char const* fmt, va_list arg)
{
	vprintf(fmt, arg);
}

