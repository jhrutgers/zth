#ifndef ZTH_WAITER_H
#define ZTH_WAITER_H
/*
 * Zth (libzth), a cooperative userspace multitasking library.
 * Copyright (C) 2019-2021  Jochem Rutgers
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
		void resetFiber() { m_fiber = nullptr; }
		bool hasFiber() const { return m_fiber; }
	private:
		Fiber* m_fiber;
	};

	class TimedWaitable : public Waitable, public Listable<TimedWaitable> {
	public:
		explicit TimedWaitable(Timestamp const& timeout = Timestamp()) : m_timeout(timeout) {}
		virtual ~TimedWaitable() {}
		Timestamp const& timeout() const { return m_timeout; }
		virtual bool poll(Timestamp const& now = Timestamp::now()) override { return timeout() <= now; }
		bool operator<(TimedWaitable const& rhs) const { return timeout() < rhs.timeout(); }
		virtual std::string str() const override {
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
		explicit PolledWaiting(F f, TimeInterval const& interval = TimeInterval())
			: TimedWaitable(Timestamp()), m_f(f) { setInterval(interval); }
		virtual ~PolledWaiting() {}

		virtual bool poll(Timestamp const& now = Timestamp::now()) override {
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

	class PollerServerBase;

	class Waiter : public Runnable {
	public:
		explicit Waiter(Worker& worker);
		virtual ~Waiter();

		void wait(TimedWaitable& w);
		void scheduleTask(TimedWaitable& w);
		void unscheduleTask(TimedWaitable& w);
		void wakeup(TimedWaitable& w);

		PollerServerBase& poller();
		void setPoller(PollerServerBase* p = nullptr);
		void wakeup();

	protected:
		bool polling() const;

		virtual int fiberHook(Fiber& f) override {
			f.setName("zth::Waiter");
			f.suspend();
			return Runnable::fiberHook(f);
		}

		virtual void entry() override;

	private:
		Worker& m_worker;
		SortedList<TimedWaitable> m_waiting;
		PollerServerBase* m_poller;
		PollerServerBase* m_defaultPoller;
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
	template <typename C> void waitUntil(C& that, bool (C::*f)(), TimeInterval const& pollInterval) {
		PolledMemberWaiting<C> w(that, f, pollInterval); waitUntil(w); }

	void scheduleTask(TimedWaitable& w);
	void unscheduleTask(TimedWaitable& w);
	void wakeup(TimedWaitable& w);

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

		PeriodicWakeUp& operator=(TimeInterval const& interval) {
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
#endif // ZTH_WAITER_H
