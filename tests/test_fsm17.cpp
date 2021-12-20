/*
 * Zth (libzth), a cooperative userspace multitasking library.
 * Copyright (C) 2019-2021  Jochem Rutgers
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <zth>

#include <gtest/gtest.h>

#if __cplusplus < 201703L
#	error Compile as C++17 or newer.
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

	fsm.run();
	EXPECT_TRUE(flag);
}
