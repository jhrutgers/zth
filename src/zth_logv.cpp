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
