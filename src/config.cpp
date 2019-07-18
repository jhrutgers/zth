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

#include <cstdlib>
#include <string.h>

namespace zth {

/*!
 * \brief Helper for #config(int, bool) to check the environment for the given name.
 */
static bool config(char const* name, bool whenUnset) {
	char const* e = getenv(name);
	if(!e || !*e)
		return whenUnset;
	else if(strcmp(e, "0") == 0)
		// Explicitly set to disabled
		return false;
	else
		// Any other value is true
		return true;
}

/*!
 * \brief Checks if a given environment option is set.
 * \param env one of #zth::Env
 * \param whenUnset when \p env does not exist in the environment, return this value
 */
bool config(int env, bool whenUnset) {
	switch(env) {
	case Env::EnableDebugPrint:			{ static bool const e = config("ZTH_CONFIG_ENABLE_DEBUG_PRINT", whenUnset); return e; }
	case Env::DoPerfEvent:				{ static bool const e = config("ZTH_CONFIG_DO_PERF_EVENT", whenUnset); return e; }
	case Env::PerfSyscall:				{ static bool const e = config("ZTH_CONFIG_PERF_SYSCALL", whenUnset); return e; }
	case Env::CheckTimesliceOverrun:	{ static bool const e = config("ZTH_CONFIG_CHECK_TIMESLICE_OVERRUN", whenUnset); return e; }
	default:
		return whenUnset;
	}
}

} // namespace

