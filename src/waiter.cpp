#include <libzth/waiter.h>
#include <libzth/worker.h>

namespace zth {

void nap(Timestamp const& sleepUntil) {
	Worker* w;
	Fiber* f;
	getContext(&w, &f);

	if(f->state() != Fiber::Running)
		return;

	f->nap(sleepUntil);
	w->release(*f);
	w->waiter().napping(*f);
	w->schedule();
}

void Waiter::napping(Fiber& fiber) {
	m_sleeping.insert(fiber);
	if(this->fiber())
		resume(*this->fiber());
}

void Waiter::entry() {
	while(true) {
		Timestamp now = Timestamp::now();

		while(!m_sleeping.empty() && m_sleeping.front().stateEnd() <= now) {
			Fiber& f = m_sleeping.front();
			m_sleeping.erase(f);
			f.wakeup();
			m_worker.add(&f);
		}

		if(m_sleeping.empty()) {
			// No fiber is sleeping. suspend() till anyone is going to nap().
			zth_dbg(waiter, "[Worker %p] No sleeping fibers anymore; suspend", &m_worker);
			suspend();
		} else if(!m_worker.schedule()) {
			// Not rescheduled, which means that we are the only runnable fiber.
			// Do a real sleep, until something interesting happens in the system.
			zth_dbg(waiter, "[Worker %p] Out of work; suspend thread", &m_worker);
			clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &m_sleeping.front().stateEnd().ts(), NULL);
		}
	}
}

} // namespace

