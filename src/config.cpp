/*
 * Zth (libzth), a cooperative userspace multitasking library.
 * Copyright (C) 2019-2022  Jochem Rutgers
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <libzth/config.h>
#include <libzth/util.h>

#include <cstdlib>
#include <cstring>

namespace zth {

/*!
 * \brief Helper for #config(int, bool) to check the environment for the given name.
 */
static bool config(char const* name, bool whenUnset)
{
	// NOLINTNEXTLINE(concurrency-mt-unsafe)
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
bool config(int env, bool whenUnset)
{
	switch(env) {
	case Env::EnableDebugPrint: {
		static bool const e = Config::SupportDebugPrint
				      && config("ZTH_CONFIG_ENABLE_DEBUG_PRINT", whenUnset);
		return e;
	}
	case Env::DoPerfEvent: {
		static bool const e = config("ZTH_CONFIG_DO_PERF_EVENT", whenUnset);
		return e;
	}
	case Env::PerfSyscall: {
		static bool const e = config("ZTH_CONFIG_PERF_SYSCALL", whenUnset);
		return e;
	}
	case Env::CheckTimesliceOverrun: {
		static bool const e = config("ZTH_CONFIG_CHECK_TIMESLICE_OVERRUN", whenUnset);
		return e;
	}
	default:
		return whenUnset;
	}
}

} // namespace zth
