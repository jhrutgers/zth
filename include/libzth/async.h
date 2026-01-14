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
	}

	Future_type* future() const
	{
		return m_future;
	}

	SharedPointer<Future_type> withFuture()
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

protected:
	static void entry(void* that)
	{
		if(likely(that))
			static_cast<TypedFiber*>(that)->entry_();
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

class FiberManipulator {
protected:
	constexpr FiberManipulator() noexcept is_default
	virtual ~FiberManipulator() is_default
	virtual void apply(Fiber& fiber) const = 0;

	template <typename R>
	friend TypedFiber<R>& operator<<(TypedFiber<R>& f, FiberManipulator const& m);
};

template <typename R>
TypedFiber<R>& operator<<(TypedFiber<R>& f, FiberManipulator const& m)
{
	m.apply(f);
	return f;
}

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
class setStackSize : public FiberManipulator {
public:
	constexpr explicit setStackSize(size_t stack) noexcept
		: m_stack(stack)
	{}
	virtual ~setStackSize() override is_default
protected:
	virtual void apply(Fiber& fiber) const override
	{
		fiber.setStackSize(m_stack);
	}

private:
	size_t m_stack;
};

/*!
 * \brief Change the name of a fiber returned by #zth_async.
 * \details This is a manipulator that calls #zth::Fiber::setName().
 * \see zth::setStackSize() for an example
 * \ingroup zth_api_cpp_fiber
 */
class setName : public FiberManipulator {
public:
#  if __cplusplus >= 201103L
	explicit setName(char const* name)
		: m_name(name ? std::string{name} : std::string{})
	{}

	explicit setName(string const& name)
		: m_name(name)
	{}
	explicit setName(string&& name)
		: m_name(std::move(name))
	{}
#  else
	explicit setName(char const* name)
		: m_name(name)
	{}

	explicit setName(string const& name)
		: m_name(name.c_str())
	{}
#  endif

	virtual ~setName() override is_default
protected:
	virtual void apply(Fiber& fiber) const override
	{
#  if __cplusplus >= 201103L
		fiber.setName(std::move(m_name));
#  else
		fiber.setName(m_name);
#  endif
	}

private:
#  if __cplusplus >= 201103L
	string m_name;
#  else
	char const* m_name;
#  endif
};

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
class passOnExit : public FiberManipulator {
public:
	constexpr explicit passOnExit(Gate& gate) noexcept
		: m_gate(&gate)
	{}

	virtual ~passOnExit() override is_default

protected:
	static void cleanup(Fiber& UNUSED_PAR(f), void* gate)
	{
		reinterpret_cast<Gate*>(gate)->pass();
	}

	virtual void apply(Fiber& fiber) const override
	{
		fiber.addCleanup(&cleanup, static_cast<void*>(m_gate));
	}

private:
	Gate* m_gate;
};

template <typename T>
class AutoFuture : public SharedPointer<Future<T> /**/> {
public:
	typedef SharedPointer<Future<T> /**/> base;
	typedef Future<T> Future_type;

	~AutoFuture() noexcept is_default

	constexpr14 AutoFuture() noexcept
		: base()
	{}

	constexpr14 AutoFuture(AutoFuture const& af) noexcept
		: base(af)
	{}

	// cppcheck-suppress noExplicitConstructor
	constexpr14 AutoFuture(base const& p) noexcept
		: base(p)
	{}

	// cppcheck-suppress noExplicitConstructor
	AutoFuture(TypedFiber<T>& fiber)
	{
		*this = fiber;
	}

	AutoFuture& operator=(TypedFiber<T>& fiber)
	{
		this->reset(fiber.withFuture().get());
		return *this;
	}

	AutoFuture& operator=(AutoFuture const& af) noexcept
	{
		this->reset(af.get());
		return *this;
	}
};

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

// F: function type (function pointer, lambda, etc.)
// R: return type of F()
// Args: argument types of F() as std::tuple<...>
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
#    ifdef ZTH_FUTURE_EXCEPTION
		try {
#    endif // ZTH_FUTURE_EXCEPTION
			this->setFuture(
				m_function(move_or_ref<typename std::tuple_element<S, Args>::type>(
					std::get<S>(m_args))...));
#    ifdef ZTH_FUTURE_EXCEPTION
		} catch(...) {
			this->setFuture(std::current_exception());
		}
#    endif // ZTH_FUTURE_EXCEPTION
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
#    ifdef ZTH_FUTURE_EXCEPTION
		try {
#    endif // ZTH_FUTURE_EXCEPTION
			m_function(move_or_ref<typename std::tuple_element<S, Args>::type>(
				std::get<S>(m_args))...);
			this->setFuture();
#    ifdef ZTH_FUTURE_EXCEPTION
		} catch(...) {
			this->setFuture(std::current_exception());
		}
#    endif // ZTH_FUTURE_EXCEPTION
	}

private:
	Function m_function;
	Args m_args;
};
#  endif // C++11

#  if __cplusplus >= 201103L
template <typename F>
struct FunctorTraits {
private:
	template <typename R, typename... Args>
	static std::tuple<Args...> argsTupleTypeImpl(R (*)(Args...));

public:
	using argsTupleType = decltype(argsTupleTypeImpl(&F::operator()));
};

template <typename F>
struct TypedFiberType {
	using returnType = decltype(F::operator()());
	using fiberType = TypedFiberN<F, returnType, typename FunctorTraits<F>::argsTupleType>;

	// Compatibility
	struct NoArg {};
	typedef NoArg a1Type;
	typedef NoArg a2Type;
	typedef NoArg a3Type;
};

template <typename R, typename... Args>
struct TypedFiberType<R (*)(Args...)> {
	using returnType = R;
	using fiberType = TypedFiberN<R (*)(Args...), R, std::tuple<Args...>>;

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

template <typename F>
class TypedFiberFactory {
public:
	typedef F Function;
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

template <typename F>
struct fiber_type {
	typedef TypedFiberFactory<F> factory;
	typedef typename factory::TypedFiber_type& fiber;
	typedef typename factory::AutoFuture_type future;
};

template <typename F>
struct fiber_type {};

#  if ZTH_TYPEDFIBER98
template <typename R>
struct fiber_type<R (*)()> : public fiber_type_impl<R (*)()> {};

template <typename R, typename A1>
struct fiber_type<R (*)(A1)> : public fiber_type_impl<R (*)(A1)> {};

template <typename R, typename A1, typename A2>
struct fiber_type<R (*)(A1, A2)> : public fiber_type_impl<R (*)(A1, A2)> {};

template <typename R, typename A1, typename A2, typename A3>
struct fiber_type<R (*)(A1, A2, A3)> : public fiber_type_impl<R (*)(A1, A2, A3)> {};
#  endif

#  if __cplusplus >= 201103L
template <typename R, typename... Args>
struct fiber_type<R (*)(Args...)> : public fiber_type_impl<R (*)(Args...)> {};
#  endif

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
	return typename fiber_type<F>::factory(
		std::forward<F>(f),
		Config::EnableDebugPrint || Config::EnablePerfEvent || Config::EnableStackWaterMark
			? name
			: nullptr);
}
#  else // Pre-C++11
template <typename F>
typename fiber_type<F>::factory factory(F f, char const* name = nullptr)
{
	return typename fiber_type<F>::factory(
		f,
		Config::EnableDebugPrint || Config::EnablePerfEvent || Config::EnableStackWaterMark
			? name
			: nullptr);
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

namespace fibered {}

} // namespace zth

#  define zth_fiber_declare_1(f)                                   \
	  namespace zth {                                          \
	  namespace fibered {                                      \
	  extern ::zth::TypedFiberFactory<decltype(&::f)> const f; \
	  }                                                        \
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
	  storage ::zth::TypedFiberFactory<decltype(&::f)> const                            \
		  f(&::f, ::zth::Config::EnableDebugPrint || ::zth::Config::EnablePerfEvent \
				  ? ZTH_STRINGIFY(f) "()"                                   \
				  : nullptr); /* NOLINT */                                  \
	  }                                                                                 \
	  }                                                                                 \
	  typedef ::zth::TypedFiberFactory<decltype(&::f)>::AutoFuture_type f##_future;
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

/*!
 * \brief Run a function as a new fiber.
 * \ingroup zth_api_c_fiber
 */
EXTERN_C ZTH_EXPORT ZTH_INLINE int zth_fiber_create(
	void (*f)(void*), void* arg = nullptr, size_t stack = 0,
	char const* name = nullptr) noexcept
{
	int res = 0;
	zth::Fiber* fib = nullptr;

	try {
		fib = new zth::Fiber(f, arg);
	} catch(std::bad_alloc const&) {
		return ENOMEM;
	} catch(...) {
		return EAGAIN;
	}

	if(unlikely(stack))
		if((res = fib->setStackSize(stack))) {
			delete fib;
			return res;
		}

	if(unlikely(name))
		fib->setName(name);

	zth::currentWorker().add(fib);
	return res;
}
#else // !__cplusplus

#  include <stddef.h>

ZTH_EXPORT int zth_fiber_create(void (*f)(void*), void* arg, size_t stack, char const* name);

#endif // !__cplusplus
#endif // ZTH_ASYNC_H
