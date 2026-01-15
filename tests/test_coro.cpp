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

namespace zth {
namespace coro {

template <typename T>
class promise;

template <typename T = void>
// NOLINTNEXTLINE
struct task {
	using promise_type = zth::coro::promise<T>;

	explicit task(std::coroutine_handle<promise_type> h)
		: m_promise{h.promise()}
	{}

	explicit task(zth::SharedPointer<promise_type>&& p)
		: m_promise{std::move(p)}
	{}

	explicit task(promise_type* p = nullptr)
		: m_promise{p}
	{}

	struct resume_result {
		bool done;
		T value;
	};

	resume_result resume()
	{
		auto* p = m_promise.get();
		if(!p)
			throw std::runtime_error("Task already completed");

		if(!p->resume())
			return {false, T{}};

		return {true, p->get_future().value()};
	}

	T run()
	{
		while(true) {
			auto res = resume();
			if(res.done)
				return res.value;

			zth::outOfWork();
		}
	}

private:
	zth::SharedPointer<promise_type> m_promise;
};

template <typename T>
class promise : public zth::RefCounted {
public:
	using return_type = T;
	using Future_type = zth::Future<return_type>;

	// NOLINTNEXTLINE(cert-dcl54-cpp,hicpp-new-delete-operators,misc-new-delete-overloads)
	static void* operator new(std::size_t n)
	{
		return ::zth::allocate<char>(n);
	}

	static void operator delete(void* ptr, std::size_t n)
	{
		::zth::deallocate<char>(static_cast<char*>(ptr), n);
	}

	auto get_return_object()
	{
		printf("get_return_object\n");
		return task();
	}

	std::suspend_always initial_suspend()
	{
		printf("initial_suspend\n");
		return {};
	}

	std::suspend_always final_suspend() noexcept
	{
		printf("final_suspend\n");
		return {};
	}

	// NOLINTNEXTLINE
	void return_value(return_type&& v)
	{
		printf("return_value: %d\n", (int)v);
		m_future.set(std::move(v)); // NOLINT
	}

	void unhandled_exception()
	{
		printf("unhandled_exception\n");
		m_future.set(std::current_exception());
	}

	Future_type& get_future()
	{
		return m_future;
	}

	auto handle()
	{
		return std::coroutine_handle<promise>::from_promise(*this);
	}

	auto task()
	{
		return zth::coro::task<T>{this};
	}

	bool resume()
	{
		if(m_running)
			return false;

		auto h = handle();
		if(h.done())
			return true;

		m_running = true;
		h.resume();
		m_running = false;
		return false;
	}

protected:
	virtual void cleanup() noexcept override
	{
		handle().destroy();
	}

private:
	Future_type m_future;
	bool m_running = false;
};

} // namespace coro
} // namespace zth

template <typename T>
struct AwaitableFuture {
	// NOLINTNEXTLINE
	zth::Future<T>& future;

	// NOLINTNEXTLINE
	AwaitableFuture(zth::Future<T>& f)
		: future{f}
	{}

	auto operator co_await() const noexcept
	{
		printf("co_await AF\n");
		return *this;
	}

	bool await_ready() const noexcept
	{
		printf("await_ready\n");
		return future.valid();
	}

	void await_suspend(std::coroutine_handle<> h) noexcept
	{
		(void)h;
		printf("await_suspend\n");
		future.wait();
	}

	T await_resume() const
	{
		printf("await_resume\n");
		return future.value();
	}
};

void foo()
{
	int i = 20;
	printf("In foo, %p\n", (void*)&i);
}

TEST(Coro, Coro)
{
	zth::Future<int> f;
	AwaitableFuture<int> af{f};

	zth::fiber([&]() { f = 1; });

	// NOLINTNEXTLINE
	auto coro = [&]() -> zth::coro::task<int> {
		int i = 10;
		printf("In coroutine, %p\n", (void*)&i);
		foo();
		printf("Before co_await\n");
		auto x = co_await af;
		printf("After co_await; %d\n", x);
		co_return 42;
	}();

	auto result = coro.run();
	EXPECT_EQ(result, 42);
}
