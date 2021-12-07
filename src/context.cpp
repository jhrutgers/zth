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

#include <libzth/context.h>

namespace zth {

int context_init() noexcept
{
	zth_dbg(context, "[%s] Initialize", currentWorker().id_str());
	return Context::init();
}

void context_deinit() noexcept
{
	zth_dbg(context, "[%s] Deinit", currentWorker().id_str());
	Context::deinit();
}

int context_create(Context*& context, ContextAttr const& attr) noexcept
{
	__try {
		context = new Context(attr);
	} __catch(std::bad_alloc const&) {
		zth_dbg(context, "[%s] Cannot create context; %s", currentWorker().id_str(), err(ENOMEM).c_str());
		return ENOMEM;
	} __catch(...) {
		// Should not be thrown by new...
		return EAGAIN;
	}

	int res = 0;

	if((res = context->create())) {
		delete context;
		context = nullptr;
		zth_dbg(context, "[%s] Cannot create context; %s", currentWorker().id_str(), err(res).c_str());
		return res;
	}

	if(likely(attr.stackSize > 0)) {
		stack_watermark_init(context->stackUsable().p, context->stackUsable().size);

#ifdef ZTH_CONTEXT_WINFIBER
		zth_dbg(context, "[%s] New context %p", currentWorker().id_str(), context);
#else
		Stack const& stack = context->stackUsable();
		zth_dbg(context, "[%s] New context %p with stack: %p-%p", currentWorker().id_str(),
			context, stack.p, stack.p + stack.size - 1u);
#endif
	}

	return 0;
}

void context_destroy(Context* context) noexcept
{
	if(!context)
		return;

	context->destroy();
	delete context;
	// NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDelete)
	zth_dbg(context, "[%s] Deleted context %p", currentWorker().id_str(), context);
}

void context_entry(Context* context) noexcept
{
#ifdef ZTH_ENABLE_ASAN
	__sanitizer_finish_switch_fiber(nullptr, nullptr, nullptr);
#endif

	zth_assert(context);

	// Go execute the fiber.
	contest->stack_guard();

	__try {
		context->attr.entry(context->attr.arg);
	} __catch(...) {
	}

	// Fiber has quit. Switch to another one.
	context->die();
	yield();

	// We should never get here, otherwise the fiber wasn't dead...
	zth_abort("Returned to finished context");
}

void context_switch(Context* from, Context* to) noexcept
{
	zth_assert(from);
	zth_assert(to);

#ifdef ZTH_ENABLE_ASAN
	zth_assert(to->alive());
	void* fake_stack = nullptr;
	__sanitizer_start_switch_fiber(from->alive() ? &fake_stack : nullptr, to->stackUsable().p, to->stackUsable().size);
#endif

	from->context_switch(*to);

#ifdef ZTH_ENABLE_ASAN
	__sanitizer_finish_switch_fiber(fake_stack, nullptr, nullptr);
#endif

	// Got back from somewhere else.
	from->stackGuard();
}

/*!
 * \brief Return the high water mark of the stack of the given context.
 * \details This does not take any #zth::stack_switch() calls into account.
 */
size_t context_stack_usage(Context* UNUSED_PAR(context))
{
#ifdef ZTH_CONTEXT_WINFIBER
	return 0;
#else
	if(!Config::EnableStackWaterMark || !context)
		return 0;

	return context->watermarkUsed();
#endif
}


} // namespace





////////////////////////////////////////////////////////////
// Stack functions

namespace zth {

#define ZTH_STACK_WATERMARKER ((uint64_t)0x5a7468574de2889eULL) // Some nice UTF-8 text.

/*!
 * \brief Helper to align the stack pointer for watermarking.
 * \param stack the pointer to the stack memory region. The pointer updated because of alignment.
 * \param sizeptr set to the memory where the size of the stack region is stored
 * \param size if not \c nullptr, it determines the size of \p stack. It is updated because of alignment.
 */
static void stack_watermark_align(void*& stack, size_t*& sizeptr, size_t* size = nullptr)
{
	if(!stack)
		return;

	uintptr_t stack_ = reinterpret_cast<uintptr_t>(stack);

#ifdef ZTH_ARM_HAVE_MPU
	// Skip guard
	if(Config::EnableStackGuard)
		stack_ = (stack_ + 2 * ZTH_ARM_STACK_GUARD_SIZE - 1) & ~(ZTH_ARM_STACK_GUARD_SIZE - 1);
#endif

	// Align to size_t
	stack_ = ((uintptr_t)stack_ + sizeof(size_t) - 1) & ~(uintptr_t)(sizeof(size_t) - 1);
	// The stack size is stored here.
	sizeptr = reinterpret_cast<size_t*>(stack_);

	// Reduce stack to store the size.
	stack_ += sizeof(size_t);
	if(size)
		// Reduce remaining stack size.
		*size -= stack_ - reinterpret_cast<uintptr_t>(stack);

	stack = reinterpret_cast<void*>(stack_);
}

/*!
 * \brief Initialize the memory region for stack high water marking.
 * \ingroup zth_api_cpp_stack
 */
void stack_watermark_init(void* stack, size_t size)
{
	if(!Config::EnableStackWaterMark || !stack || !size)
		return;

	// Most likely, but growing up stack is not implemented here (yet?)
	zth_assert(Context::stackGrowsDown(&size));

	size_t* sizeptr = nullptr;
	stack_watermark_align(stack, sizeptr, &size);
	*sizeptr = size;

	for(size_t i = 0; i * sizeof(uint64_t) <= size - sizeof(uint64_t); i++)
		static_cast<uint64_t*>(stack)[i] = ZTH_STACK_WATERMARKER;
}

/*!
 * \brief Return the size of the given stack region.
 * \details This does not take any #zth::stack_switch() calls into account.
 * \param stack the same value as supplied to #zth::stack_watermark_init
 * \ingroup zth_api_cpp_stack
 */
size_t stack_watermark_size(void* stack)
{
	if(!Config::EnableStackWaterMark || !stack)
		return 0;

	size_t* sizeptr = nullptr;
	stack_watermark_align(stack, sizeptr);
	return *sizeptr;
}

/*!
 * \brief Return the high water mark of the stack.
 * \details This does not take any #zth::stack_switch() calls into account.
 * \param stack the same value as supplied to #zth::stack_watermark_init
 * \ingroup zth_api_cpp_stack
 */
size_t stack_watermark_maxused(void* stack)
{
	if(!Config::EnableStackWaterMark || !stack)
		return 0;

	size_t* sizeptr = nullptr;
	stack_watermark_align(stack, sizeptr);
	size_t size = *sizeptr;

	size_t unused = 0;
	for(uint64_t* s = static_cast<uint64_t*>(stack); unused < size / sizeof(uint64_t) && *s == ZTH_STACK_WATERMARKER; unused++, s++);

	return size - unused * sizeof(uint64_t);
}

/*!
 * \brief Return the remaining stack size that was never touched.
 * \details This does not take any #zth::stack_switch() calls into account.
 * \param stack the same value as supplied to #zth::stack_watermark_init
 * \ingroup zth_api_cpp_stack
 */
size_t stack_watermark_remaining(void* stack)
{
	if(!Config::EnableStackWaterMark || !stack)
		return 0;

	size_t* sizeptr = nullptr;
	stack_watermark_align(stack, sizeptr);
	size_t size = *sizeptr;

	size_t unused = 0;
	for(uint64_t* s = static_cast<uint64_t*>(stack); unused < size / sizeof(uint64_t) && *s == ZTH_STACK_WATERMARKER; unused++, s++);

	return unused * sizeof(uint64_t);
}

/*!
 * \brief Return the high water mark of the stack of the given context.
 * \details This does not take any #zth::stack_switch() calls into account.
 */
size_t context_stack_usage(Context* context)
{
	if(!Config::EnableStackWaterMark || !context)
		return 0;

	return stack_watermark_maxused(context->stackUsable().p);
}

} // namespace

#ifdef ZTH_STACK_SWITCH
#  ifdef ZTH_ARCH_ARM

__attribute__((naked,noinline)) static void* zth_stack_switch_msp(void* UNUSED_PAR(arg), UNUSED_PAR(void*(*f)(void*)))
{
	__asm__ volatile (
		"push {r4, " REG_FP ", lr}\n"	// Save pc and variables
		".save {r4, " REG_FP ", lr}\n"
		"add " REG_FP ", sp, #0\n"

		"mrs r4, control\n"		// Save current control register
		"bic r3, r4, #2\n"		// Set to use MSP
		"msr control, r3\n"		// Enable MSP
		"isb\n"
		"and r4, r4, #2\n"		// r4 is set to only have the PSP bit

		// We are on MSP now.

		"push {" REG_FP ", lr}\n"	// Save frame pointer on MSP
		".setfp " REG_FP ", sp\n"
		"add " REG_FP ", sp, #0\n"

		"blx r1\n"			// Call f(arg)

		"mrs r3, control\n"		// Save current control register
		"orr r3, r3, r4\n"		// Set to use previous PSP setting
		"msr control, r3\n"		// Enable PSP setting
		"isb\n"

		// We are back on the previous stack.

		"pop {r4, " REG_FP ", pc}\n"	// Return to caller
	);
}

__attribute__((naked,noinline)) static void* zth_stack_switch_psp(void* UNUSED_PAR(arg), UNUSED_PAR(void*(*f)(void*)), void* UNUSED_PAR(sp))
{
	__asm__ volatile (
		"push {r4, " REG_FP ", lr}\n"	// Save pc and variables
		".save {r4, " REG_FP ", lr}\n"
		"add " REG_FP ", sp, #0\n"

		"mov r4, sp\n"			// Save previous stack pointer
		"mov sp, r2\n"			// Set new stack pointer
		"push {" REG_FP ", lr}\n"	// Save previous frame pointer on new stack
		".setfp " REG_FP ", sp\n"
		"add " REG_FP ", sp, #0\n"

		"blx r1\n"			// Call f(arg)

		"mov sp, r4\n"			// Restore previous stack
		"pop {r4, " REG_FP ", pc}\n"	// Return to caller
	);
}

void* zth_stack_switch(void* stack, size_t size, void*(*f)(void*), void* arg)
{
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
		void* prevGuard = nullptr;
		zth::Context* context = nullptr;

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
