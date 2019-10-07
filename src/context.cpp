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

#include <libzth/macros.h>

#ifdef ZTH_OS_WINDOWS
#  define WIN32_LEAN_AND_MEAN
#  define NOGDI
#endif

#ifdef ZTH_OS_MAC
// For ucontext_t
#  define _XOPEN_SOURCE
#  include <sched.h>
#endif

#include <libzth/context.h>
#include <libzth/util.h>
#include <libzth/worker.h>

#include <unistd.h>
#include <cstring>

#ifdef ZTH_CONTEXT_SIGALTSTACK
#  include <csetjmp>
#  include <sys/types.h>
#  include <csignal>
#  ifdef ZTH_HAVE_PTHREAD
#    include <pthread.h>
#  else
#    define pthread_sigmask(...)	sigprocmask(__VA_ARGS__)
#    define pthread_kill(...)		kill(__VA_ARGS__)
#    define pthread_self()			getpid()
#    define pthread_yield_np()		sched_yield()
#  endif
#endif

#ifdef ZTH_CONTEXT_UCONTEXT
#  include <csetjmp>
#  include <csignal>
#  include <ucontext.h>
#endif

#ifdef ZTH_CONTEXT_SJLJ
#  include <csetjmp>
#  include <csignal>
#endif

#ifdef ZTH_CONTEXT_WINFIBER
#  include <windows.h>
#elif defined(ZTH_HAVE_MMAN)
#  include <sys/mman.h>
#  ifndef MAP_STACK
#    define MAP_STACK 0
#  endif
#endif

#ifdef ZTH_HAVE_VALGRIND
#  include <valgrind/memcheck.h>
#endif

using namespace std;

namespace zth {

class Context {
public:
	void* stack;
	size_t stackSize;
	ContextAttr attr;
#ifdef ZTH_CONTEXT_SIGALTSTACK
	union {
		jmp_buf trampoline_env;
		sigjmp_buf env;
	};
	sigset_t mask;
	union {
		sig_atomic_t volatile did_trampoline;
		sigjmp_buf* volatile parent;
	};
#endif
#ifdef ZTH_CONTEXT_UCONTEXT
	sigjmp_buf env;
#endif
#ifdef ZTH_CONTEXT_SJLJ
	jmp_buf env;
#endif
#ifdef ZTH_CONTEXT_WINFIBER
	LPVOID fiber;
#endif
#ifdef ZTH_USE_VALGRIND
	unsigned int valgrind_stack_id;
#endif
};

extern "C" void context_entry(Context* context) __attribute__((noreturn,used));


////////////////////////////////////////////////////////////
// ucontext method

#ifdef ZTH_CONTEXT_UCONTEXT
#  define context_init_impl()	0
#  define context_deinit_impl()
#  define context_destroy_impl(...)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations" // I know...

static void context_trampoline(Context* context, sigjmp_buf origin) {
	if(sigsetjmp(context->env, Config::ContextSignals) == 0)
		siglongjmp(origin, 1);

	context_entry(context);
}

static int context_create_impl(Context* context, stack_t* stack) {
	if(unlikely(!stack->ss_sp))
		// Stackless fiber only saves current context; nothing to do.
		return 0;

	ucontext_t uc;
	if(getcontext(&uc))
		return EINVAL;
	
	uc.uc_link = NULL;
	uc.uc_stack = *stack;
	sigjmp_buf origin;
	makecontext(&uc, (void(*)(void))&context_trampoline, 2, context, origin);
	if(sigsetjmp(origin, Config::ContextSignals) == 0)
		setcontext(&uc);
	return 0;
}

static void context_switch_impl(Context* from, Context* to) {
	// switchcontext() restores signal masks, which is slow...
	if(sigsetjmp(from->env, Config::ContextSignals) == 0)
		siglongjmp(to->env, 1);
}

#  pragma GCC diagnostic pop
#endif // ZTH_CONTEXT_UCONTEXT




////////////////////////////////////////////////////////////
// sigaltstack() method

#ifdef ZTH_CONTEXT_SIGALTSTACK
#  define context_destroy_impl(...)

static void context_global_init() {
	// By default, block SIGUSR1 for the main/all thread(s).
	int res = 0;

	sigset_t sigs;
	sigemptyset(&sigs);
	sigaddset(&sigs, SIGUSR1);
	if((res = pthread_sigmask(SIG_BLOCK, &sigs, NULL)))
		goto error;

	return;
error:
	zth_abort("Cannot initialize signals; %s", err(res).c_str());
}
INIT_CALL(context_global_init)

static void bootstrap(Context* context) __attribute__((noreturn
#ifdef ZTH_USE_VALGRIND
	,noinline
#endif
	));
static void bootstrap(Context* context) {
	zth_dbg(context, "Bootstrapping %p", context);

	if(Config::ContextSignals) {
		int res = pthread_sigmask(SIG_SETMASK, &context->mask, NULL);
		if(unlikely(res))
			zth_abort("Cannot set signal mask");
	}

	// However, we still need to setup the context for the first time.
	if(sigsetjmp(context->env, Config::ContextSignals) == 0) {
		// Now, we are ready to be context switched to this fiber.
		// Go back to our parent, as it was not our schedule turn to continue with actual execution.
		siglongjmp(*context->parent, 1);
	}

	// This is actually the first time we enter the fiber by the normal scheduler.
	context_entry(context);
}

ZTH_TLS_STATIC(Context*, trampoline_context, NULL)

static void context_trampoline(int sig) {
	// At this point, we are in the signal handler.
	
	if(unlikely(sig != SIGUSR1))
		// Huh?
		return;

	// Put the context on the stack, as trampoline_context is about to change.
	Context* context = ZTH_TLS_GET(trampoline_context);

	if(unlikely(!context || context->did_trampoline))
		// Huh?
		return;

	if(setjmp(context->trampoline_env) == 0)
	{
		// Return immediately to complete the signal handler.
		context->did_trampoline = 1;
		return;
	}

	// If we get here, we are not in the signal handler anymore, but in the actual fiber context.
	bootstrap(context);
}

static int context_init_impl() {
	// Claim SIGUSR1 for context_create().
	struct sigaction sa;
	sa.sa_handler = &context_trampoline;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_ONSTACK;
	if(sigaction(SIGUSR1, &sa, NULL))
		return errno;

	// Let SIGUSR1 remain blocked.

	// All set.
	return 0;
}

static void context_deinit_impl() {
	// Release SIGUSR1.
	struct sigaction sa;
	sa.sa_handler = SIG_DFL;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	sigaction(SIGUSR1, &sa, NULL);
}

#ifdef ZTH_HAVE_VALGRIND
static char dummyAltStack[MINSIGSTKSZ];
#endif

static int context_create_impl(Context* context, stack_t* stack) {
	if(unlikely(!stack->ss_sp))
		// Stackless fiber only saves current context; nothing to do.
		return 0;

	int res = 0;
	context->did_trampoline = 0;

	// Let the new context inherit our signal mask.
	if(Config::ContextSignals)
		if(unlikely((res = pthread_sigmask(0, NULL, &context->mask))))
			return res;

	stack_t& ss = *stack;
	stack_t oss;

#ifdef ZTH_USE_VALGRIND
	// Disable checking during bootstrap.
	VALGRIND_DISABLE_ADDR_ERROR_REPORTING_IN_RANGE(ss.ss_sp, ss.ss_size);
#endif

#ifdef SS_AUTODISARM
	ss.ss_flags |= SS_AUTODISARM;
#endif
	if(unlikely(sigaltstack(&ss, &oss)))
		return errno;
	
	ZTH_TLS_SET(trampoline_context, context);

	// Ready to do the signal.
	sigset_t sigs;
	sigemptyset(&sigs);
	sigaddset(&sigs, SIGUSR1);
	if(unlikely((res = pthread_sigmask(SIG_UNBLOCK, &sigs, NULL))))
		goto rollback_altstack;

	// As we are not blocking SIGUSR1, we will get the signal.
	if(unlikely((res = pthread_kill(pthread_self(), SIGUSR1))))
		goto rollback_altstack;

	// Shouldn't take long...
	while(!context->did_trampoline)
		pthread_yield_np();

	// Block again.
	if(unlikely((res = pthread_sigmask(SIG_BLOCK, &sigs, NULL))))
		goto rollback_altstack;

	if(Config::Debug)
		ZTH_TLS_SET(trampoline_context, NULL);

	if(Config::EnableAssert) {
		if(unlikely(sigaltstack(NULL, &ss))) {
			res = errno;
			goto rollback_altstack;
		}
		zth_assert(!(ss.ss_flags & SS_ONSTACK));
	}

#ifndef SS_AUTODISARM
	// Reset sigaltstack.
	ss.ss_flags = SS_DISABLE;
	if(unlikely(sigaltstack(&ss, NULL))) {
		res = errno;
		goto rollback_altstack;
	}
#endif

	if(unlikely(!(oss.ss_flags & SS_DISABLE))) {
		if(sigaltstack(&oss, NULL)) {
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
		if(unlikely(sigaltstack(&dss, NULL))) {
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
	context->parent = &me;
	if(sigsetjmp(me, Config::ContextSignals) == 0)
		longjmp(context->trampoline_env, 1);

#ifdef ZTH_USE_VALGRIND
	// Disabled checking during bootstrap.
	VALGRIND_ENABLE_ADDR_ERROR_REPORTING_IN_RANGE(ss.ss_sp, ss.ss_size);
#endif

	if(Config::EnableAssert) {
		if(unlikely(sigaltstack(NULL, &oss))) {
			res = errno;
			goto rollback;
		}
		zth_assert(!(oss.ss_flags & SS_ONSTACK));
	}

	// context is setup properly and is ready for normal scheduling.
	if(Config::Debug)
		context->parent = NULL;

	return 0;

rollback_altstack:
#ifdef ZTH_USE_VALGRIND
	VALGRIND_ENABLE_ADDR_ERROR_REPORTING_IN_RANGE(ss.ss_sp, ss.ss_size);
#endif
	if(!(oss.ss_flags & SS_DISABLE))
		sigaltstack(&oss, NULL);
rollback:
	return res ? res : EINVAL;
}

static void context_switch_impl(Context* from, Context* to) {
	if(sigsetjmp(from->env, Config::ContextSignals) == 0) {
		// Go switch away from here.
		siglongjmp(to->env, 1);
	}
}

#endif // ZTH_CONTEXT_SIGALTSTACK




////////////////////////////////////////////////////////////
// Win32 fiber method

#ifdef ZTH_CONTEXT_WINFIBER
#ifdef stack_t
#  undef stack_t
#endif
struct stack_t {
	bool haveStack;
};
#  define context_deinit_impl()

static int context_init_impl() {
	int res = 0;
	if(ConvertThreadToFiber(NULL))
		return 0;
	else if((res = -(int)GetLastError()))
		return res;
	else
		return EINVAL;
}

static int context_create_impl(Context* context, stack_t* stack) {
	int res = 0;
	if(unlikely(!stack->haveStack)) {
		// Stackless fiber only saves current context.
		if((context->fiber = GetCurrentFiber()))
			return 0;
	} else if((context->fiber = CreateFiber((SIZE_T)Config::DefaultFiberStackSize, (LPFIBER_START_ROUTINE)&context_entry, (LPVOID)context)))
		return 0;

	if((res = -(int)GetLastError()))
		return res;
	else
		return EINVAL;
}

static void context_destroy_impl(Context* context) {
	if(!context->fiber)
		return;

	DeleteFiber(context->fiber);
	context->fiber = NULL;
}

static void context_switch_impl(Context* from, Context* to) {
	zth_assert(to->fiber && to->fiber != GetCurrentFiber());
	SwitchToFiber(to->fiber);
}

#endif // ZTH_CONTEXT_WINFIBER



////////////////////////////////////////////////////////////
// setjmp/longjmp fiddling method

#ifdef ZTH_CONTEXT_SJLJ
#  define context_init_impl()	0
#  define context_deinit_impl()
#  define context_destroy_impl(...)

#ifdef ZTH_ARCH_ARM
__attribute__((naked)) static void context_trampoline() {
	__asm__ (
		"ldr r0, [sp]\n"
		"b context_entry\n"
		);
}
#endif

static int context_create_impl(Context* context, stack_t* stack) {
	if(unlikely(!stack->ss_sp))
		// Stackless fiber only saves current context; nothing to do.
		return 0;

	setjmp(context->env);
#ifdef ZTH_ARCH_ARM
	// Do some fiddling with the jmp_buf...

	// We only checked against newlib 3.1.0.
#  if NEWLIB_VERSION != 30100L
#    error Unsupported newlib version.
#  endif

	void** sp = (void**)((uintptr_t)((char*)stack->ss_sp + stack->ss_size - sizeof(void*)) & ~(uintptr_t)7); // sp (must be dword aligned)
	*sp = context; // push context argument on the new stack

	size_t sp_offset = 
#  if (__ARM_ARCH_ISA_THUMB == 1 && !__ARM_ARCH_ISA_ARM) || defined(__thumb2__)
		8;
#  else
		9;
#  endif

	context->env[sp_offset] = (intptr_t)sp;
	context->env[sp_offset + 1] = (intptr_t)&context_trampoline; // lr
#else
#  error Unsupported architecture for setjmp/longjmp method.
#endif
	return 0;
}

static void context_switch_impl(Context* from, Context* to) {
	// As bare metal does not have signals, sigsetjmp and siglongjmp is not necessary.
	if(setjmp(from->env) == 0)
		longjmp(to->env, 1);
}

#endif // ZTH_CONTEXT_SJLJ



////////////////////////////////////////////////////////////
// Stack allocation

#ifdef ZTH_CONTEXT_WINFIBER
// Stack is implicit by CreateFiber().
#  define context_deletestack(...)
static int context_newstack(Context* context, stack_t* stack) {
	stack->haveStack = context->stackSize > 0;
	return 0;
}
#else // !ZTH_CONTEXT_WINFIBER
static void context_deletestack(Context* context) {
	if(context->stack) {
#ifdef ZTH_USE_VALGRIND
		VALGRIND_STACK_DEREGISTER(context->valgrind_stack_id);
#endif
#ifdef ZTH_HAVE_MMAN
		munmap(context->stack, context->stackSize);
#else
		free(context->stack);
#endif
		context->stack = NULL;
	}
}

static int context_newstack(Context* context, stack_t* stack) {
	int res = 0;
	size_t const pagesize =
#ifdef ZTH_HAVE_MMAN
		getpagesize();
#else
		sizeof(void*);
#endif
	zth_assert(__builtin_popcount(pagesize) == 1);
	context->stackSize = (context->stackSize + MINSIGSTKSZ + 3 * pagesize - 1) & ~(pagesize - 1);

	// Get a new stack.
#ifdef ZTH_HAVE_MMAN
	if(unlikely((context->stack = mmap(NULL, context->stackSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, -1, 0)) == MAP_FAILED))
#else
	if(unlikely((context->stack = malloc(context->stackSize)) == NULL))
#endif
	{
		res = errno;
		context->stack = NULL;
		goto rollback;
	}

#ifdef ZTH_HAVE_MMAN
	if(Config::EnableStackGuard) {
		stack->ss_sp = (char*)context->stack + pagesize;
		stack->ss_size = context->stackSize - 2 * pagesize;
	} else
#endif
	{
		stack->ss_sp = context->stack;
		stack->ss_size = context->stackSize;
	}
	stack->ss_flags = 0;

#ifdef ZTH_USE_VALGRIND
	context->valgrind_stack_id = VALGRIND_STACK_REGISTER(stack->ss_sp, (char*)stack->ss_sp + stack->ss_size - 1);

	if(RUNNING_ON_VALGRIND)
		zth_dbg(context, "[%s] Stack of context %p has Valgrind id %u",
			currentWorker().id_str(), context, context->valgrind_stack_id);
#endif

#ifdef ZTH_HAVE_MMAN
	if(Config::EnableStackGuard) {
		// Guard both ends of the stack.
		if(unlikely(
			mprotect(context->stack, pagesize, PROT_NONE) ||
			mprotect((char*)context->stack + context->stackSize - pagesize, pagesize, PROT_NONE)))
		{
			res = errno;
			goto rollback_mmap;
		}
	}
#endif

	return 0;

rollback_mmap:
	__attribute__((unused));
	context_deletestack(context);
rollback:
	return res ? res : EINVAL;
}
#endif // !ZTH_CONTEXT_WINFIBER




////////////////////////////////////////////////////////////
// Common functions

int context_init() {
	zth_dbg(context, "[%s] Initialize", currentWorker().id_str());
	return context_init_impl();
}

void context_deinit() {
	zth_dbg(context, "[%s] Deinit", currentWorker().id_str());
	context_deinit_impl();
}

void context_entry(Context* context) {
	zth_assert(context);
	// Go execute the fiber.
	context->attr.entry(context->attr.arg);
	// Fiber has quit. Switch to another one.
	yield();
	// We should never get here, otherwise the fiber wasn't dead...
	zth_abort("Returned to finished context");
}

int context_create(Context*& context, ContextAttr const& attr) {
	int res = 0;
	context = new Context();
	context->stack = NULL;
	context->stackSize = attr.stackSize;
	context->attr = attr;

	stack_t stack = {};
	if(unlikely(attr.stackSize > 0 && (res = context_newstack(context, &stack))))
		goto rollback_new;

	if(unlikely((res = context_create_impl(context, &stack))))
		goto rollback_stack;

	if(likely(attr.stackSize > 0)) {
#ifdef ZTH_CONTEXT_WINFIBER
		zth_dbg(context, "[%s] New context %p", currentWorker().id_str(), context);
#else
		zth_dbg(context, "[%s] New context %p with stack: %p-%p", currentWorker().id_str(), context, stack.ss_sp, (char*)stack.ss_sp + stack.ss_size - 1);
#endif
	}
	return 0;
	
rollback_stack:
	context_deletestack(context);
rollback_new:
	delete context;
	zth_dbg(context, "[%s] Cannot create context; %s", currentWorker().id_str(), err(res).c_str());
	return res ? res : EINVAL;
}

void context_switch(Context* from, Context* to) {
	zth_assert(from);
	zth_assert(to);

	context_switch_impl(from, to);

	// Got back from somewhere else.
}

void context_destroy(Context* context) {
	if(!context)
		return;
	
	context_destroy_impl(context);
	context_deletestack(context);
	delete context;
	zth_dbg(context, "[%s] Deleted context %p", currentWorker().id_str(), context);
}

} // namespace

