#ifndef ZTH_WAITER_H
#define ZTH_WAITER_H
/*
 * Zth (libzth), a cooperative userspace multitasking library.
 * Copyright (C) 2019-2022  Jochem Rutgers
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifdef __cplusplus

#	include <libzth/allocator.h>
#	include <libzth/fiber.h>
#	include <libzth/list.h>
#	include <libzth/time.h>
#	include <libzth/util.h>

#	if __cplusplus >= 201103L
#		include <type_traits>
#	endif

namespace zth {
class Worker;

class Waitable {
	ZTH_CLASS_NEW_DELETE(Waitable)
public:
	Waitable() noexcept
		: m_fiber()
	{}

	virtual ~Waitable() is_default

	Fiber& fiber() const noexcept
	{
		zth_assert(hasFiber());
		return *m_fiber;
	}

	virtual bool poll(Timestamp const& now = Timestamp::now()) noexcept = 0;

	virtual string str() const
	{
		return format("Waitable for %s", fiber().str().c_str());
	}

	void setFiber(Fiber& fiber) noexcept
	{
		m_fiber = &fiber;
	}

	void resetFiber() noexcept
	{
		m_fiber = nullptr;
	}

	bool hasFiber() const noexcept
	{
		return m_fiber;
	}

private:
	Fiber* m_fiber;
};

class TimedWaitable
	: public Waitable
	, public Listable<TimedWaitable> {
	ZTH_CLASS_NEW_DELETE(TimedWaitable)
public:
	explicit TimedWaitable(Timestamp const& timeout = Timestamp()) noexcept
		: m_timeout(timeout)
	{}

	virtual ~TimedWaitable() override is_default

	Timestamp const& timeout() const noexcept
	{
		return m_timeout;
	}

	virtual bool poll(Timestamp const& now = Timestamp::now()) noexcept override
	{
		return timeout() <= now;
	}

	bool operator<(TimedWaitable const& rhs) const noexcept
	{
		return timeout() < rhs.timeout();
	}

	virtual string str() const override
	{
		if(hasFiber())
			return format(
				"Waitable with %s timeout for %s",
				(timeout() - Timestamp::now()).str().c_str(),
				fiber().str().c_str());
		else
			return format(
				"Waitable with %s timeout",
				(timeout() - Timestamp::now()).str().c_str());
	}

protected:
	void setTimeout(Timestamp const& t) noexcept
	{
		m_timeout = t;
	}

private:
	Timestamp m_timeout;
};

template <typename F>
class PolledWaiting : public TimedWaitable {
	ZTH_CLASS_NEW_DELETE(PolledWaiting)
public:
	explicit PolledWaiting(F const& f, TimeInterval const& interval = TimeInterval())
		: TimedWaitable(Timestamp())
		, m_f(f)
	{
		setInterval(interval);
	}

	virtual ~PolledWaiting() override is_default

	virtual bool poll(Timestamp const& now = Timestamp::now()) noexcept override
	{
		if(m_f())
			return true;

		Timestamp next = timeout() + interval();
		if(unlikely(next < now))
			next = now + interval();

		setTimeout(next);
		return false;
	}

	TimeInterval const& interval() const noexcept
	{
		return m_interval;
	}

	void
	setInterval(TimeInterval const& interval, Timestamp const& now = Timestamp::now()) noexcept
	{
		m_interval = interval;
		setTimeout(now + interval);
	}

private:
	F m_f;
	TimeInterval m_interval;
};

template <typename C>
struct PolledMemberWaitingHelper {
	constexpr PolledMemberWaitingHelper(C& that, bool (C::*f)()) noexcept
		: that(that)
		, f(f)
	{}

	bool operator()() const
	{
		zth_assert(f != nullptr);
		// cppcheck-suppress nullPointerRedundantCheck
		return (that.*f)();
	}

	C& that;
	bool (C::*f)();
};

template <typename C>
class PolledMemberWaiting : public PolledWaiting<PolledMemberWaitingHelper<C> /**/> {
	ZTH_CLASS_NEW_DELETE(PolledMemberWaiting)
public:
	typedef PolledWaiting<PolledMemberWaitingHelper<C> /**/> base;

	constexpr PolledMemberWaiting(
		C& that, bool (C::*f)(), TimeInterval const& interval = TimeInterval()) noexcept
		: base(PolledMemberWaitingHelper<C>(that, f), interval)
	{}

	virtual ~PolledMemberWaiting() override is_default
};

class PollerServerBase;

/*!
 * \brief A single fiber per Worker that manages sleeping and blocked fibers.
 */
class Waiter : public Runnable {
	ZTH_CLASS_NEW_DELETE(Waiter)
public:
	explicit Waiter(Worker& worker);
	virtual ~Waiter() override;

	void wait(TimedWaitable& w);
	void scheduleTask(TimedWaitable& w);
	void unscheduleTask(TimedWaitable& w);
	void wakeup(TimedWaitable& w);

	PollerServerBase& poller();
	void setPoller(PollerServerBase* p = nullptr);
	void wakeup();

protected:
	bool polling() const;

	virtual int fiberHook(Fiber& f) override
	{
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
 * \brief Wait until the given Waitable has passed.
 * \ingroup zth_api_cpp_fiber
 */
ZTH_EXPORT void waitUntil(TimedWaitable& w);

/*!
 * \brief Wait until the given function \p f returns \c true.
 * \ingroup zth_api_cpp_fiber
 */
#	if __cplusplus >= 201103L
template <
	typename F,
	typename std::enable_if<!std::is_base_of<TimedWaitable, F>::value, int>::type = 0>
void waitUntil(F f, TimeInterval const& pollInterval = TimeInterval())
{
	PolledWaiting<F> w(f, pollInterval);
	waitUntil(w);
}
#	else
ZTH_EXPORT inline void waitUntil(bool (*f)(), TimeInterval const& pollInterval = TimeInterval())
{
	PolledWaiting<bool (*)()> w(f, pollInterval);
	waitUntil(w);
}
#	endif

/*!
 * \brief Wait until the given member function \p f returns \c true.
 * \ingroup zth_api_cpp_fiber
 */
template <typename C>
void waitUntil(C& that, bool (C::*f)(), TimeInterval const& pollInterval = TimeInterval())
{
	PolledMemberWaiting<C> w(that, f, pollInterval);
	waitUntil(w);
}

void scheduleTask(TimedWaitable& w);
void unscheduleTask(TimedWaitable& w);
void wakeup(TimedWaitable& w);

/*!
 * \brief Sleep until the given time stamp.
 * \ingroup zth_api_cpp_fiber
 */
ZTH_EXPORT inline void nap(Timestamp const& sleepUntil)
{
	TimedWaitable w(sleepUntil);
	waitUntil(w);
}

/*!
 * \brief Sleep for the given time interval.
 * \ingroup zth_api_cpp_fiber
 */
ZTH_EXPORT inline void nap(TimeInterval const& sleepFor)
{
	if(!sleepFor.hasPassed())
		nap(Timestamp::now() + sleepFor);
}

/*!
 * \brief Sleep for the given amount of milliseconds.
 * \ingroup zth_api_cpp_fiber
 */
ZTH_EXPORT inline void mnap(long sleepFor_ms)
{
	nap(TimeInterval((time_t)(sleepFor_ms / 1000L), sleepFor_ms % 1000L * 1000000L));
}

/*!
 * \brief Sleep for the given amount of microseconds.
 * \ingroup zth_api_cpp_fiber
 */
ZTH_EXPORT inline void unap(long sleepFor_us)
{
	nap(TimeInterval((time_t)(sleepFor_us / 1000000L), sleepFor_us % 1000000L * 1000L));
}

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
	ZTH_CLASS_NEW_DELETE(PeriodicWakeUp)
public:
	explicit PeriodicWakeUp(TimeInterval const& interval)
		: m_interval(interval)
		, m_t(Timestamp::now())
	{}

	Timestamp const& t() const noexcept
	{
		return m_t;
	}

	TimeInterval const& interval() const noexcept
	{
		return m_interval;
	}

	void setInterval(TimeInterval const& interval) noexcept
	{
		m_interval = interval;
	}

	PeriodicWakeUp& operator=(TimeInterval const& interval) noexcept
	{
		setInterval(interval);
		return *this;
	}

	bool nap()
	{
		return nap(t());
	}

	bool nap(Timestamp const& reference, Timestamp const& now = Timestamp::now());

	void operator()()
	{
		nap();
	}

private:
	TimeInterval m_interval;
	Timestamp m_t;
};

} // namespace zth

/*!
 * \copydoc zth::nap(zth::TimeInterval const&)
 * \details This is a C-wrapper for zth::nap(zth::TimeInterval const&).
 * \ingroup zth_api_c_fiber
 */
EXTERN_C ZTH_EXPORT ZTH_INLINE void zth_nap(struct timespec const* ts)
{
	if(likely(ts))
		zth::nap(zth::TimeInterval(*ts));
}

/*!
 * \copydoc zth::mnap()
 * \details This is a C-wrapper for zth::mnap().
 * \ingroup zth_api_c_fiber
 */
EXTERN_C ZTH_EXPORT ZTH_INLINE void zth_mnap(long sleepFor_ms)
{
	zth::mnap(sleepFor_ms);
}

/*!
 * \copydoc zth::unap()
 * \details This is a C-wrapper for zth::unap().
 * \ingroup zth_api_c_fiber
 */
EXTERN_C ZTH_EXPORT ZTH_INLINE void zth_unap(long sleepFor_us)
{
	zth::unap(sleepFor_us);
}

#else // !__cplusplus

#	include <libzth/macros.h>

#	include <time.h>

ZTH_EXPORT void zth_nap(struct timespec const* ts);
ZTH_EXPORT void zth_mnap(long sleepFor_ms);
ZTH_EXPORT void zth_unap(long sleepFor_us);

#endif // __cplusplus
#endif // ZTH_WAITER_H
