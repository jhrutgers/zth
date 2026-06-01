/*
 * SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#include <libzth/util.h>

#include <libzth/indirection.h>

/*!
 * \brief Prints the given printf()-like formatted string to stdout.
 * \details This is a weak symbol. Override when required.
 * \ingroup zth_api_c_util
 */
#ifndef ZTH_OS_WINDOWS
__attribute__((weak))
#endif
void zth_logv(char const* fmt, va_list arg)
{
	ZTH_INDIRECT_PROLOGUEV(zth_logv, fmt, arg)

	(void)vprintf(fmt, arg);
}
