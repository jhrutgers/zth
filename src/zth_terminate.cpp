/*
 * SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#include <libzth/util.h>

#include <exception>

/*!
 * \brief Terminate immediately.
 *
 * By default, it calls \c std::terminate().
 */
#ifndef ZTH_OS_WINDOWS
__attribute__((weak))
#endif
__attribute__((noreturn)) void
zth_terminate()
{
	std::terminate();
}
