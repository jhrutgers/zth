/*
 * Zth (libzth), a cooperative userspace multitasking library.
 * Copyright (C) 2019-2022  Jochem Rutgers
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <libzth/util.h>

/*!
 * \brief Prints the given printf()-like formatted string to stdout.
 * \details This is a weak symbol. Override when required.
 * \ingroup zth_api_c_util
 */
__attribute__((weak)) void zth_logv(char const* fmt, va_list arg)
{
	// NOLINTNEXTLINE
	vprintf(fmt, arg);
}
