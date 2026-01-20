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

protected:
	using Future_type = zth::Future<value_type>;
	std_shared_future_base() noexcept = default;

	explicit std_shared_future_base(SharedPointer<Future_type> const& future) noexcept
		: m_future{future}
	{}

	explicit std_shared_future_base(SharedPointer<Future_type>&& future) noexcept
		: m_future{std::move(future)}
	{}

	explicit std_shared_future_base(SharedReference<Future_type> const& future) noexcept
		: m_future{future.valid() ? &future.get() : nullptr}
	{}

	explicit std_shared_future_base(SharedReference<Future_type>&& future) noexcept
		: m_future{static_cast<SharedPointer<Future_type>&&>(std::move(future))}
	{}

	explicit std_shared_future_base(AutoFuture<value_type>&& future) noexcept
		: m_future{static_cast<SharedPointer<Future_type>&&>(std::move(future))}
	{}

	// cppcheck-suppress-macro noExplicitConstructor
#  define ZTH_FUTURE_CTORS(future_class_type)                                    \
	  using Future_type = typename base::Future_type;                        \
                                                                                 \
	  future_class_type() noexcept = default;                                \
                                                                                 \
	  future_class_type(zth::SharedPointer<Future_type> const& f) noexcept   \
		  : base{f}                                                      \
	  {}                                                                     \
                                                                                 \
	  future_class_type(zth::SharedReference<Future_type> const& f) noexcept \
		  : base{f}                                                      \
	  {}

public:
	std_shared_future_base& operator=(std_shared_future_base&& other) noexcept = default;

	std_shared_future_base& operator=(SharedPointer<Future_type>&& other) noexcept
	{
		m_future = std::move(other);
		return *this;
	}

	std_shared_future_base& operator=(SharedReference<Future_type>&& other) noexcept
	{
		m_future = static_cast<SharedPointer<Future_type>&&>(std::move(other));
		return *this;
	}

	std_shared_future_base& operator=(AutoFuture<value_type>&& other) noexcept
	{
		m_future = static_cast<SharedPointer<Future_type>&&>(std::move(other));
		return *this;
	}

	// cppcheck-suppress-macro noExplicitConstructor
#  define ZTH_FUTURE_MOVE(future_class_type)                                               \
	  future_class_type(future_class_type&& other) noexcept = default;                 \
                                                                                           \
	  future_class_type(zth::SharedPointer<Future_type>&& f) noexcept                  \
		  : base{std::move(f)}                                                     \
	  {}                                                                               \
                                                                                           \
	  future_class_type(zth::SharedReference<Future_type>&& f) noexcept                \
		  : base{std::move(f)}                                                     \
	  {}                                                                               \
                                                                                           \
	  future_class_type(zth::AutoFuture<value_type>&& f) noexcept                      \
		  : base{std::move(f)}                                                     \
	  {}                                                                               \
                                                                                           \
	  future_class_type& operator=(future_class_type&& other) noexcept = default;      \
                                                                                           \
	  future_class_type& operator=(zth::SharedPointer<Future_type>&& other) noexcept   \
	  {                                                                                \
		  this->base::operator=(std::move(other));                                 \
		  return *this;                                                            \
	  }                                                                                \
                                                                                           \
	  future_class_type& operator=(zth::SharedReference<Future_type>&& other) noexcept \
	  {                                                                                \
		  this->base::operator=(std::move(other));                                 \
		  return *this;                                                            \
	  }                                                                                \
                                                                                           \
	  future_class_type& operator=(zth::AutoFuture<value_type>&& other) noexcept       \
	  {                                                                                \
		  this->base::operator=(std::move(other));                                 \
		  return *this;                                                            \
	  }

	std_shared_future_base& operator=(std_shared_future_base const& other) noexcept = default;

	std_shared_future_base& operator=(SharedPointer<Future_type> const& other) noexcept
	{
		m_future = other;
		return *this;
	}

	std_shared_future_base& operator=(SharedReference<Future_type> const& other) noexcept
	{
		m_future = other.valid() ? &other.get() : nullptr;
		return *this;
	}

#  define ZTH_FUTURE_COPY(future_class_type)                                                    \
	  future_class_type(future_class_type const& other) noexcept = default;                 \
                                                                                                \
	  future_class_type& operator=(future_class_type const& other) noexcept = default;      \
                                                                                                \
	  future_class_type& operator=(zth::SharedPointer<Future_type> const& other) noexcept   \
	  {                                                                                     \
		  this->base::operator=(other);                                                 \
		  return *this;                                                                 \
	  }                                                                                     \
                                                                                                \
	  future_class_type& operator=(zth::SharedReference<Future_type> const& other) noexcept \
	  {                                                                                     \
		  this->base::operator=(other);                                                 \
		  return *this;                                                                 \
	  }

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

	operator SharedPointer<Future_type> const&() const& noexcept
	{
		return m_future;
	}

	operator SharedPointer<Future_type>&&() && noexcept
	{
		return std::move(m_future);
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
	using value_type = typename base::value_type;

protected:
	ZTH_FUTURE_CTORS(explicit std_future_base)
	ZTH_FUTURE_MOVE(std_future_base)

public:
	std_future_base(std_future_base const&) = delete;
	std_future_base& operator=(std_future_base const&) = delete;

	std::shared_future<zth::type<T>> share() const noexcept
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

	std_promise_base(std_promise_base&& other) noexcept = default;

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
	using base = zth::impl::std_future_base<T>;
	using value_type = typename base::value_type;

	ZTH_FUTURE_CTORS(future)
	ZTH_FUTURE_MOVE(future)

	future(future const&) = delete;
	future& operator=(future const&) = delete;

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

	ZTH_FUTURE_CTORS(future)
	ZTH_FUTURE_MOVE(future)

	future(future const&) = delete;
	future& operator=(future const&) = delete;

	value_type get()
	{
		return *base::value();
	}
};

template <>
class future<zth::type<void>> : public zth::impl::std_future_base<void> {
	ZTH_CLASS_NEW_DELETE(future)
public:
	using base = zth::impl::std_future_base<void>;
	using value_type = void;

	ZTH_FUTURE_CTORS(future)
	ZTH_FUTURE_MOVE(future)

	future(future const&) = delete;
	future& operator=(future const&) = delete;

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

	ZTH_FUTURE_CTORS(shared_future)
	ZTH_FUTURE_MOVE(shared_future)
	ZTH_FUTURE_COPY(shared_future)

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

	ZTH_FUTURE_CTORS(shared_future)
	ZTH_FUTURE_MOVE(shared_future)
	ZTH_FUTURE_COPY(shared_future)

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

	ZTH_FUTURE_CTORS(shared_future)
	ZTH_FUTURE_MOVE(shared_future)
	ZTH_FUTURE_COPY(shared_future)

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

#  undef ZTH_FUTURE_CTORS
#  undef ZTH_FUTURE_MOVE
#  undef ZTH_FUTURE_COPY



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
	auto fib = zth::fiber(std::forward<Func>(f), std::forward<Args>(args)...);
	switch(policy) {
	case zth::launch::detached:
		return {};
	case zth::launch::awaitable:
		return fib << zth::asFuture();
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
