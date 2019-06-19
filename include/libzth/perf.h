#ifndef __ZTH_PERF_H
#define __ZTH_PERF_H
#ifdef __cplusplus

#include <libzth/config.h>
#include <libzth/time.h>
#include <libzth/util.h>

#include <list>
#include <vector>

namespace zth {

	int perf_init();
	void perf_deinit();

	class Fiber;

	class Backtrace {
	public:
		static size_t const maxDepth = 32;
		Backtrace(size_t skip = 0);
		Fiber* fiber() const { return m_fiber; }
		uint64_t fiberId() const { return m_fiberId; }
		void print() const;
	private:
		Fiber* m_fiber;
		uint64_t m_fiberId;
		size_t m_depth;
		void* m_bt[maxDepth];
		void* m_sp;
	};

	struct PerfEvent {
		enum Type { Nothing, FiberName, FiberState };

		PerfEvent() : type(Nothing) {}

		PerfEvent(UniqueID<Fiber> const& fiber)
			: fiber(fiber.id()), type(FiberName), str(strdup(fiber.name().c_str())) {}
		
		PerfEvent(UniqueID<Fiber> const& fiber, int state, Timestamp const& t = Timestamp::now())
			: t(t), fiber(fiber.id()), type(FiberState), fiberState(state) {}

		Timestamp t;
		uint64_t fiber;
		Type type;

		union {
			char* str;
			int fiberState;
		};
	};

	ZTH_TLS_DECLARE(std::vector<PerfEvent>*, perf_eventBuffer)

	void perf_flushEventBuffer();

	inline void perf_event(PerfEvent const& event) {
		if(!Config::EnablePerfEvent)
			return;
		if(unlikely(!perf_eventBuffer))
			return;

		perf_eventBuffer->push_back(event);
		zth_assert(perf_eventBuffer->size() <= Config::PerfEventBufferSize);

		if(unlikely(perf_eventBuffer->size() >= Config::PerfEventBufferThresholdToTriggerVCDWrite))
			perf_flushEventBuffer();
	}

} // namespace
#endif
#endif // __ZTH_PERF_H
