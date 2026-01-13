#ifndef ZTH_FIBER_H
#define ZTH_FIBER_H
/*
 * SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
 *
 * SPDX-License-Identifier: MPL-2.0
 */

/*!
 * \defgroup zth_api_cpp_fiber fiber
 * \ingroup zth_api_cpp
 */
/*!
 * \defgroup zth_api_c_fiber fiber
 * \ingroup zth_api_c
 */

#include <libzth/macros.h>

#ifdef __cplusplus
#  include <libzth/allocator.h>
#  include <libzth/config.h>
#  include <libzth/context.h>
#  include <libzth/init.h>
#  include <libzth/list.h>
#  include <libzth/perf.h>
#  include <libzth/time.h>
#  include <libzth/util.h>

#  include <errno.h>
#  include <exception>
#  include <utility>

#  if __cplusplus >= 201103L
#    include <functional>
#  endif

namespace zth {

/*!
 * \brief The fiber.
 *
 * This class manages a fiber's context and state, given an entry function.
 *
 * Usually, don't subclass this class (use #zth::Runnable instead), as the
 * #zth::Fiber is owned by and part of a #zth::Worker's administration. For
 * example, a Fiber object is deleted by the Worker when it is dead.
 */
class Fiber : public Listable<Fiber>, public UniqueID<Fiber> {
	ZTH_CLASS_NEW_DELETE(Fiber)
public:
	typedef void(FiberHook)(Fiber&);

	/*!
	 * \brief Hook to be called when a Fiber is created.
	 *
	 * This function is called after initialization of the fiber.
	 */
	static FiberHook* hookNew;

	/*!
	 * \brief Hook to be called when a Fiber is destroyed.
	 *
	 * This function is called just after the fiber has died.
	 */
	static FiberHook* hookDead;

	enum State { Uninitialized = 0, New, Ready, Running, Waiting, Suspended, Dead };

	typedef ContextAttr::EntryArg EntryArg;
	typedef void (*Entry)(EntryArg);
	static uintptr_t const ExitUnknown = (uintptr_t)-1;

	explicit Fiber(Entry entry, EntryArg arg = EntryArg())
		: UniqueID("zth::Fiber")
		, m_state(Uninitialized)
		, m_stateNext(Ready)
		, m_entry(entry)
		, m_entryArg(arg)
		, m_contextAttr(&fiberEntry, this)
		, m_context()
		, m_fls()
		, m_timeslice(Config::MinTimeslice())
		, m_dtMax(Config::CheckTimesliceOverrun
				  ? TimeInterval(Config::TimesliceOverrunReportThreshold())
				  : TimeInterval())
	{
		zth_init();
		setState(New);
		zth_assert(m_entry);
		zth_dbg(fiber, "[%s] New fiber %p", id_str(), normptr());
	}

	virtual ~Fiber() override
	{
		for(decltype(m_cleanup.begin()) it = m_cleanup.begin(); it != m_cleanup.end(); ++it)
			it->first(*this, it->second);

		if(state() > Uninitialized && state() < Dead)
			kill();

		zth_perf_event(*this);

		setState(Uninitialized);
		zth_assert(!fls());
		size_t stackSize_ __attribute__((unused)) = this->stackSize();
		size_t stackUsage_ __attribute__((unused)) = this->stackUsage();
		context_destroy(m_context);
		m_context = nullptr;
		zth_dbg(fiber, "[%s] Destructed. Stack usage: 0x%x of 0x%x, total CPU: %s",
			id_str(), (unsigned int)stackUsage_, (unsigned int)stackSize_,
			m_totalTime.str().c_str());
	}

	int setStackSize(size_t size) noexcept
	{
		if(state() != New)
			return EPERM;

		m_contextAttr.stackSize = size;
		return 0;
	}

	size_t stackSize() const noexcept
	{
		return m_contextAttr.stackSize;
	}

	size_t stackUsage() const
	{
		return context_stack_usage(context());
	}

	Context* context() const noexcept
	{
		return m_context;
	}

	State state() const noexcept
	{
		return m_state;
	}

	void* fls() const noexcept
	{
		return m_fls;
	}

	void setFls(void* data = nullptr) noexcept
	{
		m_fls = data;
	}

	Timestamp const& runningSince() const noexcept
	{
		return m_startRun;
	}

	Timestamp const& stateEnd() const noexcept
	{
		return m_stateEnd;
	}

	TimeInterval const& totalTime() const noexcept
	{
		return m_totalTime;
	}

	void addCleanup(void (*f)(Fiber&, void*), void* arg)
	{
		m_cleanup.push_back(std::make_pair(f, arg));
	}

	int init(Timestamp const& now = Timestamp::now())
	{
		if(state() != New)
			return EPERM;

		zth_assert(!m_context);

		zth_dbg(fiber, "[%s] Init", id_str());
		int res = context_create(m_context, m_contextAttr);
		if(res) {
			// Oops.
			kill();
			return res;
		}

		setState(m_stateNext, now);
		m_stateNext = Ready;
		m_startRun = now;

		return 0;
	}

	int run(Fiber& from, Timestamp now = Timestamp::now())
	{
		int res = 0;

again:
		switch(state()) {
		case New:
			// First call, do implicit init.
			if((res = init(now)))
				return res;

			if(hookNew)
				hookNew(*this);
			goto again;

		case Ready: {
			zth_assert(&from != this);

			// Update administration of the current fiber.
			TimeInterval dt = now - from.m_startRun;
			from.m_totalTime += dt;

			if(from.state() == Running)
				from.setState(Ready, now);

			if(unlikely(zth_config(CheckTimesliceOverrun) && from.m_dtMax < dt)) {
				perf_mark("timeslice overrun reported");
				from.m_dtMax = dt;
				log_color(
					Config::Print_perf,
					ZTH_DBG_PREFIX "Long timeslice by %s of %s\n",
					from.id_str(), dt.str().c_str());
				Backtrace().print(Config::Print_perf);
				now = Timestamp::now();
			}

			// Hand over to this.
			m_startRun = now;
			setState(Running, now);
			m_stateEnd = now + m_timeslice;

			zth_dbg(fiber, "Switch from %s to %s after %s", from.id_str(), id_str(),
				dt.str().c_str());
			context_switch(from.context(), context());

			// Ok, got back.
			// Warning! This fiber might already be dead and destructed at this point!
			// Only return here!
			return 0;
		}

		case Running:
			zth_assert(&from == this);
			// No switch required.
			return EAGAIN;

		case Dead:
			// Can't run dead fibers.
			return EPERM;

		case Uninitialized:
		case Waiting:
		case Suspended:
		default:
			zth_assert(false);
			return EINVAL;
		}
	}

	bool allowYield(Timestamp const& now = Timestamp::now()) const noexcept
	{
		return state() != Running || m_stateEnd < now;
	}

	void kill() noexcept
	{
		if(state() == Dead)
			return;

		zth_dbg(fiber, "[%s] Killed", id_str());
		setState(Dead);
	}

	void nap(Timestamp const& sleepUntil = Timestamp::null()) noexcept
	{
		switch(state()) {
		case New:
			// Postpone actual sleep
			zth_dbg(fiber, "[%s] Sleep upon startup", id_str());
			m_stateNext = Waiting;
			break;
		case Ready:
		case Running:
			if(sleepUntil.isNull())
				zth_dbg(fiber, "[%s] Sleep", id_str());
			else
				zth_dbg(fiber, "[%s] Sleep for %s", id_str(),
					(sleepUntil - Timestamp::now()).str().c_str());
			setState(Waiting);
			m_stateNext = Ready;
			break;
		case Suspended:
			zth_dbg(fiber, "[%s] Schedule sleep after resume", id_str());
			m_stateNext = Waiting;
			break;
		case Waiting:
		case Uninitialized:
		case Dead:
		default:
			return;
		}

		m_stateEnd = sleepUntil;
	}

	void wakeup() noexcept
	{
		if(likely(state() == Waiting)) {
			setState(m_stateNext);
			switch(state()) {
			case Suspended:
				zth_dbg(fiber, "[%s] Suspend after wakeup", id_str());
				m_stateNext = Ready;
				break;
			case Uninitialized:
			case New:
			case Ready:
			case Running:
			case Waiting:
			case Dead:
			default:
				zth_dbg(fiber, "[%s] Wakeup", id_str());
			}
		} else if(state() == Suspended && m_stateNext == Waiting) {
			zth_dbg(fiber, "[%s] Set wakeup after suspend", id_str());
			m_stateNext = Ready;
		}
	}

	void suspend() noexcept
	{
		switch(state()) {
		case New:
			m_stateNext = New;
			break;
		case Running:
		case Ready:
			m_stateNext = Ready;
			break;
		case Waiting:
			m_stateNext = Waiting;
			// fall-through
		case Suspended:
		case Uninitialized:
		case Dead:
		default:
			// Ignore.
			return;
		}

		zth_dbg(fiber, "[%s] Suspend", id_str());
		setState(Suspended);
	}

	void resume() noexcept
	{
		if(state() != Suspended)
			return;

		zth_dbg(fiber, "[%s] Resume", id_str());
		setState(m_stateNext);
		if(state() == New)
			m_stateNext = Ready;
	}

	string str() const
	{
		string res = format("%s", id_str());

		switch(state()) {
		case Uninitialized:
			res += " Uninitialized";
			break;
		case New:
			res += " New";
			break;
		case Ready:
			res += " Ready";
			break;
		case Running:
			res += " Running";
			break;
		case Waiting:
			res += " Waiting";
			if(!m_stateEnd.isNull())
				res +=
					format(" (%s remaining)",
					       Timestamp::now().timeTo(stateEnd()).str().c_str());
			break;
		case Suspended:
			res += " Suspended";
			break;
		case Dead:
			res += " Dead";
			break;
		default:;
		}

		res += format(" t=%s", m_totalTime.str().c_str());
		return res;
	}

private:
	virtual void changedName(string const& name) override
	{
		zth_dbg(fiber, "[%s] Renamed to %s", id_str(), name.c_str());
	}

protected:
	void setState(State state, Timestamp const& t = Timestamp::now()) noexcept
	{
		if(m_state == state)
			return;

		m_state = state;

		zth_perf_event(*this, m_state, t);

		if(state == Dead && hookDead)
			hookDead(*this);
	}

	static void fiberEntry(void* that) noexcept
	{
		zth_assert(that);
		// cppcheck-suppress nullPointerRedundantCheck
		static_cast<Fiber*>(that)->fiberEntry_();
	}

	void fiberEntry_() noexcept
	{
		zth_dbg(fiber, "[%s] Entry", id_str());

		try {
			m_entry(m_entryArg);
		} catch(std::exception const& e) {
#  ifdef __cpp_exceptions
			zth_dbg(fiber, "[%s] Uncaught exception; %s", id_str(), e.what());
#  endif
		} catch(...) {
			zth_dbg(fiber, "[%s] Uncaught exception", id_str());
		}

		zth_dbg(fiber, "[%s] Exit", id_str());
		kill();
	}

private:
	State m_state;
	State m_stateNext;
	Entry m_entry;
	EntryArg m_entryArg;
	ContextAttr m_contextAttr;
	Context* m_context;
	void* m_fls;
	TimeInterval m_totalTime;
	Timestamp m_startRun;
	Timestamp m_stateEnd;
	TimeInterval m_timeslice;
	TimeInterval m_dtMax;
	list_type<std::pair<void (*)(Fiber&, void*), void*> /**/>::type m_cleanup;
};

/*!
 * \brief An abstract class, that can be started as a fiber.
 *
 * Create a subclass of this class if you want to have an object that can be
 * started as a fiber. In contrast to a #zth::Fiber, this class does not have
 * to be on the heap and exists before and after the actual fiber.
 *
 * \ingroup zth_api_cpp_util
 */
class Runnable {
	ZTH_CLASS_NOCOPY(Runnable)
	ZTH_CLASS_NEW_DELETE(Runnable)
protected:
	constexpr Runnable() noexcept
		: m_fiber()
	{}

public:
	virtual ~Runnable() is_default

	int run();

	Fiber* fiber() const noexcept
	{
		return m_fiber;
	}

	operator Fiber&() const noexcept
	{
		// cppcheck-suppress constVariablePointer
		Fiber* const f = fiber();
		zth_assert(f);
		// cppcheck-suppress nullPointerRedundantCheck
		return *f;
	}

	char const* id_str() const
	{
		return fiber() ? fiber()->id_str() : "detached Runnable";
	}

protected:
	virtual int fiberHook(Fiber& f)
	{
		f.addCleanup(&cleanup_, this);
		m_fiber = &f;
		return 0;
	}

	virtual void cleanup()
	{
		m_fiber = nullptr;
	}

	virtual void entry() = 0;

private:
	static void cleanup_(Fiber& UNUSED_PAR(f), void* that)
	{
		Runnable* r = static_cast<Runnable*>(that);
		if(likely(r)) {
			zth_assert(&f == r->m_fiber);
			r->cleanup();
		}
	}

	static void entry_(void* that)
	{
		if(likely(that))
			static_cast<Runnable*>(that)->entry();
	}

private:
	Fiber* m_fiber;
};

__attribute__((pure)) Fiber& currentFiber() noexcept;

/*!
 * \brief Return the fiber-local storage, as set by setFls().
 * \ingroup zth_api_cpp_fiber
 */
ZTH_EXPORT inline void* fls() noexcept
{
	return currentFiber().fls();
}

/*!
 * \brief Set the fiber-local storage.
 *
 * This is just like thread-local storage, but per fiber.
 *
 * \ingroup zth_api_cpp_fiber
 */
ZTH_EXPORT inline void setFls(void* data = nullptr) noexcept
{
	return currentFiber().setFls(data);
}

} // namespace zth

/*!
 * \copydoc zth::fls()
 * \details This is a C-wrapper for zth::fls().
 * \ingroup zth_api_c_fiber
 */
EXTERN_C ZTH_EXPORT ZTH_INLINE void* zth_fls() noexcept
{
	return zth::fls();
}

/*!
 * \copydoc zth::setFls()
 * \details This is a C-wrapper for zth::setFls().
 * \ingroup zth_api_c_fiber
 */
EXTERN_C ZTH_EXPORT ZTH_INLINE void zth_setFls(void* data = nullptr) noexcept
{
	zth::setFls(data);
}

#else // !__cplusplus

ZTH_EXPORT void* zth_fls();
ZTH_EXPORT void* zth_setFls(void* data);

#endif // __cplusplus

EXTERN_C ZTH_EXPORT int main_fiber(int argc, char** argv);

#endif // ZTH_FIBER_H
