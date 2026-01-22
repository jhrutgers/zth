/*
 * SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
 *
 * SPDX-License-Identifier: CC0-1.0
 */

// Default Zth header include.
#include <zth>

// Include anything else your program needs, as usual.
#include <cstdio>

// This is a normal function, which we are going to use as a fiber entry point.
void world()
{
	printf("World!!1\n");
}

void hello()
{
	// Create a new fiber, that calls `world()'.
	zth::fiber(world);

	// Even though the world()-fiber was started before the following print statement, 'Hello'
	// will always be printed first, as we did not yield the processor to any other fiber in
	// between. So, the output is deterministic; the printf()s will not be reordered.
	printf("Hello\n");
}

// main() is weakly defined by Zth. If not implemented by the program, Zth supplies one that just
// starts a worker and schedules main_fiber() as the only fiber. So, this is a convenient entry
// point of your fibered program.  If you have to do some pre-Zth initialization to be done, such as
// enabling peripherals of an embedded system, override the weakly defined zth_preinit(), which is
// by default doing nothing. It is called before any Zth functionality is used.
int main_fiber(int /*argc*/, char** /*argv*/)
{
	zth::fiber(hello);

	// This is the exit code of the application.  Although the application has not finished, the
	// hello fiber is still running, this value is saved and used at exit later on. If you have
	// to return a non-zero value, depending on the state of your application, do not let the
	// main fiber return yet.
	return 0;
}
