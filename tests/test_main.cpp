/*
 * SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#include <zth>

#include <gtest/gtest.h>

int main(int argc, char** argv)
{
	int res = 0;
	testing::InitGoogleTest(&argc, argv);
	try {
		zth::Worker w;
		auto f = zth::fiber([]() { return RUN_ALL_TESTS(); })
			 << zth::setName("gtest") << zth::asFuture();
		w.run();
		res = *f;
	} catch(...) {
		std::terminate();
	}

	return res;
}
