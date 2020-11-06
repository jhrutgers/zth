/*
 * Zth (libzth), a cooperative userspace multitasking library.
 * Copyright (C) 2019  Jochem Rutgers
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

#include <libzth/waiter.h>
#include <libzth/worker.h>
#include <libzth/io.h>

namespace zth {

void waitUntil(TimedWaitable& w) {
	perf_syscall("waitUntil()");
	currentWorker().waiter().wait(w);
}

void Waiter::wait(TimedWaitable& w) {
	Fiber* fiber = m_worker.currentFiber();
	if(unlikely(!fiber || fiber->state() != Fiber::Running))
		return;

	Timestamp now = Timestamp::now();
	if(unlikely(w.poll(now))) {
		yield(NULL, false, now);
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

void scheduleTask(TimedWaitable& w) {
	currentWorker().waiter().scheduleTask(w);
}

void Waiter::scheduleTask(TimedWaitable& w) {
	m_waiting.insert(w);
	if(this->fiber())
		m_worker.resume(*this->fiber());
}

void unscheduleTask(TimedWaitable& w) {
	currentWorker().waiter().unscheduleTask(w);
}

void Waiter::unscheduleTask(TimedWaitable& w) {
	if(m_waiting.contains(w))
		m_waiting.erase(w);
}


#ifdef ZTH_HAVE_POLLER
void Waiter::checkFdList() {
	if(!Config::EnableAssert && (!zth_config(EnableDebugPrint) || !Config::Print_list))
		return;

	zth_dbg(list, "[%s] Waiter poll() list:", id_str());
	size_t offset = 0;
	for(decltype(m_fdList.begin()) itw = m_fdList.begin(); itw != m_fdList.end(); ++itw) {
		zth_dbg(list, "[%s]    %s", id_str(), itw->str().c_str());
		zth_assert(offset + itw->nfds() <= m_fdPollList.size());

		for(size_t i = 0; i < (size_t)itw->nfds(); offset++, i++) {
			std::string fd;
#  ifdef ZTH_HAVE_LIBZMQ
			if(m_fdPollList[i].socket)
				fd = format("0x%" PRIxPTR, (uintptr_t)m_fdPollList[i].socket);
			else
#  endif
				format("%lld", (long long)m_fdPollList[i].fd);
			zth_dbg(list, "[%s]       %s: events 0x%04hx", id_str(), fd.c_str(), m_fdPollList[i].events);
		}
	}
	zth_assert(offset == m_fdPollList.size());
}

int Waiter::waitFd(AwaitFd& w) {
	Fiber* fiber = m_worker.currentFiber();
	if(unlikely(!fiber || fiber->state() != Fiber::Running))
		return EAGAIN;

	w.setFiber(*fiber);

	// Add our set of fds to the Waiter's administration.
	m_fdList.push_back(w);
	m_fdPollList.reserve(m_fdPollList.size() + w.nfds());
	for(int i = 0; i < w.nfds(); i++)
		m_fdPollList.push_back(w.fds()[i]);

	checkFdList();

	// Put ourselves to sleep.
	fiber->nap();
	m_worker.release(*fiber);

	// Switch to Waiter
	if(this->fiber())
		m_worker.resume(*this->fiber());

	m_worker.schedule();

	// Got back, check which fds were triggered.
	zth_assert(w.finished());

	size_t offset = 0;
	int res = 0;
	for(decltype(m_fdList.begin()) itw = m_fdList.begin(); itw != m_fdList.end(); ++itw, offset += itw->nfds())
		if(&*itw == &w) {
			// This is us
			if(!w.error()) {
				for(size_t i = 0; i < (size_t)w.nfds(); i++) {
					zth_pollfd_t& f = w.fds()[i] = m_fdPollList[offset + i];
					if(f.revents) {
						zth_dbg(waiter, "[%s] poll(%lld)'s revents: 0x%04hx", id_str(), (long long)f.fd, f.events);
						res++;
					}
				}
			}

			m_fdList.erase(w);
			m_fdPollList.erase(m_fdPollList.begin() + offset, m_fdPollList.begin() + offset + w.nfds());
			break;
		}

	checkFdList();

	if(w.error())
		return w.error();

	w.setResult(res);
	return 0;
}
#endif

void Waiter::entry() {
	zth_assert(&currentWorker() == &m_worker);
	fiber()->setName(format("zth::Waiter of %s", m_worker.id_str()));

	while(true) {
		Timestamp now = Timestamp::now();
		m_worker.load().stop(now);

		while(!m_waiting.empty() && m_waiting.front().timeout() < now) {
			TimedWaitable& w = m_waiting.front();
			m_waiting.erase(w);
			if(w.poll(now)) {
				if(w.hasFiber()) {
					w.fiber().wakeup();
					m_worker.add(&w.fiber());
				}
			} else {
				// Reinsert, as the timeout() might have changed (and therefore the position in the list).
				m_waiting.insert(w);
			}
		}

		bool doRealSleep = false;

		m_worker.load().start(now);

		if(m_waiting.empty()
#ifdef ZTH_HAVE_POLLER
			&& m_fdPollList.empty()
#endif
		) {
			// No fiber is waiting. suspend() till anyone is going to nap().
			zth_dbg(waiter, "[%s] No sleeping fibers anymore; suspend", id_str());
			m_worker.suspend(*fiber());
		} else if(!m_worker.schedule()) {
			// When true, we were not rescheduled, which means that we are the only runnable fiber.
			// Do a real sleep, until something interesting happens in the system.
			doRealSleep = true;
			m_worker.load().stop(now);
		}

#ifdef ZTH_HAVE_POLLER
		if(!m_fdPollList.empty()) {
			Timestamp const* pollTimeout = NULL;
			if(doRealSleep) {
				if(!m_worker.runEnd().isNull() && (!pollTimeout || *pollTimeout > m_worker.runEnd()))
					pollTimeout = &m_worker.runEnd();
				for(decltype(m_fdList.begin()) it = m_fdList.begin(); it != m_fdList.end(); ++it)
					if(!it->timeout().isNull() && (!pollTimeout || *pollTimeout > it->timeout()))
						pollTimeout = &it->timeout();
				if(!m_waiting.empty() && (!pollTimeout || *pollTimeout > m_waiting.front().timeout()))
					pollTimeout = &m_waiting.front().timeout();
			}

			int timeout_ms = 0;
			if(doRealSleep) {
				if(!pollTimeout) {
					// Infinite sleep.
					timeout_ms = -1;
					zth_dbg(waiter, "[%s] Out of other work than doing poll()", id_str());
				} else {
					TimeInterval dt = *pollTimeout - Timestamp::now();
					zth_dbg(waiter, "[%s] Out of other work than doing poll(); timeout is %s", id_str(), dt.str().c_str());
					timeout_ms = std::max<int>(0, (int)(dt.s() * 1000.0));
				}
			}

			if(doRealSleep) {
				perf_mark("blocking poll()");
				perf_event(PerfEvent<>(*fiber(), Fiber::Waiting));
			}

#ifdef ZTH_HAVE_LIBZMQ
			int res = ::zmq_poll(&m_fdPollList[0], (int)m_fdPollList.size(), timeout_ms);
			int error = res == -1 ? zmq_errno() : 0;
#else
			int res = ::poll(&m_fdPollList[0], (nfds_t)m_fdPollList.size(), timeout_ms);
			int error = res == -1 ? errno : 0;
#endif

			if(doRealSleep) {
				perf_event(PerfEvent<>(*fiber(), fiber()->state()));
				perf_mark("wakeup");
			}

			if(res == -1) {
				zth_dbg(waiter, "[%s] poll() failed; %s", id_str(), err(error).c_str());
				for(decltype(m_fdList.begin()) it = m_fdList.begin(); it != m_fdList.end(); ++it) {
					zth_dbg(waiter, "[%s] %s got error; wakeup", id_str(), it->str().c_str());
					it->setResult(-1, error);
					it->fiber().wakeup();
					m_worker.add(&it->fiber());
				}
			} else if(res > 0) {
				size_t offset = 0;
				for(decltype(m_fdList.begin()) it = m_fdList.begin(); res > 0 && it != m_fdList.end(); offset += it->nfds(), ++it) {
					bool wakeup = false;
					zth_assert(m_fdPollList.size() >= offset + it->nfds());
					for(size_t i = 0; res > 0 && i < (size_t)it->nfds(); i++) {
						if(m_fdPollList[offset + i].revents) {
							wakeup = true;
							res--;
						}
					}

					if(wakeup) {
						zth_dbg(waiter, "[%s] %s got ready; wakeup", id_str(), it->str().c_str());
						it->setResult(0);
						it->fiber().wakeup();
						m_worker.add(&it->fiber());
					}
				}
			}
		} else
#endif
		if(doRealSleep) {
			zth_dbg(waiter, "[%s] Out of work; suspend thread, while waiting for %s", id_str(), m_waiting.front().str().c_str());
			Timestamp const* end = &m_waiting.front().timeout();
			if(!m_worker.runEnd().isNull () && *end > m_worker.runEnd())
				end = &m_worker.runEnd();
			perf_mark("idle system; sleep");
			perf_event(PerfEvent<>(*fiber(), Fiber::Waiting));
			clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &end->ts(), NULL);
			perf_event(PerfEvent<>(*fiber(), fiber()->state()));
			perf_mark("wakeup");
		}

		sigchld_check();
	}
}

} // namespace

