/*
 * SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
 *
 * SPDX-License-Identifier: MPL-2.0
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

/*!
 * \brief Check if a given Config field is the same as the given value.
 *
 * This allows checking if the config flags passed to Zth during compilation matches those when
 * using the compiled library.
 */
void checkConfig(int check, size_t value)
{
	bool ok = false;
	char const* name = nullptr;

#define ZTH_CHECK(x)                             \
	case Check::Config_##x:                  \
		ok = (size_t)Config::x == value; \
		if(Config::EnableFullAssert)     \
			name = "" #x;            \
		break;

	switch(check) {
		ZTH_CHECK(Debug)
		ZTH_CHECK(EnableAssert)
		ZTH_CHECK(EnableFullAssert)
		ZTH_CHECK(EnableBacktrace)
		ZTH_CHECK(EnableThreads)
		ZTH_CHECK(SupportDebugPrint)
		ZTH_CHECK(EnableColorLog)
		ZTH_CHECK(DefaultFiberStackSize)
		ZTH_CHECK(EnableStackGuard)
		ZTH_CHECK(EnableStackWaterMark)
		ZTH_CHECK(ContextSignals)
		ZTH_CHECK(CheckTimesliceOverrun)
		ZTH_CHECK(PerfEventBufferSize)
		ZTH_CHECK(EnablePerfEvent)
		ZTH_CHECK(PerfSyscall)
		ZTH_CHECK(UseZMQ)
		ZTH_CHECK(UseLimitedFormatSpecifiers)
		ZTH_CHECK(EnableExceptions)
	default:;
	}

#undef ZTH_CHECK

	if(!ok) {
		if(Config::EnableFullAssert && name)
			abort("Config check %s failed", name);
		else
			abort("Config check %d failed", check);
	}
}

} // namespace zth
