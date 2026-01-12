/*
 * SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
 *
 * SPDX-License-Identifier: MPL-2.0
 */

// This file is included by test_fiber98.cpp and test_fiber11.cpp.  The test
// cases are the same, but the C++98-compatible zth::FiberType implementation
// is enforced for testing purposes. They should behave the same, but the C++11
// implementation is more efficient.

#include <zth>

#include <exception>
#include <future>

#include <gtest/gtest.h>

static void fiber_d(zth::promise<int> p)
{
	p.set_value(42);
}
zth_fiber(fiber_d)

TEST(Promise, d)
{
	zth::promise<int> p{new zth::Future<int>{}};
	auto f = p.get_future();
	zth_async fiber_d(std::move(p));

	auto i = f.get();
	EXPECT_EQ(i, 42);
}

static void fiber_exception(zth::promise<int> p)
{
	p.set_exception(std::make_exception_ptr(std::runtime_error("Test exception")));
}
zth_fiber(fiber_exception)

TEST(Promise, exception)
{
	zth::promise<int> p{new zth::Future<int>{}};
	auto f = p.get_future();
	zth_async fiber_exception(std::move(p));

	EXPECT_THROW(f.get(), std::runtime_error);
}

static void fiber_async() {}

TEST(Future, async)
{
	auto future = std::async(zth::launch::awaitable, fiber_async);
	EXPECT_EQ(future.valid(), true);
	future.wait();

	auto future2 = std::async(zth::launch::detached, fiber_async);
	EXPECT_EQ(future2.valid(), false);
	EXPECT_THROW(future2.get(), std::future_error);
}
