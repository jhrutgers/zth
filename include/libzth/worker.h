#ifndef __ZTH_WORKER_H
#define __ZTH_WORKER_H

#ifdef __cplusplus
#include <libzth/context.h>
#include <libzth/fiber.h>
#include <libzth/list.h>

#include <time.h>
#include <pthread.h>
#include <limits>
#include <cstring>

namespace zth {
	
	class Worker {
	public:
		static Worker* currentWorker() { return (Worker*)pthread_getspecific(m_currentWorker); }
#include <pthread.h>
		Fiber* currentFiber() const { return m_currentFiber; }
	
		Worker()
			: m_currentFiber()
			, m_workerFiber(&dummyWorkerEntry)
		{
			int res = 0;
			zth_dbg(worker, "[Worker %p] Created", this);

			if(currentWorker())
				zth_abort("Only one worker allowed per thread");

			if((res = pthread_setspecific(m_currentWorker, this)))
				goto error;

			if((res = context_init()))
				goto error;

			m_workerFiber.setName("zth::Worker");
			m_workerFiber.setStackSize(0); // no stack
			if((res = m_workerFiber.init()))
				goto error;

			return;

		error:
			zth_abort("Cannot create Worker; %s (error %d)", strerror(res), res);
		}

		~Worker() {
			zth_dbg(worker, "[Worker %p] Destruct", this);
			while(!m_runnableQueue.empty()) {
				m_runnableQueue.front().kill();
				cleanup(m_runnableQueue.front());
			}
			context_deinit();
		}

		void add(Fiber* fiber) {
			zth_assert(fiber);
			m_runnableQueue.push_back(*fiber);
			zth_dbg(worker, "[Worker %p] Added %s (%p)", this, fiber->name().c_str(), fiber);
			dbgStats();
		}

		void schedule(Fiber* preferFiber = NULL, Timestamp const& now = Timestamp::now()) {
			if(preferFiber)
				zth_dbg(worker, "[Worker %p] Schedule to %s (%p)", this, preferFiber->name().c_str(), preferFiber);
			else
				zth_dbg(worker, "[Worker %p] Schedule", this);

			dbgStats();

			// Check if fiber is within the runnable queue.
			zth_assert(!preferFiber ||
				m_runnableQueue.contains(*preferFiber) ||
				preferFiber == &m_workerFiber);

			if(unlikely(m_end.isBefore(now)))
			{
				// Stop worker and return to its run1() call.
				zth_dbg(worker, "[Worker %p] Time is up", this);
				preferFiber = &m_workerFiber;
			}
		
			Fiber* fiber = preferFiber;
			int res = 0;
		reschedule:
			if(likely(!fiber))
			{
				if(likely(m_currentFiber))
					// Pick next one from the list.
					fiber = m_currentFiber->listNext();
				else if(!m_runnableQueue.empty())
					// Use first of the queue.
					fiber = &m_runnableQueue.front();
				else
					// No fiber to switch to. Stay here.
					return;
			}

			zth_assert(fiber == &m_workerFiber || fiber->listPrev());
			zth_assert(fiber == &m_workerFiber || fiber->listNext());

			{
				Fiber* prevFiber = m_currentFiber;
				m_currentFiber = fiber;

				if(unlikely(fiber == &m_workerFiber && prevFiber))
					// Update head of queue to continue scheduling from where we stopped.
					m_runnableQueue.rotate(*prevFiber->listNext());

				res = fiber->run(likely(prevFiber) ? *prevFiber : m_workerFiber, now);
				// Warning! When res == 0, fiber might already been deleted.
				m_currentFiber = prevFiber;

				switch(res)
				{
				case 0:
					// Ok, just returned to this fiber. Continue execution.
					return;
				case EPERM:
					// fiber just died.
					cleanup(*fiber);
					// Retry to find a fiber.
					fiber = NULL;
					goto reschedule;
				default:
					zth_abort("Unhandled Fiber::run() error %d", res);
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
				
				zth_dbg(worker, "[Worker %p] Current fiber %s (%p) just died; switch to worker", this, fiber.name().c_str(), &fiber);
				schedule(&m_workerFiber);

				// We should not get here, as we are dead.
				zth_abort("[Worker %p] Failed to switch to worker", this);
			}

			zth_dbg(worker, "[Worker %p] Fiber %s (%p) is dead; cleanup", this, fiber.name().c_str(), &fiber);
			// Remove from runnable queue
			m_runnableQueue.erase(fiber);
			delete &fiber;
		}

		void run(TimeInterval const& duration = TimeInterval()) {
			if(duration <= 0) {
				zth_dbg(worker, "[Worker %p] Run", this);
				m_end = Timestamp(std::numeric_limits<time_t>::max());
			} else {
				zth_dbg(worker, "[Worker %p] Run for %s", this, duration.str().c_str());
				m_end = Timestamp::now() + duration;
			}

			while(!m_runnableQueue.empty() && Timestamp::now() < m_end) {
				schedule();
				zth_assert(!currentFiber());
			}
		}

	protected:
		static void* dummyWorkerEntry(void*) {
			zth_abort("The worker fiber should not be executed.");
		}

		void dbgStats() {
			zth_dbg(worker, "[Worker %p] Run queue:", this);
			if(m_runnableQueue.empty())
				zth_dbg(worker, "[Worker %p]   <empty>", this);
			else
				for(typeof(m_runnableQueue.begin()) it = m_runnableQueue.begin(); it != m_runnableQueue.end(); ++it)
					zth_dbg(worker, "[Worker %p]   %s", this, it->stats().c_str());
		}

	private:
		static pthread_key_t m_currentWorker;
		Fiber* m_currentFiber;
		List<Fiber> m_runnableQueue;
		Fiber m_workerFiber;
		Timestamp m_end;

		friend void worker_global_init();
	};

	inline void yield(Fiber* preferFiber = NULL, bool alwaysYield = false) {
		Worker* worker = Worker::currentWorker();
		if(unlikely(!worker))
			return;

		Fiber* fiber = worker->currentFiber();
		if(unlikely(!fiber))
			return;

		Timestamp now = Timestamp::now();
		if(unlikely(!alwaysYield && !fiber->allowYield(now)))
			return;

		worker->schedule(preferFiber, now);
	}

	inline void outOfWork() {
		yield(NULL, true);
	}

} // namespace
#endif // __cpusplus
#endif // __ZTH_WORKER_H
