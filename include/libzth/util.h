#ifndef __ZTH_UTIL_H
#define __ZTH_UTIL_H

#define ZTH_STRINGIFY_(x) #x
#define ZTH_STRINGIFY(x) ZTH_STRINGIFY_(x)

#ifndef likely
#  ifdef __GNUC__
#    define likely(expr) __builtin_expect(!!(expr), 1)
#  else
#    define likely(expr) (expr)
#  endif
#endif
#ifndef unlikely
#  ifdef __GNUC__
#    define unlikely(expr) __builtin_expect((expr), 0)
#  else
#    define unlikely(expr) (expr)
#  endif
#endif

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif
void zth_log(char const* fmt, ...);
void zth_logv(char const* fmt, va_list arg) __attribute__((weak));
#ifdef __cplusplus
} // extern "C"
#endif

#ifdef __cplusplus
#include <ctime>
#include <string>

#ifdef ZTH_OS_MAC
extern "C" int clock_gettime(int clk_id, struct timespec* res);
#  define CLOCK_MONOTONIC 1
#endif

#define zth_dbg(group, msg, a...) \
	do { \
		if(::zth::Config::EnableDebugPrint && ::zth::Config::Print_##group != 0) { \
			if(::zth::Config::EnableColorDebugPrint) \
				zth_log("\x1b[%d%sm > zth::" ZTH_STRINGIFY(group) ": " msg "\x1b[0m\n", \
					(::zth::Config::Print_##group % 8) + 30, \
					::zth::Config::Print_##group >= 8 ? ";1" : "", ##a); \
			else \
				zth_log(" > zth::" ZTH_STRINGIFY(group) ": " msg "\n", ##a); \
		} \
	} while(0)

#ifndef NDEBUG
	#define zth_assert(expr) do { if(unlikely(::zth::Config::EnableAssert && !(expr))) \
		::zth::zth_abort("assertion failed at " __FILE__ ":" ZTH_STRINGIFY(__LINE__) ": " ZTH_STRINGIFY(expr)); } while(false)
#else
	#define zth_assert(...)  ((void)0)
#endif

namespace zth {
	char const* banner();
    void zth_abort(char const* msg, ...) __attribute__((format(printf, 1, 2), noreturn));
	std::string pthreadId(pthread_t p = pthread_self());

	class Timestamp {
	public:
		Timestamp(time_t sec = 0, long nsec = 0)
			: m_t()
		{
			m_t.tv_sec = sec;
			m_t.tv_nsec = nsec;
		}

		static Timestamp now() {
			Timestamp t;
			int res __attribute__((unused)) = clock_gettime(CLOCK_MONOTONIC, &t.m_t);
			zth_assert(res == 0);
			return t;
		}

		bool isBefore(Timestamp const& t) const
		{
			return t.m_t.tv_sec > m_t.tv_sec || (t.m_t.tv_sec == m_t.tv_sec && t.m_t.tv_nsec >= m_t.tv_nsec);
		}

		Timestamp diff(Timestamp const& t) const
		{
			Timestamp res;

			if(likely(isBefore(t))) {
				// t is later in time than this.
				res.m_t.tv_sec = t.m_t.tv_sec - m_t.tv_sec;
				res.m_t.tv_nsec = t.m_t.tv_nsec - m_t.tv_nsec;
			} else {
				// this is later in time than t.
				res.m_t.tv_sec = m_t.tv_sec - t.m_t.tv_sec;
				res.m_t.tv_nsec = m_t.tv_nsec - t.m_t.tv_nsec;
			}

			if(res.m_t.tv_nsec < 0) {
				res.m_t.tv_nsec += 1000000000;
				res.m_t.tv_sec--;
			}

			return res;
		}

		Timestamp& add(Timestamp const& t)
		{
			m_t.tv_sec += t.m_t.tv_sec;
			m_t.tv_nsec += t.m_t.tv_nsec;
			if(m_t.tv_nsec > 1000000000) {
				m_t.tv_nsec -= 1000000000;
				m_t.tv_sec++;
			}
			return *this;
		}

		Timestamp& operator+=(Timestamp const& t) { return add(t); }

		std::string toString() const {
			char buf[32];
			return snprintf(buf, sizeof(buf), "%g s", (double)m_t.tv_sec + (double)m_t.tv_nsec * 1e-9) > 0 ? buf : "? s";
		}

	private:
		struct timespec m_t;
	};
}

#endif // __cplusplus
#endif // __ZTH_UTIL_H
