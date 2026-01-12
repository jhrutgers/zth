/*
 * SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#define ZTH_TYPEDFIBER98 0
#define FiberTest	 CXX11FiberTest

// NOLINTNEXTLINE(bugprone-suspicious-include)
#include "test_fiber.cpp"

static void fiber_exception()
{
	throw std::runtime_error("Test exception");
}
zth_fiber(fiber_exception)

TEST(FiberTest, exception)
{
	zth_async fiber_exception();

	fiber_exception_future f = zth_async fiber_exception();
	f->wait();

	EXPECT_TRUE(f->exception());
}

static int fiber_exception_i()
{
	throw std::runtime_error("Test exception");
}
zth_fiber(fiber_exception_i)

TEST(FiberTest, exception_i)
{
	zth_async fiber_exception_i();

	fiber_exception_i_future f = zth_async fiber_exception_i();
	EXPECT_THROW(f->value(), std::runtime_error);
}
