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

#include <libzth/fiber.h>
#include <libzth/worker.h>

namespace zth {

#if __cplusplus >= 201103L
std::function<Fiber::FiberHook> Fiber::hookNew;
std::function<Fiber::FiberHook> Fiber::hookDead;
#else
Fiber::FiberHook* Fiber::hookNew = NULL;
Fiber::FiberHook* Fiber::hookDead = NULL;
#endif

int Runnable::run() {
	Worker* w = Worker::currentWorker();
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

} // namespace

