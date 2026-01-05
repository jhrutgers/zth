/*
 * SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <zth>

int main_fiber(int /*argc*/, char** /*argv*/)
{
	zth::PeriodicWakeUp w(1);

	while(true) {
		printf("on\n");
		w();
		printf("off\n");
		w();
	}
}
