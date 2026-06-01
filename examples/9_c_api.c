/*
 * SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
 *
 * SPDX-License-Identifier: CC0-1.0
 */

// Default Zth header include.
#include <zth.h>

#include <stdio.h>

// In contrast to C++, fiber entry function must have type void(void*).  There
// is no need to do zth_fiber() and friends, just use zth_fiber_create() to run
// the fiber.
void fiber(void* UNUSED_PAR(arg))
{
	printf("fiber()\n");
}

int main_fiber(int UNUSED_PAR(argc), char** UNUSED_PAR(argv))
{
	printf("main_fiber()\n");
	// Start a new fiber, with arg=NULL, default stack, and no name.
	zth_fiber_create(NULL, fiber, NULL, 0, NULL);
	return 0;
}

// In a pure-C application, use a static libzth. Otherwise, it will probably not find main_fiber()
// and call the default main_fiber() which does nothing.  Alternatively, define a main() and call
// zth_main() from it.
