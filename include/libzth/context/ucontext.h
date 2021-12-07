#ifndef ZTH_CONTEXT_UCONTEXT_H
#define ZTH_CONTEXT_UCONTEXT_H
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

#ifndef ZTH_CONTEXT_CONTEXT_H
#  error This file must be included by libzth/context/context.h.
#endif

#ifdef __cplusplus

#ifdef ZTH_OS_MAC
// For ucontext_t
#  define _XOPEN_SOURCE
#  include <sched.h>
#endif

#include <csetjmp>
#include <csignal>
#include <ucontext.h>

namespace zth {
namespace impl {

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations" // I know...
class ContextUcontext : public ContextArch<ContextUcontext> {
	ZTH_CLASS_NEW_DELETE(ContextUcontext)
public:
	typedef ContextArch<ContextUcontext> base;

	constexpr explicit ContextUcontext(ContextAttr const& attr)
		: base(attr)
		, m_env()
	{}

private:
	static void context_trampoline(Context* context, sigjmp_buf origin)
	{
#ifdef ZTH_ENABLE_ASAN
		void const* oldstack = nullptr;
		size_t oldsize = 0;
		__sanitizer_finish_switch_fiber(nullptr, &oldstack, &oldsize);

		// We are jumping back.
		__sanitizer_start_switch_fiber(nullptr, oldstack, oldsize);
#endif

		if(sigsetjmp(context->m_env, Config::ContextSignals) == 0)
			siglongjmp(origin, 1);

		// Note that context_entry has the __sanitizer_finish_switch_fiber().
		context_entry(context);
	}

public:
	int create() noexcept
	{
		int res = base::create();
		if(unlikely(res))
			return res;

		if(unlikely(!stack()))
			// Stackless fiber only saves current context; nothing to do.
			return 0;

		ucontext_t uc;
		if(getcontext(&uc))
			return EINVAL;

		uc.uc_link = nullptr;
		Stack const& stack_ = stackUsable();
		uc.uc_stack.ss_sp = stack_.p;
		uc.uc_stack.ss_size = stack_.size;

		sigjmp_buf origin;
		makecontext(&uc, reinterpret_cast<void(*)(void)>(&context_trampoline), 2, context, origin);

#ifdef ZTH_ENABLE_ASAN
		void* fake_stack = nullptr;
		__sanitizer_start_switch_fiber(&fake_stack, stack_.p, stack_.size);
#endif

		if(sigsetjmp(origin, Config::ContextSignals) == 0)
			setcontext(&uc);

#ifdef ZTH_ENABLE_ASAN
		__sanitizer_finish_switch_fiber(fake_stack, nullptr, nullptr);
#endif
		return 0;
	}

	void context_switch(Context& to) noexcept
	{
		// switchcontext() restores signal masks, which is slow...
		if(sigsetjmp(m_env, Config::ContextSignals) == 0)
			siglongjmp(to->m_env, 1);
	}

private:
	sigjmp_buf m_env;
};
#pragma GCC diagnostic pop

} // namespace impl

typedef impl::ContextUcontext Context;
} // namespace zth
#endif // __cplusplus
#endif // ZTH_CONTEXT_UCONTEXT_H
