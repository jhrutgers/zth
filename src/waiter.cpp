#include <libzth/waiter.h>
#include <libzth/worker.h>

#ifdef ZTH_OS_WINDOWS
#  ifdef ZTH_HAVE_WINSOCK
#    include <winsock2.h>
#  endif
#else
#  include <sys/select.h>
#endif

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

int Waiter::waitFd(AwaitFd& w) {
	Fiber* fiber = m_worker.currentFiber();
	if(unlikely(!fiber || fiber->state() != Fiber::Running))
		return EAGAIN;
	
	w.setFiber(*fiber);
	fiber->nap();
	m_worker.release(*fiber);

	m_waitingFd.push_back(w);
	if(this->fiber())
		m_worker.resume(*this->fiber());
	
	m_worker.schedule();
	m_waitingFd.erase(w);
	zth_assert(w.finished());
	return w.error();
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

		bool doRealSleep = false;

		if(m_waiting.empty() && m_waitingFd.empty()) {
			// No fiber is waiting. suspend() till anyone is going to nap().
			zth_dbg(waiter, "[Worker %p] No sleeping fibers anymore; suspend", &m_worker);
			m_worker.suspend(*fiber());
		} else if(!m_worker.schedule()) {
			// When true, we were not rescheduled, which means that we are the only runnable fiber.
			// Do a real sleep, until something interesting happens in the system.
			doRealSleep = true;
		}

#if !defined(ZTH_OS_WINDOWS) || defined(ZTH_HAVE_WINSOCK)
		if(!m_waitingFd.empty()) {
			fd_set readfds;
			fd_set writefds;
			fd_set exceptfds;

			FD_ZERO(&readfds);
			FD_ZERO(&writefds);
			FD_ZERO(&exceptfds);

			int nfds = 0;

			for(decltype(m_waitingFd.begin()) it = m_waitingFd.begin(); it != m_waitingFd.end(); ++it) {
				switch(it->awaitType()) {
				case AwaitFd::AwaitRead: FD_SET(it->fd(), &readfds); break;
				case AwaitFd::AwaitWrite: FD_SET(it->fd(), &writefds); break;
				case AwaitFd::AwaitExcept: FD_SET(it->fd(), &exceptfds); break;
				}
				if(it->fd() > nfds)
					nfds = it->fd();
			}

			struct timeval timeout = {};
			struct timeval* ptimeout = &timeout;
			if(doRealSleep) {
				Timestamp const* end = NULL;
				if(!m_worker.runEnd().isNull())
					end = &m_worker.runEnd();
				if(!m_waiting.empty() && (!end || *end > m_waiting.front().timeout()))
					end = &m_waiting.front().timeout();

				if(!end) {
					ptimeout = NULL;
					zth_dbg(waiter, "[Worker %p] Out of other work than doing select()", &m_worker);
				} else {
					TimeInterval dt = *end - now;

					if(dt <= 0) {
						doRealSleep = false;
					} else {
						timeout.tv_sec = dt.ts().tv_sec;
						timeout.tv_usec = dt.ts().tv_nsec / 1000L;
						zth_dbg(waiter, "[Worker %p] Out of other work than doing select(); timeout is %s", &m_worker, dt.str().c_str());
					}
				}
			}

			if(doRealSleep) {
				zth_perfmark("blocking select()");
				perf_event(PerfEvent<>(*fiber(), Fiber::Waiting));
			}

			int res = select(nfds + 1, &readfds, &writefds, &exceptfds, ptimeout);
			int error = res == -1 ? errno : 0;

			if(doRealSleep) {
				perf_event(PerfEvent<>(*fiber(), fiber()->state()));
				zth_perfmark("wakeup");
			}

			if(res == -1) {
				zth_dbg(waiter, "[Worker %p] select() failed; %s\n", &m_worker, err(error).c_str());
				for(decltype(m_waitingFd.begin()) it = m_waitingFd.begin(); it != m_waitingFd.end(); ++it) {
					zth_dbg(waiter, "[Worker %p] %s got error; wakeup", &m_worker, it->str().c_str());
					it->setResult(error);
					it->fiber().wakeup();
					m_worker.add(&it->fiber());
				}
			} else if(res > 0) {
				for(decltype(m_waitingFd.begin()) it = m_waitingFd.begin(); it != m_waitingFd.end(); ++it) {
					switch(it->awaitType()) {
					case AwaitFd::AwaitRead: if(!FD_ISSET(it->fd(), &readfds)) continue; break;
					case AwaitFd::AwaitWrite: if(!FD_ISSET(it->fd(), &writefds)) continue; break;
					case AwaitFd::AwaitExcept: if(!FD_ISSET(it->fd(), &exceptfds)) continue; break;
					}

					// If we got here, it->fd() got signalled.
					zth_dbg(waiter, "[Worker %p] %s got ready; wakeup", &m_worker, it->str().c_str());
					it->setResult();
					it->fiber().wakeup();
					m_worker.add(&it->fiber());
				}
			}
		} else
#endif
		if(doRealSleep) {
			zth_dbg(waiter, "[Worker %p] Out of work; suspend thread, while waiting for %s", &m_worker, m_waiting.front().fiber().str().c_str());
			Timestamp const* end = &m_waiting.front().timeout();
			if(!m_worker.runEnd().isNull () && *end > m_worker.runEnd())
				end = &m_worker.runEnd();
			zth_perfmark("idle system; sleep");
			perf_event(PerfEvent<>(*fiber(), Fiber::Waiting));
			clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &end->ts(), NULL);
			perf_event(PerfEvent<>(*fiber(), fiber()->state()));
			zth_perfmark("wakeup");
		}
	}
}

} // namespace

