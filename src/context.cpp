#include <zth>

#include <csetjmp>
#include <csignal>
#include <sys/mman.h>

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
	sigjmp_buf* parent;
	sigset_t mask;
};

static sig_atomic_t volatile did_trampoline = 0;
static Context* volatile trampoline_context = NULL;

static void context_trampoline(int sig)
{
	// At this point, we are in the signal handler.
	
	if(sig != SIGUSR2 || did_trampoline) 
		// huh?
		return;

	// Put the context on the stack, as trampoline_context is about to change.
	Context* context = trampoline_context;

	if(setjmp(context->trampoline_env, 0) == 0)
	{
		// Return immediately to complete the signal handler.
		did_trampoline = 1;
		return;
	}

	// If we get here, we are not in the signal handler anymore, but in the actual fiber context.
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
		siglongjmp(*context->me);
	}

	// This is actually the first time we enter the fiber by the normal scheduler.
	// Go execute the fiber.
	context->attr.entry(context->attr.arg);
	// Fiber has quit. Switch to another one.
	yield();
	// We should never get here, otherwise the fiber wasn't dead...
	zth_abort("Returned to finished context");
}

int context_init()
{
	zth_dbg(context, "[th %s] Initialize", pthreadId().c_str());

	// Claim SIGUSR2 for context_create().
	int res = 0;
	sigset_t sigs;
	sigemptyset(&sigs);
	sigaddset(&sigs, SIGUSR2);
	if((res = pthread_sigmask(SIG_BLOCK, &sigs, NULL)))
		return res;
	
	struct sigaction sa;
	sa.sa_handler = &context_trampoline;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_ONSTACK;
	if(sigaction(SIGUSR2, &sa, NULL))
		return errno;

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
	sigaction(SIGUSR2, &sa, NULL);
}

int context_create(Context*& context, ContextAttr const& attr)
{
	int res = 0;
	context = new Context();
	context->stack = NULL;
	context->stackSize = attr.stackSize;
	context->attr = attr;

	if(attr->stackSize == 0)
	{
		// No stack, so no entry point can be accessed.
		// This is used if only a context is required, but not a fiber is to be started.
		context->stack = NULL;
		return 0;
	}
	
	// Let the new context inherit our signal mask.
	if((res = pthread_sigmask(0, NULL, &context->mask)))
		return res;

	size_t pagesize = getpagesize();
	zth_assert(__builtin_popcount(pagesize) == 1);
	context->stackSize = (context->stackSize + MINSIGSTKSZ + 3 * pagesize - 1) & ~(pagesize - 1);

	// Get a new stack.
	if((context->stack = mmap(NULL, context->stackSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, -1, 0)) == MAP_FAILED) {
		res = errno;
		goto rollback_new;
	}

	// Guard both ends of the stack.
	if( mprotect(context->stack, pagesize, PROT_NONE) ||
		mprotect((char*)context->stack + context->stackSize - 2 * pagesize, pagesize, PROT_NONE))
	{
		res = errno;
		goto rollback_mmap;
	}

	stack_t ss;
	ss.ss_sp = (char*)context->stack + pagesize;
	ss.ss_size = context->stackSize - 2 * pagesize;
	ss.ss_flags = 0;
	if(sigaltstack(&ss, NULL))
	{
		res = errno;
		goto rollback_mmap;
	}
	
	// If two worker threads are using signals, it is not clear which one was handled first.
	// Use a lock to make that clear.
	static pthread_mutex_t siglock = PTHREAD_MUTEX_INITIALIZER;
	if((res = pthread_mutex_lock(&siglock)))
		goto rollback_mmap;

	did_trampoline = 0;
	trampoline_context = context;

	res = pthread_kill(pthread_self(), SIGUSR2);

	if(!res)
	{
		sigset_t sigs;
		sigfillset(&sigs);
		sigdelset(&sigs, SIGUSR2);
		while(!did_trampoline)
			sigsuspend(&sigs);
	}

	trampoline_context = NULL;
	pthread_mutex_unlock(&siglock);

	if(res)
		goto rollback_mmap;

	// Ok, we have returned from the signal handler.
	// Now go back again and save a proper env.
	sigjmp_buf me;
	context->parent = &me;
	if(sigsetjmp(me, Config::ContextSignals))
		longjmp(context->trampoline_env);

	// context is setup properly and is ready for normal scheduling.
	context->parent = NULL;

	zth_dbg(context, "[th %s] New context %p", pthreadId().c_str(), context);
	return 0;

rollback_mmap:
	munmap(context->stack, context->stackSize);
rollback_new:
	delete context;
	context = NULL;
	zth_dbg(context, "[th %s] Cannot create context; error %d", pthreadId().c_str(), res);
	return res ? res : EINVAL;
}

void context_switch(Context* from, Context* to)
{
	zth_assert(from);
	zth_assert(to);

	if(sigsetjmp(from->env, Config::ContextSignals))
		// Got back from somewhere else.
		return;

	// Go switch away from here.
	siglongjmp(to->env, 1);

	// Impossible to get here.
}

void context_destroy(Context* context)
{
	if(!context)
		return;
	
	if(context->stack)
		free(context->stack);

	delete context;
	zth_dbg(context, "[th %s] Deleted context %p", pthreadId().c_str(), context);
}

} // namespace

