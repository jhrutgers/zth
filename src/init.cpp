/*
 * SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#include <libzth/init.h>

#include <libzth/async.h>
#include <libzth/config.h>
#include <libzth/util.h>
#include <libzth/worker.h>

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
struct zth_init_entry const* zth_init_head = nullptr;
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
struct zth_init_entry* zth_init_tail = nullptr;

zth_init_entry::zth_init_entry(void (*f_)(void), zth_init_entry const* next_) noexcept
	: f(f_)
	, next(next_)
{
	if(zth_init_tail)
		zth_init_tail->next = this;
	zth_init_tail = this;
	if(!zth_init_head)
		zth_init_head = this;
}

/*!
 * \brief Perform one-time global initialization of the Zth library.
 *
 * Initialization is only done once. It is safe to call it multiple times.
 *
 * The initialization sequence is initialized by #ZTH_INIT_CALL() and processed
 * in the same order as normal static initializers are executed.
 */
void zth_init()
{
	if(likely(!zth_init_head))
		// Already initialized.
		return;

	struct zth_init_entry const* p = zth_init_head;
	zth_init_head = nullptr;

	while(p) {
		// p might be overwritten during p->f(), copy next first.
		struct zth_init_entry const* p_next = p->next;
		if(p->f)
			p->f();
		p = p_next;
	}
}

/*!
 * \brief Start Zth given the given fiber function.
 *
 * It can be used instead of #zth_main() or #main().
 * In contrast, this function does not call #zth_preinit() and #zth_postdeinit().
 *
 * \return 0 when finished the fiber successfully, otherwise an errno
 */
int zth_run(void(fiber)(void*), void* arg)
{
	int res = 0;
	try {
		if(zth::Worker::instance())
			return EINVAL;

		zth::Worker w;
		zth::fiber_future<void> f = zth::fiber(fiber, arg);
		w.run();

		if(!f.get().valid()) {
			zth_dbg(thread, "zth_run() fiber did not exit normally");
			res = EFAULT;
		}
#ifdef __cpp_exceptions
	} catch(zth::errno_exception const& e) {
		zth_dbg(thread, "zth_run() caught exception with errno %d", e.code);
		res = e.code;
	} catch(std::exception const& e) {
		zth_dbg(thread, "zth_run() caught exception: %s", e.what());
		res = EFAULT;
	} catch(zth::exception const& e) {
		zth_dbg(thread, "zth_run() caught zth::exception");
		res = EFAULT;
#endif
	} catch(...) {
		zth_dbg(thread, "zth_run() caught unknown exception");
		res = EFAULT;
	}

	return res;
}

/*!
 * \brief Default main function that runs #main_fiber.
 *
 * Unless \c main() is defined in the application, this function is called by the default-provided
 * weak \c main().
 */
int zth_main(int argc, char** argv)
{
	zth_preinit();
	zth_dbg(thread, "main()");

	int res = EXIT_SUCCESS;
	try {
		zth::Worker w;
		zth::fiber_future<int> f =
			zth::fiber(main_fiber, argc, argv) << zth::setName(
				zth_config(EnableDebugPrint) || zth::Config::EnablePerfEvent
						|| zth::Config::EnableStackWaterMark
					? "main_fiber"
					: nullptr);

		w.run();

		if(!f.get().valid()) {
			zth_dbg(thread, "main_fiber() did not exit normally");
			res = EXIT_FAILURE;
		} else {
			res = *f;
		}
#ifdef __cpp_exceptions
	} catch(zth::errno_exception const& e) {
		zth_dbg(thread, "main() caught exception with errno %d", e.code);
		res = EXIT_FAILURE;
	} catch(std::exception const& e) {
		zth_dbg(thread, "main() caught exception: %s", e.what());
		res = EXIT_FAILURE;
	} catch(zth::exception const& e) {
		zth_dbg(thread, "main() caught zth::exception");
		res = EXIT_FAILURE;
#endif
	} catch(...) {
		zth_dbg(thread, "main() caught unknown exception");
		res = EXIT_FAILURE;
	}

	zth_dbg(thread, "main() returns %d", res);

	int res_post = zth_postdeinit();
	// cppcheck-suppress knownConditionTrueFalse
	if(res_post)
		res = res_post;

	return res;
}
