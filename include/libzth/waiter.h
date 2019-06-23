#ifndef __ZTH_WAITER_H
#define __ZTH_WAITER_H
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

	class AwaitFd : public Waitable, public Listable<AwaitFd> {
	public:
		enum AwaitType { AwaitRead, AwaitWrite, AwaitExcept };
		AwaitFd(int fd, AwaitType awaitType) : m_fd(fd), m_awaitType(awaitType), m_ready() {}
		virtual ~AwaitFd() {}
		virtual bool poll(Timestamp const& now = Timestamp::now()) { zth_abort("Don't call."); }

		int fd() const { return m_fd; }
		AwaitType awaitType() const { return m_awaitType; }

		virtual std::string str() const {
			char const* t = "";
			switch(awaitType()) {
			case AwaitRead: t = "read"; break;
			case AwaitWrite: t = "write"; break;
			case AwaitExcept: t = "except"; break;
			}
			return format("Waitable to %s fd %d for %s", t, fd(), fiber().str().c_str());
		}

		void setReady(bool ready) { m_ready = ready; }
		bool ready() const { return m_ready; }
	private:
		int const m_fd;
		AwaitType const m_awaitType;
		bool m_ready;
	};

	class Waiter : public Runnable {
	public:
		Waiter(Worker& worker)
			: m_worker(worker)
		{}
		virtual ~Waiter() {}

		void wait(TimedWaitable& w);
		void waitFd(AwaitFd& w);

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
