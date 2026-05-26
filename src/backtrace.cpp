/*
 * SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#define UNW_LOCAL_ONLY

#include <libzth/backtrace.h>

#include <libzth/context.h>
#include <libzth/worker.h>

#ifdef ZTH_OS_WINDOWS
#  define ZTH_BT_WIN32
#elif defined(ZTH_HAVE_LIBUNWIND)
#  define ZTH_BT_LIBUNWIND
#elif defined(ZTH_OS_MAC) && defined(ZTH_ARCH_ARM64)
// macOS on ARM64 does not seem to support backtrace() in combination with ucontext.
#  define ZTH_BT_NONE
#elif defined(ZTH_HAVE_EXECINFO)
#  define ZTH_BT_BACKTRACE
#else
#  define ZTH_BT_NONE
#endif

#ifdef ZTH_BT_NONE
#  define ZTH_BT_PRINT_NONE
#elif defined(ZTH_BT_WIN32)
#  define ZTH_BT_PRINT_WIN32
#elif defined(ZTH_HAVE_LIBBACKTRACE) && !defined(CLANG_TIDY)
#  define ZTH_BT_PRINT_LIBBACKTRACE
#elif defined(ZTH_HAVE_DL)
#  define ZTH_BT_PRINT_DL
#elif defined(ZTH_BT_BACKTRACE) || defined(ZTH_HAVE_EXECINFO)
#  define ZTH_BT_PRINT_SYMBOLS
#else
#  define ZTH_BT_PRINT_ADDR
#endif

#ifndef ZTH_BT_PRINT_NONE
#  if(defined(ZTH_OS_WINDOWS) || defined(ZTH_OS_POSIX)) \
	  && (!defined(ZTH_THREADS) || __cplusplus >= 201103L)
#    define ZTH_BT_ADDR2LINE
#  endif
#endif

extern "C" void context_entry(zth::Context* context);



///////////////////////////////////////////////////////////////////
// Implement using Windows API
//

#ifdef ZTH_BT_WIN32
#  include <windows.h>

#  include <dbghelp.h>

static bool bt_win32_sym_init()
{
	static bool sym_init = false;
	if(sym_init)
		return true;

	HANDLE process = GetCurrentProcess();
	SymSetOptions(
		SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES | SYMOPT_UNDNAME
		| SYMOPT_FAIL_CRITICAL_ERRORS);

	if(SymInitialize(process, nullptr, TRUE) == TRUE) {
		sym_init = true;
		return true;
	}

	// DbgHelp was likely initialized elsewhere in-process already.
	if(GetLastError() == ERROR_INVALID_PARAMETER) {
		sym_init = true;
		return true;
	}

	return false;
}

static void bt_capture(zth::impl::Backtrace& bt, size_t skip, size_t maxDepth)
{
	bt.bt().clear();

	HANDLE process = GetCurrentProcess();
	if(!bt_win32_sym_init()) {
		bt.truncated(true);
		return;
	}

	CONTEXT context = {};
	RtlCaptureContext(&context);

	STACKFRAME64 stackFrame = {};
#  if defined(ZTH_ARCH_X86_64)
	DWORD machineType = IMAGE_FILE_MACHINE_AMD64;
	stackFrame.AddrPC.Offset = context.Rip;
	stackFrame.AddrFrame.Offset = context.Rbp;
	stackFrame.AddrStack.Offset = context.Rsp;
#  elif defined(ZTH_ARCH_X86)
	DWORD machineType = IMAGE_FILE_MACHINE_I386;
	stackFrame.AddrPC.Offset = context.Eip;
	stackFrame.AddrFrame.Offset = context.Ebp;
	stackFrame.AddrStack.Offset = context.Esp;
#  else
#    error Unsupported architecture.
#  endif
	stackFrame.AddrPC.Mode = AddrModeFlat;
	stackFrame.AddrFrame.Mode = AddrModeFlat;
	stackFrame.AddrStack.Mode = AddrModeFlat;

	size_t depth = 0;
	while(depth < maxDepth
	      && StackWalk64(
		      machineType, process, GetCurrentThread(), &stackFrame, &context, nullptr,
		      SymFunctionTableAccess64, SymGetModuleBase64, nullptr)) {
		if(depth >= skip)
			bt.bt().push_back((void*)stackFrame.AddrPC.Offset);
		if(!stackFrame.AddrFrame.Offset
		   || stackFrame.AddrPC.Offset == reinterpret_cast<DWORD64>(&context_entry))
			break;
		depth++;
	}
}
#endif // ZTH_BT_WIN32



///////////////////////////////////////////////////////////////////
// Implement with libunwind
//

#ifdef ZTH_BT_LIBUNWIND
#  include <libunwind.h>

static void bt_capture(zth::impl::Backtrace& bt, size_t skip, size_t maxDepth)
{
	bt.bt().clear();

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
		bt.bt().push_back(reinterpret_cast<void*>(ip)); // NOLINT

		if(unw_get_proc_info(&cursor, &pip) == 0
		   && pip.start_ip == reinterpret_cast<unw_word_t>(&context_entry))
			// Stop here, as we might get segfaults when passing the context_entry
			// functions.
			break;
	}

	bt.truncated(depth == maxDepth);
}
#endif // ZTH_BT_LIBUNWIND



///////////////////////////////////////////////////////////////////
// Implement with backtrace()
//

#ifdef ZTH_BT_BACKTRACE
#  include <execinfo.h>

static void bt_capture(zth::impl::Backtrace& bt, size_t skip, size_t maxDepth)
{
	zth::impl::Backtrace::bt_type& b = bt.bt();

	b.resize(maxDepth);
	b.resize((size_t)backtrace(b.data(), (int)maxDepth));
	//  NOLINTNEXTLINE(cppcoreguidelines-prefer-member-initializer)
	bt.truncated(b.size() == maxDepth);

	size_t dst = 0;
	size_t src = skip;

	for(; src < b.size(); src++, dst++) {
		b[dst] = b[src];
		if(!b[dst] || b[dst] == reinterpret_cast<void*>(&context_entry))
			break;
	}

	b.resize(dst);
}
#endif // ZTH_BT_BACKTRACE


///////////////////////////////////////////////////////////////////
// No support for capturing backtraces.
//

#ifdef ZTH_BT_NONE
static void bt_capture(zth::impl::Backtrace& bt, size_t skip, size_t maxDepth)
{
	(void)skip;
	(void)maxDepth;
	bt.truncated(true);
}
#endif // ZTH_BT_NONE



///////////////////////////////////////////////////////////////////
// No printing.
//

#ifdef ZTH_BT_PRINT_NONE
static void bt_print(size_t index, void const* addr, int color)
{
	(void)index;
	(void)addr;
	(void)color;
}
#endif // ZTH_BT_PRINT_NONE



///////////////////////////////////////////////////////////////////
// Print plain addresses
//

static __attribute__((unused)) void bt_print_addr(size_t index, void const* addr, int color)
{
	zth::log_color(
		color, "%s%-3lu [%p]\n", color >= 0 ? ZTH_DBG_PREFIX : "", (unsigned long)index,
		addr);
}

#ifdef ZTH_BT_PRINT_ADDR
#  define bt_print bt_print_addr
#endif // ZTH_BT_PRINT_ADDR



///////////////////////////////////////////////////////////////////
// Print basic symbols
//

#ifdef ZTH_HAVE_EXECINFO
// NOLINTNEXTLINE(readability-duplicate-include)
#  include <execinfo.h>

static __attribute__((unused)) void bt_print_symbols(size_t index, void const* addr, int color)
{
	char** symbols = backtrace_symbols((void* const*)&addr, 1); // NOLINT
	char const* sym = symbols ? symbols[0] : nullptr;

	if(!sym || !*sym) {
		bt_print_addr(index, addr, color);
	} else {
		zth::log_color(
			color, "%s%-3lu %s\n", color >= 0 ? ZTH_DBG_PREFIX : "",
			(unsigned long)index, sym);
	}

	free(symbols); // NOLINT
}
#else
#  define bt_print_symbols bt_print_addr
#endif // ZTH_HAVE_EXECINFO

#ifdef ZTH_BT_PRINT_SYMBOLS
#  define bt_print bt_print_symbols
#endif // ZTH_BT_PRINT_SYMBOLS



///////////////////////////////////////////////////////////////////
// Print symbols with dladdr()
//

#ifdef ZTH_HAVE_DL
#  include <cxxabi.h>
#  include <dlfcn.h>

static void bt_print_dl(size_t index, void const* addr, int color)
{
	Dl_info info = {};
	if(!dladdr(addr, &info))
		return;

	int status = -1;
	char* demangled = abi::__cxa_demangle(info.dli_sname, nullptr, nullptr, &status);
	if(status == 0 && demangled) {
		zth::log_color(
			color, "%s%-3lu [%p] %s(%s+0x%lx) %s\n", color >= 0 ? ZTH_DBG_PREFIX : "",
			(unsigned long)index, addr, info.dli_fname, info.dli_sname,
			(unsigned long)((uintptr_t)addr - (uintptr_t)info.dli_saddr), // NOLINT
			demangled ? demangled : "??");
	} else {
		bt_print_symbols(index, addr, color);
	}

	free(demangled); // NOLINT
}
#else
#  define bt_print_dl bt_print_symbols
#endif // ZTH_HAVE_DL

#ifdef ZTH_BT_PRINT_DL
#  define bt_print bt_print_dl
#endif // ZTH_BT_PRINT_DL



///////////////////////////////////////////////////////////////////
// Use addr2line to print symbols
//

#ifdef ZTH_BT_ADDR2LINE
#  if __cplusplus >= 201103L
#    include <mutex>
#  endif

#  define ADDR2LINE_BUF_SIZE 1024

static size_t bt_addr2line_get(FILE* f, char (&buf)[ADDR2LINE_BUF_SIZE])
{
	if(fgets(buf, sizeof(buf), f)) {
		size_t len = strlen(buf);
		while(len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r'))
			buf[--len] = '\0';
		return len;
	} else {
		buf[0] = '\0';
		return 0;
	}
}

static void bt_print_addr2line_print(
	size_t index, void const* addr, char const* file, char const* func, int color)
{
	zth::log_color(
		color, "%s%-3lu [%p] (%s) %s\n", color >= 0 ? ZTH_DBG_PREFIX : "",
		(unsigned long)index, addr, file && *file ? file : "??",
		func && *func && strcmp(func, "?") != 0 ? func : "??");
}

static void bt_print_addr2line_unsafe(size_t index, void const* addr, int color)
{
	static std::map<void const*, std::pair<std::string, std::string>> cache;
	auto it = cache.find(addr);
	if(it != cache.end()) {
		bt_print_addr2line_print(
			index, addr, it->second.first.c_str(), it->second.second.c_str(), color);
		return;
	}

	uintptr_t pc = reinterpret_cast<uintptr_t>(addr);
	uintptr_t base = 0;
	char module[512] = {};

#  ifdef ZTH_OS_WINDOWS
	HMODULE hmod = nullptr;
	if(GetModuleHandleExA(
		   GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
			   | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
		   reinterpret_cast<LPCSTR>(addr), &hmod)
	   && hmod) {
		base = reinterpret_cast<uintptr_t>(hmod);
		(void)GetModuleFileNameA(hmod, module, (DWORD)sizeof(module));
	}
#  elif defined(ZTH_HAVE_DL)
	Dl_info info = {};
	if(dladdr(addr, &info) && info.dli_fbase && info.dli_fname) {
		base = reinterpret_cast<uintptr_t>(info.dli_fbase);
		(void)snprintf(module, sizeof(module), "%s", info.dli_fname);
	}
#  endif

	if(module[0] == '\0') {
		(void)snprintf(module, sizeof(module), "/proc/%u/exe", (unsigned)getpid());
	}

	uintptr_t rel = pc;
	if(base && pc >= base)
		rel = pc - base;

	char cmd[1024];
	(void)snprintf(
		cmd, sizeof(cmd), "addr2line -e \"%s\" -C -f 0x%llx", module,
		(unsigned long long)rel);

	FILE* f = popen(cmd, "r"); // NOLINT
	if(!f) {
		bt_print_dl(index, addr, color);
		return;
	}

	static char func[ADDR2LINE_BUF_SIZE];
	static char file[ADDR2LINE_BUF_SIZE];
	bt_addr2line_get(f, func);
	bt_addr2line_get(f, file);

	if((func[0] != '\0' && func[0] != '?') || (file[0] != '\0' && file[0] != '?')) {
		cache[addr] = std::make_pair(std::string(file), std::string(func));
		bt_print_addr2line_print(index, addr, file, func, color);
	} else {
		bt_print_dl(index, addr, color);
	}

	pclose(f);
}

static __attribute__((unused)) void bt_print_addr2line(size_t index, void const* addr, int color)
{
	if(zth::Config::EnableThreads) {
#  if __cplusplus < 201103L
		// No mutex support.
		return;
#  else	 // C++11
	 // addr2line is not thread-safe, so we need to serialize calls to it.
		static std::mutex mtx;
		std::lock_guard<std::mutex> lock(mtx);
		bt_print_addr2line_unsafe(index, addr, color);
#  endif // < C++11
	} else {
		bt_print_addr2line_unsafe(index, addr, color);
	}
}
#else
#  define bt_print_addr2line bt_print_dl
#endif



///////////////////////////////////////////////////////////////////
// Print symbols with libbacktrace
//

#if defined(ZTH_HAVE_LIBBACKTRACE) && !defined(CLANG_TIDY)
#  include <backtrace.h>
// NOLINTNEXTLINE(readability-duplicate-include)
#  include <cxxabi.h>

ZTH_TLS_DEFINE(backtrace_state*, bt_state, nullptr);

struct bt_print_cb_data {
	size_t index;
	int color;
	bool ok;
};

static int
bt_print_cb(void* data, uintptr_t pc, const char* filename, int lineno, const char* function)
{
	bt_print_cb_data* cbdata = static_cast<bt_print_cb_data*>(data);

	if(!filename || !*filename)
		return 1;

	int status = -1;
	char* demangled = abi::__cxa_demangle(function, nullptr, nullptr, &status);
	char const* sym = demangled ? demangled : function;

	zth::log_color(
		cbdata->color, "%s%-3lu [%p] (%s:%d) %s\n",
		cbdata->color >= 0 ? ZTH_DBG_PREFIX : "", (unsigned long)cbdata->index, (void*)pc,
		filename, lineno, sym ? sym : "??");

	free(demangled); // NOLINT
	cbdata->ok = true;
	return 0;
}

static void bt_print_libbacktrace(size_t index, void const* addr, int color)
{
	static backtrace_state* const BT_STATE_ERROR = (backtrace_state*)-1;

	if(!bt_state) {
		bt_state = backtrace_create_state(
			nullptr, (int)zth::Config::EnableThreads, nullptr, nullptr);
		if(!bt_state)
			bt_state = BT_STATE_ERROR;
	}

	if(bt_state == BT_STATE_ERROR) {
		bt_print_addr2line(index, addr, color);
		return;
	}

	bt_print_cb_data data = {index, color, false};
	if(backtrace_pcinfo(
		   bt_state, reinterpret_cast<uintptr_t>(addr), bt_print_cb, nullptr, &data)
	   || !data.ok)
		bt_print_addr2line(index, addr, color);
}
#else
#  define bt_print_libbacktrace bt_print_addr2line
#endif // ZTH_HAVE_LIBBACKTRACE

#ifdef ZTH_BT_PRINT_LIBBACKTRACE
#  define bt_print bt_print_libbacktrace
#endif // ZTH_BT_PRINT_LIBBACKTRACE



///////////////////////////////////////////////////////////////////
// Print symbols with Windows API
//

#ifdef ZTH_BT_PRINT_WIN32
static void bt_print(size_t index, void const* addr, int color)
{
	HANDLE process = GetCurrentProcess();
	if(!bt_win32_sym_init()) {
		bt_print_libbacktrace(index, addr, color);
		return;
	}

	unsigned char sym_buf[sizeof(SYMBOL_INFO) + MAX_SYM_NAME] = {};
	SYMBOL_INFO* sym = reinterpret_cast<SYMBOL_INFO*>(sym_buf);
	sym->SizeOfStruct = sizeof(SYMBOL_INFO);
	sym->MaxNameLen = MAX_SYM_NAME;

	DWORD64 addr64 = reinterpret_cast<DWORD64>(addr);
	DWORD64 sym_displacement = 0;
	if(!SymFromAddr(process, addr64, &sym_displacement, sym)) {
		bt_print_libbacktrace(index, addr, color);
		return;
	}

	IMAGEHLP_LINE64 line = {};
	line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
	DWORD line_displacement = 0;
	if(SymGetLineFromAddr64(process, addr64, &line_displacement, &line)) {
		char const* name = (sym->NameLen > 0 && sym->Name[0]) ? sym->Name : "??";
		zth::log_color(
			color, "%s%-3lu [%p] (%s:%lu) %s+0x%llx\n",
			color >= 0 ? ZTH_DBG_PREFIX : "", (unsigned long)index, addr,
			line.FileName ? line.FileName : "??", (unsigned long)line.LineNumber, name,
			(unsigned long long)sym_displacement);
	} else {
		char const* name = (sym->NameLen > 0 && sym->Name[0]) ? sym->Name : "??";
		zth::log_color(
			color, "%s%-3lu [%p] %s+0x%llx\n", color >= 0 ? ZTH_DBG_PREFIX : "",
			(unsigned long)index, addr, name, (unsigned long long)sym_displacement);
	}
}
#endif // ZTH_BT_PRINT_WIN32


////////////////////////////////////////////////////////////////////
// Backtrace wrapper
//

namespace zth {
namespace impl {

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
Backtrace::Backtrace(size_t skip, size_t maxDepth) noexcept
	: m_t0(Timestamp::now())
	, m_fiber()
	, m_fiberId()
	, m_truncated()
{
	Worker const* worker = Worker::instance();
	m_fiber = worker ? worker->currentFiber() : nullptr;
	// NOLINTNEXTLINE(cppcoreguidelines-prefer-member-initializer)
	m_fiberId = m_fiber ? m_fiber->id() : 0;

	try {
		bt_capture(*this, skip, maxDepth);
	} catch(...) {
		bt().clear();
		truncated(true);
	}

	m_t1 = Timestamp::now();
}

void Backtrace::printPartial(size_t start, ssize_t end, int color) const
{
	if(bt().empty())
		return;
	if(end < 0) {
		if(-end > (ssize_t)bt().size())
			return;
		end = (ssize_t)bt().size() + end;
	}
	if(start > (size_t)end)
		return;

	for(size_t i = start; i <= (size_t)end; i++) {
		void* addr = bt()[i];
		if(!addr)
			break;

		bt_print(i, addr, color);

		if(addr == reinterpret_cast<void*>(&context_entry))
			break;
	}
}

void Backtrace::print(int color) const
{
	if(bt().empty())
		return;

	log_color(
		color, "%sBacktrace of fiber %p #%" PRIu64 ":\n", color >= 0 ? ZTH_DBG_PREFIX : "",
		m_fiber, m_fiberId);

	printPartial(0, (ssize_t)bt().size() - 1, color);

	if(truncated())
		log_color(color, "%s<truncated>\n", color >= 0 ? ZTH_DBG_PREFIX : "");
	else
		log_color(color, "%s<end>\n", color >= 0 ? ZTH_DBG_PREFIX : "");
}

void Backtrace::printDelta(Backtrace const& other, int color) const
{
	// Make sure other was first.
	if(other.t0() > t0()) {
		other.printDelta(*this, color);
		return;
	}

	TimeInterval dt = t0() - other.t1();

	if(bt().empty() && other.bt().empty()) {
		log_color(
			color,
			"%sExecution from fiber %p #%s snapshot to %p #%s snapshot took %s\n",
			color >= 0 ? ZTH_DBG_PREFIX : "", other.m_fiber,
			str(other.m_fiberId).c_str(), m_fiber, str(m_fiberId).c_str(),
			dt.str().c_str());
		return;
	}

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

} // namespace impl
} // namespace zth
