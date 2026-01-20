#ifndef ZTH_ASYNC_H
#define ZTH_ASYNC_H
/*
 * SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#include <libzth/macros.h>

#ifdef __cplusplus

#  include <libzth/allocator.h>
#  include <libzth/config.h>
#  include <libzth/exception.h>
#  include <libzth/fiber.h>
#  include <libzth/sync.h>
#  include <libzth/util.h>
#  include <libzth/worker.h>

#  if __cplusplus >= 201103L
#    include <tuple>
#    include <type_traits>
#  endif

#  if __cplusplus < 201103L
#    undef ZTH_TYPEDFIBER98
// Always use C++98-compatible TypedFiber implementation.
#    define ZTH_TYPEDFIBER98 1
#  elif !defined(ZTH_TYPEDFIBER98)
// By default only use C++11 TypedFiber implementation, but this can be
// overridden for testing purposes.
#    define ZTH_TYPEDFIBER98 0
#  endif

namespace zth {



///////////////////////////////////////////////////////////////////////////////////
// Typed fiber base class and manipulators
//

/*!
 * \brief Typed fiber class.
 * \tparam R Return type of the fiber function.
 *
 * This is a virtual base class. Derive from this class and implement the entry function.
 *
 * Dereferencing this fiber, forces the fiber to have a future, and the caller waits till the future
 * completes.
 */
template <typename R>
class TypedFiber : public Fiber {
	ZTH_CLASS_NEW_DELETE(TypedFiber)
public:
	typedef R Return;
	typedef Future<Return> Future_type;

	constexpr TypedFiber()
		: Fiber{&entry, this}
	{}

	virtual ~TypedFiber() override is_default

	void registerFuture(Future_type* future)
	{
		m_future.reset(future);
		zth_dbg(sync, "[%s] Registered to %s", future->id_str(), id_str());

		if(state() == Fiber::Dead) {
			// Fiber already dead, set the future now.
#  ifdef ZTH_FUTURE_EXCEPTION
			setFuture(std::make_exception_ptr(fiber_already_dead()));
#  else
			setFuture();
#  endif
		}
	}

	Future_type* future() const
	{
		return m_future;
	}

	SharedPointer<Future_type> const& withFuture()
	{
		if(!m_future.get()) {
			registerFuture(
				!Config::NamedSynchronizer
					? new Future_type()
					: new Future_type(
						  format("Future of %s", this->name().c_str())
							  .c_str()));
		}

		return m_future;
	}

	typename Future_type::indirection_type operator*()
	{
		return withFuture()->operator*();
	}

	typename Future_type::member_type operator->()
	{
		return withFuture()->operator->();
	}

protected:
	static void entry(void* that)
	{
		if(unlikely(!that))
			return;

		TypedFiber& fiber = *static_cast<TypedFiber*>(that);

#  ifdef ZTH_FUTURE_EXCEPTION
		try {
#  endif // ZTH_FUTURE_EXCEPTION
			fiber.entry_();
#  ifdef ZTH_FUTURE_EXCEPTION
		} catch(...) {
			fiber.setFuture(std::current_exception());
		}
#  endif // ZTH_FUTURE_EXCEPTION
	}

	virtual void entry_() = 0;

	template <typename T>
	void setFuture(T const& r)
	{
		if(m_future.get())
			m_future->set(r);
	}

	void setFuture()
	{
		if(m_future.get())
			m_future->set();
	}

#  ifdef ZTH_FUTURE_EXCEPTION
	void setFuture(std::exception_ptr exception)
	{
		if(m_future.get())
			m_future->set(std::move(exception));
	}
#  endif // ZTH_FUTURE_EXCEPTION
private:
	SharedPointer<Future_type> m_future;
};

struct FiberManipulator {};

/*!
 * \brief Change the stack size of a fiber returned by #zth_async.
 *
 * This is a manipulator that calls #zth::Fiber::setStackSize().
 * Example:
 * \code
 * void foo() { ... }
 * zth_fiber(foo)
 *
 * int main_fiber(int argc, char** argv) {
 *     zth_async foo() << zth::setStackSize(0x10000);
 *     return 0;
 * }
 * \endcode
 *
 * \ingroup zth_api_cpp_fiber
 */
struct setStackSize : public FiberManipulator {
	size_t stack;

	constexpr setStackSize(size_t s) noexcept
		: stack(s)
	{}
};

template <typename R>
TypedFiber<R>& operator<<(TypedFiber<R>& fiber, setStackSize const& m)
{
	int res = fiber.setStackSize(m.stack);
	if(res)
		zth_throw(errno_exception(res));
	return fiber;
}

/*!
 * \brief Change the name of a fiber returned by #zth_async.
 * \details This is a manipulator that calls #zth::Fiber::setName().
 * \see zth::setStackSize() for an example
 * \ingroup zth_api_cpp_fiber
 */
struct setName : public FiberManipulator {
#  if __cplusplus >= 201103L
	explicit setName(char const* n)
		: name(n ? std::string{n} : std::string{})
	{}

	explicit setName(string const& n)
		: name(n)
	{}
	explicit setName(string&& n)
		: name(std::move(n))
	{}
#  else	 // Pre C++11
	explicit setName(char const* n)
		: name(n)
	{}

	explicit setName(string const& n)
		: name(n.c_str())
	{}
#  endif // Pre C++11

#  if __cplusplus >= 201103L
	string name;
#  else
	char const* name;
#  endif
};

template <typename R>
TypedFiber<R>& operator<<(TypedFiber<R>& fiber, setName const& m)
{
	fiber.setName(m.name);
	return fiber;
}

#  if __cplusplus >= 201103L
template <typename R>
TypedFiber<R>& operator<<(TypedFiber<R>& fiber, setName&& m)
{
	fiber.setName(std::move(m.name));
	return fiber;
}
#  endif // C++11

/*!
 * \brief Makes the fiber pass the given gate upon exit.
 *
 * Example:
 * \code
 * void foo() { ... }
 * zth_fiber(foo)
 *
 * int main_fiber(int argc, char** argv) {
 *     zth::Gate gate(3);
 *     zth_async foo() << zth::passOnExit(gate);
 *     zth_async foo() << zth::passOnExit(gate);
 *     gate.wait();
 *     // If we get here, both foo()s have finished.
 *     return 0;
 * }
 * \endcode
 *
 * \ingroup zth_api_cpp_fiber
 */
struct passOnExit : public FiberManipulator {
public:
	constexpr explicit passOnExit(Gate& g) noexcept
		: gate(g)
	{}

	static void cleanup(Fiber& UNUSED_PAR(f), void* g)
	{
		reinterpret_cast<Gate*>(g)->pass();
	}

	Gate& gate;
};

template <typename R>
TypedFiber<R>& operator<<(TypedFiber<R>& fiber, passOnExit const& m)
{
	fiber.addCleanup(&passOnExit::cleanup, static_cast<void*>(&m.gate));
	return fiber;
}

/*!
 * \brief Automatic create a future for a fiber, when needed.
 *
 * By default, a fiber does not save its return value. When a fiber is assigned to this class, a
 * future is created automatically.
 */
template <typename T>
class AutoFuture : public SharedReference<Future<T> /**/> {
public:
	typedef SharedReference<Future<T> /**/> base;
	typedef Future<T> Future_type;

	~AutoFuture() noexcept is_default

	constexpr14 AutoFuture() noexcept
		: base()
	{}

	// cppcheck-suppress noExplicitConstructor
	constexpr14 AutoFuture(AutoFuture const& af) noexcept
		: base((base const&)af)
	{}

	// cppcheck-suppress noExplicitConstructor
	constexpr14 AutoFuture(base const& p) noexcept
		: base(p)
	{}

	// cppcheck-suppress noExplicitConstructor
	constexpr14 AutoFuture(SharedPointer<Future_type> const& p) noexcept
		: base(p)
	{}

#  if __cplusplus >= 201103L
	constexpr14 AutoFuture(SharedPointer<Future_type>&& p) noexcept
		: base(std::move(p))
	{}

	AutoFuture& operator=(SharedPointer<Future_type>&& p) noexcept
	{
		this->base::operator=(std::move(p));
		return *this;
	}

	constexpr14 AutoFuture(SharedReference<Future_type>&& p) noexcept
		: base(std::move(p))
	{}

	AutoFuture& operator=(SharedReference<Future_type>&& p) noexcept
	{
		this->base::operator=(std::move(p));
		return *this;
	}
#  endif // C++11

	// cppcheck-suppress noExplicitConstructor
	AutoFuture(TypedFiber<T>& fiber)
	{
		*this = fiber;
	}

	AutoFuture& operator=(TypedFiber<T>& fiber)
	{
		*this = AutoFuture(fiber.withFuture());
		return *this;
	}

	AutoFuture& operator=(AutoFuture const& af) noexcept
	{
		this->base::operator=((base const&)af);
		return *this;
	}

	AutoFuture& operator=(SharedPointer<Future_type> const& af) noexcept
	{
		this->base::operator=(af);
		return *this;
	}

	bool valid() const noexcept
	{
		return this->get() && this->get().valid();
	}

	void wait()
	{
		this->get().wait();
	}

	std::exception_ptr exception() const noexcept
	{
		return this->get() ? this->get().exception() : nullptr;
	}
};

/*!
 * \brief Forces the fiber to have a future that outlives the fiber.
 *
 * This is a manipulator that calls #zth::TypedFiber::withFuture().
 * Example:
 * \code
 * int foo() { ... }
 * zth_fiber(foo)
 *
 * int main_fiber(int argc, char** argv) {
 *     auto f = zth_async foo() << zth::asFuture();
 *     return *f;
 * }
 * \endcode
 *
 * \ingroup zth_api_cpp_fiber
 */
struct asFuture : public FiberManipulator {};

template <typename R>
AutoFuture<R> operator<<(TypedFiber<R>& fiber, asFuture const&)
{
	return fiber.withFuture();
}



///////////////////////////////////////////////////////////////////////////////////
// Actual TypedFiber implementations
//
// The TypedFiberX classes hold the actual entry point and arguments for the fiber.  For Pre-C++11,
// up to 3 arguments are supported. For C++11 and later, arbitrary arguments are supported.
//

#  if ZTH_TYPEDFIBER98
template <typename R>
class TypedFiber0 final : public TypedFiber<R> {
	ZTH_CLASS_NEW_DELETE(TypedFiber0)
public:
	typedef TypedFiber<R> base;
	typedef R (*Function)();

	explicit TypedFiber0(Function function)
		: m_function(function)
	{}

	virtual ~TypedFiber0() final is_default

protected:
	virtual void entry_() final
	{
		this->setFuture(m_function());
	}

private:
	Function m_function;
};

template <>
class TypedFiber0<void> final : public TypedFiber<void> {
	ZTH_CLASS_NEW_DELETE(TypedFiber0)
public:
	typedef TypedFiber<void> base;
	typedef void (*Function)();

	explicit TypedFiber0(Function func)
		: m_function(func)
	{}

	virtual ~TypedFiber0() final is_default

protected:
	virtual void entry_() final
	{
		m_function();
		this->setFuture();
	}

private:
	Function m_function;
};

template <typename R, typename A1>
class TypedFiber1 final : public TypedFiber<R> {
	ZTH_CLASS_NEW_DELETE(TypedFiber1)
public:
	typedef TypedFiber<R> base;
	typedef R (*Function)(A1);

	TypedFiber1(Function func, A1 a1)
		: m_function(func)
		, m_a1(a1)
	{}

	virtual ~TypedFiber1() final is_default

protected:
	virtual void entry_() final
	{
		this->setFuture(m_function(m_a1));
	}

private:
	Function m_function;
	A1 m_a1;
};

template <typename A1>
class TypedFiber1<void, A1> final : public TypedFiber<void> {
	ZTH_CLASS_NEW_DELETE(TypedFiber1)
public:
	typedef TypedFiber<void> base;
	typedef void (*Function)(A1);

	TypedFiber1(Function func, A1 a1)
		: m_function(func)
		, m_a1(a1)
	{}

	virtual ~TypedFiber1() final is_default

protected:
	virtual void entry_() final
	{
		m_function(m_a1);
		this->setFuture();
	}

private:
	Function m_function;
	A1 m_a1;
};

template <typename R, typename A1, typename A2>
class TypedFiber2 final : public TypedFiber<R> {
	ZTH_CLASS_NEW_DELETE(TypedFiber2)
public:
	typedef TypedFiber<R> base;
	typedef R (*Function)(A1, A2);

	TypedFiber2(Function func, A1 a1, A2 a2)
		: m_function(func)
		, m_a1(a1)
		, m_a2(a2)
	{}

	virtual ~TypedFiber2() final is_default

protected:
	virtual void entry_() final
	{
		this->setFuture(m_function(m_a1, m_a2));
	}

private:
	Function m_function;
	A1 m_a1;
	A2 m_a2;
};

template <typename A1, typename A2>
class TypedFiber2<void, A1, A2> final : public TypedFiber<void> {
	ZTH_CLASS_NEW_DELETE(TypedFiber2)
public:
	typedef TypedFiber<void> base;
	typedef void (*Function)(A1, A2);

	TypedFiber2(Function func, A1 a1, A2 a2)
		: m_function(func)
		, m_a1(a1)
		, m_a2(a2)
	{}

	virtual ~TypedFiber2() final is_default

protected:
	virtual void entry_() final
	{
		m_function(m_a1, m_a2);
		this->setFuture();
	}

private:
	Function m_function;
	A1 m_a1;
	A2 m_a2;
};

template <typename R, typename A1, typename A2, typename A3>
class TypedFiber3 final : public TypedFiber<R> {
	ZTH_CLASS_NEW_DELETE(TypedFiber3)
public:
	typedef TypedFiber<R> base;
	typedef R (*Function)(A1, A2, A3);

	TypedFiber3(Function func, A1 a1, A2 a2, A3 a3)
		: m_function(func)
		, m_a1(a1)
		, m_a2(a2)
		, m_a3(a3)
	{}

	virtual ~TypedFiber3() final is_default

protected:
	virtual void entry_() final
	{
		this->setFuture(m_function(m_a1, m_a2, m_a3));
	}

private:
	Function m_function;
	A1 m_a1;
	A2 m_a2;
	A3 m_a3;
};

template <typename A1, typename A2, typename A3>
class TypedFiber3<void, A1, A2, A3> final : public TypedFiber<void> {
	ZTH_CLASS_NEW_DELETE(TypedFiber3)
public:
	typedef TypedFiber<void> base;
	typedef void (*Function)(A1, A2, A3);

	TypedFiber3(Function func, A1 a1, A2 a2, A3 a3)
		: m_function(func)
		, m_a1(a1)
		, m_a2(a2)
		, m_a3(a3)
	{}

	virtual ~TypedFiber3() final is_default

protected:
	virtual void entry_() final
	{
		m_function(m_a1, m_a2, m_a3);
		this->setFuture();
	}

private:
	Function m_function;
	A1 m_a1;
	A2 m_a2;
	A3 m_a3;
};
#  endif // ZTH_TYPEDFIBER98

#  if __cplusplus >= 201103L
// Always try to move the argument, unless it's an lvalue reference.
template <typename T, typename std::enable_if<!std::is_lvalue_reference<T>::value, int>::type = 0>
static constexpr T&& move_or_ref(T& t) noexcept
{
	return std::move(t);
}

template <typename T, typename std::enable_if<std::is_lvalue_reference<T>::value, int>::type = 0>
static constexpr T move_or_ref(T t) noexcept
{
	return t;
}

/*!
 * \brief Actual fiber implementation for arbitrary function types and arguments.
 * \tparam F function type (function pointer, lambda, etc.)
 * \tparam R return type of \p F
 * \tparam Args argument types of \p F as \c std::tuple<...>
 */
template <typename F, typename R, typename Args>
class TypedFiberN final : public TypedFiber<R> {
	ZTH_CLASS_NEW_DELETE(TypedFiberN)
public:
	using base = TypedFiber<R>;
	using Function = F;

	template <typename F_, typename... Args_>
	TypedFiberN(F_&& func, Args_&&... args)
		: m_function{std::forward<F_>(func)}
		, m_args{std::forward<Args_>(args)...}
	{}

	virtual ~TypedFiberN() final = default;

protected:
	virtual void entry_() final
	{
		entry__(typename SequenceGenerator<std::tuple_size<Args>::value>::type());
	}

private:
	template <size_t... S>
	void entry__(Sequence<S...>)
	{
		this->setFuture(m_function(move_or_ref<typename std::tuple_element<S, Args>::type>(
			std::get<S>(m_args))...));
	}

private:
	Function m_function;
	Args m_args;
};

template <typename F, typename Args>
class TypedFiberN<F, void, Args> final : public TypedFiber<void> {
	ZTH_CLASS_NEW_DELETE(TypedFiberN)
public:
	using base = TypedFiber<void>;
	using Function = F;

	template <typename F_, typename... Args_>
	TypedFiberN(F_&& func, Args_&&... args)
		: m_function{std::forward<F_>(func)}
		, m_args{std::forward<Args_>(args)...}
	{}

	virtual ~TypedFiberN() final = default;

protected:
	virtual void entry_() final
	{
		entry__(typename SequenceGenerator<std::tuple_size<Args>::value>::type());
	}

private:
	template <size_t... S>
	void entry__(Sequence<S...>)
	{
		m_function(move_or_ref<typename std::tuple_element<S, Args>::type>(
			std::get<S>(m_args))...);
		this->setFuture();
	}

private:
	Function m_function;
	Args m_args;
};
#  endif // C++11



///////////////////////////////////////////////////////////////////////////////////
// Define TypedFiberType to extract function type information to pick the right TypedFiberX class.
//

#  if __cplusplus >= 201103L
template <typename F>
struct remove_function_cvref {};

#    define REMOVE_FUNCTION_CVREF_SPECIALIZATIONS(fun) \
	    template <typename R, typename... Args>    \
	    struct remove_function_cvref<fun> {        \
		    using type = R(Args...);           \
	    };

REMOVE_FUNCTION_CVREF_SPECIALIZATIONS(R(Args...))
REMOVE_FUNCTION_CVREF_SPECIALIZATIONS(R (*)(Args...))
REMOVE_FUNCTION_CVREF_SPECIALIZATIONS(R (&)(Args...))

#    define REMOVE_FUNCTION_CVREF_MEMBER_SPECIALIZATIONS(...)             \
	    template <typename R, typename C, typename... Args>           \
	    struct remove_function_cvref<R (C::*)(Args...) __VA_ARGS__> { \
		    using type = R(Args...);                              \
	    };

#    define REMOVE_FUNCTION_CVREF_MEMBER_SPECIALIZATIONS_CV(...)               \
	    REMOVE_FUNCTION_CVREF_MEMBER_SPECIALIZATIONS(__VA_ARGS__)          \
	    REMOVE_FUNCTION_CVREF_MEMBER_SPECIALIZATIONS(const __VA_ARGS__)    \
	    REMOVE_FUNCTION_CVREF_MEMBER_SPECIALIZATIONS(volatile __VA_ARGS__) \
	    REMOVE_FUNCTION_CVREF_MEMBER_SPECIALIZATIONS(const volatile __VA_ARGS__)

REMOVE_FUNCTION_CVREF_MEMBER_SPECIALIZATIONS_CV()
REMOVE_FUNCTION_CVREF_MEMBER_SPECIALIZATIONS_CV(&)
REMOVE_FUNCTION_CVREF_MEMBER_SPECIALIZATIONS_CV(&&)

#    if defined(__cpp_noexcept_function_type) && __cpp_noexcept_function_type >= 201510
REMOVE_FUNCTION_CVREF_MEMBER_SPECIALIZATIONS_CV(noexcept)
REMOVE_FUNCTION_CVREF_MEMBER_SPECIALIZATIONS_CV(& noexcept)
REMOVE_FUNCTION_CVREF_MEMBER_SPECIALIZATIONS_CV(&& noexcept)
#    endif // __cpp_noexcept_function_type

#    undef REMOVE_FUNCTION_CVREF_SPECIALIZATIONS
#    undef REMOVE_FUNCTION_CVREF_MEMBER_SPECIALIZATIONS
#    undef REMOVE_FUNCTION_CVREF_MEMBER_SPECIALIZATIONS_CV

template <typename T>
struct functor_operator_type {};

template <typename R, typename... Args>
struct functor_operator_type<R(Args...)> {
	using return_type = R;
	using args_type = std::tuple<Args...>;
};

// Extract function type from functor's operator().
template <typename F>
struct functor_traits {
	using functor_type = typename std::decay<F>::type;

private:
	using functor_operator_type_ = functor_operator_type<
		typename remove_function_cvref<decltype(&functor_type::operator())>::type>;

public:
	using return_type = typename functor_operator_type_::return_type;
	using args_type = typename functor_operator_type_::args_type;
};

// Extract function type from function pointer.
template <typename R, typename... Args>
struct functor_traits<R (*)(Args...)> {
public:
	using functor_type = R (*)(Args...);
	using return_type = R;
	using args_type = std::tuple<Args...>;
};

template <typename R, typename... Args>
struct functor_traits<R (*&)(Args...)> {
public:
	using functor_type = R (*)(Args...);
	using return_type = R;
	using args_type = std::tuple<Args...>;
};

// Extract function type from function reference.
template <typename R, typename... Args>
struct functor_traits<R (&)(Args...)> {
public:
	using functor_type = R (&)(Args...);
	using return_type = R;
	using args_type = std::tuple<Args...>;
};

// Extract function type from function type.
template <typename R, typename... Args>
struct functor_traits<R(Args...)> {
public:
	using functor_type = R (*)(Args...);
	using return_type = R;
	using args_type = std::tuple<Args...>;
};

// Extract fiber function type information.
template <typename F>
struct TypedFiberType {
	using traitsType = functor_traits<F>;
	using returnType = typename traitsType::return_type;
	using fiberType = TypedFiberN<F, returnType, typename traitsType::args_type>;

	// Compatibility
	struct NoArg {};
	typedef NoArg a1Type;
	typedef NoArg a2Type;
	typedef NoArg a3Type;
};
#  else	 // Pre C++11
template <typename F>
struct TypedFiberType {};
#  endif // Pre C++11

#  if ZTH_TYPEDFIBER98
template <typename R>
struct TypedFiberType<R (*)()> {
	struct NoArg {};
	typedef R returnType;
	typedef TypedFiber0<R> fiberType;
	typedef NoArg a1Type;
	typedef NoArg a2Type;
	typedef NoArg a3Type;
};

template <typename R, typename A1>
struct TypedFiberType<R (*)(A1)> {
	struct NoArg {};
	typedef R returnType;
	typedef TypedFiber1<R, A1> fiberType;
	typedef A1 a1Type;
	typedef NoArg a2Type;
	typedef NoArg a3Type;
};

template <typename R, typename A1, typename A2>
struct TypedFiberType<R (*)(A1, A2)> {
	struct NoArg {};
	typedef R returnType;
	typedef TypedFiber2<R, A1, A2> fiberType;
	typedef A1 a1Type;
	typedef A2 a2Type;
	typedef NoArg a3Type;
};

template <typename R, typename A1, typename A2, typename A3>
struct TypedFiberType<R (*)(A1, A2, A3)> {
	struct NoArg {};
	typedef R returnType;
	typedef TypedFiber3<R, A1, A2, A3> fiberType;
	typedef A1 a1Type;
	typedef A2 a2Type;
	typedef A3 a3Type;
};
#  endif // ZTH_TYPEDFIBER98



///////////////////////////////////////////////////////////////////////////////////
// Define a TypedFiberFactory. It holds the entry function and creates fibers when the arguments are
// passed. The entry function and name are stored in the factory, the arguments are passed directly
// to the new fiber (managed by TypedFiberX).
//

template <typename F>
struct function_type_helper {
	typedef F type;
};

#  if __cplusplus >= 201103L
template <typename R, typename... Args>
struct function_type_helper<R(Args...)> {
	using type = R (*)(Args...);
};
#  endif // C++11

template <typename F>
class TypedFiberFactory {
public:
	typedef typename function_type_helper<F>::type Function;
	typedef typename TypedFiberType<Function>::returnType Return;
	typedef typename TypedFiberType<Function>::fiberType TypedFiber_type;
	typedef AutoFuture<Return> AutoFuture_type;

	constexpr TypedFiberFactory(Function function, char const* name) noexcept
		: m_function(function)
		, m_name(name)
	{}

#  if __cplusplus >= 201103L
	template <typename F_>
	constexpr TypedFiberFactory(F_&& function, char const* name) noexcept
		: m_function{std::forward<F_>(function)}
		, m_name{name}
	{}
#  endif // C++11

#  if ZTH_TYPEDFIBER98
	typedef typename TypedFiberType<Function>::a1Type A1;
	typedef typename TypedFiberType<Function>::a2Type A2;
	typedef typename TypedFiberType<Function>::a3Type A3;

	TypedFiber_type& operator()() const
	{
		return polish(*new TypedFiber_type(m_function));
	}

	TypedFiber_type& operator()(A1 a1) const
	{
		return polish(*new TypedFiber_type(m_function, a1));
	}

	TypedFiber_type& operator()(A1 a1, A2 a2) const
	{
		return polish(*new TypedFiber_type(m_function, a1, a2));
	}

	TypedFiber_type& operator()(A1 a1, A2 a2, A3 a3) const
	{
		return polish(*new TypedFiber_type(m_function, a1, a2, a3));
	}
#  endif // ZTH_TYPEDFIBER98

#  if __cplusplus >= 201103L
	template <typename... Args>
	TypedFiber_type& operator()(Args&&... args) const
	{
		return polish(*new TypedFiber_type{m_function, std::forward<Args>(args)...});
	}
#  endif // C++11

protected:
	TypedFiber_type& polish(TypedFiber_type& fiber) const
	{
		if(unlikely(m_name))
			fiber.setName(m_name);

		currentWorker().add(&fiber);
		return fiber;
	}

private:
	Function m_function;
	char const* m_name;
};



///////////////////////////////////////////////////////////////////////////////////
// Provide a zth::fiber_type<F> API to get fiber information.
//

namespace impl {
template <typename F>
struct fiber_type_impl {
	typedef TypedFiberFactory<F> factory;
	typedef typename factory::Function function;
	typedef typename factory::AutoFuture_type future;

	struct fiber {
		typedef typename factory::TypedFiber_type type;
		type& _fiber;

		fiber(typename factory::TypedFiber_type& f) noexcept
			: _fiber(f)
		{}

		typename factory::TypedFiber_type::Future_type::indirection_type operator*()
		{
			return *_fiber;
		}

		typename factory::TypedFiber_type::Future_type::member_type operator->()
		{
			return _fiber.operator->();
		}

		operator type&() const noexcept
		{
			return _fiber;
		}

		operator future() const noexcept
		{
			return _fiber;
		}

		template <typename Manipulator>
		fiber& operator<<(Manipulator const& m)
		{
			_fiber << m;
			return *this;
		}

		future operator<<(asFuture const&)
		{
			return _fiber << asFuture{};
		}
	};
};
} // namespace impl

#  if __cplusplus >= 201103L
template <typename F>
struct fiber_type : public impl::fiber_type_impl<F> {};

template <typename R, typename... Args>
struct fiber_type<R (*)(Args...)> : public impl::fiber_type_impl<R (*)(Args...)> {};

template <typename R, typename... Args>
struct fiber_type<R(Args...)> : public impl::fiber_type_impl<R (&)(Args...)> {};
#  else	 // Pre C++11
template <typename F>
struct fiber_type {};
#  endif // Pre C++11

#  if ZTH_TYPEDFIBER98
template <typename R>
struct fiber_type<R (*)()> : public impl::fiber_type_impl<R (*)()> {};

template <typename R, typename A1>
struct fiber_type<R (*)(A1)> : public impl::fiber_type_impl<R (*)(A1)> {};

template <typename R, typename A1, typename A2>
struct fiber_type<R (*)(A1, A2)> : public impl::fiber_type_impl<R (*)(A1, A2)> {};

template <typename R, typename A1, typename A2, typename A3>
struct fiber_type<R (*)(A1, A2, A3)> : public impl::fiber_type_impl<R (*)(A1, A2, A3)> {};
#  endif



///////////////////////////////////////////////////////////////////////////////////
// zth::factory() and zth::fiber() functions
//

namespace impl {
static inline char const* fiber_name(char const* name)
{
	return (Config::EnableDebugPrint || Config::EnablePerfEvent || Config::EnableStackWaterMark)
		       ? name
		       : nullptr;
}
} // namespace impl

#  if __cplusplus >= 201103L
/*!
 * \brief Create a new fiber.
 *
 * Actually, it returns a factory, that allows passing the fiber arguments
 * afterwards.
 *
 * \ingroup zth_api_cpp_fiber
 */
template <typename F>
typename fiber_type<F>::factory factory(F&& f, char const* name = nullptr)
{
	return typename fiber_type<F>::factory(std::forward<F>(f), impl::fiber_name(name));
}

template <typename F>
typename fiber_type<F>::factory factory(F const& f, char const* name = nullptr)
{
	return typename fiber_type<F>::factory(f, impl::fiber_name(name));
}
#  else	 // Pre-C++11
template <typename F>
typename fiber_type<F>::factory factory(F f, char const* name = nullptr)
{
	return typename fiber_type<F>::factory(f, impl::fiber_name(name));
}
#  endif // Pre-C++11

#  if ZTH_TYPEDFIBER98 && __cplusplus < 201103L
template <typename F>
typename fiber_type<F>::fiber fiber(F f)
{
	return factory<F>(f)();
}

template <typename F, typename A1>
typename fiber_type<F>::fiber fiber(F f, A1 a1)
{
	return factory<F>(f)(a1);
}

template <typename F, typename A1, typename A2>
typename fiber_type<F>::fiber fiber(F f, A1 a1, A2 a2)
{
	return factory<F>(f)(a1, a2);
}

template <typename F, typename A1, typename A2, typename A3>
typename fiber_type<F>::fiber fiber(F f, A1 a1, A2 a2, A3 a3)
{
	return factory<F>(f)(a1, a2, a3);
}
#  endif // ZTH_TYPEDFIBER98

#  if __cplusplus >= 201103L
template <typename F, typename... Args>
typename fiber_type<F>::fiber fiber(F&& f, Args&&... args)
{
	return factory<F>(std::forward<F>(f))(std::forward<Args>(args)...);
}
#  endif // C++11



///////////////////////////////////////////////////////////////////////////////////
// zth_async macros
//

namespace fibered {}

} // namespace zth

#  define zth_fiber_declare_1(f)                                              \
	  namespace zth {                                                     \
	  namespace fibered {                                                 \
	  extern typename ::zth::fiber_type<decltype(&::f)>::factory const f; \
	  }                                                                   \
	  }

/*!
 * \brief Do the declaration part of #zth_fiber() (to be used in an .h file).
 * \ingroup zth_api_cpp_fiber
 * \hideinitializer
 */
#  define zth_fiber_declare(...) FOREACH(zth_fiber_declare_1, ##__VA_ARGS__)

#  define zth_fiber_define_1(storage, f)                                                    \
	  namespace zth {                                                                   \
	  namespace fibered {                                                               \
	  storage typename ::zth::fiber_type<decltype(&::f)>::factory const                 \
		  f(&::f, ::zth::Config::EnableDebugPrint || ::zth::Config::EnablePerfEvent \
				  ? ZTH_STRINGIFY(f) "()"                                   \
				  : nullptr); /* NOLINT */                                  \
	  }                                                                                 \
	  }                                                                                 \
	  typedef typename ::zth::fiber_type<decltype(&::f)>::future f##_future;
#  define zth_fiber_define_extern_1(f) zth_fiber_define_1(extern, f)
#  define zth_fiber_define_static_1(f) zth_fiber_define_1(static constexpr, f)

/*!
 * \brief Do the definition part of #zth_fiber() (to be used in a .cpp file).
 * \ingroup zth_api_cpp_fiber
 * \hideinitializer
 */
#  define zth_fiber_define(...) FOREACH(zth_fiber_define_extern_1, ##__VA_ARGS__)

/*!
 * \brief Prepare every given function to become a fiber by #zth_async.
 * \ingroup zth_api_cpp_fiber
 * \hideinitializer
 */
#  define zth_fiber(...) FOREACH(zth_fiber_define_static_1, ##__VA_ARGS__)

/*!
 * \def zth_async
 * \brief Run a function as a new fiber.
 *
 * The function must have passed through #zth_fiber() (or friends) first.
 * Example:
 *
 * \code
 * void foo(int i) { ... }
 * zth_fiber(foo)
 *
 * int main_fiber(int argc, char** argv) {
 *     zth_async foo(42);
 *     return 0;
 * }
 * \endcode
 *
 * \ingroup zth_api_cpp_fiber
 * \hideinitializer
 */
#  define zth_async ::zth::fibered::



///////////////////////////////////////////////////////////////////////////////////
// C API
//

/*!
 * \brief Run a function as a new fiber.
 * \ingroup zth_api_c_fiber
 */
EXTERN_C ZTH_EXPORT ZTH_INLINE int zth_fiber_create(
	void (*f)(void*), void* arg = nullptr, size_t stack = 0,
	char const* name = nullptr) noexcept
{
	try {
		zth::factory(f, name)(arg) << zth::setStackSize(stack);
	} catch(std::bad_alloc const&) {
		return ENOMEM;
	} catch(...) {
		return EAGAIN;
	}

	return 0;
}
#else // !__cplusplus

#  include <stddef.h>

ZTH_EXPORT int zth_fiber_create(void (*f)(void*), void* arg, size_t stack, char const* name);

#endif // !__cplusplus
#endif // ZTH_ASYNC_H
