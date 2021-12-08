/*
 * Zth (libzth), a cooperative userspace multitasking library.
 * Copyright (C) 2019-2021  Jochem Rutgers
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <libzth/async.h>
#include <libzth/worker.h>

__attribute__((weak)) int main_fiber(int UNUSED_PAR(argc), char** UNUSED_PAR(argv))
{
	return 0;
}
#ifndef DOXYGEN
zth_fiber(main_fiber);
#endif

/*!
 * \brief Initialization function to be called by the default-supplied \c main(), before doing
 * anything else.
 *
 * This function can be used to run machine/board-specific initialization in \c main() even before
 * #zth_init() is invoked. The default (weak) implementation does nothing.
 */
__attribute__((weak)) void zth_preinit() {}

/*!
 * \brief Initialization function to be called by the default-supplied \c main(), just before
 * shutting down.
 *
 * This function can be used to run machine/board-specific cleanup in \c main() before returning.
 * The default (weak) implementation does nothing.
 *
 * \return the exit code of the application, which overrides the returned value from \c main_fiber()
 * when non-zero
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
		main_fiber_future f = async main_fiber(argc, argv);
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
