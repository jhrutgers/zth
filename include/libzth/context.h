#ifndef ZTH_CONTEXT_H
#define ZTH_CONTEXT_H
/*
 * SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#include <libzth/macros.h>

#include <libzth/config.h>

/*!
 * \defgroup zth_api_c_stack stack
 * \ingroup zth_api_c
 */
/*!
 * \defgroup zth_api_cpp_stack stack
 * \ingroup zth_api_cpp
 */

/*!
 * \brief Call the function \p f using the new stack pointer.
 * \param stack the new stack area. If \c nullptr, the Worker's stack is used.
 *	If \p size is 0, this is the stack pointer, otherwise the stack pointer
 *	is calculated according to the ABI (growing up/down) within \p stack +
 *	\p size.
 * \param size the new stack size. This is not required to make the jump, but
 *	if 0, there will not be any stack overflow guard used.
 * \param f the function with prototype \c void*(void*) to be called. When \p f
 *	returns, the previous stack is restored.
 * \param arg the argument to pass to \p f.
 * \ingroup zth_api_c_stack
 * \hideinitializer
 */
#ifdef ZTH_STACK_SWITCH
EXTERN_C ZTH_EXPORT void*
zth_stack_switch(void* stack, size_t size, void* (*f)(void*) noexcept, void* arg) noexcept;
#else // !ZTH_STACK_SWITCH
#  define zth_stack_switch(stack, size, f, arg) \
	  ({                                    \
	    (void)(stack);                      \
	    (void)(size);                       \
	    ((f)(arg));                         \
	  })
#endif // !ZTH_STACK_SWITCH

#ifdef __cplusplus
#  include <libzth/util.h>

#  if __cplusplus >= 201103L
#    include <tuple>
#  endif

#  if __cplusplus < 201103L
#    undef ZTH_STACK_SWITCH98
// Always use C++98-compatible stack_switch() implementation.
#    define ZTH_STACK_SWITCH98 1
#  elif !defined(ZTH_STACK_SWITCH98)
// By default only use C++11 stack_switch() implementation, but this can be
// overridden for testing purposes.
#    define ZTH_STACK_SWITCH98 0
#  endif

namespace zth {

class Context;

struct ContextAttr {
	typedef void* EntryArg;
	typedef void (*Entry)(EntryArg);

	constexpr explicit ContextAttr(Entry entry_ = nullptr, EntryArg arg_ = EntryArg()) noexcept
		: stackSize(Config::DefaultFiberStackSize)
		, entry(entry_)
		, arg(arg_)
	{}

	size_t stackSize;
	Entry entry;
	EntryArg arg;
};

int context_init() noexcept;
void context_deinit() noexcept;
int context_create(Context*& context, ContextAttr const& attr) noexcept;
void context_switch(Context* from, Context* to) noexcept;
void context_destroy(Context* context) noexcept;
size_t context_stack_usage(Context* context) noexcept;

void stack_watermark_init(void* stack, size_t size) noexcept;
size_t stack_watermark_size(void* stack) noexcept;
size_t stack_watermark_maxused(void* stack) noexcept;
size_t stack_watermark_remaining(void* stack) noexcept;



namespace impl {

#  if ZTH_STACK_SWITCH98
template <typename R>
struct FunctionIO0 {
	union {
		struct {
			R (*f)();
		};
		R r;
	};

	void* operator()()
	{
		r = f();
		return &r;
	}
};

template <>
struct FunctionIO0<void> {
	union {
		struct {
			void (*f)();
		};
	};

	void* operator()()
	{
		f();
		return nullptr;
	}
};

template <typename R, typename A1>
struct FunctionIO1 {
	union {
		struct {
			R (*f)(A1);
			A1 a1;
		};
		R r;
	};

	void* operator()()
	{
		r = f(a1);
		return &r;
	}
};

template <typename A1>
struct FunctionIO1<void, A1> {
	union {
		struct {
			void (*f)(A1);
			A1 a1;
		};
	};

	void* operator()()
	{
		f(a1);
		return nullptr;
	}
};

template <typename R, typename A1, typename A2>
struct FunctionIO2 {
	union {
		struct {
			R (*f)(A1, A2);
			A1 a1;
			A2 a2;
		};
		R r;
	};

	void* operator()()
	{
		r = f(a1, a2);
		return &r;
	}
};

template <typename A1, typename A2>
struct FunctionIO2<void, A1, A2> {
	union {
		struct {
			void (*f)(A1, A2);
			A1 a1;
			A2 a2;
		};
	};

	void* operator()()
	{
		f(a1, a2);
		return nullptr;
	}
};

template <typename R, typename A1, typename A2, typename A3>
struct FunctionIO3 {
	union {
		struct {
			R (*f)(A1, A2, A3);
			A1 a1;
			A2 a2;
			A3 a3;
		};
		R r;
	};
	void* operator()()
	{
		r = f(a1, a2, a3);
		return &r;
	}
};

template <typename A1, typename A2, typename A3>
struct FunctionIO3<void, A1, A2, A3> {
	union {
		struct {
			void (*f)(A1, A2, A3);
			A1 a1;
			A2 a2;
			A3 a3;
		};
	};

	void* operator()()
	{
		f(a1, a2, a3);
		return nullptr;
	}
};

#  elif __cplusplus >= 201103L

template <typename T>
struct Packed {
	using type = typename std::decay<T>::type;
};

template <typename T>
struct Packed<T&> {
	using type = std::reference_wrapper<typename std::decay<T>::type>;
};

template <typename... A>
struct Arguments {
	template <typename R>
	using function_type = R(A...);
	using tuple_type = std::tuple<typename Packed<A>::type...>;
	constexpr static size_t size = sizeof...(A);
};

template <typename R, typename A, typename A_>
struct FunctionION {
	union {
		struct {
			typename A::template function_type<R>* f;
			typename A_::tuple_type a;
		};
		typename Packed<R>::type r;
	};

	void* operator()() noexcept
	{
		call(typename SequenceGenerator<A::size>::type(), A());
		return &r;
	}

private:
	template <size_t... S, typename... _A>
	void call(Sequence<S...>, Arguments<_A...>) noexcept
	{
		r = f(std::forward<_A>(std::get<S>(a))...);
	}
};

template <typename A, typename A_>
struct FunctionION<void, A, A_> {
	union {
		struct {
			typename A::template function_type<void>* f;
			typename A_::tuple_type a;
		};
	};

	void* operator()() noexcept
	{
		call(typename SequenceGenerator<A::size>::type(), A());
		return nullptr;
	}

private:
	template <size_t... S, typename... _A>
	void call(Sequence<S...>, Arguments<_A...>) noexcept
	{
		f(std::forward<_A>(std::get<S>(a))...);
	}
};
#  endif // C++11

template <typename F>
void* stack_switch_fwd(void* f) noexcept
{
	return (*static_cast<F*>(f))();
}

} // namespace impl

#  if ZTH_STACK_SWITCH98
/*!
 * \copydoc zth::stack_switch(void,size_t,void(*)())
 */
template <typename R>
inline R stack_switch(void* stack, size_t size, R (*f)())
{
	impl::FunctionIO0<R> f_ = {{f}};
	return *static_cast<R*>(
		zth_stack_switch(stack, size, &impl::stack_switch_fwd<decltype(f_)>, &f_));
}

/*!
 * \brief \copybrief zth_stack_switch()
 * \details Type-safe C++ wrapper for #zth_stack_switch().
 * \ingroup zth_api_cpp_stack
 */
inline void stack_switch(void* stack, size_t size, void (*f)())
{
	impl::FunctionIO0<void> f_ = {{f}};
	zth_stack_switch(stack, size, &impl::stack_switch_fwd<decltype(f_)>, &f_);
}

/*!
 * \copydoc zth::stack_switch(void,size_t,void(*)())
 * \ingroup zth_api_cpp_stack
 */
template <typename R, typename A1>
inline R stack_switch(void* stack, size_t size, R (*f)(A1), A1 a1)
{
	impl::FunctionIO1<R, A1> f_ = {{f, a1}};
	return *static_cast<R*>(
		zth_stack_switch(stack, size, &impl::stack_switch_fwd<decltype(f_)>, &f_));
}

/*!
 * \copydoc zth::stack_switch(void,size_t,void(*)())
 * \ingroup zth_api_cpp_stack
 */
template <typename A1>
inline void stack_switch(void* stack, size_t size, void (*f)(A1), A1 a1)
{
	impl::FunctionIO1<void, A1> f_ = {{f, a1}};
	zth_stack_switch(stack, size, &impl::stack_switch_fwd<decltype(f_)>, &f_);
}

/*!
 * \copydoc zth::stack_switch(void,size_t,void(*)())
 * \ingroup zth_api_cpp_stack
 */
template <typename R, typename A1, typename A2>
inline R stack_switch(void* stack, size_t size, R (*f)(A1, A2), A1 a1, A2 a2)
{
	impl::FunctionIO2<R, A1, A2> f_ = {{f, a1, a2}};
	return *static_cast<R*>(
		zth_stack_switch(stack, size, &impl::stack_switch_fwd<decltype(f_)>, &f_));
}

/*!
 * \copydoc zth::stack_switch(void,size_t,void(*)())
 * \ingroup zth_api_cpp_stack
 */
template <typename A1, typename A2>
inline void stack_switch(void* stack, size_t size, void (*f)(A1, A2), A1 a1, A2 a2)
{
	impl::FunctionIO2<void, A1, A2> f_ = {{f, a1, a2}};
	zth_stack_switch(stack, size, &impl::stack_switch_fwd<decltype(f_)>, &f_);
}

/*!
 * \copydoc zth::stack_switch(void,size_t,void(*)())
 * \ingroup zth_api_cpp_stack
 */
template <typename R, typename A1, typename A2, typename A3>
inline R stack_switch(void* stack, size_t size, R (*f)(A1, A2, A3), A1 a1, A2 a2, A3 a3)
{
	impl::FunctionIO3<R, A1, A2, A3> f_ = {{f, a1, a2, a3}};
	return *static_cast<R*>(
		zth_stack_switch(stack, size, &impl::stack_switch_fwd<decltype(f_)>, &f_));
}

/*!
 * \copydoc zth::stack_switch(void,size_t,void(*)())
 * \ingroup zth_api_cpp_stack
 */
template <typename A1, typename A2, typename A3>
inline void stack_switch(void* stack, size_t size, void (*f)(A1, A2, A3), A1 a1, A2 a2, A3 a3)
{
	impl::FunctionIO3<void, A1, A2, A3> f_ = {{f, a1, a2, a3}};
	zth_stack_switch(stack, size, &impl::stack_switch_fwd<decltype(f_)>, &f_);
}

#  elif __cplusplus >= 201103L // !ZTH_STACK_SWITCH98

/*!
 * \brief \copybrief zth_stack_switch()
 * \details Type-safe C++ wrapper for #zth_stack_switch().
 * \ingroup zth_api_cpp_stack
 */
template <typename R, typename... A, typename... A_>
inline typename std::enable_if<!std::is_void<R>::value, R>::type
stack_switch(void* stack, size_t size, R (*f)(A...) noexcept, A_&&... a) noexcept
{
	impl::FunctionION<R, impl::Arguments<A...>, impl::Arguments<A_&&...>> f_{
		{f, {std::forward<A_>(a)...}}};
	return std::move(*static_cast<typename impl::Packed<R>::type*>(
		zth_stack_switch(stack, size, &impl::stack_switch_fwd<decltype(f_)>, &f_)));
}

/*!
 * \brief \copybrief zth_stack_switch()
 * \details Type-safe C++ wrapper for #zth_stack_switch().
 * \ingroup zth_api_cpp_stack
 */
template <typename... A, typename... A_>
inline void stack_switch(void* stack, size_t size, void (*f)(A...) noexcept, A_&&... a) noexcept
{
	impl::FunctionION<void, impl::Arguments<A...>, impl::Arguments<A_&&...>> f_{
		{f, {std::forward<A_>(a)...}}};
	zth_stack_switch(stack, size, &impl::stack_switch_fwd<decltype(f_)>, &f_);
}
#  endif		       // !ZTH_STACK_SWITCH98

} // namespace zth
#endif // __cplusplus
#endif // ZTH_CONTEXT_H
