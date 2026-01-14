#ifndef ZTH_FUTURE_H
#define ZTH_FUTURE_H
/*
 * SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#if defined(__cplusplus) && __cplusplus >= 201103L

#  include <libzth/macros.h>

#  include <libzth/sync.h>
#  include <libzth/time.h>

#  include <future>
#  include <stdexcept>
#  include <type_traits>



//////////////////////////////////////////////////////////////////////
// std::promise and std::future
//

namespace zth {
template <typename T>
struct type {};

template <typename T>
using promise = ::std::promise<zth::type<T>>;

template <typename T>
using future = ::std::future<zth::type<T>>;

template <typename T>
using shared_future = ::std::shared_future<zth::type<T>>;

namespace impl {
template <typename T>
class std_shared_future_base {
public:
	using value_type = T;
	using Future_type = zth::Future<value_type>;

protected:
	std_shared_future_base() noexcept = default;

	explicit std_shared_future_base(SharedPointer<Future_type>&& future) noexcept
		: m_future{std::move(future)}
	{}

public:
	bool valid() const noexcept
	{
		return m_future;
	}

	void wait() const
	{
		check_valid();
		m_future->wait();
	}

	template <class Rep, class Period>
	std::future_status
	wait_for(std::chrono::duration<Rep, Period> const& timeout_duration) const
	{
		check_valid();
		return m_future->block(timeout_duration) ? std::future_status::ready
							 : std::future_status::timeout;
	}

	template <class Clock, class Duration>
	std::future_status
	wait_until(const std::chrono::time_point<Clock, Duration>& timeout_time) const
	{
		check_valid();
		return m_future->block(timeout_time) ? std::future_status::ready
						     : std::future_status::timeout;
	}

protected:
	T value() const
	{
		check_valid();
		return m_future->value();
	}

	void check_valid() const
	{
		if(!valid())
			zth_throw(std::future_error(std::future_errc::no_state));
	}

	zth::SharedPointer<zth::Future<T>> m_future;
};

template <typename T>
class std_future_base : public impl::std_shared_future_base<T> {
public:
	using base = impl::std_shared_future_base<T>;

protected:
	std_future_base() noexcept = default;

	explicit std_future_base(SharedPointer<typename base::Future_type> future) noexcept
		: base{std::move(future)}
	{}

	std_future_base(std_future_base&& other) noexcept
	{
		*this = std::move(other);
	}

public:
	std_future_base& operator=(std_future_base&& other)
	{
		this->m_future = std::move(other.m_future);
		return *this;
	}

	std_future_base(std_future_base const&) = delete;
	std_future_base& operator=(std_future_base const&) = delete;

	std::shared_future<zth::type<T>> share() const
	{
		auto f = std::shared_future<zth::type<T>>(std::move(this->m_future));
		return f;
	}
};

template <typename T>
class std_promise_base {
public:
	using value_type = T;
	using Future_type = zth::Future<value_type>;

protected:
	std_promise_base()
		: m_future{new Future_type{}}
	{}

	explicit std_promise_base(SharedPointer<Future_type> future) noexcept
		: m_future{std::move(future)}
	{}

	std_promise_base(std_promise_base&& other) noexcept
	{
		*this = std::move(other);
	}

public:
	std_promise_base& operator=(std_promise_base&& other) noexcept
	{
		this->m_future = std::move(other.m_future);
		return *this;
	}

	std_promise_base(std_promise_base const&) = delete;
	std_promise_base& operator=(std_promise_base const&) = delete;

	void swap(std_promise_base& other) noexcept
	{
		std::swap(m_future, other.m_future);
	}

	std::future<zth::type<T>> get_future()
	{
		check_valid();

		if(m_got_future)
			zth_throw(std::future_error(std::future_errc::future_already_retrieved));

		m_got_future = true;
		return m_future;
	}

	template <typename U>
	void set_value(U&& value)
	{
		check_not_satisfied();
		m_future->set(std::forward<U>(value));
	}

	void set_value()
	{
		check_not_satisfied();
		m_future->set();
	}

	void set_exception(std::exception_ptr exception)
	{
		check_not_satisfied();
		m_future->set(std::move(exception));
	}

protected:
	void check_valid() const
	{
		if(!m_future)
			zth_throw(std::future_error(std::future_errc::no_state));
	}

	void check_not_satisfied() const
	{
		check_valid();

		if(m_future->valid())
			zth_throw(std::future_error(std::future_errc::promise_already_satisfied));
	}

private:
	SharedPointer<Future_type> m_future;
	bool m_got_future = false;
};

} // namespace impl
} // namespace zth

namespace std {
template <typename T>
class future<zth::type<T>> : public zth::impl::std_future_base<T> {
	ZTH_CLASS_NEW_DELETE(future)
public:
	using value_type = T;
	using base = zth::impl::std_future_base<T>;

	future() noexcept = default;

	// cppcheck-suppress noExplicitConstructor
	future(zth::SharedPointer<typename base::Future_type> f) noexcept
		: base{std::move(f)}
	{}

	value_type get()
	{
		return base::value();
	}
};

template <typename T>
class future<zth::type<T&>> : public zth::impl::std_future_base<T*> {
	ZTH_CLASS_NEW_DELETE(future)
public:
	using value_type = T&;
	using base = zth::impl::std_future_base<T*>;

	future() noexcept = default;

	// cppcheck-suppress noExplicitConstructor
	future(zth::SharedPointer<typename base::Future_type> f) noexcept
		: base{std::move(f)}
	{}

	value_type get()
	{
		return *base::value();
	}
};

template <>
class future<zth::type<void>> : public zth::impl::std_future_base<void> {
	ZTH_CLASS_NEW_DELETE(future)
public:
	using value_type = void;
	using base = zth::impl::std_future_base<void>;

	future() noexcept = default;

	// cppcheck-suppress noExplicitConstructor
	future(zth::SharedPointer<typename base::Future_type> f) noexcept
		: base{std::move(f)}
	{}

	void get()
	{
		wait();
	}
};

template <typename T>
class shared_future<zth::type<T>> : public zth::impl::std_shared_future_base<T> {
	ZTH_CLASS_NEW_DELETE(shared_future)
public:
	using value_type = T;
	using base = zth::impl::std_shared_future_base<T>;

	value_type get()
	{
		return base::value();
	}
};

template <typename T>
class shared_future<zth::type<T&>> : public zth::impl::std_shared_future_base<T*> {
	ZTH_CLASS_NEW_DELETE(shared_future)
public:
	using value_type = T&;
	using base = zth::impl::std_shared_future_base<T*>;

	value_type get()
	{
		return *base::value();
	}
};

template <>
class shared_future<zth::type<void>> : public zth::impl::std_shared_future_base<void> {
	ZTH_CLASS_NEW_DELETE(shared_future)
public:
	using value_type = void;
	using base = zth::impl::std_shared_future_base<void>;

	void get()
	{
		wait();
	}
};

template <typename T>
class promise<zth::type<T>> : public zth::impl::std_promise_base<T> {
	ZTH_CLASS_NEW_DELETE(promise)
public:
	using value_type = T;
	using base = zth::impl::std_promise_base<T>;

	promise() = default;

	// cppcheck-suppress noExplicitConstructor
	promise(zth::SharedPointer<typename base::Future_type> future) noexcept
		: base{std::move(future)}
	{}
};

template <typename T>
class promise<zth::type<T&>> : public zth::impl::std_promise_base<T*> {
public:
	using value_type = T&;
	using base = zth::impl::std_promise_base<T*>;

	promise() = default;

	// cppcheck-suppress noExplicitConstructor
	promise(zth::SharedPointer<typename base::Future_type> future) noexcept
		: base{std::move(future)}
	{}
};

template <>
class promise<zth::type<void>> : public zth::impl::std_promise_base<void> {
public:
	using base = zth::impl::std_promise_base<void>;

	promise() = default;

	// cppcheck-suppress noExplicitConstructor
	promise(zth::SharedPointer<typename base::Future_type> future) noexcept
		: base{std::move(future)}
	{}
};

} // namespace std



//////////////////////////////////////////////////////////////////////
// std::async
//

namespace zth {
enum class launch { detached = 1, awaitable = 2 };

namespace impl {
#  if __cplusplus < 201703L
template <typename Func, typename... Args>
using invoke_result = typename std::result_of<Func(Args...)>::type;
#  else
template <typename Func, typename... Args>
using invoke_result = typename std::invoke_result_t<Func, Args...>;
#  endif
} // namespace impl

template <typename Func, typename... Args>
zth::future<zth::impl::invoke_result<Func, Args...>> async(Func&& f, Args&&... args)
{
	return async(zth::launch::detached, std::forward<Func>(f), std::forward<Args>(args)...);
}

template <typename Func, typename... Args>
zth::future<zth::impl::invoke_result<Func, Args...>>
async(zth::launch policy, Func&& f, Args&&... args)
{
	auto& fib = zth::fiber(std::forward<Func>(f), std::forward<Args>(args)...);
	switch(policy) {
	case zth::launch::detached:
		return {};
	case zth::launch::awaitable:
		return fib.withFuture();
	default:
		zth_throw(std::invalid_argument("policy"));
	}
}
} // namespace zth

namespace std {
template <typename Func, typename... Args>
zth::future<zth::impl::invoke_result<Func, Args...>>
async(zth::launch policy, Func&& f, Args&&... args)
{
	return zth::async(policy, std::forward<Func>(f), std::forward<Args>(args)...);
}
} // namespace std


#endif // C++11
#endif // ZTH_FUTURE_H
