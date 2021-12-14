// Default Zth header include.
#include <zth.h>

#include <stdio.h>

// In constrast to C++, fiber entry function must have type void(void*).  There
// is no need to do zth_fiber() and friends, just use zth_fiber_create() to run
// the fiber.
void fiber(void* UNUSED_PAR(arg))
{
	printf("fiber()\n");
}

int main_fiber(int UNUSED_PAR(argc), char** UNUSED_PAR(argv))
{
	printf("main_fiber()\n");
	// Start a new fiber, with arg=NULL, default stack, and no name.
	zth_fiber_create(fiber, NULL, 0, NULL);
	return 0;
}
