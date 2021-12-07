#ifndef ZTH_CONTEXT_SJLJ_H
#define ZTH_CONTEXT_SJLJ_H
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

#include <csetjmp>
#include <csignal>

namespace zth {
namespace impl {

class ContextSjlj : public ContextArch<ContextSjlj> {
	ZTH_CLASS_NEW_DELETE(ContextSjlj)
public:
	typedef ContextArch<ContextSjlj> base;

	constexpr explicit ContextSjlj(ContextAttr const& attr) noexcept
		: base(attr)
		, m_env()
	{}

private:
#ifdef ZTH_OS_BAREMETAL
	// As bare metal does not have signals, sigsetjmp and siglongjmp is not necessary.

	typedef jmp_buf jmp_buf_type;

	int setjmp(jmp_buf env)
	{
		return ::setjmp(env);
	}

	void longjmp(jmp_buf env, int val = 1)
	{
		return ::longjmp(env, val);
	}
#else
	typedef sigjmp_buf jmp_buf_type;

	int setjmp(sigjmp_buf env)
	{
		return ::sigsetjmp(env, Config::ContextSignals);
	}

	void longjmp(sigjmp_buf env, int val = 1)
	{
		return ::siglongjmp(env, val);
	}
#endif

public:
	int create() noexcept
	{
		int res = base::create();
		if(res)
			return res;

		if(unlikely(!stack()))
			// Stackless fiber only saves current context; nothing to do.
			return 0;

		setjmp(m_env);
		void** sp_ = sp(Stack(stackUsable(), stackUsableSize()));
		set_sp(m_env, sp_);
		set_pc(m_env, &context_trampoline_from_jmp_buf);
		stack_push(sp_, this);
		return 0;
	}

	void context_switch(Context& to) noexcept
	{
		context_push_regs();

		if(setjmp(m_env) == 0) {
			context_prepare_jmp(to);
			longjmp(to.m_env, 1);
		}

		context_pop_regs();
	}

private:
	jmp_buf_type m_env;
};

} // namespace impl

typedef impl::ContextSjlj Context;
} // namespace zth
#endif // __cplusplus
#endif // ZTH_CONTEXT_SJLJ_H
