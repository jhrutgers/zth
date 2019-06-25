#ifndef __ZTH_PERF_H
#define __ZTH_PERF_H
#ifdef __cplusplus

#include <libzth/config.h>
#include <libzth/time.h>
#include <libzth/util.h>

#include <list>
#include <vector>
#include <stdlib.h>

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

	template <bool Enable = Config::EnablePerfEvent>
	struct PerfEvent {
		enum Type { Nothing, FiberName, FiberState, Log, Marker };

		PerfEvent() : type(Nothing) {}

		PerfEvent(UniqueID<Fiber> const& fiber)
			: fiber(fiber.id()), type(FiberName), str(strdup(fiber.name().c_str())) {}
		
		PerfEvent(UniqueID<Fiber> const& fiber, int state, Timestamp const& t = Timestamp::now())
			: t(t), fiber(fiber.id()), type(FiberState), fiberState(state) {}
		
		PerfEvent(UniqueID<Fiber> const& fiber, std::string const& str, Timestamp const& t = Timestamp::now())
			: t(t), fiber(fiber.id()), type(Log), str(strdup(str.c_str())) {}
		
		PerfEvent(UniqueID<Fiber> const& fiber, char const* marker, Timestamp const& t = Timestamp::now())
			: t(t), fiber(fiber.id()), type(Marker), c_str(marker) {}

		PerfEvent(UniqueID<Fiber> const& fiber, Timestamp const& t, char const* fmt, ...) __attribute__((format(ZTH_ATTR_PRINTF, 4, 5)))
			: t(t), fiber(fiber.id()), type(Log)
		{
			va_list args;
			va_start(args, fmt);
			if(vasprintf(&str, fmt, args) == -1)
				str = NULL;
			va_end(args);
		}

		PerfEvent(UniqueID<Fiber> const& fiber, Timestamp const& t, char const* fmt, va_list args)
			: t(t), fiber(fiber.id()), type(Log) { if(vasprintf(&str, fmt, args) == -1) str = NULL; }

		void release() {
			switch(type) {
			case FiberName:
			case Log:
				if(str) {
					free(str);
					str = NULL;
				}
				break;
			default:;
			}
		}

		Timestamp t;
		uint64_t fiber;
		Type type;

		union {
			char* str;
			char const* c_str;
			int fiberState;
		};
	};
	
	template <>
	struct PerfEvent<false> {
		enum Type { Nothing, FiberName, FiberState, Log, Marker };

		PerfEvent() {}
		PerfEvent(UniqueID<Fiber> const& fiber) {}
		PerfEvent(UniqueID<Fiber> const& fiber, int state, Timestamp const& t = Timestamp()) {}
		PerfEvent(UniqueID<Fiber> const& fiber, std::string const& str, Timestamp const& t = Timestamp()) {}
		PerfEvent(UniqueID<Fiber> const& fiber, char const* marker, Timestamp const& t = Timestamp()) {}
		PerfEvent(UniqueID<Fiber> const& fiber, Timestamp const& t, char const* fmt, ...) __attribute__((format(ZTH_ATTR_PRINTF, 4, 5))) {}
		PerfEvent(UniqueID<Fiber> const& fiber, Timestamp const& t, char const* fmt, va_list args) {}
		void release() {}

		union {
			struct timespec t;
			uint64_t fiber;
			Type type;
			char* str;
			char const* c_str;
			int fiberState;
		};
	};

	typedef std::vector<PerfEvent<> > perf_eventBuffer_type;
	ZTH_TLS_DECLARE(perf_eventBuffer_type*, perf_eventBuffer)

	void perf_flushEventBuffer();

	inline void perf_event(PerfEvent<> const& event) {
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
