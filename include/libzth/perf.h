#ifndef ZTH_PERF_H
#define ZTH_PERF_H
/*
 * Zth (libzth), a cooperative userspace multitasking library.
 * Copyright (C) 2019-2021  Jochem Rutgers
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

/*!
 * \defgroup zth_api_cpp_perf perf
 * \ingroup zth_api_cpp
 */
/*!
 * \defgroup zth_api_c_perf perf
 * \ingroup zth_api_c
 */

#ifdef __cplusplus

#include <libzth/macros.h>
#include <libzth/config.h>
#include <libzth/time.h>
#include <libzth/util.h>

#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <list>
#include <vector>

namespace zth {

	int perf_init();
	void perf_deinit();

	class Fiber;
	//__attribute__((pure)) UniqueID<Fiber> const& currentFiberID();
	UniqueID<Fiber> const& currentFiberID();

	class Backtrace {
	public:
		Backtrace(size_t skip = 0, size_t maxDepth = 128);
		Fiber* fiber() const { return m_fiber; }
		uint64_t fiberId() const { return m_fiberId; }
		std::vector<void*> const& bt() const { return m_bt; }
		bool truncated() const { return m_truncated; }
		Timestamp const& t0() const { return m_t0; }
		Timestamp const& t1() const { return m_t1; }
		void printPartial(size_t start, ssize_t end = -1, int color = -1) const;
		void print(int color = -1) const;
		void printDelta(Backtrace const& other, int color = -1) const;
	private:
		Timestamp m_t0;
		Timestamp m_t1;
		Fiber* m_fiber;
		uint64_t m_fiberId;
		std::vector<void*> m_bt;
		bool m_truncated;
	};

	/*!
	 * \ingroup zth_api_cpp_perf
	 */
	template <bool Enable = Config::EnablePerfEvent>
	struct PerfEvent {
		enum Type { Nothing, FiberName, FiberState, Log, Marker };

		PerfEvent() : type(Nothing) {}

		explicit PerfEvent(UniqueID<Fiber> const& fiber)
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

		PerfEvent(UniqueID<Fiber> const& fiber, Timestamp const& t, char const* fmt, va_list args) __attribute__((format(ZTH_ATTR_PRINTF, 4, 0)))
			: t(t), fiber(fiber.id()), type(Log) { if(vasprintf(&str, fmt, args) == -1) str = NULL; }

		// release() is called by PerfFiber (see perf.cpp), not by the dtor of PerfEvent.
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

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
	template <>
	struct PerfEvent<false> {
		enum Type { Nothing, FiberName, FiberState, Log, Marker };

		PerfEvent() {}
		explicit PerfEvent(UniqueID<Fiber> const& fiber) {}
		PerfEvent(UniqueID<Fiber> const& fiber, int state, Timestamp const& t = Timestamp()) {}
		PerfEvent(UniqueID<Fiber> const& fiber, std::string const& str, Timestamp const& t = Timestamp()) {}
		PerfEvent(UniqueID<Fiber> const& fiber, char const* marker, Timestamp const& t = Timestamp()) {}
		PerfEvent(UniqueID<Fiber> const& fiber, Timestamp const& t, char const* fmt, ...) __attribute__((format(ZTH_ATTR_PRINTF, 4, 5))) {}
		PerfEvent(UniqueID<Fiber> const& fiber, Timestamp const& t, char const* fmt, va_list args) __attribute__((format(ZTH_ATTR_PRINTF, 4, 0))) {}
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
#pragma GCC diagnostic pop

	typedef std::vector<PerfEvent<> > perf_eventBuffer_type;
	ZTH_TLS_DECLARE(perf_eventBuffer_type*, perf_eventBuffer)

	void perf_flushEventBuffer();

	/*!
	 * \ingroup zth_api_cpp_perf
	 */
	ZTH_EXPORT inline void perf_event(PerfEvent<> const& event) {
		if(!Config::EnablePerfEvent || unlikely(!perf_eventBuffer)) {
			// Release now, as there is no event buffer...
			PerfEvent<> e(event);
			e.release();
			return;
		}

		perf_eventBuffer->push_back(event);
		zth_assert(perf_eventBuffer->size() <= Config::PerfEventBufferSize);

		if(unlikely(perf_eventBuffer->size() >= Config::PerfEventBufferThresholdToTriggerVCDWrite))
			perf_flushEventBuffer();
	}

	/*!
	 * \ingroup zth_api_cpp_perf
	 */
	ZTH_EXPORT inline void perf_mark(char const* marker) {
		if(Config::EnablePerfEvent)
			perf_event(PerfEvent<>(currentFiberID(), marker));
	}

	/*!
	 * \ingroup zth_api_cpp_perf
	 */
	ZTH_EXPORT __attribute__((format(ZTH_ATTR_PRINTF, 1, 2))) inline void perf_log(char const* fmt, ...) {
		if(!Config::EnablePerfEvent)
			return;

		va_list args;
		va_start(args, fmt);
		perf_event(PerfEvent<>(currentFiberID(), Timestamp::now(), fmt, args));
		va_end(args);
	}

	/*!
	 * \ingroup zth_api_cpp_perf
	 */
	ZTH_EXPORT __attribute__((format(ZTH_ATTR_PRINTF, 1, 0))) inline void perf_logv(char const* fmt, va_list args) {
		if(Config::EnablePerfEvent)
			perf_event(PerfEvent<>(currentFiberID(), Timestamp::now(), fmt, args));
	}

	inline void perf_syscall(char const* syscall, Timestamp const& t = Timestamp()) {
		if(Config::EnablePerfEvent && zth_config(PerfSyscall))
			perf_event(PerfEvent<>(currentFiberID(), syscall, t.isNull() ? Timestamp::now() : t));
	}

	/*!
	 * \ingroup zth_api_cpp_perf
	 */
	template <typename T = float>
	class Load {
	public:
		typedef T type;

		explicit Load(type rc = 1)
			: m_rc(rc)
			, m_active()
			, m_current(Timestamp::now())
			, m_load()
		{}

		type rc() const { return m_rc; }

		void setRc(type rc) { m_rc = rc; }

		void start(Timestamp const& now = Timestamp::now()) {
			if(isActive())
				return;

			idle((type)(now - m_current).s());
			m_current = now;
			m_active = true;
		}

		void stop(Timestamp const& now = Timestamp::now()) {
			if(!isActive())
				return;

			active((type)(now - m_current).s());
			m_current = now;
			m_active = false;
		}

		bool isActive() const { return m_active; }

		type load() const { return m_load; }

		type load(Timestamp const& now) const {
			type dt_s = (type)(now - m_current).s();
			if(isActive())
				return active(load(), dt_s);
			else
				return idle(load(), dt_s);
		}

		void idle(type dt_s) {
			m_load = idle(load(), dt_s);
		}

		void active(type dt_s) {
			m_load = active(load(), dt_s);
		}

	protected:
		type idle(type load, type dt_s) const {
			if(dt_s <= 0)
				return load;

			return ((type)1 - alpha(dt_s)) * load;
		}

		type active(type load, type dt_s) const {
			if(dt_s <= 0)
				return load;

			type a = alpha(dt_s);
			return a + ((type)1 - a) * load;
		}

		type alpha(type dt_s) const {
			return dt_s / (rc() + dt_s);
		}

	private:
		type m_rc;
		bool m_active;
		Timestamp m_current;
		type m_load;
	};

} // namespace

EXTERN_C ZTH_EXPORT ZTH_INLINE void zth_perf_mark_(char const* marker) { zth::perf_mark(marker); }

/*!
 * \copydoc zth::perf_log()
 * \details This is a C-wrapper for zth::perf_log().
 * \ingroup zth_api_c_perf
 */
EXTERN_C ZTH_EXPORT ZTH_INLINE __attribute__((format(ZTH_ATTR_PRINTF, 1, 2))) void zth_perf_log(char const* fmt, ...) {
	va_list args; va_start(args, fmt); zth::perf_logv(fmt, args); va_end(args); }

/*!
 * \copydoc zth::perf_logv()
 * \details This is a C-wrapper for zth::perf_logv().
 * \ingroup zth_api_c_perf
 */
EXTERN_C ZTH_EXPORT ZTH_INLINE __attribute__((format(ZTH_ATTR_PRINTF, 1, 0))) void zth_perf_logv(char const* fmt, va_list args) { zth::perf_logv(fmt, args); }

#else // !__cplusplus

ZTH_EXPORT void zth_perf_mark_(char const* marker);
ZTH_EXPORT __attribute__((format(ZTH_ATTR_PRINTF, 1, 2))) void zth_perf_log(char const* fmt, ...);
ZTH_EXPORT __attribute__((format(ZTH_ATTR_PRINTF, 1, 0))) void zth_perf_logv(char const* fmt, va_list args);

#endif // __cplusplus

/*!
 * \copydoc zth::perf_mark()
 * \details This is a C-wrapper for zth::perf_mark().
 * \ingroup zth_api_c_perf
 * \hideinitializer
 */
#define zth_perf_mark(marker)	zth_perf_mark_("" marker)

#endif // ZTH_PERF_H
