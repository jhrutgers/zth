#ifndef __ZTH_WORKER_H
#define __ZTH_WORKER_H

#ifdef __cplusplus
#include <libzth/context.h>
#include <libzth/fiber.h>

#include <time.h>
#include <pthread.h>
#include <limits>

namespace zth {
	
	class Worker {
	public:
		static Worker* currentWorker() { return (Worker*)pthread_getspecific(m_currentWorker); }
#include <pthread.h>
		Fiber* currentFiber() const { return m_currentFiber; }
	
		Worker()
			: m_currentFiber()
			, m_runnableQueue()
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
			m_workerFiber.next = m_workerFiber.prev = &m_workerFiber;
			if((res = m_workerFiber.init()))
				goto error;

			return;

		error:
			zth_abort("Cannot create Worker; error %d", res);
		}

		~Worker() {
			zth_dbg(worker, "[Worker %p] Destruct", this);
			while(m_runnableQueue)
			{
				m_runnableQueue->kill();
				cleanup(m_runnableQueue);
			}
			context_deinit();
		}

		void add(Fiber* fiber) {
			if(likely(m_runnableQueue)) {
				fiber->prev = m_runnableQueue;
				fiber->next = m_runnableQueue->next;
				fiber->next->prev = fiber->prev->next = fiber;
			} else {
				m_runnableQueue = fiber->next = fiber->prev = fiber;
			}

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
				(m_runnableQueue && m_runnableQueue->listContains(*preferFiber)) ||
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
					fiber = m_currentFiber->next;
				else if(m_runnableQueue)
					// Use first of the queue.
					fiber = m_runnableQueue;
				else
					// No fiber to switch to. Stay here.
					return;
			}

			zth_assert(fiber->prev);
			zth_assert(fiber->next);

			{
				Fiber* prevFiber = m_currentFiber;
				m_currentFiber = fiber;

				if(unlikely(fiber == &m_workerFiber && prevFiber))
					// Update head of queue to continue scheduling from where we stopped.
					m_runnableQueue = prevFiber->next;

				res = fiber->run(likely(prevFiber) ? *prevFiber : m_workerFiber, now);
				m_currentFiber = prevFiber;

				switch(res)
				{
				case 0:
					// Ok, just returned to this fiber. Continue execution.
					return;
				case EPERM:
					// fiber died.
					cleanup(fiber);
					// Retry to find a fiber.
					fiber = NULL;
					goto reschedule;
				default:
					zth_abort("Unhandled Fiber::run() error %d", res);
				}
			}
		}

		void cleanup(Fiber* fiber) {
			if(!fiber)
				return;

			zth_assert(fiber->state() == Fiber::Dead);

			if(unlikely(m_currentFiber == fiber))
			{
				// It seems that the current fiber is dead.
				// In this case, we cannot delete the fiber and its context,
				// as that is the context we are currently using.
				// Return to the worker's context and sort it out from there.
				
				zth_dbg(worker, "[Worker %p] Current fiber %s (%p) just died; switch to worker", this, fiber->name().c_str(), fiber);
				schedule(&m_workerFiber);

				// We should not get here, as we are dead.
				zth_abort("[Worker %p] Failed to switch to worker", this);
			}

			zth_dbg(worker, "[Worker %p] Fiber %s (%p) is dead; cleanup", this, fiber->name().c_str(), fiber);
			// Remove from runnable queue
			fiber->prev->next = fiber->next;
			fiber->next->prev = fiber->prev;
			if(m_runnableQueue == fiber)
				m_runnableQueue = fiber->next;
			if(m_runnableQueue == fiber)
				m_runnableQueue = NULL;
			
			delete fiber;
		}

		void run(Timestamp const& duration = Timestamp()) {
			if(duration.isNull()) {
				zth_dbg(worker, "[Worker %p] Run", this);
				m_end = Timestamp(std::numeric_limits<time_t>::max());
			} else {
				zth_dbg(worker, "[Worker %p] Run for %s", this, duration.toString().c_str());
				m_end = Timestamp::now() + duration;
			}

			while(m_runnableQueue && Timestamp::now().isBefore(m_end)) {
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
			if(!m_runnableQueue)
				zth_dbg(worker, "[Worker %p]   <empty>", this);
			else
				for(Fiber::Iterator it = m_runnableQueue->iterate(); it; ++it)
					zth_dbg(worker, "[Worker %p]   %s", this, it->stats().c_str());
		}

	private:
		static pthread_key_t m_currentWorker;
		Fiber* m_currentFiber;
		Fiber* m_runnableQueue;
		Fiber m_workerFiber;
		Timestamp m_end;

		friend void worker_global_init();
	};

	inline void yield(Fiber* preferFiber = NULL)
	{
		Worker* worker = Worker::currentWorker();
		if(unlikely(!worker))
			return;

		Fiber* fiber = worker->currentFiber();
		if(unlikely(!fiber))
			return;

		Timestamp now = Timestamp::now();
		if(unlikely(!fiber->allowYield(now)))
			return;

		worker->schedule(preferFiber, now);
	}

} // namespace
#endif // __cpusplus
#endif // __ZTH_WORKER_H
