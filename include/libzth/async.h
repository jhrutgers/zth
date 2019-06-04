#ifndef __ZTH_ASYNC_H
#define __ZTH_ASYNC_H
#ifdef __cplusplus

#include <libzth/util.h>
#include <libzth/fiber.h>
#include <libzth/sync.h>

namespace zth {

	template <typename R, typename F>
	class TypedFiber : public Fiber {
	public:
		typedef R Return;
		typedef F* Function;
		typedef Future<Return> Future_type;

		TypedFiber(Function function)
			: Fiber(&entry, this)
		{}

		virtual ~TypedFiber() {}

		void registerFuture(Future_type* future = NULL) {
			m_future = future;
		}

	protected:
		static void entry(void* that) {
			if(likely(that))
				((TypedFiber*)that)->entry_();
		}

		virtual void entry_() = 0;

		void setFuture(R const& r) {
			if(m_future)
				m_future->set(r);
		}

		void setFuture() {
			if(m_future)
				m_future->set();
		}

		Function function() const {
			return m_function;
		}
	private:
		Function m_function;
		Future_type* m_future;
	};

	template <typename R>
	class TypedFiber0 : public TypedFiber<R, R()()> {
	public:
		TypedFiber0(Function function) : TypedFiber<R, R()()>(function) {}
		virtual ~TypedFiber0() {}
	protected:
		virtual void entry_() { setFuture(function()()); }
	};
	
	template <>
	class TypedFiber0<void> : public TypedFiber<void, void()()> {
	public:
		TypedFiber0(Function function) : TypedFiber<void, void()()>(function) {}
		virtual ~TypedFiber0() {}
	protected:
		virtual void entry_() { function()(); setFuture(); }
	};

	struct TypedFiberType {
		template <typename R> static R returnType(R(*f)());
		template <typename R> static TypedFiber0<R> fiberType(R(*f)());
	};

	template <typename F>
	class TypedFiberHandle {
	public:
		typedef F Function;
		typedef decltype(TypedFiberType::returnType((F*)0)) Return;
		typedef F TypedFiber_type;
		typedef TypedFiber_type::Future_type Future_type;

		TypedFiberHandle(TypedFiber_type& fiber) {
			fiber->registerFuture(&m_future);
			fiber->addCleanup(&cleanup, this);
		}

		~TypedFiberHandle() {

		}

	protected:
		static void cleanup(void* that) {
			if(likely(that))
				((TypedFiberHandle*)that)->cleanup_();
		}
		void cleanup_() {
			m_fiber = NULL;
		}
	private:
		TypedFiber_type* m_fiber;
		Future<R> m_future
	};

	template <typename F>
	class TypedFiberFactory {
	public:
		typedef F* Function;
		typedef TypedFiber<F> TypedFiber_type
		typedef TypedFiberHandle<F> TypedFiberHandle_type;

		TypedFiberFactory(Function function, char const* name)
			: m_function(function)
			, m_name(name)
		{}

		zth_auto_ptr<TypedFiberHandle_type> operator()() const {
			return handle(*new TypedFiber_type(m_function));
		}

		zth_auto_ptr<TypedFiberHandle_type> handle(TypedFiber_type& fiber) {
			if(m_name)
				fiber.setName(m_name);
			return new TypedFiberHandle_type(*fiber);
		}

	private:
		Function m_function;
		char const* m_name;
	};
	
} // namespace

#define declare_fibered_1(f) \
	namespace zth { namespace fibered { \
		extern ::zth::TypedFiberFactory<decltype(::f)> const f; \
	} } \
#define declare_fibered(...) FOREACH(declare_fibered_1, __VA_ARGS__)

#define define_fibered_1(f) \
	namespace zth { namespace fibered { \
		::zth::TypedFiberFactory<decltype(::f)> const f(&::f, ::zth::Config::EnableDebugPrint ? ZTH_STRINGIFY(f) "()" : NULL); \
	} } \
	typedef zth_auto_ptr<::zth::TypedFiberFactory<decltype(::f)>::TypedFiberHandle_type> f_fiber;

#define define_fibered(...) FOREACH(define_fibered_1, __VA_ARGS__)

#define make_fibered(...) \
	declare_fibered(__VA_ARGS__) \
	define_fibered(__VA_ARGS__)

#define async ::zth::fibered::

#endif // __cplusplus
#endif // __ZTH_ASYNC_H
