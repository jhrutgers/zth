#ifndef ZTH_CONTEXT_ARCH_ARM_H
#define ZTH_CONTEXT_ARCH_ARM_H
/*
 * SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef ZTH_CONTEXT_CONTEXT_H
#  error This file must be included by libzth/context/context.h.
#endif

#ifdef __cplusplus

#  include "libzth/regs.h"
#  include "libzth/worker.h"

#  if defined(ZTH_ARM_HAVE_MPU) && defined(ZTH_OS_BAREMETAL) && !defined(ZTH_HAVE_MMAN)
#    define ZTH_ARM_DO_STACK_GUARD
#  endif

#  ifdef ZTH_ARM_DO_STACK_GUARD
#    define ZTH_ARM_STACK_GUARD_BITS 5
#    define ZTH_ARM_STACK_GUARD_SIZE (1 << ZTH_ARM_STACK_GUARD_BITS)
#  endif

// Using the fp reg alias does not seem to work well...
#  ifdef __thumb__
#    define REG_FP "r7"
#  else
#    define REG_FP "r11"
#  endif

namespace zth {
namespace impl {

template <typename Impl>
class ContextArch : public ContextBase<Impl> {
public:
	typedef ContextBase<Impl> base;
	using base::impl;
	using typename base::Stack;

protected:
	constexpr explicit ContextArch(ContextAttr const& attr) noexcept
		: base(attr)
#  ifdef ZTH_ARM_DO_STACK_GUARD
		, m_guard()
#  endif
	{}

public:
	// cppcheck-suppress duplInheritedMember
	static size_t pageSize() noexcept
	{
#  ifdef ZTH_ARM_DO_STACK_GUARD
		if(Config::EnableStackGuard)
			return (size_t)ZTH_ARM_STACK_GUARD_SIZE;
#  endif
		return std::max(sizeof(void*) * 2U, base::pageSize());
	}

	// cppcheck-suppress duplInheritedMember
	size_t calcStackSize(size_t size) noexcept
	{
#  ifndef ZTH_ARM_USE_PSP
		// If using PSP switch, signals and interrupts are not
		// executed on the fiber stack, but on the MSP.
		size += MINSIGSTKSZ;
#  endif
		if(Config::EnableStackGuard) {
#  if defined(ZTH_HAVE_MMAN)
			// Both ends of the stack are guarded using mprotect().
			size += impl().pageSize() * 2U;
#  elif defined(ZTH_ARM_DO_STACK_GUARD)
			// Only the end of the stack is protected using MPU.
			size += impl().pageSize();
#  endif
		}

		return size;
	}

	// cppcheck-suppress duplInheritedMember
	void stackAlign(Stack& stack) noexcept
	{
		base::stackAlign(stack);
		if(!stack.p || !stack.size)
			return;

#  ifdef ZTH_ARM_DO_STACK_GUARD
		if(Config::EnableStackGuard) {
			// Stack grows down, exclude the page at the end (lowest address).
			int dummy __attribute__((unused)) = 0;
			zth_assert(Impl::stackGrowsDown(&dummy));
			m_guard = stack.p;
			size_t const ps = impl().pageSize();
			// There should be enough room, as computed by calcStackSize().
			zth_assert(stack.size > ps);
			stack.p += ps;
			stack.size -= ps;
		}
#  endif

		// Stack must be double-word aligned.  This should already be
		// guaranteed by page alignment, but check anyway.
		zth_assert(((uintptr_t)stack.p & (sizeof(void*) * 2U - 1U)) == 0);
		zth_assert(((uintptr_t)stack.size & (sizeof(void*) * 2U - 1U)) == 0);
	}



	////////////////////////////////////////////////////////////
	// ARM MPU for stack guard

	__attribute__((always_inline)) static void* sp() noexcept
	{
		void* sp_reg;
		asm("mov %0, sp;" : "=r"(sp_reg));
		return sp_reg;
	}

#  ifdef ZTH_ARM_DO_STACK_GUARD
	int stackGuardInit() noexcept
	{
		// Don't do anything upon init, enable guards after context
		// switching.
		return 0;
	}

	void stackGuardDeinit() noexcept {}

	void* guard()
	{
		return m_guard;
	}

	void* setGuard(void* guard)
	{
		void* old = m_guard;
		m_guard = guard;
		return old;
	}

private:
#    define REG_MPU_RBAR_ADDR_UNUSED ((unsigned)(((unsigned)-ZTH_ARM_STACK_GUARD_SIZE) >> 5))

#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wconversion"

	static int stackGuardRegion() noexcept
	{
		if(!Config::EnableStackGuard || Config::EnableThreads)
			return -1;

		static int region = 0;

		if(likely(region))
			return region;

		region = reg_mpu_type().field.dregion - 1;
		reg_mpu_rnr rnr;
		reg_mpu_rasr rasr;

		// Do not try to use region 0, leave at least one unused for other application
		// purposes.
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

	static void stackGuardImpl(void* guard) noexcept
	{
		if(!Config::EnableStackGuard || Config::EnableThreads)
			return;

		int const region = stackGuardRegion();
		if(unlikely(region < 0))
			return;

		// Must be aligned to guard size
		zth_assert(((uintptr_t)guard & (ZTH_ARM_STACK_GUARD_SIZE - 1)) == 0);

		reg_mpu_rbar rbar(0);

		if(likely(guard)) {
			rbar.field.addr = (uintptr_t)guard >> 5;
			zth_dbg(context, "Set MPU stack guard to %p (sp=%p)", guard, sp());
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
			// However, it takes some time, which may not be worth it in a release
			// build. If skipped, it will take affect, but only after a few
			// instructions, which are already in the pipeline.
			__dsb();
			__isb();
		}
	}

public:
	void stackGuard() noexcept
	{
		// The guard is outside of the usable stack area.
		stackGuardImpl(m_guard);
	}

	void stackGuard(Stack const& stack) noexcept
	{
		// The guard is at the end of the usable stack area.
		stackGuardImpl(stack.p);
	}

	void* stackGuard(void* p) noexcept
	{
		void* prev = setGuard(p);
		stackGuard();

		if(!Config::Debug && !p) {
			// The guard is disabled. For non-debug builds, the
			// barriers are not in place by stackGuardImpl(). In
			// that case, do it here, to make sure that you can
			// rely on that the guard is really disabled when this
			// function returns.
			__dsb();
			__isb();
		}

		return prev;
	}

#    pragma GCC diagnostic pop
#  endif // ZTH_ARM_DO_STACK_GUARD



	////////////////////////////////////////////////////////////
	// jmp_buf fiddling for sjlj

#  ifdef ZTH_OS_BAREMETAL
private:
	static size_t get_lr_offset() noexcept
	{
		static size_t lr_offset = 0;

		if(unlikely(lr_offset == 0)) {
			// Not initialized yet.

			// env is filled with several registers. The number of registers depends on
			// the ARM architecture and Thumb mode, and compile settings for newlib. We
			// have to change the sp and lr registers in this env.  These follow other
			// caller-saved registers, but may be placed before VPU registers.
			//
			// Instead of hardcoding the offset, we try to find it dynamically. The sp
			// and lr registers are always next to each other. We assume here that the
			// sp is not part of the VPU registers, so this is our marker.

			static_assert(
				sizeof(jmp_buf) >= sizeof(void*) * 4U,
				"jmp_buf size too small to find lr offset");

			jmp_buf test_env = {};
			::setjmp(test_env);

			uintptr_t sp_ = 0;
			asm volatile("mov %0, sp;" : "=r"(sp_));
			zth_assert(sp_);

			// Note that jmp_buf may be an array type with other elements than void*.
			// NOLINTNEXTLINE
			uintptr_t const* test_env_ = reinterpret_cast<uintptr_t const*>(test_env);
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wsizeof-array-div"
			size_t sp_offset = sizeof(jmp_buf) / sizeof(void*) - 2U;
#    pragma GCC diagnostic pop
			for(; sp_offset > 0 && test_env_[sp_offset] != sp_; sp_offset--)
				;

			if(sp_offset == 0) {
				// Should not be possible.
				zth_abort("Invalid lr offset");
			}

			lr_offset = sp_offset + 1U;
		}

		return lr_offset;
	}

public:
	// cppcheck-suppress duplInheritedMember
	static void** sp(Stack const& stack) noexcept
	{
		int dummy __attribute__((unused)) = 0;
		zth_assert(Impl::stackGrowsDown(&dummy));
		// sp must be dword aligned
		return (void**)((uintptr_t)(stack.p + stack.size - sizeof(void*)) & ~(uintptr_t)7);
	}

	// cppcheck-suppress duplInheritedMember
	static void stack_push(void**& sp, void* p) noexcept
	{
		*sp = p;
		sp--;
	}

	// cppcheck-suppress duplInheritedMember
	static void set_sp(jmp_buf& env, void** sp) noexcept
	{
		// NOLINTNEXTLINE
		uintptr_t* env_ = reinterpret_cast<uintptr_t*>(env);
		env_[get_lr_offset() - 1U] = (uintptr_t)sp;
	}

	// cppcheck-suppress duplInheritedMember
	static void set_pc(jmp_buf& env, void* pc) noexcept
	{
		// NOLINTNEXTLINE
		uintptr_t* env_ = reinterpret_cast<uintptr_t*>(env);
		env_[get_lr_offset()] = (uintptr_t)pc; // lr = pc after return
	}

	// cppcheck-suppress duplInheritedMember
	__attribute__((naked)) static void context_trampoline_from_jmp_buf() noexcept
	{
		// The this pointer is saved on the stack, as r0 is not part of
		// the jmp_buf. Load it and call context_entry according to
		// normal ABI.

		// clang-format off
		asm volatile(
			"ldr r0, [sp]\n"
			// Terminate stack frame list here, only for debugging purposes.
			"mov " REG_FP ", #0\n"
			"mov lr, #0\n"
			"push {" REG_FP ", lr}\n"
			"mov " REG_FP ", sp\n"
			"bl context_entry\n"
		);
		// clang-format on
	}

#    if defined(ZTH_CONTEXT_SJLJ)
#      if defined(__ARM_FP) && !defined(__SOFTFP__) && NEWLIB_VERSION < 40300
#	define ZTH_CONTEXT_FPU
	inline __attribute__((always_inline)) void context_push_regs() noexcept
	{
		// newlib does not do saving/restoring of FPU registers.
		// We have to do it here ourselves.
		asm volatile("vstm %0, {d8-d15}" : : "r"(m_fpu_env));
	}

	inline __attribute__((always_inline)) void context_pop_regs() noexcept
	{
		asm volatile("vldm %0, {d8-d15}" : : "r"(m_fpu_env));
	}
#      endif

#      ifdef ZTH_ARM_USE_PSP
	inline __attribute__((always_inline)) void
	context_prepare_jmp(Impl& to, jmp_buf& env) noexcept
	{
		// Use the PSP for fibers and the MSP for the worker.
		// So, if there is no stack in the Context, assume it is a worker, running
		// from MSP. Otherwise it is a fiber using PSP. Both are executed privileged.
		if(unlikely(!impl().stackUsable() || !to.stackUsable())) {
			unsigned int control;
			asm("mrs %0, control\n" : "=r"(control));

			if(to.stackUsable())
				control |= 2U; // set SPSEL to PSP
			else
				control &= ~2U; // set SPSEL to MSR

			// As the SP changes, do not return from this function,
			// but actually do the longjmp.
			asm volatile(
				"msr control, %1\n"
				"isb\n"
				"mov r0, %0\n"
				"movs r1, #1\n"
				"b longjmp\n"
				:
				: "r"(env), "r"(control)
				: "r0", "r1", "memory");
		}
	}
#      endif
#    endif // ZTH_CONTEXT_SJLJ
#  endif   // ZTH_OS_BAREMETAL

#  ifdef ZTH_STACK_SWITCH
private:
	__attribute__((naked, noinline)) static void*
	stack_switch_msp(void* UNUSED_PAR(arg), UNUSED_PAR(void* (*f)(void*) noexcept)) noexcept
	{
		// clang-format off
		asm volatile(
			"push {r4, " REG_FP ", lr}\n"	// Save pc and variables
#ifdef __cpp_exceptions
			".save {r4, " REG_FP ", lr}\n"
#endif
			"add " REG_FP ", sp, #0\n"

			"mrs r4, control\n"		// Save current control register
			"bic r3, r4, #2\n"		// Set to use MSP
			"msr control, r3\n"		// Enable MSP
			"isb\n"
			"and r4, r4, #2\n"		// r4 is set to only have the PSP bit

			// We are on MSP now.

			"push {" REG_FP ", lr}\n"	// Save frame pointer on MSP
#ifdef __cpp_exceptions
			".setfp " REG_FP ", sp\n"
#endif
			"add " REG_FP ", sp, #0\n"

			"blx r1\n"			// Call f(arg)

			"mrs r3, control\n"		// Save current control register
			"orr r3, r3, r4\n"		// Set to use previous PSP setting
			"msr control, r3\n"		// Enable PSP setting
			"isb\n"

			// We are back on the previous stack.

			"pop {r4, " REG_FP ", pc}\n"	// Return to caller
		);
		// clang-format on
	}

	__attribute__((naked, noinline)) static void* stack_switch_psp(
		void* UNUSED_PAR(arg), UNUSED_PAR(void* (*f)(void*) noexcept),
		void* UNUSED_PAR(sp)) noexcept
	{
		// clang-format off
		asm volatile(
			"push {r4, " REG_FP ", lr}\n"	// Save pc and variables
#ifdef __cpp_exceptions
			".save {r4, " REG_FP ", lr}\n"
#endif
			"add " REG_FP ", sp, #0\n"

			"mov r4, sp\n"			// Save previous stack pointer
			"mov sp, r2\n"			// Set new stack pointer
			"push {" REG_FP ", lr}\n"	// Save previous frame pointer on new stack
#ifdef __cpp_exceptions
			".setfp " REG_FP ", sp\n"
#endif
			"add " REG_FP ", sp, #0\n"

			"blx r1\n"			// Call f(arg)

			"mov sp, r4\n"			// Restore previous stack
			"pop {r4, " REG_FP ", pc}\n"	// Return to caller
		);
		// clang-format on
	}

public:
	void* stack_switch(void* stack, size_t size, void* (*f)(void*) noexcept, void* arg) noexcept
	{
		if(!stack) {
			zth::Worker* worker = zth::Worker::instance();

			if(worker)
				worker->contextSwitchDisable();

			void* res = stack_switch_msp(arg, f);

			if(worker)
				worker->contextSwitchEnable();

			return res;
		} else {
			void* sp = (void*)(((uintptr_t)stack + size) & ~(uintptr_t)7);
#    ifdef ZTH_ARM_HAVE_MPU
			void* prevGuard = nullptr;

			if(zth::Config::EnableStackGuard && size >= ZTH_ARM_STACK_GUARD_SIZE * 2) {
				void* guard = (void*)(((uintptr_t)stack
						       + 2 * ZTH_ARM_STACK_GUARD_SIZE - 1)
						      & ~(ZTH_ARM_STACK_GUARD_SIZE - 1));
				prevGuard = setGuard(guard);
				stackGuard();
			}
#    endif

			void* res = stack_switch_psp(arg, f, sp);

#    ifdef ZTH_ARM_HAVE_MPU
			if(prevGuard) {
				setGuard(prevGuard);
				stackGuard();
			}
#    endif
			return res;
		}
	}
#  endif // ZTH_STACK_SWITCH

private:
#  ifdef ZTH_ARM_HAVE_MPU
	// clang-format off
	ZTH_REG_DEFINE(uint32_t, reg_mpu_type, 0xE000ED90,
		reserved1 : 8,
		region : 8,
		dregion : 8,
		reserved2 : 7,
		separate : 1)

	ZTH_REG_DEFINE(uint32_t, reg_mpu_ctrl, 0xE000ED94,
		reserved : 29,
		privdefena : 1,
		hfnmiena : 1,
		enable : 1)

	ZTH_REG_DEFINE(uint32_t, reg_mpu_rnr, 0xE000ED98,
		reserved : 24,
		region : 8)

	ZTH_REG_DEFINE(uint32_t, reg_mpu_rbar, 0xE000ED9C,
		addr : 27,
		valid : 1,
		region : 4)

	ZTH_REG_DEFINE(uint32_t, reg_mpu_rasr, 0xE000EDA0,
		reserved1 : 3,
		xn : 1,
		reserved2 : 1,
		ap : 3,
		reserved3 : 2,
		tex : 3,
		s : 1,
		c : 1,
		b : 1,
		srd : 8,
		reserved4 : 2,
		size : 5,
		enable : 1)
	// clang-format on
#  endif // ZTH_ARM_HAVE_MPU

private:
#  ifdef ZTH_ARM_DO_STACK_GUARD
	void* m_guard;
#  endif
#  ifdef ZTH_CONTEXT_FPU
	double m_fpu_env[8];
#  endif
};

} // namespace impl
} // namespace zth

#  if !defined(__cpp_exceptions) && defined(__NEWLIB__)
// It seems that newlib depends on this function by setjmp.o, even when exceptions are disabled. It
// would pull in all unwinding code, which is dead code anyway.  Forcibly define the symbol here to
// prevent that.
EXTERN_C void __aeabi_unwind_cpp_pr0()
{
	zth_abort("No exceptions");
}
#  endif

#endif // __cplusplus
#endif // ZTH_CONTEXT_ARCH_ARM_H
