/*
 * SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <zth>

#include <cstdio>

#include <cassert>

int main_fiber(int /*argc*/, char** /*argv*/)
{
	puts(zth::banner());
	printf("Hello again\n");

	// This should be defined in the CMakeLists.txt.
	assert(!zth::Config::Debug);
	assert(!zth::Config::EnableExceptions);
	zth::checkConfig();

	return 0;
}
