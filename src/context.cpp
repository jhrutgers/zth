#include <zth>

#include <unistd.h>
#include <csetjmp>
#include <csignal>
#include <sys/mman.h>

#ifdef ZTH_USE_VALGRIND
#  include <valgrind/valgrind.h>
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

static pthread_key_t context_key;

static void context_global_init() __attribute__((constructor));
static void context_global_init() {
	// By default, block SIGUSR1 for the main/all thread(s).
	int res = 0;

	sigset_t sigs;
	sigemptyset(&sigs);
	sigaddset(&sigs, SIGUSR1);
	if((res = pthread_sigmask(SIG_BLOCK, &sigs, NULL)))
		goto error;

	if((res = pthread_key_create(&context_key, NULL)))
		goto error;

	return;
error:
	zth_abort("Cannot initialize signals; error %d", res);
}

static void context_trampoline(int sig)
{
	// At this point, we are in the signal handler.
	
	if(unlikely(sig != SIGUSR1))
		// Huh?
		return;

	// Put the context on the stack, as trampoline_context is about to change.
	Context* context = (Context*)pthread_getspecific(context_key);

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

	if(attr.stackSize == 0)
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

	stack_t ss;

	if(Config::EnableStackGuard) {
		// Guard both ends of the stack.
		if( mprotect(context->stack, pagesize, PROT_NONE) ||
			mprotect((char*)context->stack + context->stackSize - pagesize, pagesize, PROT_NONE))
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

#ifdef ZTH_USE_VALGRIND
	context->valgrind_stack_id = VALGRIND_STACK_REGISTER(ss.ss_sp, (char*)ss.ss_sp + ss.ss_size);
#endif

	ss.ss_flags = 0;
	if(sigaltstack(&ss, NULL))
	{
		res = errno;
		goto rollback_stack;
	}
	
	if((res = pthread_setspecific(context_key, context)))
		goto rollback_stack;

	// Ready to do the signal.
	sigset_t sigs;
	sigemptyset(&sigs);
	sigaddset(&sigs, SIGUSR1);
	if((res = pthread_sigmask(SIG_UNBLOCK, &sigs, NULL)))
		goto rollback_stack;

	// As we are not blocking SIGUSR1, we will get the signal.
	if((res = pthread_kill(pthread_self(), SIGUSR1)))
		goto rollback_stack;

	// Shouldn't take long...
	while(!context->did_trampoline)
		pthread_yield_np();

	// Block again.
	if((res = pthread_sigmask(SIG_BLOCK, &sigs, NULL)))
		return res;

	if(Config::Debug)
		pthread_setspecific(context_key, NULL);

	// Ok, we have returned from the signal handler.
	// Now go back again and save a proper env.
	sigjmp_buf me;
	context->parent = &me;
	if(sigsetjmp(me, Config::ContextSignals) == 0)
		longjmp(context->trampoline_env, 1);

	// context is setup properly and is ready for normal scheduling.
	if(Config::Debug)
		context->parent = NULL;

	// Reset sigaltstack.
	//...todo

	zth_dbg(context, "[th %s] New context %p", pthreadId().c_str(), context);
	return 0;

rollback_stack:
#ifdef ZTH_USE_VALGRIND
	VALGRIND_STACK_DEREGISTER(context->valgrind_stack_id);
#endif
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
		VALGRIND_STACK_DEREGISTER(context->valgrind_stack_id);
		munmap(context->stack, context->stackSize);
		context->stack = NULL;
	}

	delete context;
	zth_dbg(context, "[th %s] Deleted context %p", pthreadId().c_str(), context);
}

} // namespace

