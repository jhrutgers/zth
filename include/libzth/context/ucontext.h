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
	static void context_trampoline(Context* context, sigjmp_buf origin)
	{
		// We got here via setcontext().

#  ifdef ZTH_ENABLE_ASAN
		void const* oldstack = nullptr;
		size_t oldsize = 0;
		__sanitizer_finish_switch_fiber(nullptr, &oldstack, &oldsize);

		// We are jumping back.
		__sanitizer_start_switch_fiber(nullptr, oldstack, oldsize);
#  endif

		// Save the current context, and return to create().
		if(sigsetjmp(context->m_env, Config::ContextSignals) == 0)
			siglongjmp(origin, 1);

		// Note that context_entry has the __sanitizer_finish_switch_fiber().
		context_entry(context);
	}

public:
	// cppcheck-suppress duplInheritedMember
	int create() noexcept
	{
		zth_dbg(context, "[%s] Creating context %p", zth::currentWorker().id_str(), this);
		int res = base::create();
		zth_dbg(context, "[%s] Creating context %p = %d", zth::currentWorker().id_str(),
			this, res);
		if(unlikely(res))
			return res;

		if(unlikely(!stack()))
			// Stackless fiber only saves current context; nothing to do.
			return 0;

		// Get current context, to inherit signals/masks.
		ucontext_t uc;
		zth_dbg(context, "[%s] Getting ucontext for context %p",
			zth::currentWorker().id_str(), this);
		if(getcontext(&uc))
			return EINVAL;

		// Modify the stack of the new context.
		uc.uc_link = nullptr;
		Stack const& stack_ = stackUsable();
		uc.uc_stack.ss_sp = stack_.p;
		uc.uc_stack.ss_size = stack_.size;
		zth_dbg(context, "[%s] Stack for context %p: %p-%p", zth::currentWorker().id_str(),
			this, stack_.p, stack_.p + stack_.size - 1U);

		// Modify the function to call from this new context.
		sigjmp_buf origin;
		makecontext(
			&uc, reinterpret_cast<void (*)(void)>(&context_trampoline), 2, this,
			origin);

#  ifdef ZTH_ENABLE_ASAN
		void* fake_stack = nullptr;
		__sanitizer_start_switch_fiber(&fake_stack, stack_.p, stack_.size);
#  endif

		// switchcontext() is slow, we want to use sigsetjmp/siglongjmp instead.
		// So, we initialize the sigjmp_buf from the just created context.
		// After this initial setup, context_switch() is good to go.
		if(sigsetjmp(origin, Config::ContextSignals) == 0) {
			// Here we go into the context for the first time.
			zth_dbg(context, "[%s] Setting context for context %p",
				zth::currentWorker().id_str(), this);
			setcontext(&uc);
		}

		// Got back from context_trampoline(). The context is ready now.
		zth_dbg(context, "[%s] Created context %p", zth::currentWorker().id_str(), this);

#  ifdef ZTH_ENABLE_ASAN
		__sanitizer_finish_switch_fiber(fake_stack, nullptr, nullptr);
#  endif
		return 0;
	}

	// cppcheck-suppress duplInheritedMember
	void context_switch(Context& to) noexcept
	{
		// switchcontext() restores signal masks, which is slow...
		if(sigsetjmp(m_env, Config::ContextSignals) == 0)
			siglongjmp(to.m_env, 1);
	}

private:
	sigjmp_buf m_env;
};
#  pragma GCC diagnostic pop

} // namespace zth
#endif // __cplusplus
#endif // ZTH_CONTEXT_UCONTEXT_H
