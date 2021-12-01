// Default Zth header include.
#include <zth>

// Include anything else your program needs, as usual.
#include <cstdio>

// This is a normal function, which we are going to use as a fiber entry point.
void world()
{
	printf("World!!1\n");
}
// The zth_fiber() marks the given list of function as fiber entries and does
// everything that is required to allow `async world()'.
zth_fiber(world)

void hello()
{
	// The following line does not call the function world() directly, but
	// creates a fiber and schedules it at the current Worker.  world() will
	// now be executed asynchronously to hello().
	async world();

	// Even though world() was started before the following print statement,
	// 'Hello' will always be printed first, as we did not yield the processor
	// to any other fiber in between. So, the output is deterministic; the
	// printf()s will not be reordered.
	printf("Hello\n");
}
zth_fiber(hello)

// main() is weakly defined by Zth. If not implemented by the program, Zth
// supplies one that just starts a worker and schedules main_fiber() as the
// only fiber. So, this is a convenient entry point of your fibered program.
// If you have to do some pre-Zth initialization to be done, such as enabling
// peripherals of an embedded system, override the weakly defined
// zth_preinit(), which is by default doing nothing. It is called before any
// Zth functionality is used.
void main_fiber(int /*argc*/, char** /*argv*/)
{
	async hello();
}
// There is no need to call zth_fiber(main_fiber), as that is implicit for
// main_fiber().

