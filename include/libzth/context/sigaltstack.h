#ifndef ZTH_CONTEXT_SIGALTSTACK_H
#define ZTH_CONTEXT_SIGALTSTACK_H
/*
 * Zth (libzth), a cooperative userspace multitasking library.
 * Copyright (C) 2019-2021  Jochem Rutgers
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

#ifndef ZTH_CONTEXT_CONTEXT_H
#  error This file must be included by libzth/context/context.h.
#endif

#ifdef __cplusplus

#ifdef ZTH_ENABLE_ASAN
#  error Invalid configuration combination sigaltstack with asan.
#endif

#include <csetjmp>
#include <sys/types.h>
#include <csignal>

#ifdef ZTH_HAVE_PTHREAD
#  include <pthread.h>
#  ifndef ZTH_OS_MAC
#    define pthread_yield_np()	pthread_yield()
#  endif
#else
#  define pthread_sigmask(...)	sigprocmask(__VA_ARGS__)
#  define pthread_kill(...)	kill(__VA_ARGS__)
#  define pthread_self()	getpid()
#  define pthread_yield_np()	sched_yield()
#endif

namespace zth {
namespace impl {

static void context_global_init()
{
	// By default, block SIGUSR1 for the main/all thread(s).
	int res = 0;

	sigset_t sigs;
	sigemptyset(&sigs);
	sigaddset(&sigs, SIGUSR1);
	if((res = pthread_sigmask(SIG_BLOCK, &sigs, nullptr)))
		goto error;

	return;
error:
	zth_abort("Cannot initialize signals; %s", err(res).c_str());
}
ZTH_INIT_CALL(context_global_init)

} // namespace impl

class Context : public impl::ContextArch<Context> {
	ZTH_CLASS_NEW_DELETE(Context)
public:
	typedef impl::ContextArch<Context> base;

	constexpr explicit Context(ContextAttr const& attr) noexcept
		: base(attr)
		, m_trampoline_env()
		, m_mask()
		, m_did_trampoline()
	{}

private:
	static void context_trampoline(int sig)
	{
		// At this point, we are in the signal handler.

		if(unlikely(sig != SIGUSR1))
			// Huh?
			return;

		// Put the context on the stack, as trampoline_context is about to change.
		Context* context = ZTH_TLS_GET(trampoline_context);

		if(unlikely(!context || context->m_did_trampoline))
			// Huh?
			return;

		if(setjmp(context->m_trampoline_env) == 0) {
			// Return immediately to complete the signal handler.
			context->m_did_trampoline = 1;
			return;
		}

		// If we get here, we are not in the signal handler anymore, but in the actual fiber context.
		zth_dbg(context, "Bootstrapping %p", context);

		if(Config::ContextSignals) {
			int res = pthread_sigmask(SIG_SETMASK, &context->m_mask, nullptr);
			if(unlikely(res))
				zth_abort("Cannot set signal mask");
		}

		// However, we still need to setup the context for the first time.
		if(sigsetjmp(context->m_env, Config::ContextSignals) == 0) {
			// Now, we are ready to be context switched to this fiber.
			// Go back to our parent, as it was not our schedule turn to continue with actual execution.
			siglongjmp(*context->m_parent, 1);
		}

		// This is actually the first time we enter the fiber by the normal scheduler.
		context_entry(context);
	}

	ZTH_TLS_MEMBER(Context*, trampoline_context)

public:
	static int init() noexcept
	{
		int res = base::init();
		if(res)
			return res;

		// Claim SIGUSR1 for context_create().
		struct sigaction sa;
		sa.sa_handler = &context_trampoline;
		sigemptyset(&sa.sa_mask);
		sa.sa_flags = SA_ONSTACK;
		if(sigaction(SIGUSR1, &sa, nullptr))
			return errno;

		// Let SIGUSR1 remain blocked.

		// All set.
		return 0;
	}

	static void deinit() noexcept
	{
		// Release SIGUSR1.
		struct sigaction sa;
		sa.sa_handler = SIG_DFL;
		sigemptyset(&sa.sa_mask);
		sa.sa_flags = 0;
		sigaction(SIGUSR1, &sa, nullptr);

		base::deinit();
	}

	int create() noexcept
	{
		int res = base::create();
		if(unlikely(res))
			return res;

		Stack const& stack_ = stackUsable();
		if(unlikely(!stack_))
			// Stackless fiber only saves current context; nothing to do.
			return 0;

		m_did_trampoline = 0;
		stack_t ss = {};
		stack_t oss = {};

		// Let the new context inherit our signal mask.
		if(Config::ContextSignals)
			if(unlikely((res = pthread_sigmask(0, nullptr, &m_mask))))
				goto rollback;

		ss.ss_sp = stack_.p;
		ss.ss_size = stack_.size;

#ifdef ZTH_USE_VALGRIND
		// Disable checking during bootstrap.
		VALGRIND_DISABLE_ADDR_ERROR_REPORTING_IN_RANGE(ss.ss_sp, ss.ss_size);
#endif

#ifdef SS_AUTODISARM
		ss.ss_flags |= SS_AUTODISARM;
#endif
		if(unlikely(sigaltstack(&ss, &oss))) {
			res = errno;
			goto rollback;
		}

		ZTH_TLS_SET(trampoline_context, this);

		// Ready to do the signal.
		sigset_t sigs;
		sigemptyset(&sigs);
		sigaddset(&sigs, SIGUSR1);
		if(unlikely((res = pthread_sigmask(SIG_UNBLOCK, &sigs, nullptr))))
			goto rollback_altstack;

		// As we are not blocking SIGUSR1, we will get the signal.
		if(unlikely((res = pthread_kill(pthread_self(), SIGUSR1))))
			goto rollback_altstack;

		// Shouldn't take long...
		while(!m_did_trampoline)
			pthread_yield_np();

		// Block again.
		if(unlikely((res = pthread_sigmask(SIG_BLOCK, &sigs, nullptr))))
			goto rollback_altstack;

		if(Config::Debug)
			ZTH_TLS_SET(trampoline_context, nullptr);

		if(Config::EnableAssert) {
			if(unlikely(sigaltstack(nullptr, &ss))) {
				res = errno;
				goto rollback_altstack;
			}
			zth_assert(!(ss.ss_flags & SS_ONSTACK));
		}

#ifndef SS_AUTODISARM
		// Reset sigaltstack.
		ss.ss_flags = SS_DISABLE;
		if(unlikely(sigaltstack(&ss, nullptr))) {
			res = errno;
			goto rollback_altstack;
		}
#endif

		if(unlikely(!(oss.ss_flags & SS_DISABLE))) {
			if(sigaltstack(&oss, nullptr)) {
				res = errno;
				goto rollback;
			}
		}
#ifdef ZTH_HAVE_VALGRIND
		else if(RUNNING_ON_VALGRIND) {
			// Valgrind has a hard time tracking when we left the signal handler
			// and are not using the altstack anymore. Reset the altstack, such
			// that successive sigaltstack() do not fail with EPERM.
			stack_t dss;
			dss.ss_sp = dummyAltStack;
			dss.ss_size = sizeof(dummyAltStack);
			dss.ss_flags = 0;
			if(unlikely(sigaltstack(&dss, nullptr))) {
				res = errno;
				goto rollback;
			}

			// Although the previous stack was disabled, it is still valid.
			VALGRIND_MAKE_MEM_DEFINED(ss.ss_sp, ss.ss_size);
		}
#endif

		// Ok, we have returned from the signal handler.
		// Now go back again and save a proper env.

		sigjmp_buf me;
		m_parent = &me;
		if(sigsetjmp(me, Config::ContextSignals) == 0)
			longjmp(m_trampoline_env, 1);

#ifdef ZTH_USE_VALGRIND
		// Disabled checking during bootstrap.
		VALGRIND_ENABLE_ADDR_ERROR_REPORTING_IN_RANGE(ss.ss_sp, ss.ss_size);
#endif

		if(Config::EnableAssert) {
			if(unlikely(sigaltstack(nullptr, &oss))) {
				res = errno;
				goto rollback;
			}
			zth_assert(!(oss.ss_flags & SS_ONSTACK));
		}

		// context is setup properly and is ready for normal scheduling.
		if(Config::Debug)
			m_parent = nullptr;

		return 0;

	rollback_altstack:
#ifdef ZTH_USE_VALGRIND
		VALGRIND_ENABLE_ADDR_ERROR_REPORTING_IN_RANGE(ss.ss_sp, ss.ss_size);
#endif
		if(!(oss.ss_flags & SS_DISABLE))
			sigaltstack(&oss, nullptr);
	rollback:
		base::destroy();
		return res ? res : EINVAL;
	}

	void context_switch(Context& to) noexcept
	{
		if(sigsetjmp(m_env, Config::ContextSignals) == 0) {
			// Go switch away from here.
			siglongjmp(to.m_env, 1);
		}
	}

private:
	union {
		jmp_buf m_trampoline_env;
		sigjmp_buf m_env;
	};
	sigset_t m_mask;
	union {
		sig_atomic_t volatile m_did_trampoline;
		sigjmp_buf* volatile m_parent;
	};
#ifdef ZTH_HAVE_VALGRIND
	static char dummyAltStack[MINSIGSTKSZ];
#endif
};

ZTH_TLS_DEFINE(Context*, Context::trampoline_context, nullptr)

#ifdef ZTH_HAVE_VALGRIND
char Context::dummyAltStack[MINSIGSTKSZ];
#endif

} // namespace zth
#endif // __cplusplus
#endif // ZTH_CONTEXT_SIGALTSTACK_H
