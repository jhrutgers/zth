#ifndef __ZTH_TIME_H
#define __ZTH_TIME_H
#ifdef __cplusplus

#include <libzth/config.h>
#include <libzth/util.h>

#include <ctime>
#include <cmath>
#include <limits>
#include <cinttypes>

#ifdef ZTH_OS_MAC
extern "C" int clock_gettime(int clk_id, struct timespec* res);
#  define CLOCK_MONOTONIC 1
#endif

namespace zth {
	
	class TimeInterval {
	public:
		TimeInterval(time_t s = 0, long ns = 0, bool negative = false)
			: m_t()
			, m_negative(negative)
		{
			m_t.tv_sec = s;
			m_t.tv_nsec = ns;
			zth_assert(s >= 0);
		}

		template <typename T>
		TimeInterval(T dt)
			: m_t()
			, m_negative(dt < 0)
		{
			if(std::numeric_limits<T>::is_integer) {
				// seconds only.
				m_t.tv_sec = (time_t)(dt >= 0 ? dt : -dt);
				m_t.tv_nsec = 0;
			} else {
				double dt_ = (double)dt;

				zth_assert(
					dt_ == dt_ && // Should not be NaN
					std::fabs(dt_) < (double)(std::numeric_limits<time_t>::max() / 2 - 1));

				if(dt_ < 0)
					m_t.tv_sec = (time_t)-dt_;
				else
					m_t.tv_sec = (time_t)dt_;

				m_t.tv_nsec = (long)(std::fmod(std::fabs(dt_), 1.0) * 1e9);

				if(m_t.tv_nsec > 1000000000L) {
					m_t.tv_nsec -= 1000000000L;
					m_t.tv_sec++;
				}
			}
		}

		bool isNegative() const { return m_negative; }
		bool isPositive() const { return !isNegative(); }
		struct timespec const& ts() const { return m_t; }
		double s() const {
			double t = (double)m_t.tv_sec + (double)m_t.tv_nsec * 1e-9;
			if(m_negative)
				t = -t;
			return t;
		}

		bool isAbsBiggerThan(TimeInterval const& t) const {
			return m_t.tv_sec > t.m_t.tv_sec || (m_t.tv_sec == t.m_t.tv_sec && m_t.tv_nsec > t.m_t.tv_nsec);
		}
		
		bool isBiggerThan(TimeInterval const& t) const {
			return
				(!m_negative && t.m_negative) || 
				(!m_negative && !t.m_negative && this->isAbsBiggerThan(t)) ||
				(m_negative && t.m_negative && t.isAbsBiggerThan(*this));
		}

		bool operator==(TimeInterval const& rhs) const { return m_t.tv_nsec == rhs.m_t.tv_nsec && m_t.tv_sec == rhs.m_t.tv_sec && m_negative == rhs.m_negative; }
		bool operator>(TimeInterval const& rhs) const { return this->isBiggerThan(rhs); }
		bool operator>=(TimeInterval const& rhs) const { return *this == rhs || this->isBiggerThan(rhs); }
		bool operator<(TimeInterval const& rhs) const { return !(*this >= rhs); }
		bool operator<=(TimeInterval const& rhs) const { return *this == rhs || !(*this > rhs); }

		void add(TimeInterval const& t) {
			if(t.m_negative == m_negative) {
				// Add in same sign direction.
				time_t tv_sec __attribute__((unused)) = m_t.tv_sec;
				m_t.tv_sec += t.m_t.tv_sec;
				m_t.tv_nsec += t.m_t.tv_nsec;
				if(m_t.tv_nsec > 1000000000L) {
					m_t.tv_nsec -= 1000000000L;
					m_t.tv_sec++;
				}

				// Check for overflow.
				zth_assert(tv_sec <= m_t.tv_sec);
			} else {
				if(t.isAbsBiggerThan(*this)) {
					// Add in other sign direction with sign flip.
					m_t.tv_sec = t.m_t.tv_sec - m_t.tv_sec;
					m_t.tv_nsec = t.m_t.tv_nsec - m_t.tv_nsec;
					m_negative = !m_negative;
				} else {
					// Add in other sign direction (but without sign flip).
					m_t.tv_sec -= t.m_t.tv_sec;
					m_t.tv_nsec -= t.m_t.tv_nsec;
				}
				if(m_t.tv_nsec < 0) {
					m_t.tv_nsec += 1000000000L;
					m_t.tv_sec--;
				}
			}
		}

		void sub(TimeInterval const& t) {
			add(TimeInterval(t.m_t.tv_sec, t.m_t.tv_nsec, !t.m_negative));
		}

		TimeInterval& operator+=(TimeInterval const& rhs) { add(rhs); return *this; }
		TimeInterval operator+(TimeInterval const& rhs) const { TimeInterval ti(*this); ti += rhs; return ti; }
		TimeInterval& operator-=(TimeInterval const& rhs) { sub(rhs); return *this; }
		TimeInterval operator-(TimeInterval const& rhs) const { TimeInterval ti(*this); ti -= rhs; return ti; }
		TimeInterval operator-() const { return TimeInterval(m_t.tv_sec, m_t.tv_nsec, !m_negative); }

		std::string str() const {
			std::string res;

			uint64_t d = (uint64_t)(m_t.tv_sec / 3600 / 24);
			time_t rest = m_t.tv_sec - d * 3600 * 24;
			bool doPrint = d > 0;
			if(doPrint)
				res += format("%" PRIu64 "d:", d);

			int h = rest / 3600; rest -= h * 3600;
			if(doPrint) {
				res += format("%02d:", h);
			} else if(h > 0) {
				res += format("%d:", h);
				doPrint = true;
			}

			int m = rest / 60; rest -= m * 60;
			if(doPrint) {
				res += format("%02d:", m);
			} else if(m > 0) {
				res += format("%d:", m);
				doPrint = true;
			}

			double s = (double)rest + (double)m_t.tv_nsec * 1e-9;
			if(doPrint) {
				res += format("%06.3f", s);
			} else {
				res += format("%g s", s);
			}

			return res;
		}

	private:
		struct timespec m_t;
		bool m_negative;
	};


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
			zth_assert(!t.isNull());
			return t;
		}
		
		struct timespec const& ts() const { return m_t; }
		operator struct timespec const&() const { return ts(); }

		bool isBefore(Timestamp const& t) const {
			return t.m_t.tv_sec > m_t.tv_sec || (t.m_t.tv_sec == m_t.tv_sec && t.m_t.tv_nsec > m_t.tv_nsec);
		}

		bool isAfter(Timestamp const& t) const {
			return t.isBefore(*this);
		}

		bool operator==(Timestamp const& rhs) const { return m_t.tv_nsec == rhs.m_t.tv_nsec && m_t.tv_sec == rhs.m_t.tv_sec; }
		bool operator<(Timestamp const& rhs) const { return this->isBefore(rhs); }
		bool operator<=(Timestamp const& rhs) const { return *this == rhs || this->isBefore(rhs); }
		bool operator>(Timestamp const& rhs) const { return rhs.isBefore(*this); }
		bool operator>=(Timestamp const& rhs) const { return *this == rhs || rhs.isBefore(*this); }

		TimeInterval timeTo(Timestamp const& t) const {
			return TimeInterval(t.m_t.tv_sec, t.m_t.tv_nsec) - TimeInterval(m_t.tv_sec, m_t.tv_nsec);
		}

		void add(TimeInterval const& dt) {
			TimeInterval t(m_t.tv_sec, m_t.tv_nsec);
			t += dt;
			zth_assert(t.isPositive());
			m_t = t.ts();
		}

		Timestamp& operator+=(TimeInterval const& dt) { add(dt); return *this; }
		Timestamp operator+(TimeInterval const& dt) const { Timestamp t(*this); return t += dt; }
		Timestamp& operator-=(TimeInterval const& dt) { add(-dt); return *this; }
		Timestamp operator-(TimeInterval const& dt) const { Timestamp t(*this); return t -= dt; }
		TimeInterval operator-(Timestamp const& rhs) const { return rhs.timeTo(*this); }

		bool isNull() const {
			return m_t.tv_sec == 0 && m_t.tv_nsec == 0;
		}

	private:
		struct timespec m_t;
	};

} // namespace

#endif // __cplusplus
#endif // __ZTH_TIME_H
