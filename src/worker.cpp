/*
 * SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#include <libzth/worker.h>

#include <libzth/async.h>

#include <csignal>
#include <vector>

#ifndef ZTH_OS_WINDOWS
#  include <sys/types.h>
#  include <sys/wait.h>
#  include <unistd.h>
#endif

#ifdef ZTH_OS_MAC
#  include <crt_externs.h>
#  define environ (*_NSGetEnviron())
#endif

namespace zth {

#if !defined(ZTH_OS_WINDOWS) && !defined(ZTH_OS_BAREMETAL)
static void sigchld_handler(int /*unused*/);
#endif

void worker_global_init()
{
	zth_dbg(banner, "%s", banner());
#if !defined(ZTH_OS_WINDOWS) && !defined(ZTH_OS_BAREMETAL)
#  ifdef ZTH_USE_VALGRIND
	// valgrind does not seem to like the sigaction below. Not sure why.
	if(!RUNNING_ON_VALGRIND) // NOLINT(hicpp-no-assembler)
#  endif
	{
		struct sigaction sa = {};
		sa.sa_handler = &sigchld_handler;
		sigaction(SIGCHLD, &sa, nullptr);
	}
#endif
}
ZTH_INIT_CALL(worker_global_init)

#ifdef ZTH_USE_PTHREAD
static void* worker_main(void* fiber)
{
	Worker w;
	w << (Fiber*)fiber;
	w.run();
	return nullptr;
}
#endif

/*!
 * \brief Start a new thread, create a Worker, with one fiber, which executes \p f.
 * \ingroup zth_api_cpp_fiber
 */
int startWorkerThread(
	UNUSED_PAR(void (*f)()), size_t UNUSED_PAR(stack), char const* UNUSED_PAR(name))
{
#ifdef ZTH_USE_PTHREAD
	pthread_t t;
	int res = 0;

	zth_dbg(thread, "[%s] starting new Worker", thread_id_str().c_str());

	Fiber* fiber = new Fiber((void (*)(void*))f, nullptr);
	if(stack)
		if((res = fiber->setStackSize(stack)))
			return res;

	if(name)
		fiber->setName(name);

	if((res = pthread_create(&t, nullptr, &worker_main, (void*)fiber)))
		return res;

	pthread_detach(t);
	return 0;
#else
	return ENOSYS;
#endif
}

/*!
 * \brief Start an external program.
 * \ingroup zth_api_cpp_fiber
 */
int execlp(char const* file, char const* arg, ... /*, nullptr */)
{
	small_vector<char*, 8> argv;
	int res = 0;

	try {
		va_list args;
		va_start(args, arg);
		while(true) {
			char const* a = va_arg(args, char const*);
			if(!a) {
				argv.push_back(nullptr);
				break;
			} else {
				char* argcopy = strdup(a);
				if(!argcopy) {
					res = ENOMEM;
					break;
				}
				argv.push_back(argcopy);
			}
		}
		va_end(args);
	} catch(std::bad_alloc const&) {
		res = ENOMEM;
	} catch(...) {
		res = EAGAIN;
	}

	if(!res)
		res = execvp(file, argv.data());

	for(size_t i = 0; i < argv.size(); i++)
		if(argv[i])
			free(argv[i]); // NOLINT

	return res;
}

__attribute__((unused)) static cow_string thread_id_str() noexcept
{
	Worker const* w = Worker::instance();
	if(w)
		return w->id_str();

	return format(
		"pid %u",
#ifdef ZTH_OS_WINDOWS
		(unsigned int)_getpid()
#else
		(unsigned int)getpid()
#endif
	);
}

/*!
 * \brief Start an external program.
 * \ingroup zth_api_cpp_fiber
 */
int execvp(char const* UNUSED_PAR(file), char* const UNUSED_PAR(arg[]))
{
#if defined(ZTH_OS_WINDOWS) || defined(ZTH_OS_BAREMETAL)
	return ENOSYS;
#else
	perf_syscall("execvp()");
	zth_dbg(thread, "[%s] execvp %s", thread_id_str().c_str(), file);

	pid_t pid = fork();
	if(!pid) {
		// In child.
		execve(file, arg, environ);
		// If we get here, we could not create the process.
		_exit(127);
		// Unreachable.
		return EAGAIN;
	} else if(pid == -1) {
		int res = errno;
		zth_dbg(thread, "[%s] Could not fork(); %s", thread_id_str().c_str(),
			err(res).c_str());
		return res;
	} else
		return 0;
#endif
}

#if !defined(ZTH_OS_WINDOWS) && !defined(ZTH_OS_BAREMETAL)
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static volatile sig_atomic_t sigchld_cleanup = 0;

static void sigchld_handler(int /*unused*/)
{
	sigchld_cleanup = 1;
}
#endif

void sigchld_check()
{
#if !defined(ZTH_OS_WINDOWS) && !defined(ZTH_OS_BAREMETAL)
#  ifdef ZTH_USE_VALGRIND
	// The sigaction was disabled, so sigchld_cleanup will always be 0.
	if(!RUNNING_ON_VALGRIND) // NOLINT(hicpp-no-assembler)
#  endif
		if(likely(sigchld_cleanup == 0))
			return;

	Worker* w = Worker::instance();
	char const* id_str = w ? w->id_str() : "?";

	while(true) {
		sigchld_cleanup = 0;
		int wstatus = 0;
		pid_t pid = waitpid(-1, &wstatus, WNOHANG);

		if(pid == 0) {
			// No (other) child exited.
			return;
		} else if(pid == -1) {
			zth_dbg(worker, "[%s] waitpid() failed; %s", id_str, err(errno).c_str());
			return;
		} else {
			zth_dbg(worker, "[%s] Child process %u terminated with exit code %d",
				id_str, (unsigned int)pid, wstatus);
		}
	}
#endif
}

} // namespace zth
