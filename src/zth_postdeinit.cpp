/*
 * SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#include <libzth/init.h>

#include <libzth/indirection.h>

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
#ifndef ZTH_OS_WINDOWS
__attribute__((weak))
#endif
int zth_postdeinit()
{
	ZTH_INDIRECT_PROLOGUE(zth_postdeinit)

	return 0;
}
