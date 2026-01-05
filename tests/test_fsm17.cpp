/*
 * SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#include <zth>

#include <gtest/gtest.h>

#if __cplusplus < 201703L
#  error Compile as C++17 or newer.
#endif

TEST(Fsm17, CallbackLambda)
{
	using namespace zth::fsm;

	static bool flag = false;

	static constexpr auto check = action([&]() { flag = true; });

	// clang-format off
	constexpr auto transitions = compile(
		"a" + entry / check
	);
	// clang-format on

	auto fsm = transitions.spawn();
	EXPECT_TRUE(fsm.valid());
	EXPECT_FALSE(flag);

	fsm.run(true);
	EXPECT_TRUE(flag);
}
