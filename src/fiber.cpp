/*
 * SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#include <libzth/fiber.h>

#include <libzth/worker.h>

namespace zth {

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
Fiber::FiberHook* Fiber::hookNew = nullptr;
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
Fiber::FiberHook* Fiber::hookDead = nullptr;

int Runnable::run()
{
	Worker* w = Worker::instance();
	if(unlikely(!w))
		return EAGAIN;
	if(fiber())
		return EALREADY;

	Fiber* f = new Fiber(&Runnable::entry_, static_cast<void*>(this));

	try {
		int res = fiberHook(*f);
		if(unlikely(res)) {
			// Rollback.
			m_fiber = nullptr;
			delete f;
			return res;
		}
	} catch(...) {
		m_fiber = nullptr;
		delete f;
		zth_throw();
	}

	f->used();
	w->add(f);
	return 0;
}

} // namespace zth
