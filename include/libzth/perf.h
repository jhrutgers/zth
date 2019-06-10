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

	std::list<void*> backtrace(size_t depth = 32);
	void print_backtrace(size_t depth = 32);

	class Fiber;

	struct PerfEvent {
		enum Type { FiberState, Marker };

		Timestamp t;
		Fiber* fiber;
		Type type;

		union {
			struct {
				int state;
			} fiberState;
			struct {
				int dummy;
			} marker;
		};
	};

	ZTH_TLS_DECLARE(std::vector<PerfEvent>*, perf_eventBuffer)

	void perf_flushEventBuffer();

	inline void perf_trackState(Fiber& fiber, int state, Timestamp const& t = Timestamp::now(), bool force = false) {
		if(unlikely(!perf_eventBuffer))
			return;

		PerfEvent e;
		e.t = t;
		e.fiber = &fiber;
		e.type = PerfEvent::FiberState;
		e.fiberState.state = state;

		perf_eventBuffer->push_back(e);
		zth_assert(perf_eventBuffer->size() <= Config::PerfEventBufferSize);

		if(unlikely(force || perf_eventBuffer->size() >= Config::PerfEventBufferThresholdToTriggerVCDWrite))
			perf_flushEventBuffer();
	}


} // namespace
#endif
#endif // __ZTH_PERF_H
