/*
 * SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#include <libzth/fiber.h>

#include <libzth/worker.h>

namespace zth {

Fiber::FiberHook* Fiber::hookNew = nullptr;
Fiber::FiberHook* Fiber::hookDead = nullptr;

int Runnable::run()
{
	Worker* w = Worker::instance();
	if(unlikely(!w))
		return EAGAIN;

	Fiber* f = new Fiber(&Runnable::entry_, (void*)this);
	try {
		int res = fiberHook(*f);
		if(unlikely(res)) {
			// Rollback.
			delete f;
			return res;
		}
	} catch(...) {
		delete f;
		zth_throw();
	}

	w->add(f);
	return 0;
}

} // namespace zth
