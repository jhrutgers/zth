/*
 * SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#include <libzth/util.h>

namespace zth {

#ifndef ZTH_OS_WINDOWS
__attribute__((weak))
#endif
void assert_handler(char const* file, int line, char const* expr)
{
	if(Config::EnableFullAssert)
		abort("assertion failed at %s:%d: %s", file ? file : "?", line, expr ? expr : "?");
	else
		abort("assertion failed at %s:%d", file ? file : "?", line);
}

} // namespace zth
