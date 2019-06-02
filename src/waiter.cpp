#include <libzth/waiter.h>
#include <libzth/worker.h>

namespace zth {

void nap(Timestamp const& sleepUntil) {
	Worker* w = Worker::currentWorker();
	if(unlikely(!w))
		return;

	Fiber* f = w->currentFiber();
	if(unlikely(!f || f->state() != Fiber::Running))
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

		if(m_sleeping.empty())
			suspend();
		else
			outOfWork();
	}
}

} // namespace

