/*
 * SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#define UNW_LOCAL_ONLY

#include <libzth/backtrace.h>

#include <libzth/context.h>
#include <libzth/worker.h>

#if defined(ZTH_CONFIG_ENABLE_BACKTRACE) && !ZTH_CONFIG_ENABLE_BACKTRACE
#  define ZTH_BT_NONE
#elif defined(ZTH_OS_WINDOWS)
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
#  if (defined(ZTH_OS_WINDOWS) || defined(ZTH_OS_POSIX)) \
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
#  if defined(__GNUG__)
#    include <cxxabi.h>
#  endif

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
static __attribute__((unused)) void bt_print(size_t index, void const* addr, int color)
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

#if defined(ZTH_HAVE_EXECINFO) && !defined(ZTH_BT_PRINT_NONE)
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

#if defined(ZTH_HAVE_DL) && !defined(ZTH_BT_PRINT_NONE)
#  include <cxxabi.h>
#  include <dlfcn.h>

static __attribute__((unused)) void bt_print_dl(size_t index, void const* addr, int color)
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

#if defined(ZTH_BT_ADDR2LINE) && !defined(ZTH_BT_PRINT_NONE)
#  if __cplusplus >= 201103L
#    include <mutex>
#  endif
#  ifdef ZTH_OS_POSIX
#    include <csignal>
#    include <sys/types.h>
#    include <sys/wait.h>
#    include <unistd.h>
#  endif

#  define ADDR2LINE_BUF_SIZE 1024

static void bt_print_addr2line_print(
	size_t index, void const* addr, char const* file, char const* func, int color)
{
	bool has_func = func && *func && func[0] != '?';
	zth::log_color(
		color, "%s%-3lu [%p] (%s) %s\n", color >= 0 ? ZTH_DBG_PREFIX : "",
		(unsigned long)index, addr, file && *file ? file : "??", has_func ? func : "??");
}

static bool bt_addr2line_has_symbol(char const* func, char const* file)
{
	bool has_func = func && *func && func[0] != '?';
	bool has_file = file && *file && file[0] != '?';
	return has_func || has_file;
}

struct bt_addr2line_module_info {
	uintptr_t base;
	uintptr_t runtime_base;
	uintptr_t preferred_base;
	zth::string module;

	bt_addr2line_module_info()
		: base(0)
		, runtime_base(0)
		, preferred_base(0)
	{}
};

struct bt_addr2line_process {
	bool started;
#  ifdef ZTH_OS_WINDOWS
	HANDLE process;
	HANDLE stdin_write;
	HANDLE stdout_read;
#  else
	pid_t pid;
	int stdin_write;
	int stdout_read;
#  endif

	bt_addr2line_process()
		: started(false)
#  ifdef ZTH_OS_WINDOWS
		, process(nullptr)
		, stdin_write(nullptr)
		, stdout_read(nullptr)
#  else
		, pid(-1)
		, stdin_write(-1)
		, stdout_read(-1)
#  endif
	{}
};

typedef zth::map_type<zth::string, bt_addr2line_process>::type bt_addr2line_process_map;
typedef zth::map_type<uintptr_t, bt_addr2line_module_info>::type bt_addr2line_module_map;
typedef zth::map_type<void const*, std::pair<zth::string, zth::string>>::type
	bt_addr2line_symbol_map;

#  ifdef ZTH_OS_WINDOWS
static void
bt_addr2line_close_child_handles(HANDLE out_read, HANDLE out_write, HANDLE in_read, HANDLE in_write)
{
	if(out_read)
		CloseHandle(out_read);
	if(out_write)
		CloseHandle(out_write);
	if(in_read)
		CloseHandle(in_read);
	if(in_write)
		CloseHandle(in_write);
}

static void bt_addr2line_stop(bt_addr2line_process& proc)
{
	if(proc.stdin_write) {
		CloseHandle(proc.stdin_write);
		proc.stdin_write = nullptr;
	}
	if(proc.stdout_read) {
		CloseHandle(proc.stdout_read);
		proc.stdout_read = nullptr;
	}
	if(proc.process) {
		CloseHandle(proc.process);
		proc.process = nullptr;
	}
	proc.started = false;
}

static bool bt_addr2line_start(char const* module, bt_addr2line_process& proc)
{
	// Under Windows debuggers we can hit noisy first-chance faults inside CreateProcessA even
	// when execution would continue. Skip external addr2line in that case and let callers fall
	// back to the native symbol resolution paths.
	if(IsDebuggerPresent()) {
		char env[16] = {};
		DWORD n = GetEnvironmentVariableA(
			"ZTH_ADDR2LINE_UNDER_DEBUG", env, (DWORD)sizeof(env));
		bool allow = n > 0
			     && (env[0] == '1' || env[0] == 'y' || env[0] == 'Y' || env[0] == 't'
				 || env[0] == 'T' || env[0] == 'o' || env[0] == 'O');
		if(!allow)
			return false;
	}

	char temp_path[MAX_PATH] = {};
	char const* windows_cwd = "C:\\";
	DWORD temp_len = GetTempPathA((DWORD)sizeof(temp_path), temp_path);
	if(temp_len > 0 && temp_len < sizeof(temp_path))
		windows_cwd = temp_path;

	SECURITY_ATTRIBUTES sa = {};
	sa.nLength = sizeof(sa);
	sa.bInheritHandle = TRUE;

	HANDLE child_stdout_read = nullptr;
	HANDLE child_stdout_write = nullptr;
	if(!CreatePipe(&child_stdout_read, &child_stdout_write, &sa, 0))
		return false;
	if(!SetHandleInformation(child_stdout_read, HANDLE_FLAG_INHERIT, 0)) {
		bt_addr2line_close_child_handles(
			child_stdout_read, child_stdout_write, nullptr, nullptr);
		return false;
	}

	HANDLE child_stdin_read = nullptr;
	HANDLE child_stdin_write = nullptr;
	if(!CreatePipe(&child_stdin_read, &child_stdin_write, &sa, 0)) {
		bt_addr2line_close_child_handles(
			child_stdout_read, child_stdout_write, nullptr, nullptr);
		return false;
	}
	if(!SetHandleInformation(child_stdin_write, HANDLE_FLAG_INHERIT, 0)) {
		bt_addr2line_close_child_handles(
			child_stdout_read, child_stdout_write, child_stdin_read, child_stdin_write);
		return false;
	}

	STARTUPINFOA si = {};
	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESTDHANDLES;
	si.hStdInput = child_stdin_read;
	si.hStdOutput = child_stdout_write;
	// Use the known inheritable pipe handle instead of inheriting stderr from the parent.
	// This avoids debugger-only first-chance faults when parent stderr is unavailable.
	si.hStdError = child_stdout_write;

	PROCESS_INFORMATION pi = {};
	zth::string cmd = zth::string("addr2line -e \"") + module + "\" -C -f";
	zth::string cmd_line = cmd;
	cmd_line.push_back('\0');
	bool started = CreateProcessA(
			       nullptr, &cmd_line[0], nullptr, nullptr, TRUE, CREATE_NO_WINDOW,
			       nullptr, windows_cwd, &si, &pi)
		       == TRUE;

	if(!started) {
		zth::string shell_cmd = zth::string("cmd.exe /c ") + cmd;
		zth::string shell_cmd_line = shell_cmd;
		shell_cmd_line.push_back('\0');
		started = CreateProcessA(
				  nullptr, &shell_cmd_line[0], nullptr, nullptr, TRUE,
				  CREATE_NO_WINDOW, nullptr, windows_cwd, &si, &pi)
			  == TRUE;
	}

	if(!started) {
		bt_addr2line_close_child_handles(
			child_stdout_read, child_stdout_write, child_stdin_read, child_stdin_write);
		return false;
	}

	CloseHandle(pi.hThread);
	CloseHandle(child_stdout_write);
	CloseHandle(child_stdin_read);

	proc.process = pi.hProcess;
	proc.stdin_write = child_stdin_write;
	proc.stdout_read = child_stdout_read;
	proc.started = true;
	return true;
}

static bool bt_addr2line_write_all(bt_addr2line_process& proc, char const* data, size_t len)
{
	DWORD total = 0;
	while(total < (DWORD)len) {
		DWORD written = 0;
		if(!WriteFile(
			   proc.stdin_write, data + total, (DWORD)(len - total), &written, nullptr)
		   || written == 0)
			return false;
		total += written;
	}
	return true;
}

static bool bt_addr2line_read_line(bt_addr2line_process& proc, char (&buf)[ADDR2LINE_BUF_SIZE])
{
	size_t len = 0;
	while(len + 1 < sizeof(buf)) {
		char ch = '\0';
		DWORD read = 0;
		if(!ReadFile(proc.stdout_read, &ch, 1, &read, nullptr) || read == 0)
			break;
		if(ch == '\n' || ch == '\r') {
			if(len == 0)
				continue;
			break;
		}
		buf[len++] = ch;
	}
	buf[len] = '\0';
	return len > 0;
}
#  else	 // !ZTH_OS_WINDOWS
static void bt_addr2line_stop(bt_addr2line_process& proc)
{
	if(proc.stdin_write >= 0) {
		close(proc.stdin_write);
		proc.stdin_write = -1;
	}
	if(proc.stdout_read >= 0) {
		close(proc.stdout_read);
		proc.stdout_read = -1;
	}
	if(proc.pid > 0) {
		int status = 0;
		(void)waitpid(proc.pid, &status, WNOHANG);
		proc.pid = -1;
	}
	proc.started = false;
}

static bool bt_addr2line_start(char const* module, bt_addr2line_process& proc)
{
	int in_pipe[2] = {-1, -1};
	int out_pipe[2] = {-1, -1};
	if(pipe(in_pipe) || pipe(out_pipe)) {
		if(in_pipe[0] >= 0)
			close(in_pipe[0]);
		if(in_pipe[1] >= 0)
			close(in_pipe[1]);
		if(out_pipe[0] >= 0)
			close(out_pipe[0]);
		if(out_pipe[1] >= 0)
			close(out_pipe[1]);
		return false;
	}

	pid_t pid = fork();
	if(pid == 0) {
		dup2(in_pipe[0], STDIN_FILENO);
		dup2(out_pipe[1], STDOUT_FILENO);
		close(in_pipe[0]);
		close(in_pipe[1]);
		close(out_pipe[0]);
		close(out_pipe[1]);
		execlp("addr2line", "addr2line", "-e", module, "-C", "-f", (char*)nullptr);
		_exit(127);
	}

	close(in_pipe[0]);
	close(out_pipe[1]);

	if(pid < 0) {
		close(in_pipe[1]);
		close(out_pipe[0]);
		return false;
	}

	proc.pid = pid;
	proc.stdin_write = in_pipe[1];
	proc.stdout_read = out_pipe[0];
	proc.started = true;
	return true;
}

static bool bt_addr2line_write_all(bt_addr2line_process& proc, char const* data, size_t len)
{
	while(len > 0) {
		ssize_t written = write(proc.stdin_write, data, len);
		if(written <= 0)
			return false;
		data += written;
		len -= (size_t)written;
	}
	return true;
}

static bool bt_addr2line_read_line(bt_addr2line_process& proc, char (&buf)[ADDR2LINE_BUF_SIZE])
{
	size_t len = 0;
	while(len + 1 < sizeof(buf)) {
		char ch = '\0';
		// NOLINTNEXTLINE(clang-analyzer-unix.BlockInCriticalSection)
		ssize_t r = read(proc.stdout_read, &ch, 1);
		if(r <= 0)
			break;
		if(ch == '\n' || ch == '\r') {
			if(len == 0)
				continue;
			break;
		}
		buf[len++] = ch;
	}
	buf[len] = '\0';
	return len > 0;
}
#  endif // !ZTH_OS_WINDOWS

static bool bt_addr2line_query(
	char const* module, uintptr_t rel, char (&func)[ADDR2LINE_BUF_SIZE],
	char (&file)[ADDR2LINE_BUF_SIZE])
{
	static bt_addr2line_process_map processes;
	bt_addr2line_process& proc = processes[zth::string(module)];

	if(!proc.started && !bt_addr2line_start(module, proc))
		return false;

	char query[64];
	int query_len = snprintf(query, sizeof(query), "0x%llx\n", (unsigned long long)rel);
	if(query_len <= 0)
		return false;

	if(!bt_addr2line_write_all(proc, query, (size_t)query_len)
	   || !bt_addr2line_read_line(proc, func) || !bt_addr2line_read_line(proc, file)) {
		bt_addr2line_stop(proc);
		if(!bt_addr2line_start(module, proc))
			return false;
		if(!bt_addr2line_write_all(proc, query, (size_t)query_len)
		   || !bt_addr2line_read_line(proc, func) || !bt_addr2line_read_line(proc, file)) {
			bt_addr2line_stop(proc);
			return false;
		}
	}

	return true;
}

#  ifdef ZTH_OS_WINDOWS
static bool bt_get_preferred_base_from_file(HMODULE hmod, uintptr_t& preferred_base)
{
	preferred_base = 0;

	wchar_t module_path[MAX_PATH] = {};
	if(!GetModuleFileNameW(
		   hmod, module_path, (DWORD)(sizeof(module_path) / sizeof(module_path[0]))))
		return false;

	HANDLE file = CreateFileW(
		module_path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if(file == INVALID_HANDLE_VALUE)
		return false;

	IMAGE_DOS_HEADER dos = {};
	DWORD bytes = 0;
	if(!ReadFile(file, &dos, sizeof(dos), &bytes, nullptr) || bytes != sizeof(dos)
	   || dos.e_magic != IMAGE_DOS_SIGNATURE) {
		CloseHandle(file);
		return false;
	}

	LARGE_INTEGER li = {};
	li.QuadPart = dos.e_lfanew;
	if(!SetFilePointerEx(file, li, nullptr, FILE_BEGIN)) {
		CloseHandle(file);
		return false;
	}

	DWORD signature = 0;
	if(!ReadFile(file, &signature, sizeof(signature), &bytes, nullptr)
	   || bytes != sizeof(signature) || signature != IMAGE_NT_SIGNATURE) {
		CloseHandle(file);
		return false;
	}

	IMAGE_FILE_HEADER file_header = {};
	if(!ReadFile(file, &file_header, sizeof(file_header), &bytes, nullptr)
	   || bytes != sizeof(file_header)) {
		CloseHandle(file);
		return false;
	}

	LARGE_INTEGER opt_header_pos = {};
	if(!SetFilePointerEx(file, {}, &opt_header_pos, FILE_CURRENT)) {
		CloseHandle(file);
		return false;
	}

	WORD magic = 0;
	if(!ReadFile(file, &magic, sizeof(magic), &bytes, nullptr) || bytes != sizeof(magic)) {
		CloseHandle(file);
		return false;
	}

	if(file_header.SizeOfOptionalHeader == 0) {
		CloseHandle(file);
		return false;
	}

	if(magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
		IMAGE_OPTIONAL_HEADER64 opt = {};
		if(!SetFilePointerEx(file, opt_header_pos, nullptr, FILE_BEGIN)
		   || !ReadFile(file, &opt, sizeof(opt), &bytes, nullptr) || bytes != sizeof(opt)) {
			CloseHandle(file);
			return false;
		}
		preferred_base = (uintptr_t)opt.ImageBase;
	} else if(magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
		IMAGE_OPTIONAL_HEADER32 opt = {};
		if(!SetFilePointerEx(file, opt_header_pos, nullptr, FILE_BEGIN)
		   || !ReadFile(file, &opt, sizeof(opt), &bytes, nullptr) || bytes != sizeof(opt)) {
			CloseHandle(file);
			return false;
		}
		preferred_base = (uintptr_t)opt.ImageBase;
	} else {
		CloseHandle(file);
		return false;
	}

	CloseHandle(file);
	return preferred_base != 0;
}

static void bt_addr2line_resolve_module(void const* addr, bt_addr2line_module_info& info)
{
	static bt_addr2line_module_map module_cache;

	info.base = 0;
	info.runtime_base = 0;
	info.preferred_base = 0;
	info.module.clear();

	HMODULE hmod = nullptr;
	if(GetModuleHandleExA(
		   GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
			   | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
		   reinterpret_cast<LPCSTR>(addr), &hmod)
	   && hmod) {
		uintptr_t runtime_base = reinterpret_cast<uintptr_t>(hmod);
		bt_addr2line_module_map::iterator it = module_cache.find(runtime_base);
		if(it != module_cache.end()) {
			info = it->second;
			return;
		}

		info.runtime_base = runtime_base;
		info.base = runtime_base;
		(void)bt_get_preferred_base_from_file(hmod, info.preferred_base);
		if(!info.preferred_base) {
			if(PIMAGE_NT_HEADERS nt = ImageNtHeader(hmod))
				info.preferred_base =
					static_cast<uintptr_t>(nt->OptionalHeader.ImageBase);
		}
		info.module.resize(MAX_PATH);
		info.module.resize(
			GetModuleFileNameA(hmod, info.module.data(), (DWORD)info.module.size()));
		module_cache[runtime_base] = info;
		return;
	}

	if(info.module.empty()) {
		info.module.resize(MAX_PATH);
		info.module.resize(
			GetModuleFileNameA(nullptr, info.module.data(), (DWORD)info.module.size()));
	}
}

static uintptr_t bt_addr2line_relocate_pc(uintptr_t pc, bt_addr2line_module_info const& info)
{
	if(info.runtime_base && info.preferred_base && pc >= info.runtime_base)
		return pc - info.runtime_base + info.preferred_base;
	if(info.base && pc >= info.base)
		return pc - info.base;
	return pc;
}
#  else // !ZTH_OS_WINDOWS
static void bt_addr2line_resolve_module(void const* addr, bt_addr2line_module_info& info)
{
	info.base = 0;
	info.runtime_base = 0;
	info.preferred_base = 0;
	info.module.clear();

#    ifdef ZTH_HAVE_DL
	static bt_addr2line_module_map module_cache;

	Dl_info dl_info = {};
	if(dladdr(addr, &dl_info) && dl_info.dli_fbase && dl_info.dli_fname) {
		uintptr_t base = reinterpret_cast<uintptr_t>(dl_info.dli_fbase);
		bt_addr2line_module_map::iterator it = module_cache.find(base);
		if(it != module_cache.end()) {
			info = it->second;
			return;
		}

		info.base = base;
		info.module = dl_info.dli_fname;
		module_cache[base] = info;
		return;
	}
#    endif // ZTH_HAVE_DL

#    ifdef ZTH_OS_POSIX
	// cppcheck-suppress knownConditionTrueFalse
	if(info.module.empty())
		info.module = zth::format("/proc/%u/exe", (unsigned)getpid());
#    endif // ZTH_OS_POSIX
}

static uintptr_t bt_addr2line_relocate_pc(uintptr_t pc, bt_addr2line_module_info const& info)
{
	if(info.base && pc >= info.base)
		return pc - info.base;
	return pc;
}
#  endif   // !ZTH_OS_WINDOWS

static void bt_print_addr2line_unsafe(size_t index, void const* addr, int color)
{
	static bt_addr2line_symbol_map cache;
	bt_addr2line_symbol_map::iterator it = cache.find(addr);
	if(it != cache.end()) {
		if(bt_addr2line_has_symbol(it->second.second.c_str(), it->second.first.c_str())) {
			bt_print_addr2line_print(
				index, addr, it->second.first.c_str(), it->second.second.c_str(),
				color);
		} else {
			bt_print_dl(index, addr, color);
		}
		return;
	}

	uintptr_t pc = reinterpret_cast<uintptr_t>(addr);
	bt_addr2line_module_info info = {};
	bt_addr2line_resolve_module(addr, info);
	uintptr_t rel = bt_addr2line_relocate_pc(pc, info);

	static char func[ADDR2LINE_BUF_SIZE];
	static char file[ADDR2LINE_BUF_SIZE];
	if(!bt_addr2line_query(info.module.c_str(), rel, func, file)) {
		cache[addr] = std::make_pair(zth::string(), zth::string());
		bt_print_dl(index, addr, color);
		return;
	}

	if(bt_addr2line_has_symbol(func, file)) {
		cache[addr] = std::make_pair(zth::string(file), zth::string(func));
		bt_print_addr2line_print(index, addr, file, func, color);
	} else {
		cache[addr] = std::make_pair(zth::string(), zth::string());
		bt_print_dl(index, addr, color);
	}
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
#else // !ZTH_BT_ADDR2LINE
#  define bt_print_addr2line bt_print_dl
#endif // ZTH_BT_ADDR2LINE



///////////////////////////////////////////////////////////////////
// Print symbols with libbacktrace
//

#if defined(ZTH_HAVE_LIBBACKTRACE) && !defined(CLANG_TIDY) && !defined(ZTH_BT_PRINT_NONE)
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
#else // !ZTH_HAVE_LIBBACKTRACE
#  define bt_print_libbacktrace bt_print_addr2line
#endif // !ZTH_HAVE_LIBBACKTRACE

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

	char demangled_name[MAX_SYM_NAME] = {};
	char const* name = (sym->NameLen > 0 && sym->Name[0]) ? sym->Name : "??";
	char* demangled_name_gnu = nullptr;
	if(name != nullptr && name[0] != '\0' && name[0] != '?') {
		DWORD demangled_len = UnDecorateSymbolName(
			name, demangled_name, (DWORD)sizeof(demangled_name), UNDNAME_COMPLETE);
		if(demangled_len > 0 && demangled_name[0] != '\0') {
			name = demangled_name;
		} else {
			int demangle_status = -1;
			demangled_name_gnu =
				abi::__cxa_demangle(name, nullptr, nullptr, &demangle_status);
			if(demangle_status == 0 && demangled_name_gnu
			   && demangled_name_gnu[0] != '\0')
				name = demangled_name_gnu;
		}
	}

	IMAGEHLP_LINE64 line = {};
	line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
	DWORD line_displacement = 0;
	if(SymGetLineFromAddr64(process, addr64, &line_displacement, &line)) {
		zth::log_color(
			color, "%s%-3lu [%p] (%s:%lu) %s+0x%llx\n",
			color >= 0 ? ZTH_DBG_PREFIX : "", (unsigned long)index, addr,
			line.FileName ? line.FileName : "??", (unsigned long)line.LineNumber, name,
			(unsigned long long)sym_displacement);
	} else {
		bt_print_libbacktrace(index, addr, color);
	}

	free(demangled_name_gnu); // NOLINT
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
		void const* addr = bt()[i];
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
