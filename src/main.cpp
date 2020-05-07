#include <libzth/worker.h>
#include <libzth/async.h>

__attribute__((weak)) void main_fiber(int argc, char** argv) {}
#ifndef DOXYGEN
zth_fiber(main_fiber);
#endif

/*!
 * \brief Initialization function to be called by the default-supplied \c main(), before doing anything else.
 * 
 * This function can be used to run machine/board-specific in \c main() even before #zth_init() is vinvoked.
 * The default (weak) implementation does nothing.
 */
__attribute__((weak)) void zth_preinit() {}

#ifndef ZTH_OS_WINDOWS
__attribute__((weak))
#endif
int main(int argc, char** argv) {
	zth_preinit();
	zth::Worker w;
	async main_fiber(argc, argv);
	w.run();
	return 0;
}

