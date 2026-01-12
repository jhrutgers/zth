/*
 * SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <zth>

static bool shutdown_flag = false;

// A generic trigger function that signals a zth::Signal every given interval.
// NOLINTNEXTLINE(performance-unnecessary-value-param)
void trigger(zth::Signal* s, zth::TimeInterval interval)
{
	if(!s || interval.isNegative())
		return;

	zth::Timestamp t = zth::Timestamp::now() + interval;
	while(!shutdown_flag) {
		zth::nap(t);
		s->signal();
		t += interval;
	}
}
zth_fiber(trigger)

// A daemon just runs in the background. When another fiber does something that
// is relevant for the daemon, it signals a global zth::Signal.  This wakes the
// daemon to perform its task.
static zth::Signal triggerSomeDaemon("someDaemon trigger");

void someDaemon()
{
	// The daemon is an infinite loop, which ends waiting for its trigger
	// signal.  The wait for the signal is blocking. If a wakeup is
	// required at some interval, start a timer to do this.
	zth_async trigger(&triggerSomeDaemon, 1);

	while(!shutdown_flag) {
		printf("daemon wakeup\n");
		// ...and do some work.

		triggerSomeDaemon.wait();
	}
}
zth_fiber(someDaemon)

void foo()
{
	printf("foo\n");
	// Trigger the daemon, but only wake it once if triggered multiple
	// times.
	triggerSomeDaemon.signal();
	zth::nap(0.5);
}
zth_fiber(foo)

int main_fiber(int UNUSED_PAR(argc), char** UNUSED_PAR(argv))
{
	zth_async someDaemon();

	zth::Gate gate(4);
	zth_async foo() << zth::passOnExit(gate);
	zth_async foo() << zth::passOnExit(gate);
	zth_async foo() << zth::passOnExit(gate);
	gate.wait();

	// Nap for a while, to show the interval trigger of the daemon.
	zth::nap(10);
	shutdown_flag = true;

	return 0;
}
