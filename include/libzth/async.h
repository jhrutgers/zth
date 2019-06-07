#ifndef __ZTH_ASYNC_H
#define __ZTH_ASYNC_H
#ifdef __cplusplus

#include <libzth/config.h>
#include <libzth/util.h>
#include <libzth/fiber.h>
#include <libzth/sync.h>
#include <libzth/worker.h>

namespace zth {

	template <typename R, typename F>
	class TypedFiber : public Fiber {
	public:
		typedef R Return;
		typedef F Function;
		typedef Future<Return> Future_type;

		TypedFiber(Function function)
			: Fiber(&entry, this)
			, m_function(function)
		{}

		virtual ~TypedFiber() {}

		void registerFuture(Future_type* future) {
			m_future.reset(future);
			zth_dbg(sync, "[%s (%p)] Registered to %s (%p)", future->name(), future->normptr(), name().c_str(), normptr());
		}

		Future_type* future() const { return m_future; }

	protected:
		static void entry(void* that) {
			if(likely(that))
				((TypedFiber*)that)->entry_();
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

	template <typename T>
	class AutoFuture : public SharedPointer<Future<T> > {
	public:
		typedef SharedPointer<Future<T> > base;
		typedef Future<T> Future_type;

		AutoFuture() {}
		AutoFuture(AutoFuture const& af) { *this = af; }
		template <typename F> AutoFuture(TypedFiber<T,F>* fiber) { *this = fiber; }
		virtual ~AutoFuture() {}

		template <typename F>
		AutoFuture& operator=(TypedFiber<T,F>* fiber) {
			if(fiber) {
				this->reset(new Future_type());
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
		TypedFiber0(typename base::Function function) : base(function) {}
		virtual ~TypedFiber0() {}
	protected:
		virtual void entry_() { this->setFuture(this->function()()); }
	};
	
	template <>
	class TypedFiber0<void> : public TypedFiber<void, void(*)()> {
	public:
		typedef TypedFiber<void, void(*)()> base;
		TypedFiber0(typename base::Function function) : base(function) {}
		virtual ~TypedFiber0() {}
	protected:
		virtual void entry_() { function()(); setFuture(); }
	};

	struct TypedFiberType {
		template <typename R> static R returnType(R(*f)());
		template <typename R> static TypedFiber0<R> fiberType(R(*f)());
	};

	template <typename F>
	class TypedFiberFactory {
	public:
		typedef F Function;
		typedef decltype(TypedFiberType::returnType((Function)0)) Return;
		typedef decltype(TypedFiberType::fiberType((Function)0)) TypedFiber_type;
		typedef AutoFuture<Return> AutoFuture_type;

		TypedFiberFactory(Function function, char const* name)
			: m_function(function)
			, m_name(name)
		{}

		TypedFiber_type* operator()() const {
			return polish(*new TypedFiber_type(m_function));
		}

		TypedFiber_type* polish(TypedFiber_type& fiber) const {
			if(unlikely(m_name))
				fiber.setName(m_name);

			Worker* w;
			getContext(&w, NULL);
			w->add(&fiber);

			return &fiber;
		}

	private:
		Function m_function;
		char const* m_name;
	};

	namespace fibered {}
	
} // namespace

#define declare_fibered_1(f) \
	namespace zth { namespace fibered { \
		extern ::zth::TypedFiberFactory<decltype(&::f)> const f; \
	} }
#define declare_fibered(...) FOREACH(declare_fibered_1, ##__VA_ARGS__)

#define define_fibered_1(f) \
	namespace zth { namespace fibered { \
		::zth::TypedFiberFactory<decltype(&::f)> const f(&::f, ::zth::Config::EnableDebugPrint ? ZTH_STRINGIFY(f) "()" : NULL); \
	} } \
	typedef ::zth::TypedFiberFactory<decltype(&::f)>::AutoFuture_type f##_future;

#define define_fibered(...) FOREACH(define_fibered_1, ##__VA_ARGS__)

#define make_fibered(...) \
	declare_fibered(__VA_ARGS__) \
	define_fibered(__VA_ARGS__)

#define async ::zth::fibered::

#endif // __cplusplus
#endif // __ZTH_ASYNC_H
