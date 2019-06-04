#ifndef __ZTH_FIBER_H
#define __ZTH_FIBER_H

#ifdef __cplusplus
#include <libzth/util.h>
#include <libzth/time.h>
#include <libzth/config.h>
#include <libzth/context.h>
#include <libzth/list.h>

#include <list>
#include <utility>

namespace zth
{
	class Worker;

	class Fiber : public Listable<Fiber> {
	public:
		enum State { New = 0, Ready, Running, Waiting, Suspended, Dead };

		typedef ContextAttr::EntryArg EntryArg;
		typedef void(*Entry)(EntryArg);
		static uintptr_t const ExitUnknown = (uintptr_t)-1;

		Fiber(Entry entry, EntryArg arg = EntryArg())
			: m_name("zth::Fiber")
			, m_state(New)
			, m_stateNext(Ready)
			, m_entry(entry)
			, m_entryArg(arg)
			, m_contextAttr(&fiberEntry, this)
			, m_context()
			, m_timeslice(Config::MinTimeslice_s())
		{
			zth_assert(m_entry);
			zth_dbg(fiber, "[%s (%p)] New fiber", name().c_str(), this);
		}

		virtual ~Fiber() {
			for(decltype(m_cleanup.begin()) it = m_cleanup.begin(); it != m_cleanup.end(); ++it)
				it->first(*this, it->second);

			context_destroy(m_context);
			m_context = NULL;
			zth_dbg(fiber, "[%s (%p)] Destructed. Total CPU: %s", name().c_str(), this, m_totalTime.str().c_str());
		}

		void setName(std::string const& name) {
			zth_dbg(fiber, "[%s (%p)] Renamed to %s", this->name().c_str(), this, name.c_str());
			m_name = name;
		}

		std::string const& name() const { return m_name; }

		int setStackSize(size_t size) {
			if(state() != New)
				return EPERM;

			m_contextAttr.stackSize = size;
			return 0;
		}

		size_t stackSize() const { return m_contextAttr.stackSize; }
		Context* context() const { return m_context; }
		State state() const { return m_state; }
		Timestamp const& stateEnd() const { return m_stateEnd; }
		TimeInterval const& totalTime() const { return m_totalTime; }
		void addCleanup(void(*f)(Fiber&,void*), void* arg) { m_cleanup.push_back(std::make_pair(f, arg)); }

		int init(Timestamp const& now = Timestamp::now()) {
			if(state() != New)
				return EPERM;

			zth_assert(!m_context);

			zth_dbg(fiber, "[%s (%p)] Init", this->name().c_str(), this);
			int res = context_create(m_context, m_contextAttr);
			if(res) {
				// Oops.
				kill();
				return res;
			}

			m_state = m_stateNext;
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
						from.m_state = Ready;

					// Hand over to this.
					m_startRun = now;
					m_state = Running;
					m_stateEnd = now + m_timeslice;

					zth_dbg(fiber, "Switch from %s (%p) to %s (%p) after %s", from.name().c_str(), &from, name().c_str(), this, dt.str().c_str());
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

			zth_dbg(fiber, "[%s (%p)] Killed", name().c_str(), this);
			m_state = Dead;
		}

		void nap(Timestamp const& sleepUntil) {
			switch(state()) {
			case New:
				// Postpone actual sleep
				zth_dbg(fiber, "[%s (%p)] Sleep upon startup", name().c_str(), this);
				m_stateNext = Waiting;
				break;
			case Ready:
			case Running:
				zth_dbg(fiber, "[%s (%p)] Sleep", name().c_str(), this);
				m_state = Waiting;
				m_stateNext = Ready;
				break;
			case Suspended:
				zth_dbg(fiber, "[%s (%p)] Schedule sleep after resume", name().c_str(), this);
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
				switch((m_state = m_stateNext)) {
				case Suspended:
					zth_dbg(fiber, "[%s (%p)] Suspend after wakeup", name().c_str(), this);
					m_stateNext = Ready;
					break;
				default:
					zth_dbg(fiber, "[%s (%p)] Wakeup", name().c_str(), this);
				}
			} else if(state() == Suspended && m_stateNext == Waiting) {
				zth_dbg(fiber, "[%s (%p)] Set wakeup after suspend", name().c_str(), this);
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

			zth_dbg(fiber, "[%s (%p)] Suspend", name().c_str(), this);
			m_state = Suspended;
		}

		void resume() {
			if(state() != Suspended)
				return;

			zth_dbg(fiber, "[%s (%p)] Resume", name().c_str(), this);
			if((m_state = m_stateNext) == New)
				m_stateNext = Ready;
		}

		std::string str() const {
			std::string res = format("%s (%p)", name().c_str(), this);

			switch(state()) {
			case New:		res += " New"; break;
			case Ready:		res += " Ready"; break;
			case Running:	res += " Running"; break;
			case Waiting:	res += " Waiting"; break;
			case Suspended:	res += " Suspended"; break;
			case Dead:		res += " Dead"; break;
			}

			res += format(" t=%s", m_totalTime.str().c_str());
			return res;
		}

	protected:
		static void fiberEntry(void* that) {
			zth_assert(that);
			static_cast<Fiber*>(that)->fiberEntry_();
		}

		void fiberEntry_() {
			zth_dbg(fiber, "[%s (%p)] Entry", name().c_str(), this);
#ifdef __cpp_exceptions
			try {
#endif
				m_entry(m_entryArg);
#ifdef __cpp_exceptions
			} catch(...) {
				zth_dbg(fiber, "[%s (%p)] Uncaught exception", name().c_str(), this);
			}
#endif
			zth_dbg(fiber, "[%s (%p)] Exit", name().c_str(), this);
			kill();
		}

	private:
		std::string m_name;
		State m_state;
		State m_stateNext;
		Entry m_entry;
		EntryArg m_entryArg;
		ContextAttr m_contextAttr;
		Context* m_context;
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
