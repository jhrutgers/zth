#ifndef ZTH_CONTEXT_CONTEXT_H
#define ZTH_CONTEXT_CONTEXT_H
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

/*!
 * \file
 * \brief Context switch mechanism implementation.
 *
 * This file is a private header file, only used by src/context.cpp.  Do not
 * include it otherwise.
 */

#ifdef __cplusplus

#	include "libzth/macros.h"

#	include "libzth/util.h"
#	include "libzth/allocator.h"
#	include "libzth/context.h"
#	include "libzth/worker.h"

#	include <setjmp.h>
#	include <unistd.h>

#	ifndef ZTH_OS_WINDOWS
#		include <signal.h>
#	endif

#	ifdef ZTH_HAVE_MMAN
#		include <sys/mman.h>
#		ifndef MAP_STACK
#			define MAP_STACK 0
#		endif
#	endif

#	ifdef ZTH_HAVE_VALGRIND
#		include <valgrind/memcheck.h>
#	endif

#	ifdef ZTH_ENABLE_ASAN
#		include <sanitizer/common_interface_defs.h>
#	endif

namespace zth {
namespace impl {

/*!
 * \brief Base class of the Context.
 *
 * The Context consists of three layers:
 *
 * - ContextBase: this class, which implements the basic functionality
 * - ContextArch: a class that inherits ContextBase, which does CPU-specific
 *   overrides and implementations of the ContextBase.
 * - Context: a class that inherits ContextArch, with the specific context
 *   switch approach.
 *
 * Although the classes inherit each other and override methods, they are not
 * virtual.  All implementation is done in the header. The lowest subclass is
 * passed as template parameter to the parents, which allows invoking the most
 * specific methods.  This allows fully inlining and optimizing the functions.
 */
template <typename Impl>
class ContextBase {
	ZTH_CLASS_NOCOPY(ContextBase)
protected:
	constexpr explicit ContextBase(ContextAttr const& attr) noexcept
		: m_attr(attr)
#	ifdef ZTH_ENABLE_ASAN
		, m_alive(true)
#	endif
	{}

	/*!
	 * \brief Dtor.
	 */
	~ContextBase()
	{
		// Make sure destroy() was called before.
		zth_assert(m_stack.p == nullptr);
	}

	/*!
	 * \brief Return Impl \c this.
	 */
	Impl& impl() noexcept
	{
		return static_cast<Impl&>(*this);
	}

	/*!
	 * \brief Return Impl \c this.
	 */
	Impl const& impl() const noexcept
	{
		return static_cast<Impl const&>(*this);
	}

public:
	/*!
	 * \brief Stack information.
	 */
	struct Stack {
		constexpr Stack(void* p, size_t size) noexcept
			: p(static_cast<char*>(p))
			, size(size)
		{}

		constexpr explicit Stack(size_t size = 0) noexcept
			: p()
			, size(size)
		{}

		constexpr operator bool() const noexcept
		{
			return p != nullptr && size > 0;
		}

		char* p;
		size_t size;
	};

	/*!
	 * \brief One-time system initialization.
	 */
	static int init() noexcept
	{
		return 0;
	}

	/*!
	 * \brief Final system cleanup.
	 */
	static void deinit() noexcept {}

	/*!
	 * \brief Return the context attributes, requested by the user.
	 */
	ContextAttr& attr() noexcept
	{
		return m_attr;
	}

	/*!
	 * \brief Return the context attributes, requested by the user.
	 */
	ContextAttr const& attr() const noexcept
	{
		return m_attr;
	}

	/*!
	 * \brief Create context.
	 */
	int create() noexcept
	{
		int res = 0;

		m_stack = Stack(attr().stackSize);
		m_stackUsable = Stack();

		// Allocate stack.
		if((res = impl().initStack(m_stack, m_stackUsable)))
			return res;

		// Apply guards.
		impl().valgrindRegister();

		if(unlikely((res = impl().stackGuardInit()))) {
			// Uh, oh...
			impl().deallocStack(m_stack);
			return res;
		}

		return 0;
	}

	/*!
	 * \brief Destroy and cleanup context.
	 */
	void destroy() noexcept
	{
		impl().stackGuardDeinit();
		impl().valgrindDeregister();
		impl().deinitStack(m_stack);
		m_stackUsable = m_stack = Stack();
	}

	/*!
	 * \brief Allocate and initialize stack.
	 */
	int initStack(Stack& stack, Stack& usable) noexcept
	{
		stack.size = impl().calcStackSize(stack.size);

		size_t const page = impl().pageSize();
		zth_assert(__builtin_popcount((unsigned int)page) == 1);

		// Round up to full pages.
		stack.size = (stack.size + page - 1u) & ~(page - 1u);

		// Add another page size to allow arbitrary allocation location.
		stack.size += page;

		if(unlikely(!(stack.p = (char*)impl().allocStack(stack.size))))
			return errno;

		// Compute usable offsets.
		usable = stack;
		impl().stackAlign(usable);

		stack_watermark_init(usable.p, usable.size);
		return 0;
	}

	/*!
	 * \brief Deinit and free stack.
	 */
	void deinitStack(Stack& stack) noexcept
	{
		impl().deallocStack(stack);
	}

	/*!
	 * \brief Get system's page size.
	 */
	static size_t pageSize() noexcept
	{
		static size_t pz =
#	ifdef ZTH_HAVE_MMAN
			(size_t)sysconf(_SC_PAGESIZE);
#	else
			sizeof(void*);
#	endif
		return pz;
	}

	/*!
	 * \brief Return the stack address.
	 */
	Stack const& stack() const noexcept
	{
		return m_stack;
	}

	/*!
	 * \brief Return the start of the actual usable stack.
	 */
	Stack const& stackUsable() const noexcept
	{
		return m_stackUsable;
	}

	/*!
	 * \brief Compute the stack size, given the requested user size and current configuration.
	 */
	size_t calcStackSize(size_t size) noexcept
	{
#	ifndef ZTH_OS_WINDOWS
		size += MINSIGSTKSZ;
#	endif

#	ifdef ZTH_HAVE_MMAN
		if(Config::EnableStackGuard)
			// Both ends of the stack are guarded using mprotect().
			size += pageSize() * 2;
#	endif

		return size;
	}

	/*!
	 * \brief Allocate requested size of stack memory.
	 *
	 * Alignment does not have to be enforced.
	 *
	 * \param size the requested size, which is a multiple of #pageSize().
	 */
	void* allocStack(size_t size) noexcept
	{
		void* stack = nullptr;

#	ifdef ZTH_HAVE_MMAN
		// NOLINTNEXTLINE(hicpp-signed-bitwise,cppcoreguidelines-pro-type-cstyle-cast)
		stack =
			mmap(nullptr, size, PROT_READ | PROT_WRITE,
			     MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, -1, 0);

		if(unlikely(stack == MAP_FAILED))
			return nullptr;
#	else
		if(unlikely((stack = (void*)allocate_noexcept<char>(size)) == nullptr)) {
			errno = ENOMEM;
			return nullptr;
		}
#	endif

		return stack;
	}

	/*!
	 * \brief Frees the previously allocated stack.
	 */
	void deallocStack(Stack& stack) noexcept
	{
		if(!stack.p)
			return;

#	ifdef ZTH_HAVE_MMAN
		munmap(stack.p, stack.size);
#	else
		deallocate(static_cast<char*>(stack.p), stack.size);
#	endif

		stack.p = nullptr;
	}

	/*!
	 * \brief Compute and modify the stack alignment and size, within the allocated space.
	 */
	void stackAlign(Stack& stack) noexcept
	{
		if(!stack.p || !stack.size)
			// Nothing to align.
			return;

		size_t const ps = pageSize();

		// Align to page size.
		uintptr_t stack_old = (uintptr_t)stack.p;
		// calcStackSize() should have taken care of this.
		zth_assert(stack.size > ps);

		// The stack should not wrap.
		zth_assert(stack_old + stack.size > stack_old);

		uintptr_t stack_new = ((uintptr_t)(stack_old + ps - 1u) & ~(ps - 1u));
		stack.size = (stack.size - (stack_new - stack_old)) & ~(ps - 1u);

#	ifdef ZTH_HAVE_MMAN
		if(Config::EnableStackGuard) {
			// Do not use the pages at both sides.
			zth_assert(stack.size > ps * 2u);
			stack_new += ps;
			stack.size -= ps * 2u;
		}
#	endif

		zth_assert(stack.size > 0);
		stack.p = (char*)stack_new;
	}

	/*!
	 * \brief Register the current stack to valgrind.
	 */
	void valgrindRegister() noexcept
	{
#	ifdef ZTH_USE_VALGRIND
		if(!m_stackUsable.p)
			return;

		m_valgrind_stack_id = VALGRIND_STACK_REGISTER(
			m_stackUsable.p, m_stackUsable.p + m_stackUsable.size - 1u);

		if(RUNNING_ON_VALGRIND)
			zth_dbg(context, "[%s] Stack of context %p has Valgrind id %u",
				currentWorker().id_str(), this, m_valgrind_stack_id);
#	endif
	}

	/*!
	 * \brief Deregister the current stack from valgrind.
	 */
	void valgrindDeregister() noexcept
	{
#	ifdef ZTH_USE_VALGRIND
		VALGRIND_STACK_DEREGISTER(m_valgrind_stack_id);
		m_valgrind_stack_id = 0;
#	endif
	}

	/*!
	 * \brief Initialize guards around the stack memory.
	 */
	int stackGuardInit() noexcept
	{
#	ifdef ZTH_HAVE_MMAN
		if(Config::EnableStackGuard) {
			if(!m_stackUsable.p)
				return 0;

			size_t const ps = impl().pageSize();
			// Guard both ends of the stack.
			if(unlikely(mprotect(m_stackUsable.p - ps, ps, PROT_NONE)))
				return errno;
			if(unlikely(mprotect(m_stackUsable.p + m_stackUsable.size, ps, PROT_NONE)))
				return errno;
		}
#	endif
		return 0;
	}

	/*!
	 * \brief Release the guards around the memory.
	 */
	void stackGuardDeinit() noexcept
	{
		// Automatically released when mprotected area is munmapped.
	}

	/*!
	 * \brief Checks if the stack grows down or up.
	 * \param reference a pointer to some object that is at the caller's stack
	 * \return \c true if it grows down
	 */
	__attribute__((noinline)) static bool stackGrowsDown(void const* reference)
	{
		int i = 42;
		return reinterpret_cast<uintptr_t>(&i) < reinterpret_cast<uintptr_t>(reference);
	}

	/*!
	 * \brief Configure the guard for the current fiber.
	 *
	 * This is called after every context switch.
	 */
	void stackGuard() noexcept
	{
		impl().stackGuard(stackUsable());
	}

	/*!
	 * \brief Configure the guard for the given stack.
	 */
	void stackGuard(Stack const& UNUSED_PAR(stack)) noexcept {}

	/*!
	 * \brief Perform a context switch.
	 */
	void context_switch(Context& to) noexcept;

	/*!
	 * \brief Get the initial stack pointer for the given stack.
	 */
	static void** sp(Stack const& stack) noexcept;

	/*!
	 * \brief Push data into the stack.
	 */
	static void stack_push(void**& sp, void* p) noexcept;

	/*!
	 * \brief Set the stack pointer in a \c jmp_buf.
	 */
	static void set_sp(jmp_buf& env, void** sp) noexcept;

	/*!
	 * \brief Set the program counter in a \c jmp_buf.
	 */
	static void set_pc(jmp_buf& env, void* sp) noexcept;

	/*!
	 * \brief Entry point to jump to from a (sig)jmp_buf.
	 *
	 * As the pc is probably set via #set_pc(), which is not really
	 * supported by the standard libary, you may also have to implement
	 * this function a bit more careful.
	 *
	 * It should call context_entry in the end.
	 */
	static void context_trampoline_from_jmp_buf();

	/*!
	 * \brief Pre-sjlj context saving.
	 */
	void context_push_regs() noexcept {}

	/*!
	 * \brief Post-sjlj context restoring.
	 */
	void context_pop_regs() noexcept {}

	/*!
	 * \brief Pre-sjlj jump.
	 */
	void context_prepare_jmp(Context& UNUSED_PAR(to)) noexcept {}

	/*!
	 * \brief Flag fiber as died after it returned from \c context_entry().
	 */
	void die() noexcept
	{
#	ifdef ZTH_ENABLE_ASAN
		m_alive = false;
#	endif
	}

	/*!
	 * \brief Check if fiber is still running.
	 */
	bool alive() const noexcept
	{
#	ifdef ZTH_ENABLE_ASAN
		return m_alive;
#	else
		return true;
#	endif
	}

private:
	/*! \brief Pre-set attributes. */
	ContextAttr m_attr;
	/*! \brief Allocated stack memory. */
	Stack m_stack;
	/*! \brief Actually used stack memory, within #m_stack. */
	Stack m_stackUsable;
#	ifdef ZTH_USE_VALGRIND
	/*! \brief valgrind ID of this context's stack. */
	unsigned int m_valgrind_stack_id;
#	endif
#	ifdef ZTH_ENABLE_ASAN
	/*! \brief Flag if the fiber is still running. */
	bool m_alive;
#	endif
};

} // namespace impl
} // namespace zth

#	if defined(ZTH_ARCH_ARM)
#		include "libzth/context/arch_arm.h"
#	else
#		include "libzth/context/arch_generic.h"
#	endif

extern "C" void context_entry(zth::Context* context) noexcept __attribute__((noreturn, used));

#	if defined(ZTH_CONTEXT_SIGALTSTACK)
#		include "libzth/context/sigaltstack.h"
#	elif defined(ZTH_CONTEXT_SJLJ)
#		include "libzth/context/sjlj.h"
#	elif defined(ZTH_CONTEXT_UCONTEXT)
#		include "libzth/context/ucontext.h"
#	elif defined(ZTH_CONTEXT_WINFIBER)
#		include "libzth/context/winfiber.h"
#	else
#		error Unknown context switching approach.
#	endif

#endif // __cplusplus
#endif // ZTH_CONTEXT_CONTEXT_H
