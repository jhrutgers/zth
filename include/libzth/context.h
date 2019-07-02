#ifndef __ZTH_CONTEXT
#define __ZTH_CONTEXT
/*
 * Zth (libzth), a cooperative userspace multitasking library.
 * Copyright (C) 2019  Jochem Rutgers
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

#include <libzth/config.h>

#ifdef __cplusplus
namespace zth {
	class Context;

	struct ContextAttr {
		typedef void* EntryArg;
		typedef void(*Entry)(EntryArg);

		ContextAttr(Entry entry = NULL, EntryArg arg = EntryArg())
			: stackSize(Config::DefaultFiberStackSize)
			, entry(entry)
			, arg(arg)
		{}

		size_t stackSize;
		Entry entry;
		EntryArg arg;
	};

	int context_init();
	void context_deinit();
	int context_create(Context*& context, ContextAttr const& attr);
	void context_switch(Context* from, Context* to);
	void context_destroy(Context* context);
} // namespace
#endif // __cplusplus
#endif // __ZTH_CONTEXT
