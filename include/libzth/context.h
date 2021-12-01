#ifndef ZTH_CONTEXT_H
#define ZTH_CONTEXT_H
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
 * \param stack the new stack area. If \c NULL, the Worker's stack is used. If \p size is 0, this is the stack pointer, otherwise the stack pointer is calculated according to the ABI (growing up/down) within \p stack + \p size.
 * \param size the new stack size. This is not required to make the jump, but if 0, there will not be any stack overflow guard used.
 * \param f the function with prototype \c void*(void*) to be called. When \p f returns, the previous stack is restored.
 * \param arg the argument to pass to \p f.
 * \ingroup zth_api_c_stack
 */
#ifdef ZTH_STACK_SWITCH
EXTERN_C ZTH_EXPORT void* zth_stack_switch(void* stack, size_t size, void*(*f)(void*), void* arg);
#else
#  define zth_stack_switch(stack, size, f, arg) ({ (void)(stack); (void)(size); ((f)(arg)); })
#endif

#ifdef __cplusplus
#  include <libzth/util.h>

#  if __cplusplus >= 201103L
#    include <tuple>
#  endif

namespace zth {
	class Context;

	struct ContextAttr {
		typedef void* EntryArg;
		typedef void(*Entry)(EntryArg);

		ContextAttr(Entry entry = NULL, EntryArg arg = EntryArg())
			: stackSize(Config::DefaultFiberStackSize)
			, entry(entry)
			, arg(arg)
		{}

		size_t stackSize;
		Entry entry;
		EntryArg arg;
	};

	int context_init();
	void context_deinit();
	int context_create(Context*& context, ContextAttr const& attr);
	void context_switch(Context* from, Context* to);
	void context_destroy(Context* context);

	void stack_watermark_init(void* stack, size_t size);
	size_t stack_watermark_size(void* stack);
	size_t stack_watermark_maxused(void* stack);
	size_t stack_watermark_remaining(void* stack);
	size_t context_stack_usage(Context* context);




	namespace impl {
		template <typename R>
		struct FunctionIO0 {
			union { struct { R(*f)(); }; R r; };
			void* operator()() { r = f(); return &r; }
		};
		template <>
		struct FunctionIO0<void> {
			union { struct { void(*f)(); }; };
			void* operator()() { f(); return NULL; }
		};
		template <typename R, typename A1>
		struct FunctionIO1 {
			union { struct { R(*f)(A1); A1 a1; }; R r; };
			void* operator()() { r = f(a1); return &r; }
		};
		template <typename A1>
		struct FunctionIO1<void,A1> {
			union { struct { void(*f)(A1); A1 a1; }; };
			void* operator()() { f(a1); return NULL; }
		};
		template <typename R, typename A1, typename A2>
		struct FunctionIO2 {
			union { struct { R(*f)(A1,A2); A1 a1; A2 a2; }; R r; };
			void* operator()() { r = f(a1,a2); return &r; }
		};
		template <typename A1, typename A2>
		struct FunctionIO2<void,A1,A2> {
			union { struct { void(*f)(A1,A2); A1 a1; A2 a2; }; };
			void* operator()() { f(a1,a2); return NULL; }
		};
		template <typename R, typename A1, typename A2, typename A3>
		struct FunctionIO3 {
			union { struct { R(*f)(A1,A2,A3); A1 a1; A2 a2; A3 a3; }; R r; };
			void* operator()() { r = f(a1,a2,a3); return &r; }
		};
		template <typename A1, typename A2, typename A3>
		struct FunctionIO3<void,A1,A2,A3> {
			union { struct { void(*f)(A1,A2,A3); A1 a1; A2 a2; A3 a3; }; };
			void* operator()() { f(a1,a2,a3); return NULL; }
		};
#if __cplusplus >= 201103L
		template <typename R, typename... A>
		struct FunctionION {
			union { struct { R(*f)(A...); std::tuple<A...> a; }; R r; };
			void* operator()() { call(typename SequenceGenerator<sizeof...(A)>::type()); return &r; }
		private:
			template <size_t... S> void call(Sequence<S...>) { r = f(std::get<S>(a)...); }
		};
		template <typename... A>
		struct FunctionION<void,A...> {
			union { struct { void(*f)(A...); std::tuple<A...> a; }; };
			void* operator()() { call(typename SequenceGenerator<sizeof...(A)>::type()); return NULL; }
		private:
			template <size_t... S> void call(Sequence<S...>) { f(std::get<S>(a)...); }
		};
#endif

		template <typename F> void* stack_switch_fwd(F* f) { return (*f)(); }
	} // namespace

	/*!
	 * \copydoc zth::stack_switch(void*,size_t,void(*)())
	 */
	template <typename R>
	inline R stack_switch(void* stack, size_t size, R(*f)())
	{
		impl::FunctionIO0<R> f_ = {{f}};
		return *(R*)zth_stack_switch(stack, size, (void*(*)(void*))&impl::stack_switch_fwd<decltype(f_)>, &f_);
	}

	/*!
	 * \brief \copybrief zth_stack_switch()
	 * \details Type-safe C++ wrapper for #zth_stack_switch().
	 * \ingroup zth_api_cpp_stack
	 */
	inline void stack_switch(void* stack, size_t size, void(*f)())
	{
		impl::FunctionIO0<void> f_ = {{f}};
		zth_stack_switch(stack, size, (void*(*)(void*))&impl::stack_switch_fwd<decltype(f_)>, &f_);
	}

	/*!
	 * \copydoc zth::stack_switch(void*,size_t,void(*)())
	 * \ingroup zth_api_cpp_stack
	 */
	template <typename R, typename A1>
	inline R stack_switch(void* stack, size_t size, R(*f)(A1), A1 a1)
	{
		impl::FunctionIO1<R,A1> f_ = {{f, a1}};
		return *(R*)zth_stack_switch(stack, size, (void*(*)(void*))&impl::stack_switch_fwd<decltype(f_)>, &f_);
	}

	/*!
	 * \copydoc zth::stack_switch(void*,size_t,void(*)())
	 * \ingroup zth_api_cpp_stack
	 */
	template <typename A1>
	inline void stack_switch(void* stack, size_t size, void(*f)(A1), A1 a1)
	{
		impl::FunctionIO1<void,A1> f_ = {{f, a1}};
		zth_stack_switch(stack, size, (void*(*)(void*))&impl::stack_switch_fwd<decltype(f_)>, &f_);
	}

	/*!
	 * \copydoc zth::stack_switch(void*,size_t,void(*)())
	 * \ingroup zth_api_cpp_stack
	 */
	template <typename R, typename A1, typename A2>
	inline R stack_switch(void* stack, size_t size, R(*f)(A1,A2), A1 a1, A2 a2)
	{
		impl::FunctionIO2<R,A1,A2> f_ = {{f, a1, a2}};
		return *(R*)zth_stack_switch(stack, size, (void*(*)(void*))&impl::stack_switch_fwd<decltype(f_)>, &f_);
	}

	/*!
	 * \copydoc zth::stack_switch(void*,size_t,void(*)())
	 * \ingroup zth_api_cpp_stack
	 */
	template <typename A1, typename A2>
	inline void stack_switch(void* stack, size_t size, void(*f)(A1,A2), A1 a1, A2 a2)
	{
		impl::FunctionIO2<void,A1,A2> f_ = {{f, a1, a2}};
		zth_stack_switch(stack, size, (void*(*)(void*))&impl::stack_switch_fwd<decltype(f_)>, &f_);
	}

	/*!
	 * \copydoc zth::stack_switch(void*,size_t,void(*)())
	 * \ingroup zth_api_cpp_stack
	 */
	template <typename R, typename A1, typename A2, typename A3>
	inline R stack_switch(void* stack, size_t size, R(*f)(A1,A2,A3), A1 a1, A2 a2, A3 a3)
	{
		impl::FunctionIO3<R,A1,A2,A3> f_ = {{f, a1, a2, a3}};
		return *(R*)zth_stack_switch(stack, size, (void*(*)(void*))&impl::stack_switch_fwd<decltype(f_)>, &f_);
	}

	/*!
	 * \copydoc zth::stack_switch(void*,size_t,void(*)())
	 * \ingroup zth_api_cpp_stack
	 */
	template <typename A1, typename A2, typename A3>
	inline void stack_switch(void* stack, size_t size, void(*f)(A1,A2,A3), A1 a1, A2 a2, A3 a3)
	{
		impl::FunctionIO3<void,A1,A2,A3> f_ = {{f, a1, a2, a3}};
		zth_stack_switch(stack, size, (void*(*)(void*))&impl::stack_switch_fwd<decltype(f_)>, &f_);
	}

#  if __cplusplus >= 201103L
	/*!
	 * \copydoc zth::stack_switch(void*,size_t,void(*)())
	 * \ingroup zth_api_cpp_stack
	 */
	template <typename R, typename A1, typename A2, typename A3, typename... A>
	inline typename std::enable_if<!std::is_void<R>::value,R>::type stack_switch(void* stack, size_t size, R(*f)(A1,A2,A3,A...), A1 a1, A2 a2, A3 a3, A... a)
	{
		impl::FunctionION<R,A1,A2,A3,A...> f_ = {{f, std::make_tuple(a1, a2, a3, a...)}};
		return *(R*)zth_stack_switch(stack, size, (void*(*)(void*))&impl::stack_switch_fwd<decltype(f_)>, &f_);
	}

	/*!
	 * \copydoc zth::stack_switch(void*,size_t,void(*)())
	 * \ingroup zth_api_cpp_stack
	 */
	template <typename A1, typename A2, typename A3, typename... A>
	inline void stack_switch(void* stack, size_t size, void(*f)(A1,A2,A3,A...), A1 a1, A2 a2, A3 a3, A... a)
	{
		impl::FunctionION<void,A1,A2,A3,A...> f_ = {{f, std::make_tuple(a1, a2, a3, a...)}};
		zth_stack_switch(stack, size, (void*(*)(void*))&impl::stack_switch_fwd<decltype(f_)>, &f_);
	}
#  endif
} // namespace

#endif // __cplusplus
#endif // ZTH_CONTEXT_H
