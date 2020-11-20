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
#include <libzth/init.h>
#include <libzth/regs.h>

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

#ifdef ZTH_ARM_HAVE_MPU
#  define ZTH_ARM_STACK_GUARD_BITS 5
#  define ZTH_ARM_STACK_GUARD_SIZE (1 << ZTH_ARM_STACK_GUARD_BITS)
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
#ifdef ZTH_ARM_HAVE_MPU
	union {
		// These are always the same.
		void* guard;
		void* stack_watermarked;
	};
#elif !defined(ZTH_CONTEXT_WINFIBER)
	void* stack_watermarked;
#endif
};

extern "C" void context_entry(Context* context) __attribute__((noreturn,used));

#ifdef ZTH_ARCH_ARM
// Using the fp reg alias does not seem to work well...
#  ifdef __thumb__
#    define REG_FP	"r7"
#  else
#    define REG_FP	"r11"
#  endif
#endif


////////////////////////////////////////////////////////////
// ARM MPU for stack guard

#if defined(ZTH_ARM_HAVE_MPU) && defined(ZTH_OS_BAREMETAL)
#  define REG_MPU_RBAR_ADDR_UNUSED ((unsigned)(((unsigned)-ZTH_ARM_STACK_GUARD_SIZE) >> 5))

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
static int stack_guard_region() {
	if(!Config::EnableStackGuard || Config::EnableThreads)
		return -1;

	static int region = 0;

	if(likely(region))
		return region;

	region = reg_mpu_type().field.dregion - 1;
	reg_mpu_rnr rnr;
	reg_mpu_rasr rasr;

	// Do not try to use region 0, leave at least one unused for other application purposes.
	for(; region > 0; --region) {
		rnr.field.region = region;
		rnr.write();

		rasr.read();
		if(!rasr.field.enable)
			// This one is free.
			break;
	}

	if(region > 0) {
		reg_mpu_rbar rbar;
		// Initialize at the top of the address space, so it is effectively unused.
		rbar.field.addr = REG_MPU_RBAR_ADDR_UNUSED;
		rbar.write();

		rasr.field.size = ZTH_ARM_STACK_GUARD_BITS - 1;
		rasr.field.xn = 0;
		rasr.field.ap = 0; // no access

		// Internal RAM should be normal, shareable, write-through.
		rasr.field.tex = 0;
		rasr.field.c = 1;
		rasr.field.b = 0;
		rasr.field.s = 1;

		rasr.field.enable = 1;
		rasr.write();

		reg_mpu_ctrl ctrl;
		if(!ctrl.field.enable) {
			// MPU was not enabled at all.
			ctrl.field.privdefena = 1; // Assume we are currently privileged.
			ctrl.field.enable = 1;
			ctrl.write();
			__isb();
		}
		zth_dbg(context, "Using MPU region %d as stack guard", region);
	} else {
		region = -1;
		zth_dbg(context, "Cannot find free MPU region for stack guard");
	}

	return region;
}

static void stack_guard(void* guard) {
	if(!Config::EnableStackGuard || Config::EnableThreads)
		return;

	int const region = stack_guard_region();
	if(unlikely(region < 0))
		return;

	// Must be aligned to guard size
	zth_assert(((uintptr_t)guard & (ZTH_ARM_STACK_GUARD_SIZE - 1)) == 0);

	reg_mpu_rbar rbar(0);

	if(likely(guard)) {
		rbar.field.addr = (uintptr_t)guard >> 5;
		register void* sp asm ("sp");
		zth_dbg(context, "Set MPU stack guard to %p (sp=%p)", guard, sp);
	} else {
		// Initialize at the top of the address space, so it is effectively unused.
		rbar.field.addr = REG_MPU_RBAR_ADDR_UNUSED;
		zth_dbg(context, "Disabled MPU stack guard");
	}

	rbar.field.valid = 1;
	rbar.field.region = region;
	rbar.write();

	if(Config::Debug) {
		// Only required if the settings should be in effect immediately.
		// However, it takes some time, which may not be worth it in a release build.
		// If skipped, it will take affect, but only after a few instructions, which
		// are already in the pipeline.
		__dsb();
		__isb();
	}
}
#pragma GCC diagnostic pop

#else
#  define stack_guard(...)
#endif


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
ZTH_INIT_CALL(context_global_init)

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

static void context_switch_impl(Context* UNUSED_PAR(from), Context* to) {
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
	__asm__ volatile (
		"ldr r0, [sp]\n"
		// Terminate stack frame list here, only for debugging purposes.
		"mov " REG_FP ", #0\n"
		"mov lr, #0\n"
		"push {" REG_FP ", lr}\n"
		"mov " REG_FP ", sp\n"
		"bl context_entry\n"
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

	// We only checked against newlib 2.4.0, 3.1.0, and 3.3.0.
#  if NEWLIB_VERSION < 20400L || NEWLIB_VERSION > 30300L
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
#if defined(__ARM_PCS_VFP) && !defined(__SOFTFP__)
	// newlib does not do saving/restoring of FPU registers.
	// We have to do it here ourselves.
	__asm__ volatile ("vpush {d8-d15}");
	#define arm_fpu_pop() __asm__ volatile ("vpop {d8-d15}")
#else
	#define arm_fpu_pop() do{}while(0)
#endif

	// As bare metal does not have signals, sigsetjmp and siglongjmp is not necessary.
	if(setjmp(from->env) == 0)
	{
#ifdef ZTH_ARM_USE_PSP
		// Use the PSP for fibers and the MSP for the worker.
		// So, if there is no stack in the Context, assume it is a worker, running
		// from MSP. Otherwise it is a fiber using PSP. Both are executed privileged.
		if(unlikely(!from->stack || !to->stack)) {
			int control;
			__asm__ ("mrs %0, control\n" : "=r"(control));

			if(to->stack)
				control |= 2; // set SPSEL to PSP
			else
				control &= ~2; // set SPSEL to MSR

			__asm__ volatile (
				"msr control, %1\n"
				"isb\n"
				"mov r0, %0\n"
				"movs r1, #1\n"
				"b longjmp\n"
				: : "r"(to->env), "r"(control) : "r0", "r1", "memory"
			);
		}
#endif
		longjmp(to->env, 1);
	}

	arm_fpu_pop();
#undef arm_fpu_pop
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
#elif defined(ZTH_ARM_HAVE_MPU)
		Config::EnableStackGuard ? (size_t)ZTH_ARM_STACK_GUARD_SIZE : sizeof(void*);
#else
		sizeof(void*);
#endif
	zth_assert(__builtin_popcount((unsigned int)pagesize) == 1);
	context->stackSize = (context->stackSize
#ifndef ZTH_STACK_SWITCH
		// If using stack switch, signals and interrupts are not executed on the fiber stack.
		+ MINSIGSTKSZ
#endif
		+ 3 * pagesize - 1) & ~(pagesize - 1);


	// Get a new stack.
	size_t stack_watermarked_size = 0;

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
		// Exclude the pages at both sides of the stack.
		context->stack_watermarked = stack->ss_sp = (char*)context->stack + pagesize;
		stack_watermarked_size = stack->ss_size = context->stackSize - 2 * pagesize;
	} else
#elif defined(ZTH_ARM_HAVE_MPU)
	if(Config::EnableStackGuard) {
		// Stack grows down, exclude the page at the end (lowest address).
		// The page must be aligned to its size.
		uintptr_t stackEnd = ((uintptr_t)context->stack + 2 * pagesize - 1) & ~(pagesize - 1);
		stack->ss_sp = (char*)stackEnd;
		stack->ss_size = context->stackSize - (stackEnd - (uintptr_t)context->stack);
		/* context->stack_watermarked = */ context->guard = (void*)(stackEnd - ZTH_ARM_STACK_GUARD_SIZE);
		stack_watermarked_size = stack->ss_size + ZTH_ARM_STACK_GUARD_SIZE;
	} else
#endif
	{
		context->stack_watermarked = stack->ss_sp = context->stack;
		stack_watermarked_size = stack->ss_size = context->stackSize;
	}
	stack->ss_flags = 0;

	if(Config::EnableStackWaterMark)
		stack_watermark_init(context->stack_watermarked, stack_watermarked_size);
	else
		context->stack_watermarked = NULL;

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
	stack_guard(context->guard);
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
#ifdef ZTH_ARM_HAVE_MPU
	context->guard = NULL;
#endif

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
	stack_guard(from->guard);
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


////////////////////////////////////////////////////////////
// Stack functions

namespace zth {

#define ZTH_STACK_WATERMARKER ((uint64_t)0x5a7468574de2889eULL) // Some nice UTF-8 text.

/*!
 * \brief Checks if the stack grows down or up.
 * \param reference a pointer to some object that is at the caller's stack
 * \return \c true if it grows down
 */
__attribute__((noinline,unused)) static bool stack_grows_down(void const* reference)
{
	int i = 42;
	return (uintptr_t)&i < (uintptr_t)reference;
}

/*!
 * \brief Helper to align the stack pointer for watermarking.
 * \param stack the pointer to the stack memory region. The pointer updated because of alignment.
 * \param sizeptr set to the memory where the size of the stack region is stored
 * \param size if not \c NULL, it determines the size of \p stack. It is updated because of alignment.
 */
static void stack_watermark_align(void*& stack, size_t*& sizeptr, size_t* size = NULL) {
	if(!stack)
		return;

	uintptr_t stack_ = (uintptr_t)stack;

#ifdef ZTH_ARM_HAVE_MPU
	// Skip guard
	if(Config::EnableStackGuard)
		stack_ = (stack_ + 2 * ZTH_ARM_STACK_GUARD_SIZE - 1) & ~(ZTH_ARM_STACK_GUARD_SIZE - 1);
#endif

	// Align to size_t
	stack_ = ((uintptr_t)stack_ + sizeof(size_t) - 1) & ~(uintptr_t)(sizeof(size_t) - 1);
	// The stack size is stored here.
	sizeptr = (size_t*)stack_;

	// Reduce stack to store the size.
	stack_ += sizeof(size_t);
	if(size)
		// Reduce remaining stack size.
		*size -= stack_ - (uintptr_t)stack;

	stack = (void*)stack_;
}

/*!
 * \brief Initialize the memory region for stack high water marking.
 * \ingroup zth_api_cpp_stack
 */
void stack_watermark_init(void* stack, size_t size) {
	if(!Config::EnableStackWaterMark || !stack || !size)
		return;

	// Most likely, but growing up stack is not implemented here (yet?)
	zth_assert(stack_grows_down(&size));

	size_t* sizeptr;
	stack_watermark_align(stack, sizeptr, &size);
	*sizeptr = size;

	for(size_t i = 0; i * sizeof(uint64_t) <= size - sizeof(uint64_t); i++)
		((uint64_t*)stack)[i] = ZTH_STACK_WATERMARKER;
}

/*!
 * \brief Return the size of the given stack region.
 * \details This does not take any #zth::stack_switch() calls into account.
 * \param stack the same value as supplied to #zth::stack_watermark_init
 * \ingroup zth_api_cpp_stack
 */
size_t stack_watermark_size(void* stack) {
	if(!Config::EnableStackWaterMark || !stack)
		return 0;

	size_t* sizeptr;
	stack_watermark_align(stack, sizeptr);
	return *sizeptr;
}

/*!
 * \brief Return the high water mark of the stack.
 * \details This does not take any #zth::stack_switch() calls into account.
 * \param stack the same value as supplied to #zth::stack_watermark_init
 * \ingroup zth_api_cpp_stack
 */
size_t stack_watermark_maxused(void* stack) {
	if(!Config::EnableStackWaterMark || !stack)
		return 0;

	size_t* sizeptr;
	stack_watermark_align(stack, sizeptr);
	size_t size = *sizeptr;

	size_t unused = 0;
	for(uint64_t* s = (uint64_t*)stack; unused < size / sizeof(uint64_t) && *s == ZTH_STACK_WATERMARKER; unused++, s++);

	return size - unused * sizeof(uint64_t);
}

/*!
 * \brief Return the remaining stack size that was never touched.
 * \details This does not take any #zth::stack_switch() calls into account.
 * \param stack the same value as supplied to #zth::stack_watermark_init
 * \ingroup zth_api_cpp_stack
 */
size_t stack_watermark_remaining(void* stack) {
	if(!Config::EnableStackWaterMark || !stack)
		return 0;

	size_t* sizeptr;
	stack_watermark_align(stack, sizeptr);
	size_t size = *sizeptr;

	size_t unused = 0;
	for(uint64_t* s = (uint64_t*)stack; unused < size / sizeof(uint64_t) && *s == ZTH_STACK_WATERMARKER; unused++, s++);

	return unused * sizeof(uint64_t);
}

/*!
 * \brief Return the high water mark of the stack of the given context.
 * \details This does not take any #zth::stack_switch() calls into account.
 */
size_t context_stack_usage(Context* UNUSED_PAR(context)) {
#ifdef ZTH_CONTEXT_WINFIBER
	return 0;
#else
	if(!Config::EnableStackWaterMark || !context)
		return 0;

	return stack_watermark_maxused(context->stack_watermarked);
#endif
}

} // namespace

#ifdef ZTH_STACK_SWITCH
#  ifdef ZTH_ARCH_ARM

__attribute__((naked,noinline)) static void* zth_stack_switch_msp(void* UNUSED_PAR(arg), UNUSED_PAR(void*(*f)(void*))) {
	__asm__ volatile (
		"push {r4, " REG_FP ", lr}\n"	// Save pc and variables
		".save {r4, " REG_FP ", lr}\n"
		"add " REG_FP ", sp, #0\n"

		"mrs r4, control\n"				// Save current control register
		"bic r3, r4, #2\n"				// Set to use MSP
		"msr control, r3\n"				// Enable MSP
		"isb\n"
		"and r4, r4, #2\n"				// r4 is set to only have the PSP bit

		// We are on MSP now.

		"push {" REG_FP ", lr}\n"		// Save frame pointer on MSP
		".setfp " REG_FP ", sp\n"
		"add " REG_FP ", sp, #0\n"

		"blx r1\n"						// Call f(arg)

		"mrs r3, control\n"				// Save current control register
		"orr r3, r3, r4\n"				// Set to use previous PSP setting
		"msr control, r3\n"				// Enable PSP setting
		"isb\n"

		// We are back on the previous stack.

		"pop {r4, " REG_FP ", pc}\n"	// Return to caller
	);
}

__attribute__((naked,noinline)) static void* zth_stack_switch_psp(void* UNUSED_PAR(arg), UNUSED_PAR(void*(*f)(void*)), void* UNUSED_PAR(sp)) {
	__asm__ volatile (
		"push {r4, " REG_FP ", lr}\n"	// Save pc and variables
		".save {r4, " REG_FP ", lr}\n"
		"add " REG_FP ", sp, #0\n"

		"mov r4, sp\n"					// Save previous stack pointer
		"mov sp, r2\n"					// Set new stack pointer
		"push {" REG_FP ", lr}\n"		// Save previous frame pointer on new stack
		".setfp " REG_FP ", sp\n"
		"add " REG_FP ", sp, #0\n"

		"blx r1\n"						// Call f(arg)

		"mov sp, r4\n"					// Restore previous stack
		"pop {r4, " REG_FP ", pc}\n"	// Return to caller
	);
}

void* zth_stack_switch(void* stack, size_t size, void*(*f)(void*), void* arg) {
	zth::Worker* worker = zth::Worker::currentWorker();
	void* res;

	if(!stack) {
		worker->contextSwitchDisable();
		res = zth_stack_switch_msp(arg, f);
		worker->contextSwitchEnable();
		return res;
	} else {
		void* sp = (void*)(((uintptr_t)stack + size) & ~(uintptr_t)7);
#ifdef ZTH_ARM_HAVE_MPU
		void* prevGuard = NULL;
		zth::Context* context = NULL;

		if(zth::Config::EnableStackGuard && size) {
			if(unlikely(!worker))
				goto noguard;

			zth::Fiber* fiber = worker->currentFiber();
			if(unlikely(!fiber))
				goto noguard;

			context = fiber->context();
			if(unlikely(!context))
				goto noguard;

			void* guard = (void*)(((uintptr_t)stack + 2 * ZTH_ARM_STACK_GUARD_SIZE - 1) & ~(ZTH_ARM_STACK_GUARD_SIZE - 1));
			prevGuard = context->guard;
			context->guard = guard;

			zth::stack_guard(guard);
		}

noguard:
#endif

		res = zth_stack_switch_psp(arg, f, sp);

#ifdef ZTH_ARM_HAVE_MPU
		if(prevGuard) {
			context->guard = prevGuard;
			zth::stack_guard(prevGuard);
		}
#endif
	}

	return res;
}
#  endif // ZTH_ARCH_ARM
#endif // ZTH_STACK_SWITCH

