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
		Fiber& fiber() const { zth_assert(hasFiber()); return *m_fiber; }
		virtual bool poll(Timestamp const& now = Timestamp::now()) = 0;
		virtual std::string str() const { return format("Waitable for %s", fiber().str().c_str()); }
		void setFiber(Fiber& fiber) { m_fiber = &fiber; }
		bool hasFiber() const { return m_fiber; }
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
		virtual std::string str() const {
			if(hasFiber())
				return format("Waitable with %s timeout for %s", (timeout() - Timestamp::now()).str().c_str(), fiber().str().c_str());
			else
				return format("Waitable with %s timeout", (timeout() - Timestamp::now()).str().c_str());
		}
	protected:
		void setTimeout(Timestamp const& t) { m_timeout = t; }
	private:
		Timestamp m_timeout;
	};

	template <typename F>
	class PolledWaiting : public TimedWaitable {
	public:
		PolledWaiting(F f, TimeInterval const& interval = TimeInterval())
			: TimedWaitable(Timestamp()), m_f(f) { setInterval(interval); }
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
		void setInterval(TimeInterval const& interval, Timestamp const& now = Timestamp::now()) {
			m_interval = interval;
			setTimeout(now + interval);
		}
	private:
		F m_f;
		TimeInterval m_interval;
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

#ifdef ZTH_HAVE_POLLER

	class AwaitFd : public Waitable, public Listable<AwaitFd> {
	public:
		AwaitFd(zth_pollfd_t *fds, int nfds, Timestamp const& timeout = Timestamp())
			: m_fds(fds), m_nfds(nfds), m_timeout(timeout), m_error(-1), m_result(-1) { zth_assert(nfds > 0 && fds); }
		virtual ~AwaitFd() {}
		virtual bool poll(Timestamp const& UNUSED_PAR(now) = Timestamp::now()) { zth_abort("Don't call."); }

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
		void scheduleTask(TimedWaitable& w);
		void unscheduleTask(TimedWaitable& w);
#ifdef ZTH_HAVE_POLLER
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
#ifdef ZTH_HAVE_POLLER
		List<AwaitFd> m_fdList;
		std::vector<zth_pollfd_t> m_fdPollList;
#endif
	};

	/*!
	 * \ingroup zth_api_cpp_fiber
	 */
	ZTH_EXPORT void waitUntil(TimedWaitable& w);

	/*!
	 * \ingroup zth_api_cpp_fiber
	 */
	template <typename F> void waitUntil(F f, TimeInterval const& pollInterval) {
		PolledWaiting<F> w(f, pollInterval); waitUntil(w); }

	/*!
	 * \ingroup zth_api_cpp_fiber
	 */
	template <typename C> void waitUntil(C& that, void (C::*f)(), TimeInterval const& pollInterval) {
		PolledMemberWaiting<C> w(that, f, pollInterval); waitUntil(w); }

	void scheduleTask(TimedWaitable& w);
	void unscheduleTask(TimedWaitable& w);

	/*!
	 * \ingroup zth_api_cpp_fiber
	 */
	ZTH_EXPORT inline void  nap(Timestamp const& sleepUntil)	{ TimedWaitable w(sleepUntil); waitUntil(w); }

	/*!
	 * \ingroup zth_api_cpp_fiber
	 */
	ZTH_EXPORT inline void  nap(TimeInterval const& sleepFor)	{ if(!sleepFor.hasPassed()) nap(Timestamp::now() + sleepFor); }

	/*!
	 * \ingroup zth_api_cpp_fiber
	 */
	ZTH_EXPORT inline void mnap(long sleepFor_ms)				{ nap(TimeInterval((time_t)(sleepFor_ms / 1000L), sleepFor_ms % 1000L * 1000000L)); }

	/*!
	 * \ingroup zth_api_cpp_fiber
	 */
	ZTH_EXPORT inline void unap(long sleepFor_us)				{ nap(TimeInterval((time_t)(sleepFor_us / 1000000L), sleepFor_us % 1000000L * 1000L)); }

	/*!
	 * \brief Periodic wakeup after fixed interval.
	 *
	 * A common pattern is that a fiber wakes up periodically to
	 * do some work, like this:
	 *
	 * \code
	 * Timestamp t = Timestamp::now();
	 * while(true) {
	 *     do_work()
	 *
	 *     // Advance to next deadline.
	 *     t += 100_ms;
	 *
	 *     // Sleep till deadline.
	 *     nap(t);
	 * }
	 *
	 * If somehow the deadline is missed <i>n</i> times, because of debugging, or
	 * some other stalled task, the loop is executed <i>n</i> times without sleeping
	 * to catch up with t. This burst of iterations is often not required; this
	 * missed deadlines should just be skipped.
	 *
	 * One could be tempted to do it this way:
	 *
	 * \code
	 * while(true) {
	 *     do_work();
	 *     nap(100_ms);
	 * }
	 * \endcode
	 *
	 * However, the sleeping may be just more than 100 ms, so the execution drifts
	 * away from a strictly 10 Hz.
	 *
	 * Using this class, it is easier to skip missed deadlines, if it is more than one.
	 * It can be used as follows:
	 *
	 * \code
	 * PeriodicWakeUp w(100_ms);
	 * while(true) {
	 *     do_work();
	 *
	 *     // Sleep till deadline.
	 *     w();
	 * }
	 * \endcode
	 *
	 * \ingroup zth_api_cpp_fiber
	 */
	class PeriodicWakeUp {
	public:
		explicit PeriodicWakeUp(TimeInterval const& interval)
			: m_interval(interval)
			, m_t(Timestamp::now())
		{}

		Timestamp const& t() const {
			return m_t;
		}

		TimeInterval const& interval() const {
			return m_interval;
		}

		void setInterval(TimeInterval const& interval) {
			m_interval = interval;
		}

		PeriodicWakeUp& operator=(TimeInterval& interval) {
			setInterval(interval);
			return *this;
		}

		bool nap() {
			return nap(t());
		}

		bool nap(Timestamp const& reference, Timestamp const& now = Timestamp::now());

		void operator()() {
			nap();
		}

	private:
		TimeInterval m_interval;
		Timestamp m_t;
	};
} // namespace

/*!
 * \copydoc zth::nap(zth::TimeInterval const&)
 * \details This is a C-wrapper for zth::nap(zth::TimeInterval const&).
 * \ingroup zth_api_c_fiber
 */
EXTERN_C ZTH_EXPORT ZTH_INLINE void zth_nap(struct timespec const* ts) { if(likely(ts)) zth::nap(zth::TimeInterval(*ts)); }

/*!
 * \copydoc zth::mnap()
 * \details This is a C-wrapper for zth::mnap().
 * \ingroup zth_api_c_fiber
 */
EXTERN_C ZTH_EXPORT ZTH_INLINE void zth_mnap(long sleepFor_ms) { zth::mnap(sleepFor_ms); }

/*!
 * \copydoc zth::unap()
 * \details This is a C-wrapper for zth::unap().
 * \ingroup zth_api_c_fiber
 */
EXTERN_C ZTH_EXPORT ZTH_INLINE void zth_unap(long sleepFor_us) { zth::unap(sleepFor_us); }

#else // !__cplusplus

#include <libzth/macros.h>

#include <time.h>

ZTH_EXPORT void zth_nap(struct timespec const* ts);
ZTH_EXPORT void zth_mnap(long sleepFor_ms);
ZTH_EXPORT void zth_unap(long sleepFor_us);

#endif // __cplusplus
#endif // __ZTH_WAITER_H
