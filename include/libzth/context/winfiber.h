#ifndef ZTH_CONTEXT_WINFIBER_H
#define ZTH_CONTEXT_WINFIBER_H
/*
 * SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef ZTH_CONTEXT_CONTEXT_H
#  error This file must be included by libzth/context/context.h.
#endif

#ifdef __cplusplus

#  include <windows.h>

namespace zth {

class Context : public impl::ContextArch<Context> {
	ZTH_CLASS_NEW_DELETE(Context)
public:
	typedef impl::ContextArch<Context> base;

	constexpr explicit Context(ContextAttr const& attr) noexcept
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

	void deinitStack(Stack& UNUSED_PAR(stack)) noexcept {}

	int stackGuardInit() noexcept
	{
		// There is no stack to guard.
		return 0;
	}

	void stackGuardDeinit() noexcept {}

	void valgrindRegister() noexcept
	{
		// There is no stack to register.
	}

	void valgrindDeregister() noexcept {}

	int create() noexcept
	{
		int res = 0;
		if((res = base::create()))
			return res;

		if(unlikely(!stack().size)) {
			// Stackless fiber only saves current context.
			if((m_fiber = GetCurrentFiber()))
				return 0;
		} else if((m_fiber = CreateFiber(
				   (SIZE_T)Config::DefaultFiberStackSize,
				   reinterpret_cast<LPFIBER_START_ROUTINE>(&context_entry),
				   (LPVOID)this))) {
			zth_dbg(context, "[%s] Created fiber %p", currentWorker().id_str(),
				m_fiber);
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

} // namespace zth
#endif // __cplusplus
#endif // ZTH_CONTEXT_WINFIBER_H
