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



/////////////////////////////////////////////////////////////////////////////
// Awaitable types
//

template <typename T>
static inline decltype(auto) awaitable(T&& awaitable) noexcept
{
	return std::forward<T>(awaitable);
}

static inline decltype(auto) awaitable(Mutex& mutex) noexcept
{
	struct impl {
		zth::Mutex& mutex;

		char const* id_str() const noexcept
		{
			return mutex.id_str();
		}

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

	return impl{mutex};
}

static inline decltype(auto) awaitable(Semaphore& sem) noexcept
{
	struct impl {
		zth::Semaphore& sem;

		char const* id_str() const noexcept
		{
			return sem.id_str();
		}

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

	return impl{sem};
}

static inline decltype(auto) awaitable(Signal& signal) noexcept
{
	struct impl {
		zth::Signal& signal;

		char const* id_str() const noexcept
		{
			return signal.id_str();
		}

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

	return impl{signal};
}

template <typename T>
static inline decltype(auto) awaitable(Future<T>& future) noexcept
{
	struct impl {
		zth::Future<T>& future;

		char const* id_str() const noexcept
		{
			return future.id_str();
		}

		bool await_ready() const noexcept
		{
			return true;
		}

		void await_suspend(std::coroutine_handle<> UNUSED_PAR(h)) noexcept {}

		decltype(auto) await_resume() const
		{
			return *future;
		}
	};

	return impl{future};
}

template <typename T>
static inline decltype(auto) awaitable(zth::Mailbox<T>& mailbox) noexcept
{
	struct impl {
		zth::Mailbox<T>& mailbox;

		char const* id_str() const noexcept
		{
			return mailbox.id_str();
		}

		bool await_ready() const noexcept
		{
			return true;
		}

		void await_suspend(std::coroutine_handle<> UNUSED_PAR(h)) noexcept {}

		decltype(auto) await_resume() const
		{
			return mailbox.take();
		}
	};

	return impl{mailbox};
}

static inline decltype(auto) awaitable(zth::Gate& gate) noexcept
{
	struct impl {
		zth::Gate& gate;

		char const* id_str() const noexcept
		{
			return gate.id_str();
		}

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

	return impl{gate};
}



/////////////////////////////////////////////////////////////////////////////
// Promise base classes
//

template <typename Promise, typename Awaitable>
struct promise_awaitable {
	Promise& promise;
	Awaitable awaitable;
	bool suspended = false;

	char const* id_str() const noexcept requires(requires(Awaitable a) { a.id_str(); })
	{
		return awaitable.id_str();
	}

	decltype(auto) await_ready() noexcept
	{
		return awaitable.await_ready();
	}

	decltype(auto) await_suspend(std::coroutine_handle<> h) noexcept
	{
		zth_assert(promise.handle().address() == h.address());

		if constexpr(requires(Awaitable a) { a.id_str(); })
			zth_dbg(coro, "[%s] Await suspend by %s", promise.id_str(),
				awaitable.id_str());
		else
			zth_dbg(coro, "[%s] Await suspend", promise.id_str());

		suspended = true;
		promise.running(false);
		return awaitable.await_suspend(h);
	}

	decltype(auto) await_resume()
	{
		if(suspended) {
			promise.running(true);
			zth_dbg(coro, "[%s] Await resume", promise.id_str());
		}
		return awaitable.await_resume();
	}
};

class promise_base : public RefCounted, public UniqueID<promise_base> {
public:
	static void operator delete(void* ptr, std::size_t n) noexcept
	{
		::zth::deallocate<char>(static_cast<char*>(ptr), n);
	}

	ZTH_MALLOC_ATTR(
		(malloc((void (*)(void*, std::size_t))promise_base::operator delete, 1),
		 alloc_size(1)))
	__attribute__((returns_nonnull, warn_unused_result)) static void*
	operator new(std::size_t n)
	{
		return ::zth::allocate<char>(n);
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
	virtual ~promise() noexcept override
	{
		zth_assert(!running());
	}

	__attribute__((pure)) auto handle() noexcept
	{
		return std::coroutine_handle<promise_type>::from_promise(self());
	}

	decltype(auto) initial_suspend() noexcept
	{
		struct awaiter {
			std::reference_wrapper<promise> p;

			bool await_ready() noexcept
			{
				return false;
			}

			void await_suspend(std::coroutine_handle<>) noexcept {}

			void await_resume() noexcept
			{
				p.get().running(true);
			}
		};

		return awaiter{*this};
	}

	std::suspend_always final_suspend() noexcept
	{
		running(false);

		// Make sure to suspend, otherwise the coroutine is destroyed immediately upon
		// return. However, we need RefCounted to manage this.
		return {};
	}

	template <typename A>
	decltype(auto) await_transform(A&& a) noexcept
	{
		return promise_awaitable{self(), awaitable(std::forward<A>(a))};
	}

	template <typename P, typename A>
	friend class promise_awaitable;

	bool running() const noexcept
	{
		return m_running;
	}

	void resume()
	{
		if(running())
			// Runs in another fiber.
			return;

		auto h = handle();
		zth_assert(h);

		if(h.done())
			return;

		zth_dbg(coro, "[%s] Resume", id_str());
		h();
	}

	void run()
	{
		if(running())
			// Runs in another fiber.
			zth_throw(coro_invalid_state{});

		auto h = handle();
		zth_assert(h);

		zth_dbg(coro, "[%s] Run", id_str());

		while(!self().completed()) {
			h();
			zth::yield();
		}
	}

protected:
	void running(bool set) noexcept
	{
		zth_assert(m_running != set);
		m_running = set;
	}

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



/////////////////////////////////////////////////////////////////////////////
// Task
//
// A task is a coroutine that produces a single result value (or void).
//

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

	promise_type* promise() const noexcept
	{
		return m_promise.get();
	}

	char const* id_str() const noexcept
	{
		auto* p = promise();
		return p ? p->id_str() : "coro::task";
	}

	bool completed() const noexcept
	{
		auto* p = promise();
		return !p || p->completed();
	}

	Future_type& future() const
	{
		auto* p = promise();
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
		auto* p = promise();
		if(!p)
			zth_throw(coro_already_completed{});

		p->resume();
	}

	decltype(auto) run()
	{
		auto* p = promise();
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
		auto* p = promise();
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

	auto get_return_object() noexcept
	{
		zth_dbg(coro, "[%s] New %p", this->id_str(), this->handle().address());
		return task{this};
	}

	template <typename T_>
	void return_value(T_&& v) noexcept
	{
		zth_dbg(coro, "[%s] Returned", this->id_str());
		m_future.set(std::forward<T_>(v));
	}

	void unhandled_exception() noexcept
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

	auto get_return_object() noexcept
	{
		zth_dbg(coro, "[%s] New %p", this->id_str(), this->handle().address());
		return task{this};
	}

	template <typename X = void>
	void return_void() noexcept
	{
		m_future.set();
	}

	void unhandled_exception() noexcept
	{
		zth_dbg(coro, "[%s] Exit with exception", this->id_str());
		m_future.set(std::current_exception());
	}

private:
	Future_type m_future;
};



/////////////////////////////////////////////////////////////////////////////
// Generators
//
// A generator is a coroutine that produces a sequence of values. It does not produce a return
// value.
//

template <typename T>
class generator_promise;

template <typename T>
requires(!std::is_void_v<T>) struct generator {
	using return_type = T;
	using promise_type = generator_promise<return_type>;
	using Mailbox_type = typename promise_type::Mailbox_type;

	explicit generator(std::coroutine_handle<promise_type> h) noexcept
		: m_promise{h.promise()}
	{}

	explicit generator(zth::SharedPointer<promise_type> const& p) noexcept
		: m_promise{std::move(p)}
	{}

	explicit generator(zth::SharedPointer<promise_type>&& p) noexcept
		: m_promise{std::move(p)}
	{}

	explicit generator(promise_type* p = nullptr) noexcept
		: m_promise{p}
	{}

	bool completed() const noexcept
	{
		auto* p = m_promise.get();
		return !p || p->completed();
	}

	Mailbox_type& mailbox() const
	{
		auto* p = m_promise.get();
		if(!p)
			zth_throw(coro_already_completed{});

		return p->mailbox();
	}

	bool valid() const noexcept
	{
		auto* p = m_promise.get();
		return p && p->mailbox().valid();
	}

	decltype(auto) value()
	{
		return mailbox().take();
	}

	void resume()
	{
		auto* p = m_promise.get();
		if(!p)
			zth_throw(coro_already_completed{});

		p->resume();
	}

	void generate()
	{
		auto* p = m_promise.get();
		if(!p)
			zth_throw(coro_already_completed{});

		p->generate();
	}

	void run()
	{
		auto* p = m_promise.get();
		if(!p)
			zth_throw(coro_already_completed{});

		p->run();

		if(!completed())
			return;

#  ifdef ZTH_FUTURE_EXCEPTION
		auto exc = p->mailbox().exception();
		if(exc)
			std::rethrow_exception(exc);
#  endif // ZTH_FUTURE_EXCEPTION
	}

	Mailbox_type& to_fiber()
	{
		zth::fiber([](generator g) { g.run(); }, *this);
		return mailbox();
	}

	/////////////////////////////////////////////////////
	// Range support
	//

	generator& begin()
	{
		generate();
		return *this;
	}

	struct end_type {};

	end_type end()
	{
		return end_type{};
	}

	bool operator==(end_type) const
	{
		return completed();
	}

	bool operator!=(end_type) const
	{
		return !completed();
	}

	generator& operator++()
	{
		generate();
		return *this;
	}

	decltype(auto) operator++(int)
	{
		auto temp = *this;
		generate();
		return temp;
	}

	decltype(auto) operator*()
	{
		zth_assert(valid());
		return value();
	}

	decltype(auto) operator->()
	{
		zth_assert(valid());
		return &value();
	}

	/////////////////////////////////////////////////////
	// Coroutine support
	//

	auto& operator co_await() noexcept
	{
		return *this;
	}

	bool await_ready() const noexcept
	{
		return valid();
	}

	void await_suspend(std::coroutine_handle<> UNUSED_PAR(h))
	{
		auto* p = m_promise.get();
		if(!p)
			return;

		if(p->running()) {
			// Probably running in another fiber. Just wait.
			zth_dbg(coro, "[%s] Awaited by %p", p->id_str(), (void*)h.address());

			p->mailbox().wait();
		} else {
			zth_dbg(coro, "[%s] Awaited by %p; generating", p->id_str(),
				(void*)h.address());

			p->generate();
		}
	}

	decltype(auto) await_resume()
	{
		return value();
	}

private:
	zth::SharedPointer<promise_type> m_promise;
};

template <typename T>
class generator_promise final : public promise<generator_promise<T>> {
public:
	using yield_type = T;
	using Mailbox_type = Mailbox<yield_type>;
	using generator_type = generator<yield_type>;
	using base = promise<generator_promise<yield_type>>;

	/////////////////////////////////////////////////////
	// Administration
	//

	generator_promise()
		: base{"coro::generator"}
	{}

	virtual ~generator_promise() noexcept override = default;

	Mailbox_type const& mailbox() const noexcept
	{
		return m_mailbox;
	}

	Mailbox_type& mailbox() noexcept
	{
		return m_mailbox;
	}

	bool completed() const noexcept
	{
		return m_completed;
	}

	bool valid() const noexcept
	{
		return m_mailbox.valid() && !completed();
	}

	void generate()
	{
		if(completed())
			zth_throw(coro_already_completed{});
		if(mailbox().valid())
			return;
		if(this->running())
			// Just wait for another fiber to fill the mailbox.
			return;

		auto h = this->handle();
		zth_assert(h && !h.done());

		zth_dbg(coro, "[%s] Generate", this->id_str());
		try {
			this->running(true);
			do {
				h();
				zth::yield();
			} while(!completed() && !mailbox().valid());
			this->running(false);
		} catch(...) {
			this->running(false);
			zth_throw();
		}
	}

	auto get_return_object()
	{
		zth_dbg(coro, "[%s] New %p", this->id_str(), this->handle().address());
		return generator{this};
	}

	template <typename T_>
	decltype(auto) yield_value(T_&& v)
	{
		zth_dbg(coro, "[%s] Yield", this->id_str());
		m_mailbox.put(std::forward<T_>(v));

		struct awaiter {
			generator_promise& p;

			bool await_ready() const noexcept
			{
				return false;
			}

			void await_suspend(std::coroutine_handle<> UNUSED_PAR(h)) noexcept
			{
				p.running(false);
			}

			void await_resume() noexcept
			{
				p.running(true);
				p.mailbox().wait_empty();
			}
		};

		return awaiter{*this};
	}

	void return_void()
	{
		zth_dbg(coro, "[%s] Done", this->id_str());
		m_completed = true;
	}

	void unhandled_exception()
	{
		zth_dbg(coro, "[%s] Exit with exception", this->id_str());
		m_completed = true;
		zth_assert(!m_mailbox.valid());
		m_mailbox.put(std::current_exception());
	}

private:
	Mailbox_type m_mailbox;
	bool m_completed = false;
};

} // namespace coro
} // namespace zth

#endif // ZTH_HAVE_CORO
#endif // ZTH_CORO_H
