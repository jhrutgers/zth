/*
 * Zth (libzth), a cooperative userspace multitasking library.
 * Copyright (C) 2019-2022  Jochem Rutgers
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
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
	int res = 0;
	if(unlikely((res = fiberHook(*f)))) {
		// Rollback.
		delete f;
		return res;
	}

	w->add(f);
	return 0;
}

} // namespace zth
