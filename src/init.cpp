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

#include <libzth/macros.h>
#include <libzth/init.h>
#include <libzth/util.h>

struct zth_init_entry const* zth_init_head = NULL;
struct zth_init_entry* zth_init_tail = NULL;

/*!
 * \brief Perform one-time global initialization of the Zth library.
 * 
 * Initialization is only done once. It is safe to call it multiple times.
 * 
 * The initialization sequence is initialized by #ZTH_INIT_CALL() and processed
 * in the same order as normal static initializers are executed.
 */
void zth_init()
{
	if(likely(!zth_init_head))
		// Already initialized.
		return;

	struct zth_init_entry const* p = zth_init_head;
	zth_init_head = NULL;

	while(p) {
		// p might be overwritten during p->f(), copy next first.
		struct zth_init_entry const* p_next = p->next;
		if(p->f)
			p->f();
		p = p_next;
	}
}

