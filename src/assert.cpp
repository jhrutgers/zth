/*
 * Zth (libzth), a cooperative userspace multitasking library.
 * Copyright (C) 2019-2022  Jochem Rutgers
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
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
