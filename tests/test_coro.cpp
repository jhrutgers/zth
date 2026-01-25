/*
 * SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#if __cplusplus < 202002L
#  error Compile as C++20 or newer.
#endif

#include <zth>

#include <coroutine>
#include <cstdio>

#include <gtest/gtest.h>

TEST(Coro, Mutex)
{
	zth::Mutex m;
	m.lock();

	// NOLINTNEXTLINE(cppcoreguidelines-avoid-capturing-lambda-coroutines)
	auto c = [&]() -> zth::coro::task<> {
		zth::fiber([&]() { m.unlock(); });
		co_await m;
	};

	c().run();
}

TEST(Coro, Semaphore)
{
	zth::Semaphore s{};

	// NOLINTNEXTLINE(cppcoreguidelines-avoid-capturing-lambda-coroutines)
	auto c = [&]() -> zth::coro::task<> {
		zth::fiber([&]() { s.release(); });
		co_await s;
	};

	c().run();
}

TEST(Coro, Signal)
{
	zth::Signal s{};

	// NOLINTNEXTLINE(cppcoreguidelines-avoid-capturing-lambda-coroutines)
	auto c = [&]() -> zth::coro::task<> {
		zth::fiber([&]() { s.signal(); });
		co_await s;
	};

	c().run();
}

TEST(Coro, Future)
{
	zth::Future<int> f;

	// NOLINTNEXTLINE(cppcoreguidelines-avoid-capturing-lambda-coroutines)
	auto c = [&]() -> zth::coro::task<int> {
		zth::fiber([&]() { f = 42; });
		co_return co_await f;
	};

	EXPECT_EQ(c().run(), 42);
}

TEST(Coro, Future_exception)
{
	zth::Future<int> f;

	// NOLINTNEXTLINE(cppcoreguidelines-avoid-capturing-lambda-coroutines)
	auto c = [&]() -> zth::coro::task<int> {
		zth::fiber([&]() { f.set(std::make_exception_ptr(std::runtime_error("error"))); });
		co_return co_await f;
	};

	EXPECT_THROW(c().run(), std::runtime_error);
}

TEST(Coro, Mailbox)
{
	zth::Mailbox<int> m;

	// NOLINTNEXTLINE(cppcoreguidelines-avoid-capturing-lambda-coroutines)
	auto c = [&]() -> zth::coro::task<> {
		co_await m;
		co_await m;
	};

	zth::fiber([&]() {
		m.put(1);
		m.put(std::make_exception_ptr(std::runtime_error("error")));
	});

	EXPECT_THROW(c().run(), std::runtime_error);
}

TEST(Coro, Gate)
{
	zth::Gate g{2};

	// NOLINTNEXTLINE(cppcoreguidelines-avoid-capturing-lambda-coroutines)
	auto c = [&]() -> zth::coro::task<> { co_await g; };
	zth::fiber([&]() { g.wait(); });
	c().run();
}

TEST(Coro, Nested)
{
	auto inner = []() -> zth::coro::task<int> {
		co_await std::suspend_always{};
		co_return 1;
	};

	// NOLINTNEXTLINE(cppcoreguidelines-avoid-capturing-lambda-coroutines)
	auto outer = [&]() -> zth::coro::task<int> {
		int x = co_await inner();
		co_return x + 1;
	};

	EXPECT_EQ(outer().run(), 2);
}

TEST(Coro, Generator_co2range)
{
	auto gen = []() -> zth::coro::generator<int> {
		for(int i = 0; i < 5; i++)
			co_yield i;
	};

	int expected = 0;
	for(auto&& v : gen()) {
		EXPECT_EQ(v, expected);
		expected++;
	}
	EXPECT_EQ(expected, 5);
}

TEST(Coro, Generator_coro2coro)
{
	auto g = []() -> zth::coro::generator<int> {
		for(int i = 0; i < 5; i++)
			co_yield i;
	};

	auto consumer = [](zth::coro::generator<int> g) -> zth::coro::task<int> {
		int sum = 0;
		for(int i = 0; i < 5; i++)
			sum += co_await g;
		co_return sum;
	};

	EXPECT_EQ(consumer(g()).run(), 10);
}

TEST(Coro, Generator_coro2fiber)
{
	auto g = []() -> zth::coro::generator<int> {
		for(int i = 0; i < 5; i++)
			co_yield i;
	};

	auto consumer = [](zth::coro::generator<int> g) -> zth::coro::task<int> {
		int sum = 0;
		for(int i = 0; i < 5; i++)
			sum += co_await g;
		co_return sum;
	};

	EXPECT_EQ(*consumer(g()).fiber(), 10);
}

TEST(Coro, Generator_coro2fibers)
{
	auto g = []() -> zth::coro::generator<int> {
		for(int i = 0; i < 5; i++)
			co_yield i;
	};

	// NOLINTNEXTLINE(performance-unnecessary-value-param)
	auto consumer = [](zth::coro::generator<int> g) -> zth::coro::task<int> {
		int sum = 0;
		try {
			while(true) {
				sum += co_await g;

				// Let the other fiber run too.
				zth::outOfWork();
			}
		} catch(zth::coro_already_completed const&) {
			co_return sum;
		}
	};

	auto gg = g();
	auto f1 = consumer(gg).fiber() << zth::setName("f1");
	auto f2 = consumer(gg).fiber() << zth::setName("f2");
	// As fibers yield in between generating, both should get some values.
	EXPECT_NE(*f1, 0);
	EXPECT_NE(*f2, 0);
	EXPECT_EQ(*f1 + *f2, 10);
}

TEST(Coro, Generator_fiber2fibers)
{
	auto g = []() -> zth::coro::generator<int> {
		for(int i = 0; i < 5; i++)
			co_yield i;
	};

	// NOLINTNEXTLINE(performance-unnecessary-value-param)
	auto consumer = [](zth::coro::generator<int> g) -> zth::coro::task<int> {
		int sum = 0;
		try {
			while(true)
				sum += co_await g;
		} catch(zth::coro_already_completed const&) {
			co_return sum;
		}
	};

	auto gg = g();
	gg.fiber() << zth::setName("generator");
	auto f1 = consumer(gg).fiber() << zth::setName("f1");
	auto f2 = consumer(gg).fiber() << zth::setName("f2");
	zth::join(f1, f2);
	EXPECT_NE(*f1, 0);
	EXPECT_NE(*f2, 0);
	EXPECT_EQ(*f1 + *f2, 10);
}
