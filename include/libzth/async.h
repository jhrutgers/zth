#ifndef ZTH_ASYNC_H
#define ZTH_ASYNC_H
/*
 * Zth (libzth), a cooperative userspace multitasking library.
 * Copyright (C) 2019-2021  Jochem Rutgers
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <libzth/macros.h>

#ifdef __cplusplus

#	include <libzth/allocator.h>
#	include <libzth/config.h>
#	include <libzth/fiber.h>
#	include <libzth/sync.h>
#	include <libzth/util.h>
#	include <libzth/worker.h>

#	if __cplusplus >= 201103L
#		include <tuple>
#	endif

#	if __cplusplus < 201103L
#		undef ZTH_TYPEDFIBER98
// Always use C++98-compatible TypedFiber implementation.
#		define ZTH_TYPEDFIBER98 1
#	elif !defined(ZTH_TYPEDFIBER98)
// By default only use C++11 TypedFiber implementation, but this can be
// overridden for testing purposes.
#		define ZTH_TYPEDFIBER98 0
#	endif

namespace zth {

template <typename R, typename F>
class TypedFiber : public Fiber {
	ZTH_CLASS_NEW_DELETE(TypedFiber)
public:
	typedef R Return;
	typedef F Function;
	typedef Future<Return> Future_type;

	explicit TypedFiber(Function function)
		: Fiber(&entry, this)
		, m_function(function)
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

	Function function() const noexcept
	{
		return m_function;
	}

private:
	Function m_function;
	SharedPointer<Future_type> m_future;
};

class FiberManipulator {
protected:
	constexpr FiberManipulator() noexcept is_default
	virtual ~FiberManipulator() is_default
	virtual void apply(Fiber& fiber) const = 0;

	template <typename R, typename F>
	friend TypedFiber<R, F>& operator<<(TypedFiber<R, F>& f, FiberManipulator const& m);
};

template <typename R, typename F>
TypedFiber<R, F>& operator<<(TypedFiber<R, F>& f, FiberManipulator const& m)
{
	m.apply(f);
	return f;
}

/*!
 * \brief Change the stack size of a fiber returned by #async.
 *
 * This is a manipulator that calls #zth::Fiber::setStackSize().
 * Example:
 * \code
 * void foo() { ... }
 * zth_fiber(foo)
 *
 * int main_fiber(int argc, char** argv) {
 *     async foo() << zth::setStackSize(0x10000);
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
 * \brief Change the name of a fiber returned by #async.
 * \details This is a manipulator that calls #zth::Fiber::setName().
 * \see zth::setStackSize() for an example
 * \ingroup zth_api_cpp_fiber
 */
class setName : public FiberManipulator {
public:
	explicit setName(char const* name)
		: m_name(name)
	{}

#	if __cplusplus >= 201103L
	explicit setName(string const& name)
		: m_name(name)
	{}
	explicit setName(string&& name)
		: m_name(std::move(name))
	{}
#	else
	explicit setName(string const& name)
		: m_name(name.c_str())
	{}
#	endif

	virtual ~setName() override is_default
protected:
	virtual void apply(Fiber& fiber) const override
	{
#	if __cplusplus >= 201103L
		fiber.setName(std::move(m_name));
#	else
		fiber.setName(m_name);
#	endif
	}

private:
#	if __cplusplus >= 201103L
	string m_name;
#	else
	char const* m_name;
#	endif
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
 *     async foo() << zth::passOnExit(gate);
 *     async foo() << zth::passOnExit(gate);
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
		fiber.addCleanup(&cleanup, (void*)m_gate);
	}

private:
	Gate* m_gate;
};

template <typename T>
class AutoFuture : public SharedPointer<Future<T> /**/> {
	ZTH_CLASS_NEW_DELETE(AutoFuture)
public:
	typedef SharedPointer<Future<T> /**/> base;
	typedef Future<T> Future_type;

	virtual ~AutoFuture() override is_default

	constexpr14 AutoFuture() noexcept
		: base()
	{}

	constexpr14 AutoFuture(AutoFuture const& af) noexcept
		: base(af)
	{}

	template <typename F>
	// cppcheck-suppress noExplicitConstructor
	AutoFuture(TypedFiber<T, F>& fiber)
	{
		*this = fiber;
	}

	template <typename F>
	AutoFuture& operator=(TypedFiber<T, F>& fiber)
	{
		this->reset(fiber.withFuture().get());
		return *this;
	}

	AutoFuture& operator=(AutoFuture const& af)
	{
		this->reset(af.get());
		return *this;
	}
};

#	if ZTH_TYPEDFIBER98
template <typename R>
class TypedFiber0 final : public TypedFiber<R, R (*)()> {
	ZTH_CLASS_NEW_DELETE(TypedFiber0)
public:
	typedef TypedFiber<R, R (*)()> base;

	explicit TypedFiber0(typename base::Function function)
		: base(function)
	{}

	virtual ~TypedFiber0() final is_default

protected:
	virtual void entry_() final
	{
		this->setFuture(this->function()());
	}
};

template <>
class TypedFiber0<void> final : public TypedFiber<void, void (*)()> {
	ZTH_CLASS_NEW_DELETE(TypedFiber0)
public:
	typedef TypedFiber<void, void (*)()> base;

	explicit TypedFiber0(typename base::Function function)
		: base(function)
	{}

	virtual ~TypedFiber0() final is_default

protected:
	virtual void entry_() final
	{
		this->function()();
		this->setFuture();
	}
};

template <typename R, typename A1>
class TypedFiber1 final : public TypedFiber<R, R (*)(A1)> {
	ZTH_CLASS_NEW_DELETE(TypedFiber1)
public:
	typedef TypedFiber<R, R (*)(A1)> base;

	TypedFiber1(typename base::Function function, A1 a1)
		: base(function)
		, m_a1(a1)
	{}

	virtual ~TypedFiber1() final is_default

protected:
	virtual void entry_() final
	{
		this->setFuture(this->function()(m_a1));
	}

private:
	A1 m_a1;
};

template <typename A1>
class TypedFiber1<void, A1> final : public TypedFiber<void, void (*)(A1)> {
	ZTH_CLASS_NEW_DELETE(TypedFiber1)
public:
	typedef TypedFiber<void, void (*)(A1)> base;

	TypedFiber1(typename base::Function function, A1 a1)
		: base(function)
		, m_a1(a1)
	{}

	virtual ~TypedFiber1() final is_default

protected:
	virtual void entry_() final
	{
		this->function()(m_a1);
		this->setFuture();
	}

private:
	A1 m_a1;
};

template <typename R, typename A1, typename A2>
class TypedFiber2 final : public TypedFiber<R, R (*)(A1, A2)> {
	ZTH_CLASS_NEW_DELETE(TypedFiber2)
public:
	typedef TypedFiber<R, R (*)(A1, A2)> base;

	TypedFiber2(typename base::Function function, A1 a1, A2 a2)
		: base(function)
		, m_a1(a1)
		, m_a2(a2)
	{}

	virtual ~TypedFiber2() final is_default

protected:
	virtual void entry_() final
	{
		this->setFuture(this->function()(m_a1, m_a2));
	}

private:
	A1 m_a1;
	A2 m_a2;
};

template <typename A1, typename A2>
class TypedFiber2<void, A1, A2> final : public TypedFiber<void, void (*)(A1, A2)> {
	ZTH_CLASS_NEW_DELETE(TypedFiber2)
public:
	typedef TypedFiber<void, void (*)(A1, A2)> base;

	TypedFiber2(typename base::Function function, A1 a1, A2 a2)
		: base(function)
		, m_a1(a1)
		, m_a2(a2)
	{}

	virtual ~TypedFiber2() final is_default

protected:
	virtual void entry_() final
	{
		this->function()(m_a1, m_a2);
		this->setFuture();
	}

private:
	A1 m_a1;
	A2 m_a2;
};

template <typename R, typename A1, typename A2, typename A3>
class TypedFiber3 final : public TypedFiber<R, R (*)(A1, A2, A3)> {
	ZTH_CLASS_NEW_DELETE(TypedFiber3)
public:
	typedef TypedFiber<R, R (*)(A1, A2, A3)> base;

	TypedFiber3(typename base::Function function, A1 a1, A2 a2, A3 a3)
		: base(function)
		, m_a1(a1)
		, m_a2(a2)
		, m_a3(a3)
	{}

	virtual ~TypedFiber3() final is_default

protected:
	virtual void entry_() final
	{
		this->setFuture(this->function()(m_a1, m_a2, m_a3));
	}

private:
	A1 m_a1;
	A2 m_a2;
	A3 m_a3;
};

template <typename A1, typename A2, typename A3>
class TypedFiber3<void, A1, A2, A3> final : public TypedFiber<void, void (*)(A1, A2, A3)> {
	ZTH_CLASS_NEW_DELETE(TypedFiber3)
public:
	typedef TypedFiber<void, void (*)(A1, A2, A3)> base;

	TypedFiber3(typename base::Function function, A1 a1, A2 a2, A3 a3)
		: base(function)
		, m_a1(a1)
		, m_a2(a2)
		, m_a3(a3)
	{}

	virtual ~TypedFiber3() final is_default

protected:
	virtual void entry_() final
	{
		this->function()(m_a1, m_a2, m_a3);
		this->setFuture();
	}

private:
	A1 m_a1;
	A2 m_a2;
	A3 m_a3;
};
#	endif // ZTH_TYPEDFIBER98

#	if __cplusplus >= 201103L
template <typename R, typename... Args>
class TypedFiberN final : public TypedFiber<R, R (*)(Args...)> {
	ZTH_CLASS_NEW_DELETE(TypedFiberN)
public:
	typedef TypedFiber<R, R (*)(Args...)> base;

	template <typename... Args_>
	// cppcheck-suppress passedByValue
	TypedFiberN(typename base::Function function, Args_&&... args)
		: base(function)
		, m_args(std::forward<Args_>(args)...)
	{}

	virtual ~TypedFiberN() final = default;

protected:
	virtual void entry_() final
	{
		entry__(typename SequenceGenerator<sizeof...(Args)>::type());
	}

private:
	template <size_t... S>
	void entry__(Sequence<S...>)
	{
		this->setFuture(this->function()(std::get<S>(m_args)...));
	}

private:
	std::tuple<Args...> m_args;
};

template <typename... Args>
class TypedFiberN<void, Args...> final : public TypedFiber<void, void (*)(Args...)> {
	ZTH_CLASS_NEW_DELETE(TypedFiberN)
public:
	typedef TypedFiber<void, void (*)(Args...)> base;

	template <typename... Args_>
	// cppcheck-suppress passedByValue
	TypedFiberN(typename base::Function function, Args_&&... args)
		: base(function)
		, m_args(std::forward<Args_>(args)...)
	{}

	virtual ~TypedFiberN() final = default;

protected:
	virtual void entry_() final
	{
		entry__(typename SequenceGenerator<sizeof...(Args)>::type());
	}

private:
	template <size_t... S>
	void entry__(Sequence<S...>)
	{
		this->function()(std::get<S>(m_args)...);
		this->setFuture();
	}

private:
	std::tuple<Args...> m_args;
};
#	endif // C++11

template <typename F>
struct TypedFiberType {};

#	if ZTH_TYPEDFIBER98
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
#	endif // ZTH_TYPEDFIBER98

#	if __cplusplus >= 201103L
template <typename R, typename... Args>
struct TypedFiberType<R (*)(Args...)> {
	struct NoArg {};
	typedef R returnType;
	typedef TypedFiberN<R, Args...> fiberType;
	// The following types are only here for compatibility with the other TypedFiberTypes.
	typedef NoArg a1Type;
	typedef NoArg a2Type;
	typedef NoArg a3Type;
};
#	endif // C++11

template <typename F>
class TypedFiberFactory {
public:
	typedef F Function;
	typedef typename TypedFiberType<Function>::returnType Return;
	typedef typename TypedFiberType<Function>::fiberType TypedFiber_type;
	typedef typename TypedFiberType<Function>::a1Type A1;
	typedef typename TypedFiberType<Function>::a2Type A2;
	typedef typename TypedFiberType<Function>::a3Type A3;
	typedef AutoFuture<Return> AutoFuture_type;

	constexpr TypedFiberFactory(Function function, char const* name) noexcept
		: m_function(function)
		, m_name(name)
	{}

#	if ZTH_TYPEDFIBER98
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
#	endif // ZTH_TYPEDFIBER98

#	if __cplusplus >= 201103L
	template <typename... Args>
	TypedFiber_type& operator()(Args&&... args) const
	{
		return polish(*new TypedFiber_type(m_function, std::forward<Args>(args)...));
	}
#	endif // C++11

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
struct fiber_type_impl {
	typedef TypedFiberFactory<F> factory;
	typedef typename factory::TypedFiber_type& fiber;
	typedef typename factory::AutoFuture_type future;
};

template <typename F>
struct fiber_type {};

#	if ZTH_TYPEDFIBER98
template <typename R>
struct fiber_type<R (*)()> : public fiber_type_impl<R (*)()> {};

template <typename R, typename A1>
struct fiber_type<R (*)(A1)> : public fiber_type_impl<R (*)(A1)> {};

template <typename R, typename A1, typename A2>
struct fiber_type<R (*)(A1, A2)> : public fiber_type_impl<R (*)(A1, A2)> {};

template <typename R, typename A1, typename A2, typename A3>
struct fiber_type<R (*)(A1, A2, A3)> : public fiber_type_impl<R (*)(A1, A2, A3)> {};
#	endif

#	if __cplusplus >= 201103L
template <typename R, typename... Args>
struct fiber_type<R (*)(Args...)> : public fiber_type_impl<R (*)(Args...)> {};
#	endif

template <typename F>
typename fiber_type<F>::factory fiber(F f, char const* name = nullptr)
{
	return typename fiber_type<F>::factory(
		f, Config::EnableDebugPrint || Config::EnablePerfEvent ? name : nullptr);
}

namespace fibered {}

} // namespace zth

#	define zth_fiber_declare_1(f)                                   \
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
#	define zth_fiber_declare(...) FOREACH(zth_fiber_declare_1, ##__VA_ARGS__)

#	define zth_fiber_define_1(storage, f)                                                    \
		namespace zth {                                                                   \
		namespace fibered {                                                               \
		storage ::zth::TypedFiberFactory<decltype(&::f)> const                            \
			f(&::f, ::zth::Config::EnableDebugPrint || ::zth::Config::EnablePerfEvent \
					? ZTH_STRINGIFY(f) "()"                                   \
					: nullptr); /* NOLINT */                                  \
		}                                                                                 \
		}                                                                                 \
		typedef ::zth::TypedFiberFactory<decltype(&::f)>::AutoFuture_type f##_future;
#	define zth_fiber_define_extern_1(f) zth_fiber_define_1(extern, f)
#	define zth_fiber_define_static_1(f) zth_fiber_define_1(static constexpr, f)

/*!
 * \brief Do the definition part of #zth_fiber() (to be used in a .cpp file).
 * \ingroup zth_api_cpp_fiber
 * \hideinitializer
 */
#	define zth_fiber_define(...) FOREACH(zth_fiber_define_extern_1, ##__VA_ARGS__)

/*!
 * \brief Prepare every given function to become a fiber by #async.
 * \ingroup zth_api_cpp_fiber
 * \hideinitializer
 */
#	define zth_fiber(...) FOREACH(zth_fiber_define_static_1, ##__VA_ARGS__)

/*!
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
 *     async foo(42);
 *     return 0;
 * }
 * \endcode
 *
 * \ingroup zth_api_cpp_fiber
 * \hideinitializer
 */
#	define async ::zth::fibered::

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

#	include <stddef.h>

ZTH_EXPORT int zth_fiber_create(void (*f)(void*), void* arg, size_t stack, char const* name);

#endif // !__cplusplus
#endif // ZTH_ASYNC_H
