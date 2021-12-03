#include <zth>

#include <deque>

// No Zth function will yield the processor, except for the obvious ones
// (zth::yield(), zth::nap(), etc.) So, you must make sure that you relinquish
// the processor often enough to allow other fibers to proceed.  Essentially,
// there is way to do this: zth::yield().  When zth::yield() is called, the
// scheduler of the current worker is invoked and a context switch is made to
// the next fiber in line.

// There is a catch. Consider the following example:

// Tweak the load default value at will to observe differences depending on the
// speed of your CPU.
static void do_work(int amount, int load = 1000)
{
	for(int volatile i = 0; i < amount * load; i++);
}

void nice_fiber()
{
	for(int i = 0; i < 10; i++) {
		printf("Be nice %d\n", i);
		do_work(100);	// do some work
		zth::yield();
	}

	printf("nice_fiber() done\n");
}
zth_fiber(nice_fiber)

void nicer_fiber()
{
	for(int i = 0; i < 10; i++) {
		printf("Be nicer %d\n", i);
		// I am going to do do_work(100) too, like nice_fiber(), but I am not
		// sure how much time that would take without yielding.  So, I am
		// really nice to split the work in small packets.
		for(int w = 0; w < 100; w++) {
			do_work(1);
			zth::yield();
		}
	}

	printf("nicer_fiber() done\n");
}
zth_fiber(nicer_fiber)

void example_1()
{
	printf("Example 1: nice vs nicer\n");
	nice_fiber_future nice = async nice_fiber();
	nicer_fiber_future nicer = async nicer_fiber();
	nice->wait();
	nicer->wait();
}
zth_fiber(example_1)

// As nicer_fiber() yields a 100 times more often than nice_fiber(),
// nice_fiber() make significantly more progress; nice_fiber() finishes before
// nicer_fiber() gets to its second iteration.  That is not fair.  Zth
// implements the following: every fiber gets at least
// zth::Config::MinTimeslice_s() time, regardless of how many times
// zth::yield() is called. If a fiber calls yield more often, these calls are
// just ignored as long as we are within this time slice.

// So, if every fiber yields at least once in this time period, the CPU is
// fairly shared among all fibers. For your application, determine a realistic
// and usable MinTimeslice_s() value, and make sure (by profiling) that you
// yield often enough.

// Now a new question pops up, what if you really want to yield within this
// time slice?  Call zth::outOfWork() instead. This says 'give up the CPU, I
// don't have any more work to do, so there is no need to keep me running'.
// This is commonly used when polling.

static bool terminate_server = false;
static std::deque<int> work;

void server()
{
	while(true)
	{
		if(!work.empty()) {
			printf("Serving %d\n", work.front());
			work.pop_front();
			// I am nice...
			zth::yield();
		} else if(terminate_server) {
			break;
		} else {
			// If we would call zth::yield() here, we would effectively do a
			// useless busy-wait for zth::Config::MinTimeslice_s() before
			// client() would get a chance to enqueue more work.
			zth::outOfWork();
		}
	}
}
zth_fiber(server)

void client()
{
	for(int i = 0; i < 10; i++) {
		printf("Requesting %d\n", i);
		do_work(10);
		work.push_back(i);
		zth::yield();
	}
}
zth_fiber(client)

void example_2()
{
	printf("\nExample 2: server-client\n");
	server_future s = async server();
	client_future c = async client();
	c->wait();
	terminate_server = true;
	s->wait();
}
zth_fiber(example_2)




int main_fiber(int /*argc*/, char** /*argv*/)
{
	example_1();
	example_2();
	return 0;
}
