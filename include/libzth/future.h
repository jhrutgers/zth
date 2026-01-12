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
	explicit std_shared_future_base(Future_type* future = nullptr) noexcept
		: m_future{future}
	{}

public:
	bool valid() const noexcept
	{
		return m_future != nullptr;
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

	zth::Future<T>* m_future = nullptr;
};

template <typename T>
class std_future_base : public impl::std_shared_future_base<T> {
public:
	using base = impl::std_shared_future_base<T>;

protected:
	explicit std_future_base(typename base::Future_type* future = nullptr) noexcept
		: base{future}
	{}

	std_future_base(std_future_base&& other) noexcept
	{
		*this = std::move(other);
	}

public:
	std_future_base& operator=(std_future_base&& other)
	{
		this->m_future = other.m_future;
		other.m_future = nullptr;
		return *this;
	}

	std_future_base(std_future_base const&) = delete;
	std_future_base& operator=(std_future_base const&) = delete;

	std::shared_future<zth::type<T>> share() const
	{
		auto f = std::shared_future<zth::type<T>>(this->m_future);
		this->m_future = nullptr;
		return f;
	}
};

template <typename T>
class std_promise_base {
public:
	using value_type = T;
	using Future_type = zth::Future<value_type>;

protected:
	explicit std_promise_base(Future_type* future = nullptr) noexcept
		: m_future{future}
	{}

	std_promise_base(std_promise_base&& other) noexcept
	{
		*this = std::move(other);
	}

public:
	std_promise_base& operator=(std_promise_base&& other) noexcept
	{
		this->m_future = other.m_future;
		other.m_future = nullptr;
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
	Future_type* m_future = nullptr;
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

	// cppcheck-suppress noExplicitConstructor
	future(typename base::Future_type* f = nullptr) noexcept
		: base{f}
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

	// cppcheck-suppress noExplicitConstructor
	future(typename base::Future_type* f = nullptr) noexcept
		: base{f}
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

	// cppcheck-suppress noExplicitConstructor
	future(typename base::Future_type* f = nullptr) noexcept
		: base{f}
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

	explicit promise(typename base::Future_type* future = nullptr) noexcept
		: base{future}
	{}
};

template <typename T>
class promise<zth::type<T&>> : public zth::impl::std_promise_base<T*> {
public:
	using value_type = T&;
	using base = zth::impl::std_promise_base<T*>;

	explicit promise(typename base::Future_type* future = nullptr) noexcept
		: base{future}
	{}
};

template <>
class promise<zth::type<void>> : public zth::impl::std_promise_base<void> {
public:
	using base = zth::impl::std_promise_base<void>;

	explicit promise(typename base::Future_type* future = nullptr) noexcept
		: base{future}
	{}
};

} // namespace std

#endif // C++11
#endif // ZTH_FUTURE_H
