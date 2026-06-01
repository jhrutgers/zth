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
		auto run_all_tests = [](void* arg) {
			int* result = static_cast<int*>(arg);
			*result = RUN_ALL_TESTS();
		};

		int zth_res = zth_run(run_all_tests, &res);
		if(zth_res)
			res = zth_res;
	} catch(...) {
		zth_terminate();
	}

	return res;
}
