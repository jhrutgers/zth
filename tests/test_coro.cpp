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
	auto inner = []() -> zth::coro::task<int> { co_return 1; };

	// NOLINTNEXTLINE(cppcoreguidelines-avoid-capturing-lambda-coroutines)
	auto outer = [&]() -> zth::coro::task<int> {
		int x = co_await inner();
		co_return x + 1;
	};

	EXPECT_EQ(outer().run(), 2);
}

void foo()
{
	int i = 20;
	printf("In foo, %p\n", (void*)&i);
}

TEST(Coro, Coro)
{
	zth::Future<int> f;

	zth::fiber([&]() { f = 1; });

	zth::Signal sig;

	auto c = [&]() -> zth::coro::task<int> {
		printf("In inner coroutine\n");
		co_return 1;
	};

	// NOLINTNEXTLINE
	auto coro = [&]() -> zth::coro::task<int> {
		int i = 10;
		printf("In coroutine, %p\n", (void*)&i);
		foo();
		printf("Before co_await\n");
		auto x = co_await f;
		printf("After co_await future; %d\n", x);

		x = co_await c();
		printf("After co_await coro; %d\n", x);

		sig.signal();

		co_return 42;
	};

	// NOLINTNEXTLINE
	auto cf = [&]() -> zth::coro::task<int> {
		printf("In coro fiber\n");
		sig.wait();
		printf("done coro fiber\n");
		co_return 1;
	};
	auto& fut = cf().to_fiber();

	auto result = coro().run();
	*fut;
	EXPECT_EQ(result, 42);
}
