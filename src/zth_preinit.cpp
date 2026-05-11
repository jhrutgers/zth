/*
 * SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#include <libzth/init.h>

/*!
 * \brief Initialization function to be called by the default-supplied \c
 *	main(), before doing anything else.
 *
 * This function can be used to run machine/board-specific initialization in \c
 * main() even before #zth_init() is invoked. The default (weak) implementation
 * does nothing.
 */
#ifndef ZTH_OS_WINDOWS
__attribute__((weak))
#endif
void zth_preinit()
{}
