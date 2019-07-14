#include <zth>
#include <stdio.h>

// Even though all code within a fiber is executed atomically when not yielded,
// sometimes it is required to yield within a (long) critical section anyway.
// If you have many fibers, but only a few may be trying to access the critical
// section, it is still nice to yield.  In that case, synchronization
// primitives are required. Zth offers a few, namely a mutex, signal (a.k.a.
// condition), and a semaphore.  Their interface is straight-forward. Here, we
// only show an example with a mutex.

static zth::Mutex mutex;

void fiber(int id) {
	for(int i = 0; i < 3; i++) {
		printf("fiber %d outside of critical section\n", id);

		mutex.lock();
		printf(" { fiber %d at start of critical section\n", id);
		// It does not matter how and how often we yield, other fibers trying
		// to lock the mutex will not be scheduled, but others may.
		zth::yield();
		zth::outOfWork();
		printf(" } fiber %d at end of critical section\n", id);
		mutex.unlock();
	
		zth::outOfWork();
	}
}
zth_fiber(fiber)

static bool stopOther = false;

void otherFiber() {
	while(!stopOther) {
		printf("Other fiber\n");
		zth::outOfWork();
	}
}
zth_fiber(otherFiber)

void main_fiber(int argc, char** argv) {
	fiber_future f1 = async fiber(1);
	fiber_future f2 = async fiber(2);
	fiber_future f3 = async fiber(3);
	async otherFiber();
	f1->wait();
	f2->wait();
	f3->wait();
	stopOther = true;
}

