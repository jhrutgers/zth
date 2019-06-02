#ifndef __ZTH_WAITER_H
#define __ZTH_WAITER_H
#ifdef __cplusplus

#include <libzth/fiber.h>
#include <libzth/list.h>
#include <libzth/time.h>

namespace zth {
	class Worker;

	class Waiter : public Runnable {
	public:
		Waiter(Worker& worker)
			: m_worker(worker)
		{}
		virtual ~Waiter() {}

		void napping(Fiber& fiber);

	protected:
		virtual int fiberHook(Fiber& f) {
			f.setName("zth::Waiter");
			f.suspend();
			return Runnable::fiberHook(f);
		}

		virtual void entry();

	private:
		Worker& m_worker;

		struct CompareStateEnd {
			bool operator()(Fiber& lhs, Fiber& rhs) const { return lhs.stateEnd() < rhs.stateEnd(); }
		};
		SortedList<Fiber, CompareStateEnd> m_sleeping;
	};

	void nap(Timestamp const& sleepUntil);
	inline void  nap(TimeInterval const& sleepFor)	{ nap(Timestamp::now() + sleepFor); }
	inline void mnap(long sleepFor_ms)				{ nap(TimeInterval((time_t)(sleepFor_ms / 1000L), sleepFor_ms % 1000L * 1000000L)); }
	inline void unap(long sleepFor_us)				{ nap(TimeInterval((time_t)(sleepFor_us / 1000000L), sleepFor_us % 1000000L * 1000L)); }
} // namespace 
#endif // __cplusplus
#endif // __ZTH_WAITER_H
