#ifndef __ZTH_WORKER_H
#define __ZTH_WORKER_H

#ifdef __cplusplus
#include <libzth/context.h>
#include <libzth/fiber.h>

#include <time.h>

namespace zth {
	
	class Worker {
	public:
		static Worker* currentWorker() { return (Worker*)pthread_getspecific(m_currentWorker); }
		Fiber* currentFiber() const { return m_currentFiber; }
	
		Worker()
			: m_currentFiber()
			, m_workerFiber(&dummyWorkerEntry)
		{
			zth_dbg(banner, "%s", banner());
			zth_dbg(worker, "[Worker %p] Created", this);
			m_workerFiber.setName("zth::Worker");
			m_workerFiber.setStackSize(0); // no stack
			m_workerFiber.next = m_workerFiber.prev = &m_workerFiber;
		}

		~Worker() {
			while(m_runnableQueue)
			{
				m_runnableQueue->kill();
				cleanup(m_runnableQueue);
			}
			zth_dbg(worker, "[Worker %p] Destructed", this);
		}

		void add(Fiber* fiber) {
			if(likely(m_runnableQueue)) {
				fiber->prev = m_runnableQueue;
				fiber->next = m_runnableQueue->next;
				fiber->next->prev = fiber;
				m_runnableQueue->next = fiber;
			} else {
				m_runnableQueue = fiber->next = fiber->prev = fiber;
			}

			zth_dbg(worker, "[Worker %p] Added %s (%p)", this, fiber->name().c_str(), fiber);
		}

		void schedule(Fiber* preferFiber = NULL, Timestamp const& now = Timestamp::now()) {
			// Check if fiber is within the runnable queue.
			zth_assert(!preferFiber || (m_runnableQueue && m_runnableQueue->listContains(*preferFiber)));

			if(unlikely(m_end.isBefore(now)))
			{
				// Stop worker and return to its run1() call.
				zth_dbg(worker, "[Worker %p] Time is up");
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

		void cleanup(Fiber* fiber)
		{
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
			
			delete fiber;
		}

		void run1()
		{
			zth_assert(!currentFiber());
			schedule();
		}

		void run(Timestamp const& t = Timestamp((time_t)-1))
		{
			m_end = t;
			while(m_runnableQueue && Timestamp::now().isBefore(m_end))
				run1();
		}

	protected:
		static void* dummyWorkerEntry(void*)
		{
			zth_abort("The worker fiber should not be executed.");
		}

	private:
		static pthread_key_t m_currentWorker;
		Fiber* m_currentFiber;
		Fiber* m_runnableQueue;
		Fiber m_workerFiber;
		Timestamp m_end;
	};

} // namespace
#endif // __cpusplus
#endif // __ZTH_WORKER_H
