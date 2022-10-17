/*
 * Zth (libzth), a cooperative userspace multitasking library.
 * Copyright (C) 2019-2022  Jochem Rutgers
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <libzth/macros.h>

#ifdef ZTH_OS_MAC
#	define _XOPEN_SOURCE
#endif

#include <libzth/context.h>
#include <libzth/context/context.h>
#include <libzth/worker.h>



////////////////////////////////////////////////////////////
// context functions

namespace zth {

/*!
 * \brief One-time context mechanism initialization.
 *
 * Normally, let #zth::Worker call this function.
 */
int context_init() noexcept
{
	zth_dbg(context, "[%s] Initialize", currentWorker().id_str());
	return Context::init();
}

/*!
 * \brief Final cleanup.
 *
 * Normally, let #zth::Worker call this function.
 */
void context_deinit() noexcept
{
	zth_dbg(context, "[%s] Deinit", currentWorker().id_str());
	Context::deinit();
}

/*!
 * \brief Create a context.
 *
 * Normally, let #zth::Fiber manage the context.
 *
 * \param context the variable that will receive the newly created context, when successful
 * \param attr the attributes used to create the context
 * \return 0 on success, otherwise an errno
 */
int context_create(Context*& context, ContextAttr const& attr) noexcept
{
	try {
		context = new Context(attr);
	} catch(std::bad_alloc const&) {
		zth_dbg(context, "[%s] Cannot create context; %s", currentWorker().id_str(),
			err(ENOMEM).c_str());
		return ENOMEM;
	} catch(...) {
		// Should not be thrown by new...
		return EAGAIN;
	}

	int res = 0;

	if((res = context->create())) {
		delete context;
		context = nullptr;
		zth_dbg(context, "[%s] Cannot create context; %s", currentWorker().id_str(),
			err(res).c_str());
		return res;
	}

	if(likely(attr.stackSize > 0)) {
#ifdef ZTH_CONTEXT_WINFIBER
		zth_dbg(context, "[%s] New context %p", currentWorker().id_str(), context);
#else
		Context::Stack const& stack = context->stackUsable();
		zth_dbg(context, "[%s] New context %p with stack: %p-%p", currentWorker().id_str(),
			context, stack.p, stack.p + stack.size - 1u);
#endif
	}

	return 0;
}

/*!
 * \brief Destroy and cleanup a context.
 *
 * Normally, let #zth::Fiber manage the context.
 */
void context_destroy(Context* context) noexcept
{
	if(!context)
		return;

	context->destroy();
	delete context;
	// NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDelete)
	zth_dbg(context, "[%s] Deleted context %p", currentWorker().id_str(), context);
}

/*!
 * \brief Perform context switch.
 *
 * Normally, let #zth::Fiber call this function, after scheduled by the
 * #zth::Worker.
 */
void context_switch(Context* from, Context* to) noexcept
{
	zth_assert(from);
	zth_assert(to);

#ifdef ZTH_ENABLE_ASAN
	zth_assert(to->alive());
	void* fake_stack = nullptr;
	__sanitizer_start_switch_fiber(
		from->alive() ? &fake_stack : nullptr, to->stackUsable().p, to->stackUsable().size);
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
size_t context_stack_usage(Context* context) noexcept
{
	if(!Config::EnableStackWaterMark || !context)
		return 0;

	Fiber* f = currentWorker().currentFiber();
	void* guard = nullptr;

	if(f && f->context() == context) {
		// Guards may be in place on the current stack. Disable it
		// while iterating it.
		guard = context->stackGuard(nullptr);
	}

	size_t res = stack_watermark_maxused(context->stackUsable().p);

	if(guard)
		context->stackGuard(guard);

	return res;
}

} // namespace zth

/*!
 * \brief Entry point of the fiber.
 *
 * This function is called by the actual context switching mechanism.  It will
 * call the entry function, as specified by the Context's attributes during
 * creation, which is #zth::Fiber::fiberEntry() by default.
 *
 * This function does not return. When the entry function returns, the context
 * is considered dead, the Worker's scheduler is invoked, and the context is
 * destroyed.
 */
void context_entry(zth::Context* context) noexcept
{
#ifdef ZTH_ENABLE_ASAN
	__sanitizer_finish_switch_fiber(nullptr, nullptr, nullptr);
#endif

	zth_assert(context);

	// Go execute the fiber.
	context->stackGuard();

	try {
		context->attr().entry(context->attr().arg);
	} catch(...) {
	}

	// Fiber has quit. Switch to another one.
	context->die();
	zth::yield();

	// We should never get here, otherwise the fiber wasn't dead...
	zth_abort("Returned to finished context");
}



////////////////////////////////////////////////////////////
// Stack functions

namespace zth {

#define ZTH_STACK_WATERMARKER ((uint64_t)0x5a7468574de2889eULL) // Some nice UTF-8 text.

/*!
 * \brief Helper to align the stack pointer for watermarking.
 * \param stack the pointer to the stack memory region. The pointer updated because of alignment.
 * \param sizeptr set to the memory where the size of the stack region is stored
 * \param size if not \c nullptr, it determines the size of \p stack. It is updated because of
 * alignment.
 */
static void stack_watermark_align(void*& stack, size_t*& sizeptr, size_t* size = nullptr) noexcept
{
	if(!stack)
		return;

	uintptr_t stack_ = reinterpret_cast<uintptr_t>(stack);

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
void stack_watermark_init(void* stack, size_t size) noexcept
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
size_t stack_watermark_size(void* stack) noexcept
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
size_t stack_watermark_maxused(void* stack) noexcept
{
	if(!Config::EnableStackWaterMark || !stack)
		return 0;

	size_t* sizeptr = nullptr;
	stack_watermark_align(stack, sizeptr);
	size_t size = *sizeptr;

	size_t unused = 0;

#ifdef ZTH_USE_VALGRIND
	VALGRIND_DISABLE_ADDR_ERROR_REPORTING_IN_RANGE(stack, size); // NOLINT
#endif

	for(uint64_t* s = static_cast<uint64_t*>(stack);
	    unused < size / sizeof(uint64_t) && *s == ZTH_STACK_WATERMARKER; unused++, s++)
		;

#ifdef ZTH_USE_VALGRIND
	VALGRIND_ENABLE_ADDR_ERROR_REPORTING_IN_RANGE(stack, size); // NOLINT
#endif

	return size - unused * sizeof(uint64_t);
}

/*!
 * \brief Return the remaining stack size that was never touched.
 * \details This does not take any #zth::stack_switch() calls into account.
 * \param stack the same value as supplied to #zth::stack_watermark_init
 * \ingroup zth_api_cpp_stack
 */
size_t stack_watermark_remaining(void* stack) noexcept
{
	return stack_watermark_size(stack) - stack_watermark_maxused(stack);
}

} // namespace zth

#ifdef ZTH_STACK_SWITCH
void* zth_stack_switch(void* stack, size_t size, void* (*f)(void*) noexcept, void* arg) noexcept
{
	zth::Worker* worker = zth::Worker::instance();

	if(!worker) {
noswitch:
		return f(arg);
	}

	zth::Fiber* fiber = worker->currentFiber();
	if(!fiber)
		goto noswitch;

	zth::Context* context = fiber->context();
	if(!context)
		goto noswitch;

	return context->stack_switch(stack, size, f, arg);
}
#endif // ZTH_STACK_SWITCH
