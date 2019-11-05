#include <libzth/worker.h>
#include <libzth/async.h>

__attribute__((weak)) void main_fiber(int argc, char** argv) {}
#ifndef DOXYGEN
zth_fiber(main_fiber);
#endif

#ifndef ZTH_OS_WINDOWS
__attribute__((weak))
#endif
int main(int argc, char** argv) {
	zth::Worker w;
	async main_fiber(argc, argv);
	w.run();
	return 0;
}

