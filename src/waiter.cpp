/*
 * SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#include <libzth/waiter.h>

#include <libzth/poller.h>
#include <libzth/worker.h>

namespace zth {

Waiter::Waiter(Worker& worker)
	: m_worker(worker)
	, m_poller()
	, m_defaultPoller()
{}

Waiter::~Waiter() noexcept
{
	delete m_defaultPoller;
}

void waitUntil(TimedWaitable& w)
{
	perf_syscall("waitUntil()");
	currentWorker().waiter().wait(w);
}

void Waiter::wait(TimedWaitable& w)
{
	Fiber* fiber = m_worker.currentFiber();
	if(unlikely(!fiber || fiber->state() != Fiber::Running))
		return;

	Timestamp t_now = Timestamp::now();
	if(unlikely(w.poll(t_now))) {
		yield(nullptr, false, t_now);
		return;
	}

	w.setFiber(*fiber);
	fiber->nap(w.timeout());
	m_worker.release(*fiber);

	m_waiting.insert(w);
	if(this->fiber())
		m_worker.resume(*this->fiber());

	m_worker.schedule();
}

void scheduleTask(TimedWaitable& w)
{
	currentWorker().waiter().scheduleTask(w);
}

void Waiter::scheduleTask(TimedWaitable& w)
{
	m_waiting.insert(w);
	if(fiber())
		m_worker.resume(*fiber());
}

void unscheduleTask(TimedWaitable& w)
{
	currentWorker().waiter().unscheduleTask(w);
}

void Waiter::unscheduleTask(TimedWaitable& w)
{
	if(m_waiting.contains(w))
		m_waiting.erase(w);
}

void wakeup(TimedWaitable& w)
{
	currentWorker().waiter().wakeup(w);
}

void Waiter::wakeup(TimedWaitable& w)
{
	if(m_waiting.contains(w)) {
		m_waiting.erase(w);

		if(w.hasFiber()) {
			w.fiber().wakeup();
			m_worker.add(&w.fiber(), true);
			w.resetFiber();
		}
	}
}

PollerServerBase& Waiter::poller()
{
	if(m_poller)
		return *m_poller;
	if(m_defaultPoller)
		return *m_defaultPoller;

	m_defaultPoller = new DefaultPollerServer();
	return *m_defaultPoller;
}

bool Waiter::polling() const
{
	PollerServerBase const* p = m_poller;

	if(!p)
		p = m_defaultPoller;

	if(!p)
		return false;

	return !p->empty();
}

void Waiter::wakeup()
{
	if(fiber())
		m_worker.resume(*fiber());
}

void Waiter::setPoller(PollerServerBase* p)
{
	if(!m_poller) {
		// No poller set, just use the given one (which may be nullptr).
		m_poller = p;
	} else if(p) {
		// Replace poller by another one.
		m_poller->migrateTo(*p);
		m_poller = p;
	} else {
		// Replace poller by default one.
		if(!m_defaultPoller && !m_poller->empty()) {
			// ...but it doesn't exist yet and we need it.
			m_defaultPoller = new DefaultPollerServer();
		}

		if(m_defaultPoller)
			m_poller->migrateTo(*m_defaultPoller);

		m_poller = m_defaultPoller;
	}
}

void Waiter::entry()
{
	zth_assert(&currentWorker() == &m_worker);
	fiber()->setName(format("zth::Waiter of %s", m_worker.id_str()));

	while(true) {
		Timestamp t_now = Timestamp::now();
		m_worker.load().stop(t_now);

		while(!m_waiting.empty() && m_waiting.front().timeout() < t_now) {
			TimedWaitable& w = m_waiting.front();
			m_waiting.erase(w);
			if(w.poll(t_now)) {
				if(w.hasFiber()) {
					w.fiber().wakeup();
					m_worker.add(&w.fiber(), true);
				}
			} else {
				// Reinsert, as the timeout() might have changed (and therefore the
				// position in the list).
				m_waiting.insert(w);

				// Update administration, although that does not influence the
				// actual sleep time. It is mostly handy for debugging while
				// printing fiber information.
				if(Config::Debug && w.hasFiber())
					w.fiber().nap(w.timeout());
			}
		}

		bool doRealSleep = false;

		m_worker.load().start(t_now);

		if(m_waiting.empty() && !polling()) {
			// No fiber is waiting. suspend() till anyone is going to nap().
			zth_dbg(waiter, "[%s] No sleeping fibers anymore; suspend", id_str());
			m_worker.suspend(*fiber());
		} else if(!m_worker.schedule()) {
			// When true, we were not rescheduled, which means that we are the only
			// runnable fiber. Do a real sleep, until something interesting happens in
			// the system.
			doRealSleep = true;
			m_worker.load().stop(t_now);
		}

		Timestamp const* end = nullptr;
		if(!m_waiting.empty())
			end = &m_waiting.front().timeout();
		if(!m_worker.runEnd().isNull() && (!end || *end > m_worker.runEnd()))
			end = &m_worker.runEnd();

		if(polling()) {
			int timeout_ms = 0;
			if(doRealSleep) {
				if(!end) {
					// Infinite sleep.
					timeout_ms = -1;
					zth_dbg(waiter, "[%s] Out of other work than doing poll()",
						id_str());
				} else {
					TimeInterval dt = *end - Timestamp::now();
					zth_dbg(waiter,
						"[%s] Out of other work than doing poll(); timeout "
						"is %s",
						id_str(), dt.str().c_str());
					timeout_ms =
						std::max<int>(0, (int)(dt.s<float>() * 1000.0F));
				}

				if(Config::EnablePerfEvent) {
					perf_mark("blocking poll()");
					perf_fiber_state(*fiber(), Fiber::Waiting);
				}
			}

			int res = poller().poll(timeout_ms);

			if(doRealSleep && Config::EnablePerfEvent) {
				perf_fiber(*fiber());
				perf_mark("wakeup");
			}

			if(res && res != EAGAIN)
				zth_dbg(waiter, "[%s] poll() failed; %s", id_str(),
					err(res).c_str());
		} else if(doRealSleep) {
			zth_dbg(waiter, "[%s] Out of work; suspend thread, while waiting for %s",
				id_str(), m_waiting.front().str().c_str());

			if(Config::EnablePerfEvent) {
				perf_mark("idle system; sleep");
				perf_fiber_state(*fiber(), Fiber::Waiting);
			}

			realsleep(end->ts());

			if(Config::EnablePerfEvent) {
				perf_fiber_state(*fiber());
				perf_mark("wakeup");
			}
		}

		sigchld_check();
	}
}



bool PeriodicWakeUp::nap(Timestamp const& reference, Timestamp const& now)
{
	m_t = reference + interval();
	if(likely(m_t > now)) {
		// Proper sleep till next deadline.
		zth::nap(m_t);
		return true;
	} else if(m_t + interval() > now) {
		// Deadline has just passed. Don't sleep and try to catch up.
		yield();
		return false;
	} else {
		// Way passed deadline. Don't sleep and skip a few cycles.
		m_t = now;
		yield();
		return false;
	}
}

} // namespace zth
