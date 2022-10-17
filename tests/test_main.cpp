/*
 * Zth (libzth), a cooperative userspace multitasking library.
 * Copyright (C) 2019-2022  Jochem Rutgers
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
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

