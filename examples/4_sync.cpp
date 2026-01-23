/*
 * SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <zth>

#include <cstdio>

// Even though all code within a fiber is executed atomically when not yielded, sometimes it is
// required to yield within a (long) critical section anyway.  If you have many fibers, but only a
// few may be trying to access the critical section, it is still nice to yield.  In that case,
// synchronization primitives are required. Zth offers a few, namely a mutex, signal (a.k.a.
// condition), and a semaphore.  Their interface is straight-forward. Here, we only show an example
// with a mutex.

static zth::Mutex mutex;
static zth::Gate gate(4);

static void fiber(int id)
{
	zth::currentFiber().setName(zth::format("fiber %d", id));

	for(int i = 0; i < 3; i++) {
		printf("fiber %d outside of critical section\n", id);

		zth::outOfWork();
		zth::outOfWork();

		zth::Locked l(mutex);

		printf(" { fiber %d at start of critical section\n", id);
		// It does not matter how and how often we yield, other fibers trying to lock the
		// mutex will not be scheduled, but others may.
		zth::yield();
		zth::outOfWork();
		zth::outOfWork();
		zth::outOfWork();
		printf(" } fiber %d at end of critical section\n", id);
	}

	gate.pass();
}

static bool stopOther = false;

static void otherFiber()
{
	zth::currentFiber().setName("other fiber");

	while(!stopOther) {
		printf("Other fiber\n");
		zth::outOfWork();
	}
}

int main_fiber(int /*argc*/, char** /*argv*/)
{
	zth::fiber(fiber, 1);
	zth::fiber(fiber, 2);
	zth::fiber(fiber, 3);
	zth::fiber(otherFiber);

	gate.wait();

	stopOther = true;
	return 0;
}
