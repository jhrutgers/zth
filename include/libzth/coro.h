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

static inline decltype(auto) awaitable(zth::Mutex& mutex) noexcept
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

static inline decltype(auto) awaitable(zth::Semaphore& sem) noexcept
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

static inline decltype(auto) awaitable(zth::Signal& signal) noexcept
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
static inline decltype(auto) awaitable(zth::Future<T>& future) noexcept
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

	std::coroutine_handle<> await_suspend(std::coroutine_handle<> h) noexcept
	{
		zth_assert(promise.handle().address() == h.address());

		if constexpr(requires(Awaitable a) { a.id_str(); })
			zth_dbg(coro, "[%s] Await suspend by %s", promise.id_str(),
				awaitable.id_str());
		else
			zth_dbg(coro, "[%s] Await suspend", promise.id_str());

		suspended = true;
		promise.running(false);

		if constexpr(std::is_void_v<decltype(awaitable.await_suspend(h))>) {
			awaitable.await_suspend(h);
			return h;
		} else if constexpr(std::is_same_v<bool, decltype(awaitable.await_suspend(h))>) {
			awaitable.await_suspend(h);
			return h;
		} else if constexpr(std::is_convertible_v<
					    decltype(awaitable.await_suspend(h)),
					    std::coroutine_handle<>>) {
			auto c = awaitable.await_suspend(h);
			return c ? c : h;
		} else {
			static_assert(
				(sizeof(Awaitable), false),
				"await_suspend must return void, bool, or std::coroutine_handle<>");
		}
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

template <typename Promise, typename Awaitable>
promise_awaitable(Promise&, Awaitable&&) -> promise_awaitable<Promise, std::decay_t<Awaitable>>;

class promise_base : public RefCounted, public UniqueID<promise_base> {
public:
	static ZTH_MALLOC_INLINE void operator delete(void* ptr, std::size_t n) noexcept
	{
		::zth::deallocate<char>(static_cast<char*>(ptr), n);
	}

	ZTH_MALLOC_ATTR((malloc, alloc_size(1)))
	__attribute__((returns_nonnull, warn_unused_result)) static void*
	operator new(std::size_t n)
	{
		return ::zth::allocate<char>(n);
	}

	virtual ~promise_base() noexcept override = default;

protected:
	explicit promise_base(cow_string const& name)
		: UniqueID{name}
	{}
};

template <typename Promise>
class promise : public promise_base {
public:
	using promise_type = Promise;

protected:
	explicit promise(cow_string const& name)
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
			promise& p;

			bool await_ready() noexcept
			{
				return false;
			}

			void await_suspend(std::coroutine_handle<>) noexcept {}

			void await_resume() noexcept
			{
				p.running(true);
				zth_dbg(coro, "[%s] Initial resume", p.id_str());
			}
		};

		return awaiter{*this};
	}

	decltype(auto) final_suspend() noexcept
	{
		struct impl {
			promise_type& p;

			bool await_ready() noexcept
			{
				return false;
			}

			void await_suspend(std::coroutine_handle<>) noexcept
			{
				p.running(false);
			}

			void await_resume() noexcept {}
		};

		return impl{self()};
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

		while(!self().completed())
			h();
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

template <typename Task, typename Fiber>
struct task_fiber {
	Task task;
	Fiber fiber;

	decltype(auto) operator*()
	{
		return *task.future();
	}

	decltype(auto) operator->()
	{
		return &*task.future();
	}

	template <typename M>
	requires(std::is_base_of_v<FiberManipulator, M>) task_fiber& operator<<(M const& m) &
	{
		zth_assert(!task.completed());
		fiber << m;
		return *this;
	}

	template <typename M>
	requires(std::is_base_of_v<FiberManipulator, M>) task_fiber&& operator<<(M const& m) &&
	{
		*this << m;
		return std::move(*this);
	}

	operator Fiber&() const noexcept
	{
		zth_assert(!task.completed());
		return fiber;
	}

	operator typename Task::Future_type &() noexcept
	{
		return task.future();
	}

	decltype(auto) operator<<(asFuture const&)
	{
		zth_assert(!task.completed());
		return task.future();
	}
};

template <typename Task, typename Fiber>
task_fiber(Task& t, Fiber&& f) -> task_fiber<Task, std::decay_t<Fiber>>;

template <typename T, typename F>
static inline void joinable(task_fiber<T, F> const& tf, Gate& g, Hook<Gate&>& join) noexcept
{
	(void)join;
	(Fiber&)tf << passOnExit(g);
}

template <typename T = void>
class task {
public:
	using return_type = T;
	using promise_type = task_promise<return_type>;
	using Future_type = typename promise_type::Future_type;

	explicit task(std::coroutine_handle<promise_type> h) noexcept
		: m_promise{h.promise()}
	{}

	explicit task(zth::SharedPointer<promise_type> const& p) noexcept
		: m_promise{p}
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
			return;

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

	decltype(auto) operator()()
	{
		return run();
	}

	decltype(auto) fiber(char const* name = "coro::fiber")
	{
		return task_fiber{*this, zth::factory([](task t) { t.run(); }, name)(*this)};
	}

	auto& operator co_await() noexcept
	{
		return *this;
	}

	bool await_ready() const noexcept
	{
		return completed() || promise()->running();
	}

	std::coroutine_handle<promise_type> await_suspend(std::coroutine_handle<> h) noexcept
	{
		auto* p = promise();
		zth_assert(p && !p->running());
		p->set_continuation(h);
		return p->handle();
	}

	decltype(auto) await_resume()
	{
		return result();
	}

private:
	zth::SharedPointer<promise_type> m_promise;
};

template <typename T>
class task_promise_base : public promise<task_promise<T>> {
public:
	using return_type = T;
	using base = promise<task_promise<return_type>>;
	using typename base::promise_type;
	using Future_type = Future<return_type>;
	using task_type = task<return_type>;

	task_promise_base()
		: base{"coro::task"}
	{}

	virtual ~task_promise_base() noexcept override = default;

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
		return task{static_cast<promise_type*>(this)};
	}

	// cppcheck-suppress duplInheritedMember
	decltype(auto) final_suspend() noexcept
	{
		struct impl {
			promise_type& p;
			decltype(std::declval<base>().final_suspend()) awaitable;

			bool await_ready() noexcept
			{
				return awaitable.await_ready();
			}

			std::coroutine_handle<> await_suspend(std::coroutine_handle<> h) noexcept
			{
				static_assert(std::is_void_v<decltype(awaitable.await_suspend(h))>);
				awaitable.await_suspend(h);
				return p.m_continuation ? p.m_continuation : std::noop_coroutine();
			}

			decltype(auto) await_resume() noexcept
			{
				return awaitable.await_resume();
			}
		};

		return impl{this->self(), base::final_suspend()};
	}

	void unhandled_exception() noexcept
	{
		zth_dbg(coro, "[%s] Exit with exception", this->id_str());
		m_future.set(std::current_exception());
	}

	void set_continuation(std::coroutine_handle<> cont) noexcept
	{
		zth_assert(!m_continuation || !cont || m_continuation == cont);
		m_continuation = cont;
	}

private:
	Future_type m_future;
	std::coroutine_handle<> m_continuation;
};

template <typename T>
class task_promise final : public task_promise_base<T> {
public:
	using base = task_promise_base<T>;
	using typename base::Future_type;
	using typename base::return_type;

	virtual ~task_promise() noexcept override = default;

	template <typename T_>
	void return_value(T_&& v) noexcept
	{
		zth_dbg(coro, "[%s] Returned", this->id_str());
		this->future().set(std::forward<T_>(v));
	}
};

template <>
class task_promise<void> final : public task_promise_base<void> {
public:
	using base = task_promise_base<void>;
	using return_type = void;
	using typename base::Future_type;

	virtual ~task_promise() noexcept override = default;

	void return_void() noexcept
	{
		zth_dbg(coro, "[%s] Returned", this->id_str());
		this->future().set();
	}
};



/////////////////////////////////////////////////////////////////////////////
// Generators
//
// A generator is a coroutine that produces a sequence of values. It does not produce a
// return value.
//

template <typename T>
class generator_promise;

template <typename T>
class Mailbox;

template <typename T>
static inline decltype(auto) awaitable(Mailbox<T>& mailbox) noexcept;

template <typename T>
class Mailbox : public zth::Mailbox<T> {
	ZTH_CLASS_NEW_DELETE(Mailbox)
public:
	using base = zth::Mailbox<T>;
	using type = typename base::type;
	using promise_type = generator_promise<T>;

	explicit Mailbox(promise_type& owner, cow_string const& name = "coro::Mailbox")
		: base{name}
		, m_owner{&owner}
	{}

	virtual ~Mailbox() noexcept override = default;

	friend decltype(auto) awaitable<T>(Mailbox<T>& mailbox) noexcept;

	promise_type& owner() const noexcept
	{
		zth_assert(!abandoned());
		return *m_owner;
	}

	std::coroutine_handle<> waiter() const noexcept
	{
		return m_waiter;
	}

	bool valid(std::coroutine_handle<> waiter) const noexcept
	{
		return waiter && base::valid(waiter.address());
	}

	using base::valid;

	void wait(Listable& item, std::coroutine_handle<> waiter) noexcept
	{
		this->enqueue(item, this->Queue_Take).user() = waiter.address();
		zth_dbg(coro, "[%s] Block coro %p", this->id_str(), waiter.address());
	}

	using base::wait;

	type take(std::coroutine_handle<> waiter)
	{
		if(m_waiter == waiter)
			m_waiter = std::coroutine_handle<>{};

		return base::take(waiter.address());
	}

	using base::take;

	void abandon()
	{
		m_owner = nullptr;
		this->unblockAll(base::Queue_Take);
	}

	bool abandoned() const noexcept
	{
		return !m_owner;
	}

protected:
	virtual void wait(typename base::assigned_type assigned) override
	{
		while(!valid(assigned)) {
			if(abandoned())
				zth_throw(coro_already_completed{});
			this->block((size_t)base::Queue_Take);
		}
	}

	virtual typename base::assigned_type
	wakeup(Listable& item, typename base::queue_type::user_type user,
	       bool prio) noexcept override
	{
		if(!user) {
			// A fiber woke up. Pass through, we are not waiting for fibers
			// here.
			return base::wakeup(item, user, prio);
		}

		zth_dbg(coro, "[%s] Unblock coro %s", this->id_str(), str(user).c_str());
		m_waiter = std::coroutine_handle<>::from_address(user);
		return user;
	}

private:
	promise_type* m_owner = nullptr;
	std::coroutine_handle<> m_waiter;
};

template <typename T>
static inline decltype(auto) awaitable(Mailbox<T>& mb) noexcept
{
	struct impl {
		Mailbox<T>& mailbox;
		Listable item{};
		std::coroutine_handle<> waiter{};

		char const* id_str() const noexcept
		{
			return mailbox.owner().id_str();
		}

		bool await_ready() noexcept
		{
			return mailbox.abandoned()
			       || mailbox.valid(typename Mailbox<T>::assigned_type{})
			       || mailbox.owner().running();
		}

		std::coroutine_handle<> await_suspend(std::coroutine_handle<> h) noexcept
		{
			waiter = h;
			mailbox.wait(item, waiter);
			return mailbox.owner().handle();
		}

		decltype(auto) await_resume()
		{
			if(mailbox.valid(waiter)) {
				// Got it.
				return mailbox.take(waiter);
			} else if(mailbox.abandoned()) {
				// The generator has completed. Can't resume.
				zth_throw(coro_already_completed{});
			} else {
				// The generator runs in another fiber. Just supend our
				// fiber while waiting.
				return mailbox.take();
			}
		}
	};

	return impl{mb};
}

template <typename T>
class generator_promise;

template <typename Generator, typename Fiber>
struct generator_fiber {
	Generator generator;
	Fiber fiber;

	template <typename M>
	requires(std::is_base_of_v<FiberManipulator, M>) generator_fiber& operator<<(M const& m) &
	{
		zth_assert(!generator.completed());
		fiber << m;
		return *this;
	}

	template <typename M>
	requires(std::is_base_of_v<FiberManipulator, M>) generator_fiber&& operator<<(M const& m) &&
	{
		*this << m;
		return std::move(*this);
	}

	operator Fiber&() const noexcept
	{
		zth_assert(!generator.completed());
		return fiber;
	}

private:
	void operator<<(asFuture const&);
};

template <typename Generator, typename Fiber>
generator_fiber(Generator& g, Fiber&& f) -> generator_fiber<Generator, std::decay_t<Fiber>>;

template <typename G, typename F>
static inline void joinable(generator_fiber<G, F> const& gf, Gate& g, Hook<Gate&>& join) noexcept
{
	(void)join;
	(Fiber&)gf << passOnExit(g);
}

template <typename T>
requires(!std::is_void_v<T>) class generator {
public:
	using return_type = T;
	using promise_type = generator_promise<return_type>;
	using Mailbox_type = typename promise_type::Mailbox_type;

	explicit generator(std::coroutine_handle<promise_type> h) noexcept
		: m_promise{h.promise()}
	{}

	explicit generator(zth::SharedPointer<promise_type> const& p) noexcept
		: m_promise{p}
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
		auto* p = m_promise.get();
		if(!p)
			zth_throw(coro_already_completed{});

		return p->value();
	}

	void resume()
	{
		auto* p = m_promise.get();
		if(!p)
			return;

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

	decltype(auto) fiber(char const* name = "coro::fiber")
	{
		return generator_fiber{
			*this, zth::factory([](generator g) { g.run(); }, name)(*this)};
	}

	/////////////////////////////////////////////////////
	// Range support
	//

	decltype(auto) begin()
	{
		auto* p = m_promise.get();
		if(!p)
			zth_throw(coro_already_completed{});

		return p->begin();
	}

	typename promise_type::end_type end()
	{
		return {};
	}

private:
	zth::SharedPointer<promise_type> m_promise;
};

template <template <typename> class G, typename T>
requires(std::is_same_v<std::remove_reference_t<G<T>>, generator<T>>) //
	static inline decltype(auto) awaitable(G<T> g) noexcept
{
	return awaitable(g.mailbox());
}

template <typename T>
class generator_promise final : public promise<generator_promise<T>> {
public:
	using yield_type = T;
	using Mailbox_type = Mailbox<std::conditional_t<
		std::is_reference_v<yield_type>,
		std::reference_wrapper<std::remove_reference_t<yield_type>>, yield_type>>;
	using generator_type = generator<yield_type>;
	using base = promise<generator_promise<yield_type>>;

	/////////////////////////////////////////////////////
	// Administration
	//

	generator_promise()
		: base{"coro::generator"}
		, m_mailbox{*this}
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

	decltype(auto) value()
	{
		if(completed())
			zth_throw(coro_already_completed{});

		return m_mailbox.take();
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
		h();
	}

	auto get_return_object() noexcept
	{
		zth_dbg(coro, "[%s] New %p", this->id_str(), this->handle().address());
		return generator{this};
	}

	template <std::convertible_to<yield_type&> T_>
	decltype(auto) yield_value(T_&& v) noexcept
	{
		zth_dbg(coro, "[%s] Yield", this->id_str());
		m_mailbox.put(std::forward<T_>(v));

		struct impl {
			generator_promise& self;
			bool await_ready() noexcept
			{
				return false;
			}

			std::coroutine_handle<>
			await_suspend(std::coroutine_handle<> UNUSED_PAR(h)) noexcept
			{
				auto waiter = self.mailbox().waiter();
				return waiter ? waiter : std::noop_coroutine();
			}

			void await_resume() noexcept {}
		};

		return this->await_transform(impl{*this});
	}

	// cppcheck-suppress duplInheritedMember
	decltype(auto) final_suspend() noexcept
	{
		struct impl {
			generator_promise& self;
			decltype(std::declval<base>().final_suspend()) awaitable;

			bool await_ready() noexcept
			{
				return awaitable.await_ready();
			}

			std::coroutine_handle<> await_suspend(std::coroutine_handle<> h) noexcept
			{
				static_assert(std::is_void_v<decltype(awaitable.await_suspend(h))>);
				awaitable.await_suspend(h);
				auto p = self.mailbox().waiter();
				self.mailbox().abandon();
				return p ? p : std::noop_coroutine();
			}

			decltype(auto) await_resume() noexcept
			{
				return awaitable.await_resume();
			}
		};

		return impl{*this, base::final_suspend()};
	}

	void return_void() noexcept
	{
		zth_dbg(coro, "[%s] Done", this->id_str());
		m_completed = true;
	}

	void unhandled_exception() noexcept
	{
		zth_dbg(coro, "[%s] Exit with exception", this->id_str());
		m_completed = true;
		m_mailbox.put(std::current_exception());
	}

	/////////////////////////////////////////////////////
	// Range support
	//

	struct end_type {};

	class iterator {
	public:
		explicit iterator(generator_promise& p)
			: m_promise{p}
		{}

		bool operator==(end_type end) const noexcept
		{
			return m_promise == end;
		}

		bool operator!=(end_type end) const noexcept
		{
			return m_promise != end;
		}

		iterator& operator++()
		{
			++m_promise;
			return *this;
		}

		decltype(auto) operator++(int)
		{
			return m_promise++;
		}

		decltype(auto) operator*()
		{
			return *m_promise;
		}

		decltype(auto) operator->()
		{
			return &(*m_promise);
		}

	private:
		generator_promise& m_promise;
	};

	iterator begin()
	{
		if(!completed() && !valid())
			generate();
		return iterator{*this};
	}

	end_type end() noexcept
	{
		return {};
	}

	bool operator==(end_type) const noexcept
	{
		return completed();
	}

	bool operator!=(end_type) const noexcept
	{
		return !completed();
	}

	generator_promise& operator++()
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

private:
	Mailbox_type m_mailbox;
	bool m_completed = false;
};

} // namespace coro
} // namespace zth

#endif // ZTH_HAVE_CORO
#endif // ZTH_CORO_H
