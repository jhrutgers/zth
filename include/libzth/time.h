#ifndef __ZTH_TIME_H
#define __ZTH_TIME_H
/*
 * Zth (libzth), a cooperative userspace multitasking library.
 * Copyright (C) 2019  Jochem Rutgers
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

#ifdef __cplusplus

#include <libzth/config.h>
#include <libzth/util.h>

#include <cstdio>
#include <ctime>
#include <cmath>
#include <limits>
#include <inttypes.h>

#ifdef ZTH_OS_MAC
extern "C" int clock_gettime(int clk_id, struct timespec* res);
extern "C" int clock_nanosleep(int clk_id, int flags, struct timespec const* request, struct timespec* remain);
#  define CLOCK_MONOTONIC 1
#  define TIMER_ABSTIME 1
#endif

namespace zth {
	
	class TimeInterval {
	public:
		static long const BILLION = 1000000000L;

		constexpr static TimeInterval infinity() { return TimeInterval(std::numeric_limits<time_t>::max()); }
		constexpr static TimeInterval null() { return TimeInterval(); }

		constexpr TimeInterval()
			: m_t()
			, m_negative()
		{}

#if __cplusplus >= 201103L
		constexpr TimeInterval(time_t s, long ns = 0, bool negative = false)
			: m_t{s, ns}
			, m_negative(negative)
		{}
#else
		TimeInterval(time_t s, long ns = 0, bool negative = false)
			: m_t()
			, m_negative(negative)
		{
			m_t.tv_sec = s;
			m_t.tv_nsec = ns;
			zth_assert(isNormal());
		}
#endif

		constexpr TimeInterval(struct timespec const& ts)
			: m_t(ts)
			, m_negative()
		{}

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

				zth_assert(dt_ == dt_); // Should not be NaN

				if(std::fabs(dt_) > (double)(std::numeric_limits<time_t>::max() / 2 - 1))
					m_t.tv_sec = std::numeric_limits<time_t>::max();
				else
					m_t.tv_sec = (time_t)std::fabs(dt_);
				m_t.tv_nsec = (long)(std::fmod(std::fabs(dt_), 1.0) * 1e9);

				if(m_t.tv_nsec >= BILLION) {
					m_t.tv_nsec -= BILLION;
					m_t.tv_sec++;
				}
			}
		}

		TimeInterval(TimeInterval const& t) : m_t(t.ts()), m_negative(t.isNegative()) {}

		constexpr bool isNormal() const { return m_t.tv_sec >= 0 && m_t.tv_nsec >= 0 && m_t.tv_nsec < BILLION; }
		constexpr bool isNegative() const { return m_negative; }
		constexpr bool isPositive() const { return !isNegative(); }
		constexpr bool isNull() const { return m_t.tv_sec == 0 && m_t.tv_nsec == 0; }
		constexpr bool isInfinite() const { return m_t.tv_sec > std::numeric_limits<time_t>::max() / 2; }
		constexpr bool hasPassed() const { return isNegative() || isNull(); }
		constexpr struct timespec const& ts() const { return m_t; }
		double s() const {
			double t = (double)m_t.tv_sec + (double)m_t.tv_nsec * 1e-9;
			if(m_negative)
				t = -t;
			return t;
		}

		constexpr bool isAbsBiggerThan(TimeInterval const& t) const {
			return m_t.tv_sec > t.m_t.tv_sec || (m_t.tv_sec == t.m_t.tv_sec && m_t.tv_nsec > t.m_t.tv_nsec);
		}
		
		constexpr bool isBiggerThan(TimeInterval const& t) const {
			return
				(!m_negative && t.m_negative) || 
				(!m_negative && !t.m_negative && this->isAbsBiggerThan(t)) ||
				(m_negative && t.m_negative && t.isAbsBiggerThan(*this));
		}

		constexpr bool operator==(TimeInterval const& rhs) const { return m_t.tv_nsec == rhs.m_t.tv_nsec && m_t.tv_sec == rhs.m_t.tv_sec && m_negative == rhs.m_negative; }
		constexpr bool operator>(TimeInterval const& rhs) const { return this->isBiggerThan(rhs); }
		constexpr bool operator>=(TimeInterval const& rhs) const { return *this == rhs || this->isBiggerThan(rhs); }
		constexpr bool operator<(TimeInterval const& rhs) const { return !(*this >= rhs); }
		constexpr bool operator<=(TimeInterval const& rhs) const { return *this == rhs || !(*this > rhs); }

		void add(TimeInterval const& t) {
			if(t.m_negative == m_negative) {
				// Add in same sign direction.
				time_t tv_sec __attribute__((unused)) = m_t.tv_sec;
				m_t.tv_sec += t.m_t.tv_sec;
				m_t.tv_nsec += t.m_t.tv_nsec;
				if(m_t.tv_nsec > BILLION) {
					m_t.tv_nsec -= BILLION;
					m_t.tv_sec++;
				}

				// Check for overflow.
				if(tv_sec > m_t.tv_sec)
					m_t.tv_sec = std::numeric_limits<time_t>::max(); // infinite
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
					m_t.tv_nsec += BILLION;
					m_t.tv_sec--;
				}
			}
		}

		void sub(TimeInterval const& t) {
			add(TimeInterval(t.ts().tv_sec, t.ts().tv_nsec, !t.isNegative()));
		}

		void mul(double x) {
			*this = TimeInterval(s() * x);
		}

		TimeInterval& operator+=(TimeInterval const& rhs) { add(rhs); return *this; }
		TimeInterval operator+(TimeInterval const& rhs) const { TimeInterval ti(*this); ti += rhs; return ti; }
		TimeInterval& operator-=(TimeInterval const& rhs) { sub(rhs); return *this; }
		TimeInterval operator-(TimeInterval const& rhs) const { TimeInterval ti(*this); ti -= rhs; return ti; }
		TimeInterval operator-() const { return TimeInterval(ts().tv_sec, ts().tv_nsec, !isNegative()); }
		TimeInterval& operator*=(double x) { mul(x); return *this; }
		TimeInterval operator*(double x) const { return TimeInterval(s() * x); }
		TimeInterval& operator/=(double x) { mul(1.0 / x); return *this; }
		TimeInterval operator/(double x) const { return TimeInterval(s() / x); }

		std::string str() const {
			std::string res;
			if(isInfinite()) {
				res = "infinity";
				return res;
			}

			if(m_negative)
				res = "-";

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
	
	template <> inline cow_string str<TimeInterval const&>(TimeInterval const& value) { return value.str(); }

#if __cplusplus >= 201103L
	/*! \brief Define literals like \c 123_s, which is a #TimeInterval of 123 seconds.  */
	ZTH_EXPORT constexpr inline TimeInterval operator"" _s(unsigned long long int x) { return TimeInterval((time_t)std::min<unsigned long long int>(x, (unsigned long long int)std::numeric_limits<time_t>::max)); }
	/*! \brief Define literals like \c 123_ms, which is a #TimeInterval of 123 milliseconds.  */
	ZTH_EXPORT constexpr inline TimeInterval operator"" _ms(unsigned long long int x) { return TimeInterval((time_t)std::min<unsigned long long int>(x / 1000ULL, (unsigned long long int)std::numeric_limits<time_t>::max), ((long)x % 1000L) * 1000000L); }
	/*! \brief Define literals like \c 123_us, which is a #TimeInterval of 123 microseconds.  */
	ZTH_EXPORT constexpr inline TimeInterval operator"" _us(unsigned long long int x) { return TimeInterval((time_t)std::min<unsigned long long int>(x / 1000000ULL, (unsigned long long int)std::numeric_limits<time_t>::max), ((long)x % 1000000L) * 1000L); }
	/*! \brief Define literals like \c 12.3_s, which is a #TimeInterval of 12.3 seconds.  */
	ZTH_EXPORT inline TimeInterval operator"" _s(long double x) { return TimeInterval(x); }
#endif

	class Timestamp {
	public:
		Timestamp()
			: m_t()
		{
			*this = null();
		}

		Timestamp(struct timespec const& ts) : m_t(ts) {}

		Timestamp(time_t sec, long nsec)
			: m_t()
		{
			m_t.tv_sec = sec;
			m_t.tv_nsec = nsec;
		}

		static Timestamp now() {
			Timestamp t(0, 0);
			int res __attribute__((unused)) = clock_gettime(CLOCK_MONOTONIC, &t.m_t);
			zth_assert(res == 0);
			zth_assert(!t.isNull());
			return t;
		}
		
		struct timespec const& ts() const { return m_t; }
		operator struct timespec const&() const { return ts(); }

		bool isBefore(Timestamp const& t) const {
			return ts().tv_sec < t.ts().tv_sec || (ts().tv_sec == t.ts().tv_sec && ts().tv_nsec < t.ts().tv_nsec);
		}

		bool isAfter(Timestamp const& t) const {
			return t.isBefore(*this);
		}

		bool operator==(Timestamp const& rhs) const { return ts().tv_nsec == rhs.ts().tv_nsec && ts().tv_sec == rhs.ts().tv_sec; }
		bool operator<(Timestamp const& rhs) const { return this->isBefore(rhs); }
		bool operator<=(Timestamp const& rhs) const { return *this == rhs || this->isBefore(rhs); }
		bool operator>(Timestamp const& rhs) const { return rhs.isBefore(*this); }
		bool operator>=(Timestamp const& rhs) const { return *this == rhs || rhs.isBefore(*this); }

		TimeInterval timeTo(Timestamp const& t) const {
			return TimeInterval(t.ts().tv_sec, t.ts().tv_nsec) - TimeInterval(ts().tv_sec, ts().tv_nsec);
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

		static Timestamp null() { return Timestamp(0, 0); }

		bool isNull() const {
			return m_t.tv_sec == 0 && m_t.tv_nsec == 0;
		}

	private:
		struct timespec m_t;
	};

	extern Timestamp const startTime;

} // namespace

#endif // __cplusplus
#endif // __ZTH_TIME_H
