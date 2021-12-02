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

#ifdef __cplusplus

#include <libzth/config.h>
#include <libzth/util.h>
#include <libzth/fiber.h>
#include <libzth/sync.h>
#include <libzth/worker.h>

#if __cplusplus >= 201103L
#  include <tuple>
#endif

namespace zth {

	template <typename R, typename F>
	class TypedFiber : public Fiber {
	public:
		typedef R Return;
		typedef F Function;
		typedef Future<Return> Future_type;

		explicit TypedFiber(Function function)
			: Fiber(&entry, this)
			, m_function(function)
		{}

		virtual ~TypedFiber() {}

		void registerFuture(Future_type* future) {
			m_future.reset(future);
			zth_dbg(sync, "[%s] Registered to %s", future->id_str(), id_str());
		}

		Future_type* future() const { return m_future; }

	protected:
		static void entry(void* that) {
			if(likely(that))
				static_cast<TypedFiber*>(that)->entry_();
		}

		virtual void entry_() = 0;

		template <typename T>
		void setFuture(T const& r) {
			if(m_future.get())
				m_future->set(r);
		}

		void setFuture() {
			if(m_future.get())
				m_future->set();
		}

		Function function() const {
			return m_function;
		}

	private:
		Function m_function;
		SharedPointer<Future_type> m_future;
	};

	struct FiberManipulator {
	protected:
		FiberManipulator() {}
		virtual void apply(Fiber& fiber) const = 0;

		template <typename R, typename F> friend TypedFiber<R,F>* operator<<(TypedFiber<R,F>* f, FiberManipulator const& m);
	private:
		FiberManipulator(FiberManipulator const&)
#if __cplusplus >= 201103L
			= delete
#endif
			;
	};

	template <typename R, typename F>
	TypedFiber<R,F>* operator<<(TypedFiber<R,F>* f, FiberManipulator const& m) {
		if(likely(f))
			m.apply(*f);
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
	 * void main_fiber(int argc, char** argv) {
	 *     async foo() << zth::setStackSize(0x10000);
	 * }
	 * \endcode
	 *
	 * \ingroup zth_api_cpp_fiber
	 */
	struct setStackSize : public FiberManipulator {
	public:
		explicit setStackSize(size_t stack) : m_stack(stack) {}
	protected:
		virtual void apply(Fiber& fiber) const override { fiber.setStackSize(m_stack); }
	private:
		size_t m_stack;
	};

	/*!
	 * \brief Change the name of a fiber returned by #async.
	 * \details This is a manipulator that calls #zth::Fiber::setName().
	 * \see zth::setStackSize() for an example
	 * \ingroup zth_api_cpp_fiber
	 */
	struct setName : public FiberManipulator {
	public:
		explicit setName(char const* name) : m_name(name) {}
#if __cplusplus >= 201103L
		explicit setName(std::string const& name) : m_name(name) {}
		explicit setName(std::string&& name) : m_name(std::move(name)) {}
#else
		explicit setName(std::string const& name) : m_name(name.c_str()) {}
#endif
	protected:
		virtual void apply(Fiber& fiber) const override {
#if __cplusplus >= 201103L
			fiber.setName(std::move(m_name));
#else
			fiber.setName(m_name);
#endif
		}
	private:
#if __cplusplus >= 201103L
		std::string m_name;
#else
		char const* m_name;
#endif
	};

	/*!
	 * \brief Makes the fiber pass the given gate upon exit.
	 *
	 * Example:
	 * \code
	 * void foo() { ... }
	 * zth_fiber(foo)
	 *
	 * void main_fiber(int argc, char** argv) {
	 *     zth::Gate gate(3);
	 *     async foo() << zth::passOnExit(gate);
	 *     async foo() << zth::passOnExit(gate);
	 *     gate.wait();
	 *     // If we get here, both foo()s have finished.
	 * }
	 * \endcode
	 *
	 * \ingroup zth_api_cpp_fiber
	 */
	struct passOnExit : public FiberManipulator {
	public:
		explicit passOnExit(Gate& gate) : m_gate(&gate) {}
	protected:
		static void cleanup(Fiber& UNUSED_PAR(f), void* gate) { reinterpret_cast<Gate*>(gate)->pass(); }
		virtual void apply(Fiber& fiber) const override { fiber.addCleanup(&cleanup, (void*)m_gate); }
	private:
		Gate* m_gate;
	};

	template <typename T>
	class AutoFuture : public SharedPointer<Future<T> > {
	public:
		typedef SharedPointer<Future<T> > base;
		typedef Future<T> Future_type;

		AutoFuture() : base() {}
		AutoFuture(AutoFuture const& af) : base() { *this = af; }
		// cppcheck-suppress noExplicitConstructor
		template <typename F> AutoFuture(TypedFiber<T,F>* fiber) { *this = fiber; }
		virtual ~AutoFuture() {}

		template <typename F>
		AutoFuture& operator=(TypedFiber<T,F>* fiber) {
			if(fiber) {
				this->reset(new Future_type(format("Future of %s", fiber->name().c_str()).c_str()));
				fiber->registerFuture(this->get());
			} else {
				this->reset();
			}
			return *this;
		}

		AutoFuture& operator=(AutoFuture const& af) {
			this->reset(af.get());
			return *this;
		}
	};

	template <typename R>
	class TypedFiber0 : public TypedFiber<R, R(*)()> {
	public:
		typedef TypedFiber<R, R(*)()> base;
		explicit TypedFiber0(typename base::Function function) : base(function) {}
		virtual ~TypedFiber0() {}
	protected:
		virtual void entry_() { this->setFuture(this->function()()); }
	};

	template <>
	class TypedFiber0<void> : public TypedFiber<void, void(*)()> {
	public:
		typedef TypedFiber<void, void(*)()> base;
		explicit TypedFiber0(typename base::Function function) : base(function) {}
		virtual ~TypedFiber0() {}
	protected:
		virtual void entry_() override { this->function()(); this->setFuture(); }
	};

	template <typename R, typename A1>
	class TypedFiber1 : public TypedFiber<R, R(*)(A1)> {
	public:
		typedef TypedFiber<R, R(*)(A1)> base;
		TypedFiber1(typename base::Function function, A1 a1) : base(function), m_a1(a1) {}
		virtual ~TypedFiber1() {}
	protected:
		virtual void entry_() { this->setFuture(this->function()(m_a1)); }
	private:
		A1 m_a1;
	};

	template <typename A1>
	class TypedFiber1<void, A1> : public TypedFiber<void, void(*)(A1)> {
	public:
		typedef TypedFiber<void, void(*)(A1)> base;
		TypedFiber1(typename base::Function function, A1 a1) : base(function), m_a1(a1) {}
		virtual ~TypedFiber1() {}
	protected:
		virtual void entry_() { this->function()(m_a1); this->setFuture(); }
	private:
		A1 m_a1;
	};

	template <typename R, typename A1, typename A2>
	class TypedFiber2 : public TypedFiber<R, R(*)(A1, A2)> {
	public:
		typedef TypedFiber<R, R(*)(A1, A2)> base;
		TypedFiber2(typename base::Function function, A1 a1, A2 a2) : base(function), m_a1(a1), m_a2(a2) {}
		virtual ~TypedFiber2() {}
	protected:
		virtual void entry_() { this->setFuture(this->function()(m_a1, m_a2)); }
	private:
		A1 m_a1;
		A2 m_a2;
	};

	template <typename A1, typename A2>
	class TypedFiber2<void, A1, A2> : public TypedFiber<void, void(*)(A1, A2)> {
	public:
		typedef TypedFiber<void, void(*)(A1, A2)> base;
		TypedFiber2(typename base::Function function, A1 a1, A2 a2) : base(function), m_a1(a1), m_a2(a2) {}
		virtual ~TypedFiber2() {}
	protected:
		virtual void entry_() { this->function()(m_a1, m_a2); this->setFuture(); }
	private:
		A1 m_a1;
		A2 m_a2;
	};

	template <typename R, typename A1, typename A2, typename A3>
	class TypedFiber3 : public TypedFiber<R, R(*)(A1, A2, A3)> {
	public:
		typedef TypedFiber<R, R(*)(A1, A2, A3)> base;
		TypedFiber3(typename base::Function function, A1 a1, A2 a2, A3 a3) : base(function), m_a1(a1), m_a2(a2), m_a3(a3) {}
		virtual ~TypedFiber3() {}
	protected:
		virtual void entry_() { this->setFuture(this->function()(m_a1, m_a2, m_a3)); }
	private:
		A1 m_a1;
		A2 m_a2;
		A3 m_a3;
	};

	template <typename A1, typename A2, typename A3>
	class TypedFiber3<void, A1, A2, A3> : public TypedFiber<void, void(*)(A1, A2, A3)> {
	public:
		typedef TypedFiber<void, void(*)(A1, A2, A3)> base;
		TypedFiber3(typename base::Function function, A1 a1, A2 a2, A3 a3) : base(function), m_a1(a1), m_a2(a2), m_a3(a3) {}
		virtual ~TypedFiber3() {}
	protected:
		virtual void entry_() { this->function()(m_a1, m_a2, m_a3); this->setFuture(); }
	private:
		A1 m_a1;
		A2 m_a2;
		A3 m_a3;
	};

#if __cplusplus >= 201103L
	template <typename R, typename... Args>
	class TypedFiberN : public TypedFiber<R, R(*)(Args...)> {
	public:
		typedef TypedFiber<R, R(*)(Args...)> base;
		TypedFiberN(typename base::Function function, Args... args) : base(function), m_args(args...) {}
		virtual ~TypedFiberN() {}
	protected:
		virtual void entry_() { entry__(typename SequenceGenerator<sizeof...(Args)>::type()); }
	private:
		template <size_t... S> void entry__(Sequence<S...>) { this->setFuture(this->function()(std::get<S>(m_args)...)); }
	private:
		std::tuple<Args...> m_args;
	};

	template <typename... Args>
	class TypedFiberN<void, Args...> : public TypedFiber<void, void(*)(Args...)> {
	public:
		typedef TypedFiber<void, void(*)(Args...)> base;
		TypedFiberN(typename base::Function function, Args... args) : base(function), m_args(args...) {}
		virtual ~TypedFiberN() {}
	protected:
		virtual void entry_() { entry__(typename SequenceGenerator<sizeof...(Args)>::type()); }
	private:
		template <size_t... S> void entry__(Sequence<S...>) { this->function()(std::get<S>(m_args)...); this->setFuture(); }
	private:
		std::tuple<Args...> m_args;
	};
#endif

	template <typename F> struct TypedFiberType {};
	template <typename R> struct TypedFiberType<R(*)()> {
		struct NoArg {};
		typedef R returnType;
		typedef TypedFiber0<R> fiberType;
		typedef NoArg a1Type;
		typedef NoArg a2Type;
		typedef NoArg a3Type;
	};
	template <typename R, typename A1> struct TypedFiberType<R(*)(A1)> {
		struct NoArg {};
		typedef R returnType;
		typedef TypedFiber1<R,A1> fiberType;
		typedef A1 a1Type;
		typedef NoArg a2Type;
		typedef NoArg a3Type;
	};
	template <typename R, typename A1, typename A2> struct TypedFiberType<R(*)(A1, A2)> {
		struct NoArg {};
		typedef R returnType;
		typedef TypedFiber2<R,A1,A2> fiberType;
		typedef A1 a1Type;
		typedef A2 a2Type;
		typedef NoArg a3Type;
	};
	template <typename R, typename A1, typename A2, typename A3> struct TypedFiberType<R(*)(A1, A2, A3)> {
		struct NoArg {};
		typedef R returnType;
		typedef TypedFiber3<R,A1,A2,A3> fiberType;
		typedef A1 a1Type;
		typedef A2 a2Type;
		typedef A3 a3Type;
	};

#if __cplusplus >= 201103L
	template <typename R, typename... Args> struct TypedFiberType<R(*)(Args...)> {
		struct NoArg {};
		typedef R returnType;
		typedef TypedFiberN<R,Args...> fiberType;
		// The following types are only here for compatibility with the other TypedFiberTypes.
		typedef NoArg a1Type;
		typedef NoArg a2Type;
		typedef NoArg a3Type;
	};
#endif

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

		TypedFiberFactory(Function function, char const* name)
			: m_function(function)
			, m_name(name)
		{}

		TypedFiber_type* operator()() const {
			return polish(*new TypedFiber_type(m_function));
		}

		TypedFiber_type* operator()(A1 a1) const {
			return polish(*new TypedFiber_type(m_function, a1));
		}

		TypedFiber_type* operator()(A1 a1, A2 a2) const {
			return polish(*new TypedFiber_type(m_function, a1, a2));
		}

		TypedFiber_type* operator()(A1 a1, A2 a2, A3 a3) const {
			return polish(*new TypedFiber_type(m_function, a1, a2, a3));
		}

#if __cplusplus >= 201103L
		template <typename... Args>
		TypedFiber_type* operator()(Args&&... args) const {
			return polish(*new TypedFiber_type(m_function, std::forward<Args>(args)...));
		}
#endif

		TypedFiber_type* polish(TypedFiber_type& fiber) const {
			if(unlikely(m_name))
				fiber.setName(m_name);

			currentWorker().add(&fiber);
			return &fiber;
		}

	private:
		Function m_function;
		char const* m_name;
	};

	namespace fibered {}

} // namespace

#define zth_fiber_declare_1(f) \
	namespace zth { namespace fibered { \
		extern ::zth::TypedFiberFactory<decltype(&::f)> const f; \
	} }

/*!
 * \brief Do the declaration part of #zth_fiber() (to be used in an .h file).
 * \ingroup zth_api_cpp_fiber
 * \hideinitializer
 */
#define zth_fiber_declare(...) FOREACH(zth_fiber_declare_1, ##__VA_ARGS__)

#define zth_fiber_define_1(storage, f) \
	namespace zth { namespace fibered { \
		storage ::zth::TypedFiberFactory<decltype(&::f)> const f(&::f, ::zth::Config::EnableDebugPrint || ::zth::Config::EnablePerfEvent ? ZTH_STRINGIFY(f) "()" : NULL); \
	} } \
	typedef ::zth::TypedFiberFactory<decltype(&::f)>::AutoFuture_type f##_future;
#define zth_fiber_define_extern_1(f)	zth_fiber_define_1(extern, f)
#define zth_fiber_define_static_1(f)	zth_fiber_define_1(static, f)

/*!
 * \brief Do the definition part of #zth_fiber() (to be used in a .cpp file).
 * \ingroup zth_api_cpp_fiber
 * \hideinitializer
 */
#define zth_fiber_define(...) FOREACH(zth_fiber_define_extern_1, ##__VA_ARGS__)

/*!
 * \brief Prepare every given function to become a fiber by #async.
 * \ingroup zth_api_cpp_fiber
 * \hideinitializer
 */
#define zth_fiber(...) FOREACH(zth_fiber_define_static_1, ##__VA_ARGS__)

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
 * void main_fiber(int argc, char** argv) {
 *     async foo(42);
 * }
 * \endcode
 *
 * \ingroup zth_api_cpp_fiber
 * \hideinitializer
 */
#define async ::zth::fibered::

/*!
 * \brief Run a function as a new fiber.
 * \ingroup zth_api_c_fiber
 */
EXTERN_C ZTH_EXPORT ZTH_INLINE int zth_fiber_create(void(*f)(void*), void* arg = NULL, size_t stack = 0, char const* name = NULL) {
	int res = 0;
	zth::Fiber* fib = new zth::Fiber(f, arg);

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

ZTH_EXPORT int zth_fiber_create(void(*f)(void*), void* arg, size_t stack, char const* name);

#endif // __cplusplus
#endif // ZTH_ASYNC_H
