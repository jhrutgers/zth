#ifndef ZTH_CORO_H
#define ZTH_CORO_H
/*
 * SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#if defined(__cplusplus) && __cplusplus >= 202002L
#  define ZTH_HAVE_CORO
#endif

#ifdef ZTH_HAVE_CORO
#  include <libzth/macros.h>

#  include <libzth/exception.h>
#  include <libzth/sync.h>

#  include <coroutine>

namespace zth {
namespace coro {

template <typename T>
class task_promise;

template <typename T = void>
struct task {
	using return_type = T;
	using promise_type = task_promise<return_type>;
	using Future_type = typename promise_type::Future_type;

	explicit task(std::coroutine_handle<promise_type> h) noexcept
		: m_promise{h.promise()}
	{}

	explicit task(zth::SharedPointer<promise_type> const& p) noexcept
		: m_promise{std::move(p)}
	{}

	explicit task(zth::SharedPointer<promise_type>&& p) noexcept
		: m_promise{std::move(p)}
	{}

	explicit task(promise_type* p = nullptr) noexcept
		: m_promise{p}
	{}

	bool completed() const noexcept
	{
		auto* p = m_promise.get();
		return !p || p->completed();
	}

	Future_type& future() const
	{
		auto* p = m_promise.get();
		if(!p)
			zth_throw(coro_already_completed{});

		return p->future();
	}

	decltype(auto) result()
	{
		return *future();
	}

	void resume()
	{
		auto* p = m_promise.get();
		if(!p)
			zth_throw(coro_already_completed{});

		p->resume();
	}

	decltype(auto) run()
	{
		auto* p = m_promise.get();
		if(!p)
			zth_throw(coro_already_completed{});

		p->run();
		return result();
	}

	Future_type& to_fiber()
	{
		zth::fiber([](task t) { t.run(); }, *this);
		return future();
	}

	auto& operator co_await() noexcept
	{
		return *this;
	}

	bool await_ready() const noexcept
	{
		return completed();
	}

	void await_suspend(std::coroutine_handle<> UNUSED_PAR(h))
	{
		auto* p = m_promise.get();
		if(!p)
			return;

		if(p->running()) {
			// Probably running in another fiber. Just wait.
			zth_dbg(coro, "[%s] Awaited by %p", p->id_str(), (void*)h.address());

			p->future().wait();
		} else {
			zth_dbg(coro, "[%s] Awaited by %p; resuming", p->id_str(),
				(void*)h.address());

			p->run();
		}
	}

	decltype(auto) await_resume()
	{
		return result();
	}

private:
	zth::SharedPointer<promise_type> m_promise;
};

class promise_base : public RefCounted, public UniqueID<promise_base> {
public:
	// NOLINTNEXTLINE
	static void* operator new(std::size_t n)
	{
		return ::zth::allocate<char>(n);
	}

	static void operator delete(void* ptr, std::size_t n)
	{
		::zth::deallocate<char>(static_cast<char*>(ptr), n);
	}

	virtual ~promise_base() noexcept override = default;

protected:
	promise_base(cow_string const& name)
		: UniqueID{name}
	{}
};

template <typename Promise>
class promise : public promise_base {
public:
	using promise_type = Promise;

protected:
	promise(cow_string const& name)
		: promise_base{name}
	{}

public:
	virtual ~promise() noexcept override = default;

	auto handle() noexcept
	{
		return std::coroutine_handle<promise_type>::from_promise(self());
	}

	std::suspend_always initial_suspend() noexcept
	{
		return {};
	}

	std::suspend_always final_suspend() noexcept
	{
		// Make sure to suspend, otherwise the coroutine is destroyed immediately upon
		// return. However, we need RefCounted to manage this.
		return {};
	}

	bool running() const noexcept
	{
		return m_running;
	}

	void resume()
	{
		if(m_running)
			return;

		auto h = handle();
		zth_assert(h);

		if(h.done())
			return;

		m_running = true;
		zth_dbg(coro, "[%s] Resuming", id_str());
		h();
		m_running = false;
	}

	void run()
	{
		if(running())
			zth_throw(coro_invalid_state{});

		auto h = handle();
		zth_assert(h);

		m_running = true;
		zth_dbg(coro, "[%s] Run", id_str());

		try {
			while(!self().completed()) {
				h();
				zth::yield();
			}
		} catch(...) {
			m_running = false;
			zth_throw();
		}

		m_running = false;
	}

protected:
	promise_type const& self() const noexcept
	{
		return static_cast<promise_type const&>(*this);
	}

	promise_type& self() noexcept
	{
		return static_cast<promise_type&>(*this);
	}

private:
	// Invoked when RefCounted count reaches zero.
	virtual void cleanup() noexcept final
	{
		zth_dbg(coro, "[%s] Cleanup", id_str());
		handle().destroy();
	}

private:
	bool m_running = false;
};

template <typename T>
class task_promise final : public promise<task_promise<T>> {
public:
	using return_type = T;
	using Future_type = Future<return_type>;
	using task_type = task<return_type>;
	using base = promise<task_promise<return_type>>;

	/////////////////////////////////////////////////////
	// Administration
	//

	task_promise()
		: base{"coro::task"}
	{}

	virtual ~task_promise() noexcept override = default;

	Future_type& future() noexcept
	{
		return m_future;
	}

	bool completed() const noexcept
	{
		return m_future.valid();
	}

	auto get_return_object()
	{
		zth_dbg(coro, "[%s] New %p", this->id_str(), this->handle().address());
		return task{this};
	}

	template <typename T_>
	void return_value(T_&& v)
	{
		zth_dbg(coro, "[%s] Returned", this->id_str());
		m_future.set(std::forward<T_>(v));
	}

	void unhandled_exception()
	{
		zth_dbg(coro, "[%s] Exit with exception", this->id_str());
		m_future.set(std::current_exception());
	}

private:
	Future_type m_future;
};

template <>
class task_promise<void> final : public promise<task_promise<void>> {
public:
	using return_type = void;
	using Future_type = Future<return_type>;
	using task_type = task<return_type>;
	using base = promise<task_promise<return_type>>;

	task_promise()
		: base{"coro::task"}
	{}

	virtual ~task_promise() noexcept override = default;

	Future_type& future() noexcept
	{
		return m_future;
	}

	bool completed() const noexcept
	{
		return m_future.valid();
	}

	auto get_return_object()
	{
		zth_dbg(coro, "[%s] New %p", this->id_str(), this->handle().address());
		return task{this};
	}

	template <typename X = void>
	void return_void()
	{
		m_future.set();
	}

	void unhandled_exception()
	{
		zth_dbg(coro, "[%s] Exit with exception", this->id_str());
		m_future.set(std::current_exception());
	}

private:
	Future_type m_future;
};

} // namespace coro
} // namespace zth

static inline decltype(auto) operator co_await(zth::Mutex& m) noexcept
{
	struct awaitable {
		zth::Mutex& mutex;

		bool await_ready() const noexcept
		{
			return true;
		}

		void await_suspend(std::coroutine_handle<> UNUSED_PAR(h)) noexcept {}

		zth::Locked await_resume() const
		{
			return zth::Locked{mutex};
		}
	};

	return awaitable{m};
}

static inline decltype(auto) operator co_await(zth::Semaphore& s) noexcept
{
	struct awaitable {
		zth::Semaphore& sem;

		bool await_ready() const noexcept
		{
			return true;
		}

		void await_suspend(std::coroutine_handle<> UNUSED_PAR(h)) noexcept {}

		decltype(auto) await_resume() const
		{
			return sem.acquire();
		}
	};

	return awaitable{s};
}

static inline decltype(auto) operator co_await(zth::Signal& s) noexcept
{
	struct awaitable {
		zth::Signal& signal;

		bool await_ready() const noexcept
		{
			return true;
		}

		void await_suspend(std::coroutine_handle<> UNUSED_PAR(h)) noexcept {}

		decltype(auto) await_resume() const
		{
			return signal.wait();
		}
	};

	return awaitable{s};
}

template <typename T>
static inline decltype(auto) operator co_await(zth::Future<T>& f) noexcept
{
	struct awaitable {
		zth::Future<T>& future;

		bool await_ready() const noexcept
		{
			return future.valid();
		}

		void await_suspend(std::coroutine_handle<> UNUSED_PAR(h)) noexcept
		{
			future.wait();
		}

		decltype(auto) await_resume() const
		{
			return *future;
		}
	};

	return awaitable{f};
}

template <typename T>
static inline decltype(auto) operator co_await(zth::Mailbox<T>& m) noexcept
{
	struct awaitable {
		zth::Mailbox<T>& mailbox;

		bool await_ready() const noexcept
		{
			return mailbox.valid();
		}

		void await_suspend(std::coroutine_handle<> UNUSED_PAR(h)) noexcept
		{
			mailbox.wait();
		}

		decltype(auto) await_resume() const
		{
			return mailbox.take();
		}
	};

	return awaitable{m};
}

static inline decltype(auto) operator co_await(zth::Gate& m) noexcept
{
	struct awaitable {
		zth::Gate& gate;

		bool await_ready() const noexcept
		{
			return true;
		}

		void await_suspend(std::coroutine_handle<> UNUSED_PAR(h)) noexcept {}

		void await_resume() const
		{
			gate.wait();
		}
	};

	return awaitable{m};
}

#endif // ZTH_HAVE_CORO
#endif // ZTH_CORO_H
