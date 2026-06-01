/*
 * SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#include <libzth/util.h>

#include <libzth/indirection.h>

#ifndef ZTH_OS_WINDOWS
__attribute__((weak))
#endif
void zth_assert_handler(char const* file, int line, char const* expr)
{
	if(::zth_indirection.zth_assert_handler) {
		::zth_indirection.zth_assert_handler(file, line, expr);
		zth_terminate();
	}

	if(zth::Config::EnableFullAssert)
		zth::abort(
			"assertion failed at %s:%d: %s", file ? file : "?", line,
			expr ? expr : "?");
	else
		zth::abort("assertion failed at %s:%d", file ? file : "?", line);
}
