#include <zth>

// To generate VCD output, run this example like:
//   ZTH_CONFIG_DO_PERF_EVENT=1 examples/5_perf
// Play around by setting ZTH_CONFIG_ENABLE_DEBUG_PRINT=[01] or running the Debug or Release build.

// Tweak the load default value at will to observe differences depending on the
// speed of your CPU.
static void do_work(int amount, int load = 1000000)
{
	for(int volatile i = 0; i < amount * load; i++);
}

void scheduling()
{
	// When ZTH_CONFIG_DO_PERF_EVENT is set to 1, the perf API is enabled,
	// which includes generating a VCD file.  This VCD file shows every fiber
	// in the system and indicates the state of every fiber at every instance
	// in time.  This allows you to investigate the scheduling behavior of
	// fibers.  The state of every fiber is indicated in IEEE 1164 std_logic
	// format, using the following values:
	// - U (undefined): the fiber has not been created yet
	// - L (low): the fiber has been created, but not initialized
	//   (zth::Fiber::New state)
	// - 0: the fiber is ready (zth::Fiber::Ready state), but not actually
	//   using the CPU
	// - 1: the fiber is currently running, which can be only one fiber per
	//   zth::Worker.
	// - W (weak): the fiber is blocked (zth::Fiber::Waiting)
	//   (sleeping/poll/blocking read/etc.)
	// - Z (high impedance): the fiber is suspended (zth::Fiber::Suspended)
	// - X (unknown): the fiber is dead (zth::Fiber::Dead), but not cleaned up
	// - - (don't care): the fiber has been cleaned up, which also indicates
	//   that the zth::Fiber object has been deleted.

	// The VCD file will show for this fiber the following sequence:
	//   U L 1 W 0 1 X -
	// The Waiter (usually fiber #3) manages the mnap of below.  Open the
	// generated VCD file, using gtkwave, for example.  Try to understand the
	// relation between fibers in the VCD.
	zth::mnap(10);
}
zth_fiber(scheduling)

void measure()
{
	// This example measures the duration of a task. There are several helpful
	// functions for this.  First, take a time stamp. zth::Timestamp::now() is
	// (supposed to be) fast and is used everywhere, so go ahead and safe t0.
	zth::Timestamp t0 = zth::Timestamp::now();

	// perf_mark() allow you to put very efficiently a marker (which must be a
	// C string literal) in the VCD log at the current time. We are going to
	// use this to double check our measurements in the VCD file. This is like
	// printf()-debugging, but with accurate timing information.
	zth::perf_mark("do_work(1)");

	// Go do some lengthy work.
	do_work(1);

	// Set another marker.
	zth::perf_mark("do_work(1) done");

	// Compute time difference. This is not completely fair, as this includes
	// running perf_mark() twice.
	zth::TimeInterval dt = zth::Timestamp::now() - t0;

	// Let us format some log string.
	zth::string log = zth::format("do_work(1) took %s", dt.str().c_str());
	// Put the log string also in the VCD file...
	zth::perf_log("%s", log.c_str());
	// ...and to stdout.
	printf("%s\n", log.c_str());

	// Open the generated VCD file.  Every fiber has two 'signals': the
	// schedule state of the fiber, and a string with logs.  The perf_mark()
	// and perf_log() should show up there.  Try to measure the length between
	// the perf_mark()s in the VCD log string and compare it with the measured
	// time interval.
}
zth_fiber(measure)

void stack()
{
	// Make a snapshot of the current stack backtrace.
	zth::Backtrace bt;
	// Print it to stdout.
	bt.print();

	// Make another snapshot.
	zth::Backtrace bt2;
	bt2.printDelta(bt);
}
zth_fiber(stack)



void main_fiber(int /*argc*/, char** /*argv*/)
{
	async scheduling();
	async measure();
	async stack();
}

