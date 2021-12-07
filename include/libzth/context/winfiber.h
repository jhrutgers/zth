#ifndef ZTH_CONTEXT_WINFIBER_H
#define ZTH_CONTEXT_WINFIBER_H
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

#include <windows.h>

namespace zth {
namespace impl {

class ContextWinfiber : public ContextArch<ContextWinfiber> {
	ZTH_CLASS_NEW_DELETE(ContextWinfiber)
public:
	typedef ContextArch<ContextWinfiber> base;

	constexpr explicit ContextWinfiber(ContextAttr const& attr) noexcept
		: base(attr)
		, m_fiber()
	{}

	static int init() noexcept
	{
		int res = 0;
		if(ConvertThreadToFiber(nullptr))
			return 0;
		else if((res = -(int)GetLastError()))
			return res;
		else
			return EINVAL;
	}

	int initStack(Stack& UNUSED_PAR(stack), Stack& UNUSED_PAR(usable)) noexcept
	{
		// Stack is managed by Windows.
		usable = stack;
		return 0;
	}

	void deinitStack(Stack& UNUSED_PAR(stack)) noexcept
	{
	}

	int stackGuardInit() noexcept
	{
		// There is no stack to guard.
		return 0;
	}

	void stackGuardDeinit() noexcept
	{
	}

	void valgrindRegister() noexcept
	{
		// There is no stack to register.
	}

	void valgrindDeregister() noexcept
	{
	}

	int create() noexcept
	{
		int res = 0;
		if((res = base::create()))
			return res;

		if(unlikely(!stackSize())) {
			// Stackless fiber only saves current context.
			if((m_fiber = GetCurrentFiber()))
				return 0;
		} else if((m_fiber = CreateFiber((SIZE_T)Config::DefaultFiberStackSize, reinterpret_cast<LPFIBER_START_ROUTINE>(&context_entry), (LPVOID)this))) {
			zth_dbg(context, "[%s] Created fiber %p", currentWorker().id_str(), m_fiber);
			return 0;
		}

		if((res = -(int)GetLastError()))
			return res;
		else
			return EINVAL;
	}

	void destroy() noexcept
	{
		if(!m_fiber)
			return;

		// If the current fiber is the one to delete, it is probably not created by us.
		if(likely(m_fiber != GetCurrentFiber())) {
			zth_dbg(context, "[%s] Delete fiber %p", currentWorker().id_str(), m_fiber);
			DeleteFiber(m_fiber);
		}

		m_fiber = nullptr;
	}

	void context_switch(Context& to) noexcept
	{
		zth_assert(to.m_fiber && to.m_fiber != GetCurrentFiber());
		SwitchToFiber(to.m_fiber);
	}

private:
	LPVOID m_fiber;
};

} // namespace impl

typedef impl::ContextWinfiber Context;
} // namespace zth
#endif // __cplusplus
#endif // ZTH_CONTEXT_WINFIBER_H
