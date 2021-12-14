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

// This file is included by test_stack98.cpp and test_stack11.cpp.  The test
// cases are the same, but the C++98-compatible zth::stack_switch() implementation
// is enforced for testing purposes. They should behave the same, but the C++11
// implementation is more efficient.

#include <zth>

#include <gtest/gtest.h>

TEST(StackTest, Dummy)
{
	SUCCEED();
}

static char stack[0x1000];

static int f_vv() noexcept {}

TEST(StackTest, vv)
{
	zth::stack_switch(stack, sizeof(stack), &f_vv);
}

static int f_iv() noexcept
{
	return 1;
}

TEST(StackTest, iv)
{
	EXPECT_EQ(zth::stack_switch(stack, sizeof(stack), &f_iv), 1);
}

static int f_ii(int i) noexcept
{
	return i + 1;
}

TEST(StackTest, ii)
{
	EXPECT_EQ(zth::stack_switch(stack, sizeof(stack), &f_ii, 2), 3);
	EXPECT_EQ(zth::stack_switch(stack, sizeof(stack), &f_ii, 10), 11);
}

static int f_iRi(int& i) noexcept
{
	return ++i;
}

TEST(StackTest, iRi)
{
	int i = 4;

	zth::stack_switch<int, int&>(stack, sizeof(stack), &f_iRi, i);
	EXPECT_EQ(i, 5);

	i = 20;
	zth::stack_switch<int, int&>(stack, sizeof(stack), &f_iRi, i);
	EXPECT_EQ(i, 21);
}

static int f_iPi(int* i) noexcept
{
	return ++(*i);
}

TEST(StackTest, iPi)
{
	int i = 6;

	zth::stack_switch(stack, sizeof(stack), &f_iPi, &i);
	EXPECT_EQ(i, 7);

	i = 30;
	zth::stack_switch(stack, sizeof(stack), &f_iPi, &i);
	EXPECT_EQ(i, 31);
}

struct S {
	S()
	{
		printf("default ctor() %p\n", this);
	}

	S(S const& /*s*/)
	{
		printf("copy ctor() %p\n", this);
	}

	S(S&& /*s*/)
	{
		printf("move ctor() %p\n", this);
	}

	S& operator=(S const& /*s*/)
	{
		printf("copy assignment %p\n", this);
		return *this;
	}

	S& operator=(S&& /*s*/)
	{
		printf("move assignment %p\n", this);
		return *this;
	}
};

static S f_SS(S s) noexcept
{
	return s;
}

TEST(StackTest, SS)
{
	S s;
	zth::stack_switch(stack, sizeof(stack), &f_SS, s);
#if !ZTH_STACK_SWITCH98
	zth::stack_switch(stack, sizeof(stack), &f_SS, std::move(s));
#endif
	zth::stack_switch(stack, sizeof(stack), &f_SS, S());
}

static S f_SRSi(S& s, int /*o*/) noexcept
{
	return s;
}

TEST(StackTest, SRSi)
{
	S s;
	zth::stack_switch<S, S&>(stack, sizeof(stack), &f_SRSi, s, 100);
}

#if !ZTH_STACK_SWITCH98
static S& f_RSRS(S& s) noexcept
{
	return s;
}

TEST(StackTest, RSRS)
{
	S s;
	zth::stack_switch(stack, sizeof(stack), &f_RSRS, s);
}
#endif

static double f_ddRfPi(double d, float const& f, int* i) noexcept
{
	return d + f * *i;
}

TEST(StackTest, ddRfPi)
{
	float f = 2.3f;
	int i = 2;
	double res = zth::stack_switch<double, double, float const&, int*>(
		stack, sizeof(stack), &f_ddRfPi, 2.2, f, &i);
	EXPECT_NEAR(res, 6.8, 0.001);
}
