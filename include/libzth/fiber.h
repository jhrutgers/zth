#ifndef __ZTH_FIBER_H
#define __ZTH_FIBER_H

#ifdef __cplusplus
#include <libzth/util.h>
#include <libzth/time.h>
#include <libzth/config.h>
#include <libzth/context.h>
#include <libzth/list.h>
#include <libzth/perf.h>

#include <list>
#include <utility>
#include <errno.h>

namespace zth {

	class Fiber : public Listable<Fiber>, public UniqueID<Fiber> {
	public:
		enum State { Uninitialized = 0, New, Ready, Running, Waiting, Suspended, Dead };

		typedef ContextAttr::EntryArg EntryArg;
		typedef void(*Entry)(EntryArg);
		static uintptr_t const ExitUnknown = (uintptr_t)-1;

		Fiber(Entry entry, EntryArg arg = EntryArg())
			: UniqueID("zth::Fiber")
			, m_state(Uninitialized)
			, m_stateNext(Ready)
			, m_entry(entry)
			, m_entryArg(arg)
			, m_contextAttr(&fiberEntry, this)
			, m_context()
			, m_fls()
			, m_timeslice(Config::MinTimeslice_s())
		{
			setState(New);
			zth_assert(m_entry);
			zth_dbg(fiber, "[%s] New fiber %p", id_str(), normptr());
		}

		virtual ~Fiber() {
			for(decltype(m_cleanup.begin()) it = m_cleanup.begin(); it != m_cleanup.end(); ++it)
				it->first(*this, it->second);

			if(state() > Uninitialized && state() < Dead)
				kill();
			
			perf_event(PerfEvent<>(*this));

			setState(Uninitialized);
			zth_assert(!fls());
			context_destroy(m_context);
			m_context = NULL;
			zth_dbg(fiber, "[%s] Destructed. Total CPU: %s", id_str(), m_totalTime.str().c_str());
		}

		int setStackSize(size_t size) {
			if(state() != New)
				return EPERM;

			m_contextAttr.stackSize = size;
			return 0;
		}

		size_t stackSize() const { return m_contextAttr.stackSize; }
		Context* context() const { return m_context; }
		State state() const { return m_state; }
		void* fls() const { return m_fls; }
		void setFls(void* data = NULL) { m_fls = data; }
		Timestamp const& stateEnd() const { return m_stateEnd; }
		TimeInterval const& totalTime() const { return m_totalTime; }
		void addCleanup(void(*f)(Fiber&,void*), void* arg) { m_cleanup.push_back(std::make_pair(f, arg)); }

		int init(Timestamp const& now = Timestamp::now()) {
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

		int run(Fiber& from, Timestamp const& now = Timestamp::now()) {
			int res = 0;

		again:
			switch(state())
			{
			case New:
				// First call, do implicit init.
				if((res = init(now)))
					return res;
				goto again;

			case Ready:
				{
					// Update administration of the current fiber.
					TimeInterval dt = now - from.m_startRun;
					from.m_totalTime += dt;
					if(from.state() == Running)
						from.setState(Ready, now);

					// Hand over to this.
					m_startRun = now;
					setState(Running, now);
					m_stateEnd = now + m_timeslice;

					zth_dbg(fiber, "Switch from %s to %s after %s", from.id_str(), id_str(), dt.str().c_str());
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

			default:
				zth_assert(false);
				return EINVAL;
			}
		}

		bool allowYield(Timestamp const& now = Timestamp::now()) {
			return state() != Running || m_stateEnd < now;
		}

		void kill() {
			if(state() == Dead)
				return;

			zth_dbg(fiber, "[%s] Killed", id_str());
			setState(Dead);
		}

		void nap(Timestamp const& sleepUntil = Timestamp::null()) {
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
					zth_dbg(fiber, "[%s] Sleep for %s", id_str(), (sleepUntil - Timestamp::now()).str().c_str());
				setState(Waiting);
				m_stateNext = Ready;
				break;
			case Suspended:
				zth_dbg(fiber, "[%s] Schedule sleep after resume", id_str());
				m_stateNext = Waiting;
				break;
			case Waiting:
			default:
				return;
			}

			m_stateEnd = sleepUntil;
		}
		
		void wakeup() {
			if(likely(state() == Waiting)) {
				setState(m_stateNext);
				switch(state()) {
				case Suspended:
					zth_dbg(fiber, "[%s] Suspend after wakeup", id_str());
					m_stateNext = Ready;
					break;
				default:
					zth_dbg(fiber, "[%s] Wakeup", id_str());
				}
			} else if(state() == Suspended && m_stateNext == Waiting) {
				zth_dbg(fiber, "[%s] Set wakeup after suspend", id_str());
				m_stateNext = Ready;
			}
		}

		void suspend() {
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
			case Suspended:
			default:
				// Ignore.
				return;
			}

			zth_dbg(fiber, "[%s] Suspend", id_str());
			setState(Suspended);
		}

		void resume() {
			if(state() != Suspended)
				return;

			zth_dbg(fiber, "[%s] Resume", id_str());
			setState(m_stateNext);
			if(state() == New)
				m_stateNext = Ready;
		}

		std::string str() const {
			std::string res = format("%s", id_str());

			switch(state()) {
			case Uninitialized:		res += " Uninitialized"; break;
			case New:				res += " New"; break;
			case Ready:				res += " Ready"; break;
			case Running:			res += " Running"; break;
			case Waiting:			res += " Waiting";
									if(!m_stateEnd.isNull())
										res += format(" (%s remaining)", Timestamp::now().timeTo(stateEnd()).str().c_str());
									break;
			case Suspended:			res += " Suspended"; break;
			case Dead:				res += " Dead"; break;
			}

			res += format(" t=%s", m_totalTime.str().c_str());
			return res;
		}

	protected:
		virtual void changedName(std::string const& name) {
			zth_dbg(fiber, "[%s] Renamed to %s", id_str(), name.c_str());
		}

		void setState(State state, Timestamp const& t = Timestamp::now()) {
			if(m_state == state)
				return;

			m_state = state;

			perf_event(PerfEvent<>(*this, m_state, t));
		}

		static void fiberEntry(void* that) {
			zth_assert(that);
			static_cast<Fiber*>(that)->fiberEntry_();
		}

		void fiberEntry_() {
			zth_dbg(fiber, "[%s] Entry", id_str());
#ifdef __cpp_exceptions
			try {
#endif
				m_entry(m_entryArg);
#ifdef __cpp_exceptions
			} catch(...) {
				zth_dbg(fiber, "[%s] Uncaught exception", id_str());
			}
#endif
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
		std::list<std::pair<void(*)(Fiber&,void*),void*> > m_cleanup;
	};

	class Runnable {
	public:
		Runnable() 
			: m_fiber()
		{}

		virtual ~Runnable() {}
		int run();
		Fiber* fiber() const { return m_fiber; }
		operator Fiber&() { zth_assert(fiber()); return *fiber(); }

	protected:
		virtual int fiberHook(Fiber& f) {
			f.addCleanup(&cleanup_, this);
			m_fiber = &f;
			return 0;
		}

		virtual void cleanup() {}
		static void cleanup_(Fiber& f, void* that) {
			Runnable* r = (Runnable*)that;
			if(r) {
				zth_assert(&f == r->m_fiber);
				r->cleanup();
			}
		}

		virtual void entry() = 0;
		static void entry_(void* that) { if(that) ((Runnable*)that)->entry(); }

	private:
		Runnable(Runnable const&);
		Runnable& operator=(Runnable&);
		Fiber* m_fiber;
	};


} // namespace
#endif // __cplusplus
#endif // __ZTH_FIBER_H
