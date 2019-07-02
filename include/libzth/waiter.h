#ifndef __ZTH_WAITER_H
#define __ZTH_WAITER_H
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

#ifdef __cplusplus

#include <libzth/fiber.h>
#include <libzth/list.h>
#include <libzth/time.h>
#include <libzth/io.h>

#ifdef ZTH_HAVE_LIBZMQ
#  include <zmq.h>
#  ifdef ZTH_HAVE_POLL
#    undef POLLIN
#    define POLLIN ZMQ_POLLIN
#    undef POLLOUT
#    define POLLOUT ZMQ_POLLOUT
#    undef POLLERR
#    define POLLERR ZMQ_POLLERR
#    undef POLLPRI
#    define POLLPRI ZMQ_POLLPRI
#    undef POLLRDBAND
#    undef POLLWRBAND
#    undef POLLHUP
#  endif
#endif

namespace zth {
	class Worker;

	class Waitable {
	public:
		Waitable() : m_fiber() {}
		virtual ~Waitable() {}
		Fiber& fiber() const { zth_assert(m_fiber); return *m_fiber; }
		virtual bool poll(Timestamp const& now = Timestamp::now()) = 0;
		virtual std::string str() const { return format("Waitable for %s", fiber().str().c_str()); }
		void setFiber(Fiber& fiber) { m_fiber = &fiber; }
	private:
		Fiber* m_fiber;
	};

	class TimedWaitable : public Waitable, public Listable<TimedWaitable> {
	public:
		TimedWaitable(Timestamp const& timeout) : m_timeout(timeout) {}
		virtual ~TimedWaitable() {}
		Timestamp const& timeout() const { return m_timeout; }
		virtual bool poll(Timestamp const& now = Timestamp::now()) { return timeout() <= now; }
		bool operator<(TimedWaitable const& rhs) const { return timeout() < rhs.timeout(); }
		virtual std::string str() const { return format("Waitable with %s timeout for %s", (timeout() - Timestamp::now()).str().c_str(), fiber().str().c_str()); }
	protected:
		void setTimeout(Timestamp const& t) { m_timeout = t; }
	private:
		Timestamp m_timeout;
	};

	template <typename F>
	class PolledWaiting : public TimedWaitable {
	public:
		PolledWaiting(F f, TimeInterval const& interval)
			: TimedWaitable(Timestamp::now() + interval), m_f(f), m_interval(interval) {}
		virtual ~PolledWaiting() {}

		virtual bool poll(Timestamp const& now = Timestamp::now()) {
			if(m_f())
				return true;

			Timestamp next = timeout() + interval();
			if(unlikely(next < now))
				next = now + interval();

			setTimeout(next);
			return false;
		}

		TimeInterval const& interval() const { return m_interval; }
	private:
		F m_f;
		TimeInterval const m_interval;
	};

	template <typename C>
	struct PolledMemberWaitingHelper {
		PolledMemberWaitingHelper(C& that, bool (C::*f)()) : that(that), f(f) {}
		bool operator()() const { return (that.*f)(); }
		C& that;
		bool (C::*f)();
	};

	template <typename C>
	class PolledMemberWaiting : public PolledWaiting<PolledMemberWaitingHelper<C> > {
	public:
		typedef PolledWaiting<PolledMemberWaitingHelper<C> > base;
		PolledMemberWaiting(C& that, bool (C::*f)(), TimeInterval interval)
			: base(PolledMemberWaitingHelper<C>(that, f), interval) {}
		virtual ~PolledMemberWaiting() {}
	};

#if defined(ZTH_HAVE_POLL) || defined(ZTH_HAVE_LIBZMQ)

	class AwaitFd : public Waitable, public Listable<AwaitFd> {
	public:
		AwaitFd(zth_pollfd_t *fds, int nfds, Timestamp const& timeout = Timestamp())
			: m_fds(fds), m_nfds(nfds), m_timeout(timeout), m_error(-1), m_result(-1) { zth_assert(nfds > 0 && fds); }
		virtual ~AwaitFd() {}
		virtual bool poll(Timestamp const& now = Timestamp::now()) { zth_abort("Don't call."); }

		zth_pollfd_t* fds() const { return m_fds; }
		int nfds() const { return m_nfds; }
		Timestamp const& timeout() const { return m_timeout; }

		virtual std::string str() const {
			if(timeout().isNull())
				return format("Waitable for %d fds for %s", (int)m_nfds, fiber().str().c_str());
			else
				return format("Waitable for %d fds for %s with %s timeout", (int)m_nfds, fiber().str().c_str(),
					(timeout() - Timestamp::now()).str().c_str());
		}

		void setResult(int result, int error = 0) { m_result = result; m_error = error; }
		bool finished() const { return m_error >= 0; }
		int error() const { return finished() ? m_error : 0; }
		int result() const { return m_result; }
		bool intr() const { return error() == EINTR; }
	private:
		zth_pollfd_t* m_fds;
		int m_nfds;
		Timestamp m_timeout;
		int m_error;
		int m_result;
	};

	class Await1Fd : public AwaitFd {
	public:
		explicit Await1Fd(int fd, short events, Timestamp const& timeout = Timestamp())
			: AwaitFd(&m_fd, 1, timeout), m_fd() { m_fd.fd = fd; m_fd.events = events; }
#ifdef ZTH_HAVE_LIBZMQ
		explicit Await1Fd(void* socket, short events, Timestamp const& timeout = Timestamp())
			: AwaitFd(&m_fd, 1, timeout), m_fd() { m_fd.socket = socket; m_fd.events = events; }
#endif
		virtual ~Await1Fd() {}

		zth_pollfd_t const& fd() const { return m_fd; }
		zth_pollfd_t& fd() { return m_fd; }
	private:
		zth_pollfd_t m_fd;
	};
#endif

	class Waiter : public Runnable {
	public:
		Waiter(Worker& worker)
			: m_worker(worker)
		{}
		virtual ~Waiter() {}

		void wait(TimedWaitable& w);
#ifdef ZTH_HAVE_POLL
		void checkFdList();
		int waitFd(AwaitFd& w);
#endif

	protected:
		virtual int fiberHook(Fiber& f) {
			f.setName("zth::Waiter");
			f.suspend();
			return Runnable::fiberHook(f);
		}

		virtual void entry();

	private:
		Worker& m_worker;
		SortedList<TimedWaitable> m_waiting;
#if defined(ZTH_HAVE_POLL) || defined(ZTH_HAVE_LIBZMQ)
		List<AwaitFd> m_fdList;
		std::vector<zth_pollfd_t> m_fdPollList;
#endif
	};

	void waitUntil(TimedWaitable& w);
	template <typename F> void waitUntil(F f, TimeInterval const& pollInterval) {
		PolledWaiting<F> w(f, pollInterval); waitUntil(w); }
	template <typename C> void waitUntil(C& that, void (C::*f)(), TimeInterval const& pollInterval) {
		PolledWaiting<C> w(that, f, pollInterval); waitUntil(w); }

	inline void  nap(Timestamp const& sleepUntil)	{ TimedWaitable w(sleepUntil); waitUntil(w); }
	inline void  nap(TimeInterval const& sleepFor)	{ nap(Timestamp::now() + sleepFor); }
	inline void mnap(long sleepFor_ms)				{ nap(TimeInterval((time_t)(sleepFor_ms / 1000L), sleepFor_ms % 1000L * 1000000L)); }
	inline void unap(long sleepFor_us)				{ nap(TimeInterval((time_t)(sleepFor_us / 1000000L), sleepFor_us % 1000000L * 1000L)); }

} // namespace 
#endif // __cplusplus
#endif // __ZTH_WAITER_H
