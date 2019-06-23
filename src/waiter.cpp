#include <libzth/waiter.h>
#include <libzth/worker.h>

namespace zth {

void waitUntil(TimedWaitable& w) {
	currentWorker().waiter().wait(w);
}

void Waiter::wait(TimedWaitable& w) {
	Fiber* fiber = m_worker.currentFiber();
	if(unlikely(!fiber || fiber->state() != Fiber::Running))
		return;
	
	w.setFiber(*fiber);
	fiber->nap(w.timeout());
	m_worker.release(*fiber);

	m_waiting.insert(w);
	if(this->fiber())
		m_worker.resume(*this->fiber());
	
	m_worker.schedule();
}

void Waiter::waitFd(AwaitFd& w) {
	Fiber* fiber = m_worker.currentFiber();
	if(unlikely(!fiber || fiber->state() != Fiber::Running))
		return;
	
	w.setFiber(*fiber);
	fiber->nap();
	m_worker.release(*fiber);

	// todo: m_waitingFd.insert(w);
	if(this->fiber())
		m_worker.resume(*this->fiber());
	
	m_worker.schedule();
}

void Waiter::entry() {
	zth_assert(&currentWorker() == &m_worker);

	while(true) {
		Timestamp now = Timestamp::now();

		while(!m_waiting.empty() && m_waiting.front().timeout() <= now) {
			TimedWaitable& w = m_waiting.front();
			m_waiting.erase(w);
			if(w.poll(now)) {
				w.fiber().wakeup();
				m_worker.add(&w.fiber());
			} else {
				// Reinsert, as the timeout() might have changed (and therefore the position in the list).
				m_waiting.insert(w);
			}
		}

		if(m_waiting.empty()) {
			// No fiber is sleeping. suspend() till anyone is going to nap().
			zth_dbg(waiter, "[Worker %p] No sleeping fibers anymore; suspend", &m_worker);
			m_worker.suspend(*fiber());
		} else if(!m_worker.schedule()) {
			// Not rescheduled, which means that we are the only runnable fiber.
			// Do a real sleep, until something interesting happens in the system.
			zth_dbg(waiter, "[Worker %p] Out of work; suspend thread, while waiting for %s", &m_worker, m_waiting.front().fiber().str().c_str());
			clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &m_waiting.front().timeout().ts(), NULL);
		}
	}
}

} // namespace

