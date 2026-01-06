/*
 * SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#include <libzth/async.h>
#include <libzth/config.h>
#include <libzth/worker.h>

__attribute__((weak)) int main_fiber(int UNUSED_PAR(argc), char** UNUSED_PAR(argv))
{
	return 0;
}
#ifndef DOXYGEN
zth_fiber_define_1(static, main_fiber);
#endif

/*!
 * \brief Initialization function to be called by the default-supplied \c
 *	main(), before doing anything else.
 *
 * This function can be used to run machine/board-specific initialization in \c
 * main() even before #zth_init() is invoked. The default (weak) implementation
 * does nothing.
 */
__attribute__((weak)) void zth_preinit() {}

/*!
 * \brief Initialization function to be called by the default-supplied \c
 *	main(), just before shutting down.
 *
 * This function can be used to run machine/board-specific cleanup in \c main()
 * before returning.  The default (weak) implementation does nothing.
 *
 * \return the exit code of the application, which overrides the returned value
 *	from \c main_fiber() when non-zero
 */
__attribute__((weak)) int zth_postdeinit()
{
	return 0;
}

#ifndef ZTH_OS_WINDOWS
__attribute__((weak))
#endif
int main(int argc, char** argv)
{
	zth_preinit();
	zth_dbg(thread, "main()");

	int res = 0;
	{
		zth::Worker w;
		main_fiber_future f =
			async main_fiber(argc, argv) << zth::setName(
				zth::Config::EnableDebugPrint || zth::Config::EnablePerfEvent
						|| zth::Config::EnableStackWaterMark
					? "main_fiber"
					: nullptr);

		w.run();
		if(f->valid())
			res = f->value();
	}

	zth_dbg(thread, "main() returns %d", res);

	int res_post = zth_postdeinit();
	// cppcheck-suppress knownConditionTrueFalse
	if(res_post)
		res = res_post;

	return res;
}
