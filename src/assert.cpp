/*
 * SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#include <libzth/util.h>

#include <cassert>

namespace zth {

#ifndef ZTH_OS_WINDOWS
__attribute__((weak))
#endif
void assert_handler(char const* file, int line, char const* expr)
{
	if(Config::EnableFullAssert)
		abort("assertion failed at %s:%d: %s", file ? file : "?", line, expr ? expr : "?");
	else
		abort("assertion failed at %s:%d", file ? file : "?", line);
}

} // namespace zth

#ifdef ZTH_OS_MACOS
void __assert_rtn(const char* func, const char* file, unsigned int line, const char* exp)
{
	(void)func;
	zth::assert_handler(file, (int)line, exp);
}
#elif defined(__NEWLIB__)
void __assert_func(const char* file, int line, const char* func, const char* failedexpr)
{
	(void)func;
	zth::assert_handler(file, line, failedexpr);
}
#else  // assume glibc
void __assert_fail(
	const char* __assertion, const char* __file, unsigned int __line,
	const char* __function) noexcept
{
	(void)__function;
	zth::assert_handler(__file, (int)__line, __assertion);
}

void __assert_perror_fail(
	int __errnum, const char* __file, unsigned int __line, const char* __function) noexcept

{
	(void)__function;
	zth::assert_handler(__file, (int)__line, zth::err(__errnum).c_str());
}
#endif // glibc
