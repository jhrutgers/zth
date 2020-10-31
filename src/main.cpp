#include <libzth/worker.h>
#include <libzth/async.h>

__attribute__((weak)) void main_fiber(int UNUSED_PAR(argc), char** UNUSED_PAR(argv)) {}
#ifndef DOXYGEN
zth_fiber(main_fiber);
#endif

/*!
 * \brief Initialization function to be called by the default-supplied \c main(), before doing anything else.
 *
 * This function can be used to run machine/board-specific initialization in \c main() even before #zth_init() is invoked.
 * The default (weak) implementation does nothing.
 */
__attribute__((weak)) void zth_preinit() {}

/*!
 * \brief Initialization function to be called by the default-supplied \c main(), just before shutting down.
 *
 * This function can be used to run machine/board-specific cleanup in \c main() before returning.
 * The default (weak) implementation does nothing.
 */
__attribute__((weak)) void zth_postdeinit() {}

#ifndef ZTH_OS_WINDOWS
__attribute__((weak))
#endif
int main(int argc, char** argv) {
	zth_preinit();
	{
		zth::Worker w;
		async main_fiber(argc, argv);
		w.run();
	}
	zth_postdeinit();
	return 0;
}

