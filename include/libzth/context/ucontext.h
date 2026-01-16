#ifndef ZTH_CONTEXT_UCONTEXT_H
#define ZTH_CONTEXT_UCONTEXT_H
/*
 * SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef ZTH_CONTEXT_CONTEXT_H
#  error This file must be included by libzth/context/context.h.
#endif

#ifdef __cplusplus

#  ifdef ZTH_OS_MAC
// For ucontext_t
#    ifndef _XOPEN_SOURCE
#      error Please define _XOPEN_SOURCE before including headers.
#    endif
#    include <sched.h>
#  endif

#  include <csetjmp>
#  include <csignal>
#  include <ucontext.h>

namespace zth {

#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations" // I know...
class Context : public impl::ContextArch<Context> {
	ZTH_CLASS_NEW_DELETE(Context)
public:
	typedef impl::ContextArch<Context> base;

	constexpr explicit Context(ContextAttr const& attr) noexcept
		: base(attr)
		, m_env()
	{}

private:
	// It seems that in Apple's aarch64, makecontext() does not pass 64-bit pointer arguments.
	// Wrap the arguments in a struct, and pass it via separate low/high 32-bit args.
	struct context_trampoline_args {
		sigjmp_buf origin;
		Context* context;
	};

	static void context_trampoline(uint32_t args_high, uint32_t args_low) noexcept
	{
#  ifdef ZTH_ENABLE_ASAN
		void const* oldstack = nullptr;
		size_t oldsize = 0;
		__sanitizer_finish_switch_fiber(nullptr, &oldstack, &oldsize);

		Stack& workerStack = currentWorker().workerStack();
		if(unlikely(!workerStack))
			workerStack = Stack((void*)oldstack, oldsize);
#  endif

		context_trampoline_args* args = nullptr;
		if(sizeof(void*) == 4) {
			// NOLINTNEXTLINE
			args = reinterpret_cast<context_trampoline_args*>(
				(static_cast<uintptr_t>(args_low)));
		} else {
			// NOLINTNEXTLINE
			args = reinterpret_cast<context_trampoline_args*>(
				((static_cast<uintptr_t>(args_high) << 32)
				 | (static_cast<uintptr_t>(args_low))));
		}

		// Copy argument to local stack.
		Context* context = args->context;

		// We got here via setcontext().

#  ifdef ZTH_ENABLE_ASAN
		// We are jumping back.
		void* fake_stack = nullptr;
		__sanitizer_start_switch_fiber(&fake_stack, oldstack, oldsize);
#  endif

		// Save the current context, and return to create().
		if(sigsetjmp(context->m_env, Config::ContextSignals) == 0)
			siglongjmp(args->origin, 1);

#  ifdef ZTH_ENABLE_ASAN
		__sanitizer_finish_switch_fiber(fake_stack, nullptr, nullptr);
#  endif

		// args is no longer valid here.
		context_entry(context);
	}

public:
	// cppcheck-suppress duplInheritedMember
	int create() noexcept
	{
		int res = base::create();
		if(unlikely(res))
			return res;

		if(unlikely(!stack()))
			// Stackless fiber only saves current context; nothing to do.
			return 0;

		// Get current context, to inherit signals/masks.
		ucontext_t uc;
		if(getcontext(&uc))
			return EINVAL;

		// Modify the stack of the new context.
		uc.uc_link = nullptr;
		Stack const& stack_ = stackUsable();
		uc.uc_stack.ss_sp = stack_.p;
		uc.uc_stack.ss_size = stack_.size;

		// Modify the function to call from this new context.
		context_trampoline_args args = {};
		args.context = this;

		makecontext(
			&uc, reinterpret_cast<void (*)(void)>(&context_trampoline), 2,
			static_cast<uint32_t>((reinterpret_cast<uintptr_t>(&args)) >> 32),
			static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&args)));

#  ifdef ZTH_ENABLE_ASAN
		void* fake_stack = nullptr;
		__sanitizer_start_switch_fiber(&fake_stack, stack_.p, stack_.size);
#  endif

		// switchcontext() is slow, we want to use sigsetjmp/siglongjmp instead.
		// So, we initialize the sigjmp_buf from the just created context.
		// After this initial setup, context_switch() is good to go.
		if(sigsetjmp(args.origin, Config::ContextSignals) == 0) {
			// Here we go into the context for the first time.
			setcontext(&uc);
		}

		// Got back from context_trampoline(). The context is ready now.

#  ifdef ZTH_ENABLE_ASAN
		__sanitizer_finish_switch_fiber(fake_stack, nullptr, nullptr);
#  endif
		return 0;
	}

	// cppcheck-suppress duplInheritedMember
	void context_switch(Context& to) noexcept
	{
#  ifdef ZTH_ENABLE_ASAN
		zth_assert(to.alive());
		void* fake_stack = nullptr;

		Stack const* stack = &to.stackUsable();
		if(unlikely(!*stack))
			stack = &currentWorker().workerStack();

		__sanitizer_start_switch_fiber(
			alive() ? &fake_stack : nullptr, stack->p, stack->size);
#  endif

		// switchcontext() restores signal masks, which is slow...
		if(sigsetjmp(m_env, Config::ContextSignals) == 0)
			siglongjmp(to.m_env, 1);

#  ifdef ZTH_ENABLE_ASAN
		__sanitizer_finish_switch_fiber(fake_stack, nullptr, nullptr);
#  endif
	}

private:
	sigjmp_buf m_env;
};
#  pragma GCC diagnostic pop

} // namespace zth
#endif // __cplusplus
#endif // ZTH_CONTEXT_UCONTEXT_H
