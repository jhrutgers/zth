/*
 * SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#include <libzth/fiber.h>

#ifndef ZTH_OS_WINDOWS
__attribute__((weak))
#endif
int main_fiber(int UNUSED_PAR(argc), char** UNUSED_PAR(argv))
{
	return 0;
}
