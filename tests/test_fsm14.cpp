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

#if __cplusplus < 201402L
#	error Compile as C++14 or newer.
#endif

TEST(Fsm14, Grammar)
{
	using namespace zth::fsm;

	// clang-format off
	constexpr auto transitions = compile(
		"a"_S					>>= "b",
		"b"					>>= "c"_S,
		"c"	+ always			>>= "d",
		"d"			/ nothing	>>= "e",
		"e"	+ always	/ nothing	>>= "f",
			+ always			>>= "g",
			+ always	/ nothing	>>= "h",
			  always			>>= "i",
			  always	/ nothing	>>= "j",
					  nothing	>>= "k",
		"f"_S					,
		"g"					,
		"h"	+ always			,
		"i"			/ nothing	,
		"j"	+ always	/ nothing	,
			+ always			,
			+ always	/ nothing	,
			  always			,
			  always	/ nothing	,
					  nothing	,
		"k"	+ "x"_S				>>= "l",
		"l"_S	+ "x"				>>= "m",
		"m"	+ "x"		/ nothing	>>= "n",
			+ "x"_S				>>= "o",
			+ "x"_S		/ nothing	>>= "p",
		"n"	+ "x"_S				,
		"o"	+ "x"		/ nothing	,
			+ "x"_S				,
			+ "x"_S		/ nothing	,
		"p"	+ input("x")			>>= "q",
		"q"	+ input("x")	/ nothing	>>= "r",
			+ input("x")			>>= "s",
			+ input("x")	/ nothing	>>= "a",
		"r"	+ input("x")			,
		"s"	+ input("x")	/ nothing	,
			+ input("x")			,
			  input("x")			,
			+ input("x")	/ nothing	,
			  input("x")	/ nothing
	);
	// clang-format on

	transitions.dump();
}

TEST(Fsm14, RuntimeGrammar)
{
	using namespace zth::fsm;

	// clang-format off
	auto transitions = compile(
		"a"_S					>>= "b",
		"b"					>>= "c"_S,
		"c"	+ always			>>= "d",
		"d"			/ nothing	>>= "e",
		"e"	+ always	/ nothing	>>= "f",
			+ always			>>= "g",
			+ always	/ nothing	>>= "h",
			  always			>>= "i",
			  always	/ nothing	>>= "j",
					  nothing	>>= "k",
		"f"_S					,
		"g"					,
		"h"	+ always			,
		"i"			/ nothing	,
		"j"	+ always	/ nothing	,
			+ always			,
			+ always	/ nothing	,
			  always			,
			  always	/ nothing	,
					  nothing	,
		"k"	+ "x"_S				>>= "l",
		"l"_S	+ "x"				>>= "m",
		"m"	+ "x"		/ nothing	>>= "n",
			+ "x"_S				>>= "o",
			+ "x"_S		/ nothing	>>= "p",
		"n"	+ "x"_S				,
		"o"	+ "x"		/ nothing	,
			+ "x"_S				,
			+ "x"_S		/ nothing	,
		"p"	+ input("x")			>>= "q",
		"q"	+ input("x")	/ nothing	>>= "r",
			+ input("x")			>>= "s",
			+ input("x")	/ nothing	>>= "a",
		"r"	+ input("x")			,
		"s"	+ input("x")	/ nothing	,
			+ input("x")			,
			  input("x")			,
			+ input("x")	/ nothing	,
			  input("x")	/ nothing
	);
	// clang-format on

	transitions.dump();
}

TEST(Fsm14, InvalidFirst)
{
	using namespace zth::fsm;

	EXPECT_THROW({ compile(always); }, invalid_fsm);
}

TEST(Fsm14, NonContiguous)
{
	using namespace zth::fsm;

	EXPECT_THROW(
		{
			// clang-format on
			compile("a", "b", "a");
			// clang-format off
	}, invalid_fsm);
}

TEST(Fsm14, UnknownTarget)
{
	using namespace zth::fsm;

	EXPECT_THROW({
			// clang-format on
			compile("a" + always >>= "b");
			// clang-format off
	}, invalid_fsm);
}

TEST(Fsm14, Empty)
{
	using namespace zth::fsm;

	constexpr auto transitions = compile();
	auto fsm = transitions.spawn();
	EXPECT_TRUE(fsm.valid());
	fsm.run(true);

	Fsm fsm2;
	EXPECT_FALSE(fsm2.valid());
	transitions.init(fsm2);
	EXPECT_TRUE(fsm2.valid());
	fsm2.run(true);
}

static bool f_check_flag;

static void f_check()
{
	f_check_flag = true;
}

TEST(Fsm14, CallbackFunction)
{
	using namespace zth::fsm;

	static constexpr auto check = action(f_check);

	// clang-format off
	static constexpr auto transitions = compile(
		"a" + entry / check
	);
	// clang-format on

	auto fsm = transitions.spawn();
	EXPECT_TRUE(fsm.valid());
	EXPECT_FALSE(f_check_flag);

	fsm.run(true);
	EXPECT_TRUE(f_check_flag);
}

struct CheckFsm : public zth::fsm::Fsm {
	void check()
	{
		flag = true;
	}

	void check_const() const
	{
		flag = true;
	}

	mutable bool flag = false;
};

TEST(Fsm14, CallbackMember)
{
	using namespace zth::fsm;

	static constexpr auto check = action(&CheckFsm::check);

	// clang-format off
	static constexpr auto transitions = compile(
		"a" + entry / check
	);
	// clang-format on

	CheckFsm fsm;
	EXPECT_FALSE(fsm.valid());

	transitions.init(fsm);
	EXPECT_TRUE(fsm.valid());
	EXPECT_FALSE(fsm.flag);

	fsm.run(true);
	EXPECT_TRUE(fsm.flag);
}

TEST(Fsm14, CallbackConstMember)
{
	using namespace zth::fsm;

	static constexpr auto check = action(&CheckFsm::check_const);

	// clang-format off
	static constexpr auto transitions = compile(
		"a" + entry / check
	);
	// clang-format on

	CheckFsm fsm;
	EXPECT_FALSE(fsm.valid());

	transitions.init(fsm);
	EXPECT_TRUE(fsm.valid());
	EXPECT_FALSE(fsm.flag);

	fsm.run(true);
	EXPECT_TRUE(fsm.flag);
}

struct CountingFsm : public zth::fsm::Fsm {
	void action_()
	{
		count++;
	}

	bool guard_true_()
	{
		count++;
		return true;
	}

	bool guard_false_()
	{
		count++;
		return false;
	}

	int count = 0;
};

static constexpr auto count_action = zth::fsm::action(&CountingFsm::action_);
static constexpr auto count_guard_true = zth::fsm::guard(&CountingFsm::guard_true_);
static constexpr auto count_guard_false = zth::fsm::guard(&CountingFsm::guard_false_);

TEST(Fsm14, Action)
{
	using namespace zth::fsm;

	// clang-format off
	static constexpr auto transitions = compile(
		"a"		/ count_action	>>= "b",
		"b" + never	/ count_action	>>= "a",
		"b" + entry	/ count_action
	);
	// clang-format on

	CountingFsm fsm;
	transitions.init(fsm);
	fsm.run(true);

	EXPECT_EQ(fsm.count, 2);
}

TEST(Fsm14, GuardFalse)
{
	using namespace zth::fsm;

	// clang-format off
	static constexpr auto transitions = compile(
		"a" + count_guard_false	>>= "b",
		"a" + count_guard_false	>>= "b",
		"a" + always		>>= "b",
		"b" + count_guard_false
	);
	// clang-format on

	CountingFsm fsm;
	transitions.init(fsm);
	fsm.run(true);

	EXPECT_EQ(fsm.count, 3);
}

TEST(Fsm14, GuardTrue)
{
	using namespace zth::fsm;

	// clang-format off
	static constexpr auto transitions = compile(
		"a" + count_guard_true	>>= "b",
		"a" + count_guard_false	>>= "b",
		"a" + always		>>= "b",
		"b" + count_guard_false
	);
	// clang-format on

	CountingFsm fsm;
	transitions.init(fsm);
	fsm.run(true);

	EXPECT_EQ(fsm.count, 2);
}

TEST(Fsm14, Entry)
{
	using namespace zth::fsm;

	// clang-format off
	static constexpr auto transitions = compile(
		"a"	+ entry		/ count_action	,
			+ always			>>= "b",
		"b"	+ entry		/ count_action	>>= "c",
		"c"
	);
	// clang-format on

	CountingFsm fsm;
	transitions.init(fsm);
	fsm.run(true);

	EXPECT_EQ(fsm.count, 2);
}

TEST(Fsm14, Stop)
{
	using namespace zth::fsm;

	// clang-format off
	static constexpr auto transitions = compile(
		"a"			>>= "b"_S,
		"b"	+ entry / stop	,
			+ always	>>= "c",
		"c"
	);
	// clang-format on

	auto fsm = transitions.spawn();
	fsm.run();

	EXPECT_TRUE(fsm.flag(Fsm::Flag::stop));
	EXPECT_EQ(fsm.state(), "b"_S);

	fsm.run(true);
	EXPECT_FALSE(fsm.flag(Fsm::Flag::stop));
	EXPECT_EQ(fsm.state(), "c"_S);
}

TEST(Fsm14, Stack)
{
	using namespace zth::fsm;

	// clang-format off
	static constexpr auto transitions = compile(
		"a"	+ entry		/ count_action	,		// hit twice
			+ popped			>>= "b",
			+ always	/ push		>>= "x",
		"b"	+ popped			>>= "c",
			+ always	/ push		>>= "x",
		"c"					,

		"x"			/ count_action	>>= "y",	// hit twice
		"y"			/ pop
	);
	// clang-format on

	CountingFsm fsm;
	transitions.init(fsm);
	fsm.run(true);

	EXPECT_EQ(fsm.state(), "c"_S);
	EXPECT_EQ(fsm.count, 4);
}

TEST(Fsm14, Input)
{
	using namespace zth::fsm;

	// clang-format off
	static constexpr auto transitions = compile(
		"a"	+ "x"			>>= "b",
		"b"	+ "x"	/ consume	>>= "c",
		"c"
	);
	// clang-format on

	auto fsm = transitions.spawn();
	fsm.run(true);
	EXPECT_EQ(fsm.state(), "a"_S)

	fsm.input("y");
	fsm.run(true);
	EXPECT_EQ(fsm.state(), "a"_S);

	fsm.input("x");
	fsm.run(true);
	EXPECT_EQ(fsm.state(), "c"_S);
	EXPECT_FALSE(fsm.hasInput("x"));
	EXPECT_TRUE(fsm.hasInput("y"));
}

TEST(Fsm14, DelayedGuard)
{
	using namespace zth::fsm;

	// clang-format off
	static constexpr auto transitions = compile(
		"a"	+ timeout_ms<100>		>>= "b",
		"b"				/ stop
	);
	// clang-format on

	auto fsm = transitions.spawn();
	fsm.run();
}
