/*
 * SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#include <libzth/macros.h>
#include <libzth/init.h>
#include <libzth/util.h>

struct zth_init_entry const* zth_init_head = nullptr;
struct zth_init_entry* zth_init_tail = nullptr;

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
	zth_init_head = nullptr;

	while(p) {
		// p might be overwritten during p->f(), copy next first.
		struct zth_init_entry const* p_next = p->next;
		if(p->f)
			p->f();
		p = p_next;
	}
}
