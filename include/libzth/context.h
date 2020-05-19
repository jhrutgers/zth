#ifndef __ZTH_CONTEXT
#define __ZTH_CONTEXT
/*
 * Zth (libzth), a cooperative userspace multitasking library.
 * Copyright (C) 2019  Jochem Rutgers
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

#ifdef ZTH_STACK_SWITCH
/*!
 * \brief Call the function \p f using the new stack pointer.
 * \param sp the new stack pointer.  If \c NULL, the Worker's stack is used.
 * \param ss the new stack size. This is not required to make the jump, but if 0, there will not be any stack overflow guard used.
 * \param f the function to be called. When \p f returns, the previous stack is restored.
 * \param arg the argument to pass to \p f.
 */
EXTERN_C ZTH_EXPORT void* zth_stack_switch(void* sp, size_t ss, void*(*f)(void*), void* arg);
#else
#  define zth_stack_switch(sp, f, ...) ((f)(__VA_ARGS__))
#endif

#ifdef __cplusplus
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

	namespace impl {
		template <typename T> struct stack_switch_type_check1 { enum { value = sizeof(T) <= sizeof(void*) }; };
		template <> struct stack_switch_type_check1<void> { enum { value = 1 }; };
		template <typename T> struct stack_switch_type_check1<T&> { enum { value = 0 }; };

		template <typename R, typename A1 = void, typename A2 = void, typename A3 = void,
			bool supported =
				stack_switch_type_check1<R>::value &&
				stack_switch_type_check1<A1>::value &&
				stack_switch_type_check1<A2>::value &&
				stack_switch_type_check1<A3>::value>
		struct stack_switch_type_check { typedef R type; };

		template <typename R, typename A1, typename A2, typename A3>
		struct stack_switch_type_check<R,A1,A2,A3,false> {};

		template <typename A1, typename A2, typename A3>
		struct stack_switch_type_check<void,A1,A2,A3,true> { typedef void* type; };
	}

	template <typename R>
	ZTH_INLINE typename impl::stack_switch_type_check<R>::type stack_switch(void* sp, R(*f)()) {
		return reinterpret_cast<typename impl::stack_switch_type_check<R>::type>(zth_stack_switch(sp, (void(*)())f)); }
	template <typename R, typename A1>
	ZTH_INLINE typename impl::stack_switch_type_check<R,A1>::type stack_switch(void* sp, R(*f)(A1), A1 a1) {
		return reinterpret_cast<typename impl::stack_switch_type_check<R>::type>(zth_stack_switch(sp, (void(*)())f, a1)); }
	template <typename R, typename A1, typename A2>
	ZTH_INLINE typename impl::stack_switch_type_check<R,A1,A2>::type stack_switch(void* sp, R(*f)(A1,A2), A1 a1, A2 a2) {
		return reinterpret_cast<typename impl::stack_switch_type_check<R>::type>(zth_stack_switch(sp, (void(*)())f, a1, a2)); }
	template <typename R, typename A1, typename A2, typename A3>
	ZTH_INLINE typename impl::stack_switch_type_check<R,A1,A2,A3>::type stack_switch(void* sp, R(*f)(A1,A2,A3), A1 a1, A2 a2, A3 a3) {
		return reinterpret_cast<typename impl::stack_switch_type_check<R>::type>(zth_stack_switch(sp, (void(*)())f, a1, a2, a3)); }

} // namespace

#endif // __cplusplus
#endif // __ZTH_CONTEXT
