#ifndef ZTH_WORKER_H
#define ZTH_WORKER_H
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

#ifdef __cplusplus
#include <libzth/config.h>
#include <libzth/context.h>
#include <libzth/fiber.h>
#include <libzth/list.h>
#include <libzth/waiter.h>
#include <libzth/perf.h>
#include <libzth/init.h>
#include <libzth/allocator.h>

#include <time.h>
#include <limits>
#include <cstring>

namespace zth {

	void sigchld_check();

	class Worker;

	/*!
	 * \ingroup zth_api_cpp_fiber
	 */
	class Worker : public UniqueID<Worker>, public ThreadLocalSingleton<Worker> {
		ZTH_CLASS_NEW_DELETE(Worker)
	public:
		Fiber* currentFiber() const noexcept { return m_currentFiber; }

		Worker()
			: UniqueID("Worker")
			, m_currentFiber()
			, m_workerFiber(&dummyWorkerEntry)
			, m_waiter(*this)
			, m_disableContextSwitch()
		{
			zth_init();

			int res = 0;
			zth_dbg(worker, "[%s] Created", id_str());

			if(instance() != this)
				zth_abort("Only one worker allowed per thread");

			if((res = context_init()))
				goto error;

			m_workerFiber.setName("zth::Worker");
			m_workerFiber.setStackSize(0); // no stack
			if((res = m_workerFiber.init()))
				goto error;

			if((res = perf_init()))
				goto error;

			perf_event(PerfEvent<>(m_workerFiber));

			if((res = waiter().run()))
				goto error;

			return;

		error:
			zth_abort("Cannot create Worker; %s (error %d)", strerror(res), res);
		}

		virtual ~Worker() {
			zth_dbg(worker, "[%s] Destruct", id_str());

			while(!m_suspendedQueue.empty()) {
				Fiber& f = m_suspendedQueue.front();
				resume(f);
				f.kill();
			}
			while(!m_runnableQueue.empty()) {
				m_runnableQueue.front().kill();
				cleanup(m_runnableQueue.front());
			}

			perf_deinit();
			context_deinit();
		}

		Waiter& waiter() { return m_waiter; }

		void add(Fiber* fiber) {
			zth_assert(fiber);
			zth_assert(fiber->state() != Fiber::Waiting); // We don't manage 'Waiting' here.
			if(unlikely(fiber->state() == Fiber::Suspended)) {
				m_suspendedQueue.push_back(*fiber);
				zth_dbg(worker, "[%s] Added suspended %s", id_str(), fiber->id_str());
			} else {
				m_runnableQueue.push_front(*fiber);
				zth_dbg(worker, "[%s] Added runnable %s", id_str(), fiber->id_str());
			}
			dbgStats();
		}

		Worker& operator<<(Fiber* fiber) {
			add(fiber);
			return *this;
		}

		void contextSwitchEnable(bool enable = false) {
			if(enable) {
				zth_assert(m_disableContextSwitch > 0);
				m_disableContextSwitch--;
			} else
				m_disableContextSwitch++;
		}

		void contextSwitchDisable() {
			contextSwitchEnable(false);
		}

		bool contextSwitchEnabled() const {
			return m_disableContextSwitch == 0;
		}

		void release(Fiber& fiber) {
			if(unlikely(fiber.state() == Fiber::Suspended)) {
				m_suspendedQueue.erase(fiber);
				zth_dbg(worker, "[%s] Removed %s from suspended queue", id_str(), fiber.id_str());
			} else {
				if(&fiber == m_currentFiber)
					m_runnableQueue.rotate(*fiber.listNext());

				m_runnableQueue.erase(fiber);
				zth_dbg(worker, "[%s] Removed %s from runnable queue", id_str(), fiber.id_str());
			}
			dbgStats();
		}

		bool schedule(Fiber* preferFiber = NULL, Timestamp const& now = Timestamp::now()) {
			if(unlikely(!contextSwitchEnabled()))
				// Don't switch, immediately continue.
				return true;

			if(preferFiber)
				zth_dbg(worker, "[%s] Schedule to %s", id_str(), preferFiber->id_str());
			else
				zth_dbg(worker, "[%s] Schedule", id_str());

			dbgStats();

			// Check if fiber is within the runnable queue.
			zth_assert(!preferFiber ||
				preferFiber == &m_workerFiber ||
				m_runnableQueue.contains(*preferFiber));

			if(unlikely(!runEnd().isNull() && runEnd().isBefore(now))) {
				// Stop worker and return to its run1() call.
				zth_dbg(worker, "[%s] Time is up", id_str());
				preferFiber = &m_workerFiber;
			}

			Fiber* nextFiber = preferFiber;
			bool didSchedule = false;
		reschedule:
			if(likely(!nextFiber))
			{
				if(likely(!m_runnableQueue.empty()))
					// Use first of the queue.
					nextFiber = &m_runnableQueue.front();
				else
					// No fiber to switch to.
					nextFiber = &m_workerFiber;
			}

			zth_assert(nextFiber == &m_workerFiber || nextFiber->listPrev());
			zth_assert(nextFiber == &m_workerFiber || nextFiber->listNext());

			{
				// NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDelete)
				Fiber* prevFiber = m_currentFiber;
				m_currentFiber = nextFiber;

				if(unlikely(nextFiber != &m_workerFiber))
					m_runnableQueue.rotate(*nextFiber->listNext());

				int res = nextFiber->run(likely(prevFiber) ? *prevFiber : m_workerFiber, now);
				// Warning! When res == 0, fiber might already have been deleted.
				m_currentFiber = prevFiber;

				switch(res)
				{
				case 0:
					// Ok, just returned to this fiber. Continue execution.
					return true;
				case EAGAIN:
					// Switching to the same fiber.
					return didSchedule;
				case EPERM:
					// fiber just died.
					cleanup(*nextFiber);
					// Retry to find a fiber.
					nextFiber = NULL;
					didSchedule = true;
					goto reschedule;
				default:
					zth_abort("Unhandled Fiber::run() error: %s", err(res).c_str());
				}
			}
		}

		void cleanup(Fiber& fiber) {
			zth_assert(fiber.state() == Fiber::Dead);

			if(unlikely(m_currentFiber == &fiber))
			{
				// It seems that the current fiber is dead.
				// In this case, we cannot delete the fiber and its context,
				// as that is the context we are currently using.
				// Return to the worker's context and sort it out from there.

				zth_dbg(worker, "[%s] Current fiber %s just died; switch to worker", id_str(), fiber.id_str());
				zth_assert(contextSwitchEnabled());
				schedule(&m_workerFiber);

				// We should not get here, as we are dead.
				zth_abort("[Worker %p] Failed to switch to worker", this);
			}

			zth_dbg(worker, "[%s] Fiber %s is dead; cleanup", id_str(), fiber.id_str());
			// Remove from runnable queue
			m_runnableQueue.erase(fiber);
			zth_assert(&fiber != &m_workerFiber);
			// NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDelete)
			delete &fiber;

			sigchld_check();
		}

		void suspend(Fiber& fiber) {
			switch(fiber.state()) {
			case Fiber::New:
			case Fiber::Ready:
				release(fiber);
				fiber.suspend();
				add(&fiber);
				break;
			case Fiber::Running:
				release(fiber);
				fiber.suspend();
				add(&fiber);
				zth_assert(currentFiber() == &fiber);
				zth_assert(contextSwitchEnabled());
				// Current fiber suspended, immediate reschedule.
				schedule();
				break;
			case Fiber::Waiting:
				// Not on my queue...
				fiber.suspend();
				break;
			default:
				; // Ignore.
			}
		}

		void resume(Fiber& fiber) {
			if(fiber.state() != Fiber::Suspended)
				return;

			release(fiber);
			fiber.resume();
			add(&fiber);
		}

		Timestamp const& runEnd() const {
			return m_end;
		}

		void run(TimeInterval const& duration = TimeInterval()) {
			if(duration <= 0) {
				zth_dbg(worker, "[%s] Run", id_str());
				m_end = Timestamp::null();
			} else {
				zth_dbg(worker, "[%s] Run for %s", id_str(), duration.str().c_str());
				m_end = Timestamp::now() + duration;
			}

			while(!m_runnableQueue.empty() && (runEnd().isNull() || Timestamp::now() < runEnd())) {
				schedule();
				zth_assert(!currentFiber());
			}

			sigchld_check();
		}

		Load<>& load() {
			return m_load;
		}

	protected:
		static void dummyWorkerEntry(void*) {
			zth_abort("The worker fiber should not be executed.");
		}

		bool isInWorkerContext() const {
			return m_currentFiber == NULL || m_currentFiber == &m_workerFiber;
		}

		void dbgStats() {
			if(!Config::SupportDebugPrint || !Config::Print_worker)
				return;

			zth_dbg(list, "[%s] Run queue:", id_str());
			if(m_runnableQueue.empty())
				zth_dbg(list, "[%s]   <empty>", id_str());
			else
				for(decltype(m_runnableQueue.begin()) it = m_runnableQueue.begin(); it != m_runnableQueue.end(); ++it)
					zth_dbg(list, "[%s]   %s", id_str(), it->str().c_str());

			zth_dbg(list, "[%s] Suspended queue:", id_str());
			if(m_suspendedQueue.empty())
				zth_dbg(list, "[%s]   <empty>", id_str());
			else
				for(decltype(m_suspendedQueue.begin()) it = m_suspendedQueue.begin(); it != m_suspendedQueue.end(); ++it)
					zth_dbg(list, "[%s]   %s", id_str(), it->str().c_str());
		}

	private:
		Fiber* m_currentFiber;
		List<Fiber> m_runnableQueue;
		List<Fiber> m_suspendedQueue;
		Fiber m_workerFiber;
		Waiter m_waiter;
		Timestamp m_end;
		int m_disableContextSwitch;
		Load<> m_load;

		friend void worker_global_init();
	};

	/*!
	 * \ingroup zth_api_cpp_fiber
	 */
	ZTH_EXPORT __attribute__((pure)) inline Worker& currentWorker() noexcept {
		// Dereference is guarded by the safe_ptr.
		return *Worker::instance();
	}

	/*!
	 * \ingroup zth_api_cpp_fiber
	 */
	ZTH_EXPORT __attribute__((pure)) inline Fiber& currentFiber() noexcept {
		Worker const& w = currentWorker();
		Fiber* f = w.currentFiber();
		zth_assert(f);
		return *f;
	}

	__attribute__((pure)) inline UniqueID<Fiber> const& currentFiberID() {
		return currentFiber();
	}

	inline void getContext(Worker** worker, Fiber** fiber) {
		Worker& current_worker = currentWorker();
		if(likely(worker))
			*worker = &current_worker;

		if(likely(fiber)) {
			Fiber* currentFiber_ = *fiber = current_worker.currentFiber();
			if(unlikely(!currentFiber_))
				zth_abort("Not within fiber context");
		}
	}

	/*!
	 * \ingroup zth_api_cpp_fiber
	 */
	ZTH_EXPORT inline void yield(Fiber* preferFiber = NULL, bool alwaysYield = false, Timestamp const& now = Timestamp::now())
	{
		Fiber const& f = currentFiber();

		perf_syscall("yield()", now);
		if(unlikely(!alwaysYield && !f.allowYield(now)))
			return;

		currentWorker().schedule(preferFiber, now);
	}

	/*!
	 * \ingroup zth_api_cpp_fiber
	 */
	ZTH_EXPORT inline void outOfWork() {
		yield(NULL, true);
	}

	inline void suspend() {
		Worker* worker;
		Fiber* f;
		getContext(&worker, &f);
		worker->suspend(*f);
	}

	inline void resume(Fiber& fiber) {
		Worker* worker;
		getContext(&worker, NULL);
		worker->resume(fiber);
	}

	int startWorkerThread(void(*f)(), size_t stack = 0, char const* name = NULL);
	int execlp(char const* file, char const* arg, ... /*, NULL */);
	int execvp(char const* file, char* const arg[]);

} // namespace

/*!
 * \copydoc zth::yield()
 * \details This is a C-wrapper for zth::yield().
 * \ingroup zth_api_c_fiber
 */
EXTERN_C ZTH_EXPORT ZTH_INLINE void zth_yield() { zth::yield(); }

/*!
 * \copydoc zth::outOfWork()
 * \details This is a C-wrapper for zth::outOfWork().
 * \ingroup zth_api_c_fiber
 */
EXTERN_C ZTH_EXPORT ZTH_INLINE void zth_outOfWork() { zth::outOfWork(); }

/*!
 * \ingroup zth_api_c_fiber
 */
EXTERN_C ZTH_EXPORT ZTH_INLINE int zth_worker_create() {
	if(!zth::Worker::instance())
		return EINVAL;

	new zth::Worker();
	return 0;
}

/*!
 * \ingroup zth_api_c_fiber
 */
EXTERN_C ZTH_EXPORT ZTH_INLINE void zth_worker_run(struct timespec const* ts = NULL) {
	zth::Worker* w = zth::Worker::instance();
	if(unlikely(!w))
		return;
	w->run(ts ? zth::TimeInterval(ts->tv_sec, ts->tv_nsec) : zth::TimeInterval());
}

/*!
 * \ingroup zth_api_c_fiber
 */
EXTERN_C ZTH_EXPORT ZTH_INLINE void zth_worker_destroy() {
	zth::Worker* w = zth::Worker::instance();
	if(unlikely(!w))
		return;
	delete w;
}

/*!
 * \copydoc zth::execvp()
 * \details This is a C-wrapper for zth::execvp().
 * \ingroup zth_api_c_fiber
 */
EXTERN_C ZTH_EXPORT ZTH_INLINE int zth_startWorkerThread(void(*f)(), size_t stack = 0, char const* name = NULL) {
	return zth::startWorkerThread(f, stack, name); }

/*!
 * \copydoc zth::execvp()
 * \details This is a C-wrapper for zth::execvp().
 * \ingroup zth_api_c_fiber
 */
EXTERN_C ZTH_EXPORT ZTH_INLINE int zth_execvp(char const* file, char* const arg[]) { return zth::execvp(file, arg); }

#else // !__cplusplus

#include <time.h>

ZTH_EXPORT void zth_yield();
ZTH_EXPORT void zth_outOfWork();

ZTH_EXPORT int zth_worker_create();
ZTH_EXPORT void zth_worker_run(struct timespec const* ts);
ZTH_EXPORT int zth_worker_destroy();

ZTH_EXPORT int zth_startWorkerThread(void(*f)(), size_t stack, char const* name);
ZTH_EXPORT int zth_execvp(char const* file, char* const arg[]);

#endif // __cplusplus
#endif // ZTH_WORKER_H
