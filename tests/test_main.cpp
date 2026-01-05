/*
 * SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#include <zth>

#include <gtest/gtest.h>

static int test_main()
{
	return RUN_ALL_TESTS();
}
zth_fiber(test_main)

int main(int argc, char** argv)
{
	int res = 0;
	testing::InitGoogleTest(&argc, argv);
	{
		zth::Worker w;
		test_main_future f = async test_main();
		w.run();
		res = f->value();
	}
	return res;
}
