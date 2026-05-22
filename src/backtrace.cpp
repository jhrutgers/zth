/*
 * SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#define UNW_LOCAL_ONLY

#include <libzth/macros.h>

#include <libzth/allocator.h>

#ifdef ZTH_OS_MAC
#  ifndef _BSD_SOURCE
#    define _BSD_SOURCE
#  endif
#endif

#include <libzth/perf.h>
#include <libzth/worker.h>

#if __cplusplus < 201103L
#  include <inttypes.h>
#else
#  include <cinttypes>
#endif

#include <cstdlib>
#include <fcntl.h>
#include <map>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#if !defined(ZTH_OS_WINDOWS) && !defined(ZTH_OS_BAREMETAL)
#  include <cxxabi.h>
#  include <dlfcn.h>
#  include <execinfo.h>
#endif

#ifdef ZTH_HAVE_LIBUNWIND
#  include <libunwind.h>
#endif

namespace zth {

extern "C" void context_entry(Context* context);

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
Backtrace::Backtrace(size_t UNUSED_PAR(skip), size_t UNUSED_PAR(maxDepth))
	: m_t0(Timestamp::now())
	, m_fiber()
	, m_fiberId()
	, m_truncated(true)
{
	Worker const* worker = Worker::instance();
	m_fiber = worker ? worker->currentFiber() : nullptr;
	// NOLINTNEXTLINE(cppcoreguidelines-prefer-member-initializer)
	m_fiberId = m_fiber ? m_fiber->id() : 0;

#ifdef ZTH_HAVE_LIBUNWIND
	unw_context_t uc;

	if(unw_getcontext(&uc))
		return;

	unw_cursor_t cursor;
	unw_init_local(&cursor, &uc);
	size_t depth = 0;
	for(size_t i = 0; i < skip && unw_step(&cursor) > 0; i++)
		;

	unw_proc_info_t pip;

	while(unw_step(&cursor) > 0 && depth < maxDepth) {
		// cppcheck-suppress knownConditionTrueFalse
		if(depth == 0) {
			unw_word_t sp = 0;
			unw_get_reg(&cursor, UNW_REG_SP, &sp);
		}

		unw_word_t ip = 0;
		unw_get_reg(&cursor, UNW_REG_IP, &ip);
		m_bt.push_back(reinterpret_cast<void*>(ip));

		if(unw_get_proc_info(&cursor, &pip) == 0
		   && pip.start_ip == reinterpret_cast<unw_word_t>(&context_entry))
			// Stop here, as we might get segfaults when passing the context_entry
			// functions.
			break;
	}

	m_truncated = depth == maxDepth;
#elif defined(ZTH_OS_MAC) && defined(ZTH_ARCH_ARM64)
	// macOS on ARM64 does not seem to support backtrace() in combination with ucontext.
	m_truncated = true;
#elif !defined(ZTH_OS_WINDOWS) && !defined(ZTH_OS_BAREMETAL)
	m_bt.resize(maxDepth);
	m_bt.resize((size_t)backtrace(m_bt.data(), (int)maxDepth));
	//  NOLINTNEXTLINE(cppcoreguidelines-prefer-member-initializer)
	m_truncated = m_bt.size() == maxDepth;
#endif

	m_t1 = Timestamp::now();
}

void Backtrace::printPartial(
	size_t UNUSED_PAR(start), ssize_t UNUSED_PAR(end), int UNUSED_PAR(color)) const
{
#if !defined(ZTH_OS_WINDOWS) && !defined(ZTH_OS_BAREMETAL)
	if(bt().empty())
		return;
	if(end < 0) {
		if(-end > (ssize_t)bt().size())
			return;
		end = (ssize_t)bt().size() + end;
	}
	if(start > (size_t)end)
		return;

#  ifdef ZTH_OS_MAC
	FILE* atosf = nullptr;
	if(Config::Debug) {
		char atos[256];
		int len = snprintf(atos, sizeof(atos), "atos -p %d\n", getpid());
		if(len > 0 && (size_t)len < sizeof(atos)) {
			fflush(nullptr);
			atosf = popen(atos, "w");
		}
	}
#  endif

	char** syms =
#  ifdef ZTH_OS_MAC
		!atosf ? nullptr :
#  endif
		       backtrace_symbols(&bt()[start], (int)((size_t)end - start + 1));

	for(size_t i = start; i <= (size_t)end; i++) {
#  ifdef ZTH_OS_MAC
		if(atosf) {
			fprintf(atosf, "%p\n", bt()[i]);
			continue;
		}
#  endif

		Dl_info info;
		if(dladdr(bt()[i], &info)) {
			int status = -1;
			char* demangled =
				abi::__cxa_demangle(info.dli_sname, nullptr, nullptr, &status);
			if(status == 0 && demangled) {
#  ifdef ZTH_OS_MAC
				log_color(
					color, "%s%-3zd 0x%0*" PRIxPTR " %s + %" PRIuPTR "\n",
					color >= 0 ? ZTH_DBG_PREFIX : "", i, (int)sizeof(void*) * 2,
					reinterpret_cast<uintptr_t>(bt()[i]), demangled,
					reinterpret_cast<uintptr_t>(bt()[i])
						- reinterpret_cast<uintptr_t>(info.dli_saddr));
#  else
				log_color(
					color, "%s%-3zd %s(%s+0x%" PRIxPTR ") [0x%" PRIxPTR "]\n",
					color >= 0 ? ZTH_DBG_PREFIX : "", i, info.dli_fname,
					demangled,
					reinterpret_cast<uintptr_t>(bt()[i])
						- reinterpret_cast<uintptr_t>(info.dli_saddr),
					reinterpret_cast<uintptr_t>(bt()[i]));
#  endif

				free(demangled); // NOLINT
				continue;
			}
		}

		if(syms)
			log_color(
				color, "%s%-3zd %s\n", color >= 0 ? ZTH_DBG_PREFIX : "", i,
				syms[i - start]);
		else
			log_color(
				color, "%s%-3zd 0x%0*" PRIxPTR "\n",
				color >= 0 ? ZTH_DBG_PREFIX : "", i, (int)sizeof(void*) * 2,
				reinterpret_cast<uintptr_t>(bt()[i]));
	}

	if(syms)
		free(syms); // NOLINT

#  ifdef ZTH_OS_MAC
	if(atosf)
		pclose(atosf);
#  endif
#endif
}

void Backtrace::print(int UNUSED_PAR(color)) const
{
#if !defined(ZTH_OS_WINDOWS) && !defined(ZTH_OS_BAREMETAL)
	log_color(
		color, "%sBacktrace of fiber %p #%" PRIu64 ":\n", color >= 0 ? ZTH_DBG_PREFIX : "",
		m_fiber, m_fiberId);
	if(!bt().empty())
		printPartial(0, (ssize_t)bt().size() - 1, color);

	if(truncated())
		log_color(color, "%s<truncated>\n", color >= 0 ? ZTH_DBG_PREFIX : "");
	else
		log_color(color, "%s<end>\n", color >= 0 ? ZTH_DBG_PREFIX : "");
#endif
}

void Backtrace::printDelta(Backtrace const& other, int color) const
{
	// Make sure other was first.
	if(other.t0() > t0()) {
		other.printDelta(*this, color);
		return;
	}

	TimeInterval dt = t0() - other.t1();

	if(other.fiberId() != fiberId() || other.truncated() || truncated()) {
		log_color(color, "%sExecuted from:\n", color >= 0 ? ZTH_DBG_PREFIX : "");
		other.print(color);
		log_color(color, "%sto:\n", color >= 0 ? ZTH_DBG_PREFIX : "");
		print(color);
		log_color(color, "%stook %s\n", color >= 0 ? ZTH_DBG_PREFIX : "", dt.str().c_str());
		return;
	}

	// Find common base
	ssize_t common = 0;
	{
		Backtrace::bt_type const& this_bt = bt();
		Backtrace::bt_type const& other_bt = other.bt();
		size_t max_common = std::min(this_bt.size(), other_bt.size());
		while((size_t)common < max_common
		      && this_bt[this_bt.size() - 1 - (size_t)common]
				 == other_bt[other_bt.size() - 1 - (size_t)common])
			common++;
	}

	log_color(
		color, "%sExecution from fiber %p #%s:\n", color >= 0 ? ZTH_DBG_PREFIX : "",
		m_fiber, str(m_fiberId).c_str());
	other.printPartial(0, -common - 1, color);
	log_color(color, "%sto:\n", color >= 0 ? ZTH_DBG_PREFIX : "");
	printPartial(0, -common - 1, color);
	if(common > 0) {
		log_color(color, "%shaving in common:\n", color >= 0 ? ZTH_DBG_PREFIX : "");
		other.printPartial(other.bt().size() - (size_t)common, color);
	}
	log_color(color, "%stook %s\n", color >= 0 ? ZTH_DBG_PREFIX : "", dt.str().c_str());
}

} // namespace zth
