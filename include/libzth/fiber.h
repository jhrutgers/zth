#ifndef __ZTH_FIBER_H
#define __ZTH_FIBER_H

#ifdef __cplusplus
#include <libzth/util.h>
#include <libzth/config.h>
#include <libzth/context.h>

namespace zth
{
	class Worker;

	template <typename ChildClass>
	class ListElement {
	public:
		ListElement()
			: prev()
			, next()
		{}

		class Iterator {
		public:
			Iterator(ChildClass const& start) : m_start(&start), m_current() {}
			bool valid() const { return likely(m_current != m_start); }
			operator bool() const { return valid(); }
			Iterator& next() { m_current = likely(m_current) ? m_current->next : m_start; return *this; }
			Iterator& operator++() { return next(); }
			Iterator operator++(int) { Iterator i = *this; next(); return i; }
			ChildClass const& operator*() const { return likely(m_current) ? *m_current : *m_start; }
			ChildClass const* operator->() const { return &this->operator*(); }
		private:
			ChildClass const* m_start;
			ChildClass const* m_current;
		};

		Iterator iterate() { return Iterator(*static_cast<ChildClass*>(this)); }
		Iterator iterate() const { return Iterator(*static_cast<ChildClass const*>(this)); }

		bool listContains(ChildClass& elem) const {
			for(Iterator it = iterate(); it; ++it)
				if(&*it == &elem)
					return true;
			return false;
		}

	private:
		ChildClass* prev;
		ChildClass* next;

		friend class Worker;
	};

	class Fiber : public ListElement<Fiber> {
	public:
		enum State { New = 0, Ready, Running, Dead };

		typedef void*(*Entry)(void*);

		Fiber(Entry entry)
			: m_name("zth::Fiber")
			, m_state(New)
			, m_entry(entry)
			, m_stacksize(Config::DefaultFiberStackSize)
		{
			zth_dbg(fiber, "[%s (%p)] New fiber", name().c_str(), this);
			m_context = context_create();
		}

		~Fiber() {
			context_destroy(m_context);
			m_context = NULL;
			zth_dbg(fiber, "[%s (%p)] Destructed", name().c_str(), this);
		}

		void setName(std::string const& name) {
			zth_dbg(fiber, "[%s (%p)] Renamed to %s", this->name().c_str(), this, name.c_str());
			m_name = name;
		}

		std::string const& name() const { return m_name; }

		int setStackSize(size_t size) {
			if(state() != New)
				return EPERM;

			m_stacksize = size;
			return 0;
		}

		size_t stackSize() const { return m_stacksize; }
		Context* context() const { return m_context; }
		State state() const { return m_state; }
		Timestamp const& totalTime() const { return m_totalTime; }

		int init();

		int run(Fiber& from, Timestamp const& now = Timestamp::now()) {
			int res = 0;

			switch(state())
			{
			case New:
				// First call, do implicit init.
				if((res = init()))
					return res;
				// fall-through
			case Ready:
				{
					m_startRun = now;
					Timestamp dt = from.m_startRun.diff(now);
					from.m_totalTime += dt;
					// Hand over to this.
					zth_dbg(fiber, "Switch from %s (%p) to %s (%p) after %s", from.name().c_str(), &from, name().c_str(), this, dt.toString().c_str());
					context_switch(from.context(), context());
					// Ok, got back.
					return 0;
				}

			case Running:
				zth_assert(&from == this);
				// No switch required.
				return 0;

			case Dead:
				// Can't run dead fibers.
				return EPERM;

			default:
				zth_assert(false);
				return EINVAL;
			}
		}

		int kill() {
			zth_dbg(fiber, "[%s (%p)] Killed", name().c_str(), this);
			m_state = Dead;
			return 0;
		}

	private:
		std::string m_name;
		State m_state;
		Entry m_entry;
		size_t m_stacksize;
		Context* m_context;
		Timestamp m_totalTime;
		Timestamp m_startRun;
	};

} // namespace
#endif // __cplusplus
#endif // __ZTH_FIBER_H
