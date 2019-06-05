#include <zth>

#include <unistd.h>
#include <csetjmp>
#include <csignal>
#include <sys/mman.h>
#include <cstring>
#include <pthread.h>

#ifdef ZTH_HAVE_VALGRIND
#  include <valgrind/memcheck.h>
#endif

#ifdef ZTH_OS_MAC
#  ifndef MAP_STACK
#    define MAP_STACK 0
#  endif
#endif

using namespace std;

namespace zth
{

class Context {
public:
	union {
		jmp_buf trampoline_env;
		sigjmp_buf env;
	};
	void* stack;
	size_t stackSize;
	ContextAttr attr;
	sigset_t mask;
	union {
		sig_atomic_t volatile did_trampoline;
		sigjmp_buf* volatile parent;
	};
#ifdef ZTH_USE_VALGRIND
	unsigned int valgrind_stack_id;
#endif
};

ZTH_TLS_STATIC(Context*, trampoline_context, NULL)

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
	zth_abort("Cannot initialize signals; %s (error %d)", strerror(res), res);
}
INIT_CALL(context_global_init)

static void bootstrap(Context* context) __attribute__((noreturn
#ifdef ZTH_USE_VALGRIND
	,noinline
#endif
	));
static void bootstrap(Context* context)
{
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
	// Go execute the fiber.
	context->attr.entry(context->attr.arg);
	// Fiber has quit. Switch to another one.
	yield();
	// We should never get here, otherwise the fiber wasn't dead...
	zth_abort("Returned to finished context");
}

static void context_trampoline(int sig)
{
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

#ifdef ZTH_HAVE_VALGRIND
static char dummyAltStack[MINSIGSTKSZ];
#endif

int context_init()
{
	zth_dbg(context, "[th %s] Initialize", pthreadId().c_str());

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

void context_deinit()
{
	zth_dbg(context, "[th %s] Deinit", pthreadId().c_str());

	struct sigaction sa;
	sa.sa_handler = SIG_DFL;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	sigaction(SIGUSR1, &sa, NULL);
}

int context_create(Context*& context, ContextAttr const& attr)
{
	int res = 0;
	context = new Context();
	context->stack = NULL;
	context->stackSize = attr.stackSize;
	context->attr = attr;
	context->did_trampoline = 0;

	if(unlikely(attr.stackSize == 0)) {
		// No stack, so no entry point can be accessed.
		// This is used if only a context is required, but not a fiber is to be started.
		context->stack = NULL;
		return 0;
	}
	
	// Let the new context inherit our signal mask.
	if(Config::ContextSignals)
		if(unlikely((res = pthread_sigmask(0, NULL, &context->mask))))
			return res;

	size_t const pagesize = getpagesize();
	zth_assert(__builtin_popcount(pagesize) == 1);
	context->stackSize = (context->stackSize + MINSIGSTKSZ + 3 * pagesize - 1) & ~(pagesize - 1);

	// Get a new stack.
	if(unlikely((context->stack = mmap(NULL, context->stackSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, -1, 0)) == MAP_FAILED)) {
		res = errno;
		goto rollback_new;
	}

	stack_t ss;
	stack_t oss;

	if(Config::EnableStackGuard) {
		// Guard both ends of the stack.
		if( unlikely(
		        mprotect(context->stack, pagesize, PROT_NONE) ||
		        mprotect((char*)context->stack + context->stackSize - pagesize, pagesize, PROT_NONE)))
		{
			res = errno;
			goto rollback_mmap;
		}

		ss.ss_sp = (char*)context->stack + pagesize;
		ss.ss_size = context->stackSize - 2 * pagesize;
	} else {
		ss.ss_sp = context->stack;
		ss.ss_size = context->stackSize;
	}

	ss.ss_flags = 0
#ifdef SS_AUTODISARM
		| SS_AUTODISARM
#endif
		;
	if(unlikely(sigaltstack(&ss, &oss))) {
		res = errno;
		goto rollback_stack;
	}
	
	ZTH_TLS_SET(trampoline_context, context);

	// Ready to do the signal.
	sigset_t sigs;
	sigemptyset(&sigs);
	sigaddset(&sigs, SIGUSR1);
	if(unlikely((res = pthread_sigmask(SIG_UNBLOCK, &sigs, NULL))))
		goto rollback_altstack;

#ifdef ZTH_USE_VALGRIND
	// Disable checking during bootstrap.
	VALGRIND_DISABLE_ADDR_ERROR_REPORTING_IN_RANGE(ss.ss_sp, ss.ss_size);
#endif

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
			goto rollback_stack;
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
			goto rollback_altstack;
		}

		// Although the previous stack was disabled, it is still valid.
		VALGRIND_MAKE_MEM_DEFINED(ss.ss_sp, ss.ss_size);
	}
#endif

	// Ok, we have returned from the signal handler.
	// Now go back again and save a proper env.
#ifdef ZTH_USE_VALGRIND
	context->valgrind_stack_id = VALGRIND_STACK_REGISTER(ss.ss_sp, (char*)ss.ss_sp + ss.ss_size - 1);

	if(RUNNING_ON_VALGRIND)
		zth_dbg(context, "[th %s] Stack of context %p has Valgrind id %u and is at %p-%p",
			pthreadId().c_str(), context, context->valgrind_stack_id,
			ss.ss_sp, (char*)ss.ss_sp + ss.ss_size - 1);
#endif

	sigjmp_buf me;
	context->parent = &me;
	if(sigsetjmp(me, Config::ContextSignals) == 0)
		longjmp(context->trampoline_env, 1);

#ifdef ZTH_USE_VALGRIND
	// Disable checking during bootstrap.
	VALGRIND_ENABLE_ADDR_ERROR_REPORTING_IN_RANGE(ss.ss_sp, ss.ss_size);
#endif

	if(Config::EnableAssert) {
		if(unlikely(sigaltstack(NULL, &ss))) {
			res = errno;
			goto rollback_altstack;
		}
		zth_assert(!(ss.ss_flags & SS_ONSTACK));
	}

	// context is setup properly and is ready for normal scheduling.
	if(Config::Debug)
		context->parent = NULL;

	zth_dbg(context, "[th %s] New context %p with stack: %p-%p", pthreadId().c_str(), context, ss.ss_sp, (char*)ss.ss_sp + ss.ss_size - 1);
	return 0;

rollback_altstack:
	if(!(oss.ss_flags & SS_DISABLE))
		sigaltstack(&oss, NULL);
rollback_stack:
#ifdef ZTH_USE_VALGRIND
	VALGRIND_ENABLE_ADDR_ERROR_REPORTING_IN_RANGE(ss.ss_sp, ss.ss_size);
	VALGRIND_STACK_DEREGISTER(context->valgrind_stack_id);
#endif
rollback_mmap:
	munmap(context->stack, context->stackSize);
rollback_new:
	delete context;
	context = NULL;
	zth_dbg(context, "[th %s] Cannot create context; %s (error %d)", pthreadId().c_str(), strerror(res), res);
	return res ? res : EINVAL;
}

void context_switch(Context* from, Context* to)
{
	zth_assert(from);
	zth_assert(to);

	if(sigsetjmp(from->env, Config::ContextSignals) == 0) {
		// Go switch away from here.
		siglongjmp(to->env, 1);
	}

	// Got back from somewhere else.
}

void context_destroy(Context* context)
{
	if(!context)
		return;
	
	if(context->stack) {
#ifdef ZTH_USE_VALGRIND
		VALGRIND_STACK_DEREGISTER(context->valgrind_stack_id);
#endif
		munmap(context->stack, context->stackSize);
		context->stack = NULL;
	}

	delete context;
	zth_dbg(context, "[th %s] Deleted context %p", pthreadId().c_str(), context);
}

} // namespace

