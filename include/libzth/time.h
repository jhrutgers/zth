#ifndef ZTH_TIME_H
#define ZTH_TIME_H
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
 * \defgroup zth_api_cpp_time time
 * \ingroup zth_api_cpp
 */

#ifdef __cplusplus

#	include <libzth/allocator.h>
#	include <libzth/config.h>
#	include <libzth/util.h>

#	include <algorithm>
#	include <cmath>
#	include <cstdio>
#	include <ctime>
#	include <inttypes.h>
#	include <limits>

#	if defined(ZTH_OS_MAC) || defined(ZTH_OS_BAREMETAL)
#		ifdef ZTH_CUSTOM_CLOCK_GETTIME
extern "C" int clock_gettime(int clk_id, struct timespec* res);
#		endif
extern "C" int
clock_nanosleep(int clk_id, int flags, struct timespec const* request, struct timespec* remain);
#		ifndef CLOCK_MONOTONIC
#			define CLOCK_MONOTONIC 1
#		endif
#		ifndef TIMER_ABSTIME
#			define TIMER_ABSTIME 1
#		endif
#	endif

namespace zth {

class Timestamp;

/*!
 * \brief Convenient wrapper around \c struct \c timespec that contains a time interval.
 * \ingroup zth_api_cpp_time
 */
class TimeInterval {
	ZTH_CLASS_NEW_DELETE(TimeInterval)
private:
	// Do not convert automatically.
	TimeInterval(Timestamp const&);

public:
	static long const BILLION = 1000000000L;

	constexpr static TimeInterval infinity() noexcept
	{
		return TimeInterval(std::numeric_limits<time_t>::max());
	}

	constexpr static TimeInterval null() noexcept
	{
		return TimeInterval();
	}

	constexpr TimeInterval() noexcept
		: m_t()
		, m_negative()
	{}

#	if __cplusplus >= 201103L
	// cppcheck-suppress noExplicitConstructor
	constexpr TimeInterval(time_t s, long ns = 0, bool negative = false) noexcept
		: m_t{s, ns}
		, m_negative{negative}
	{}
#	else
	// cppcheck-suppress noExplicitConstructor
	TimeInterval(time_t s, long ns = 0, bool negative = false) noexcept
		: m_t()
		, m_negative(negative)
	{
		m_t.tv_sec = s;
		m_t.tv_nsec = ns;
		zth_assert(isNormal());
	}
#	endif

	// cppcheck-suppress noExplicitConstructor
	constexpr TimeInterval(struct timespec const& ts) noexcept
		: m_t(ts)
		, m_negative()
	{}

	TimeInterval& operator=(TimeInterval const& t) noexcept
	{
		m_t = t.m_t;
		m_negative = t.m_negative;
		return *this;
	}

	constexpr TimeInterval(TimeInterval const& t) noexcept
		: m_t(t.ts())
		, m_negative(t.isNegative())
	{}

	// cppcheck-suppress noExplicitConstructor
	TimeInterval(float dt)
		: m_t()
		, m_negative()
	{
		init_float<float>(dt);
	}

	// cppcheck-suppress noExplicitConstructor
	TimeInterval(double dt)
		: m_t()
		, m_negative()
	{
		init_float<double>(dt);
	}

	// cppcheck-suppress noExplicitConstructor
	TimeInterval(long double dt)
		: m_t()
		, m_negative()
	{
		init_float<long double>(dt);
	}

	template <typename T>
	// cppcheck-suppress noExplicitConstructor
	constexpr14 TimeInterval(T dt) noexcept
		: m_t()
		, m_negative()
	{
		init_int<T>(dt);
	}

private:
	template <typename T>
	void init_float(T dt)
	{
		zth_assert(dt == dt); // Should not be NaN

		m_negative = dt < 0;

		if(std::fabs(dt) > (T)(std::numeric_limits<time_t>::max() / 2 - 1))
			m_t.tv_sec = std::numeric_limits<time_t>::max();
		else
			m_t.tv_sec = (time_t)std::fabs(dt);
		m_t.tv_nsec = (long)(std::fmod(std::fabs(dt), (T)1.0) * (T)1e9);

		if(m_t.tv_nsec >= BILLION) {
			m_t.tv_nsec -= BILLION;
			m_t.tv_sec++;
		}
	}

	template <typename T>
	constexpr14 void init_int(T dt) noexcept
	{
		// seconds only.
		m_negative = dt < 0;
		m_t.tv_sec = (time_t)(dt >= 0 ? dt : -dt);
		m_t.tv_nsec = 0;
	}

public:
	constexpr bool isNormal() const noexcept
	{
		return m_t.tv_sec >= 0 && m_t.tv_nsec >= 0 && m_t.tv_nsec < BILLION;
	}

	constexpr bool isNegative() const noexcept
	{
		return m_negative;
	}

	constexpr bool isPositive() const noexcept
	{
		return !isNegative();
	}

	constexpr bool isNull() const noexcept
	{
		return m_t.tv_sec == 0 && m_t.tv_nsec == 0;
	}

	constexpr bool isInfinite() const noexcept
	{
		return m_t.tv_sec > std::numeric_limits<time_t>::max() / 2;
	}

	constexpr bool hasPassed() const noexcept
	{
		return isNegative() || isNull();
	}

	constexpr struct timespec const& ts() const noexcept
	{
		return m_t;
	}

	constexpr14 double s() const noexcept
	{
		return s<double>();
	}

	template <typename T>
	constexpr14 T s() const noexcept
	{
		T t = (T)m_t.tv_sec + (T)m_t.tv_nsec * (T)1e-9;
		if(m_negative)
			t = -t;
		return t;
	}

	constexpr bool isAbsBiggerThan(TimeInterval const& t) const noexcept
	{
		return m_t.tv_sec > t.m_t.tv_sec
		       || (m_t.tv_sec == t.m_t.tv_sec && m_t.tv_nsec > t.m_t.tv_nsec);
	}

	constexpr bool isBiggerThan(TimeInterval const& t) const noexcept
	{
		return (!m_negative && t.m_negative)
		       || (!m_negative && !t.m_negative && this->isAbsBiggerThan(t))
		       || (m_negative && t.m_negative && t.isAbsBiggerThan(*this));
	}

	constexpr bool operator==(TimeInterval const& rhs) const noexcept
	{
		return m_t.tv_nsec == rhs.m_t.tv_nsec && m_t.tv_sec == rhs.m_t.tv_sec
		       && m_negative == rhs.m_negative;
	}

	constexpr bool operator>(TimeInterval const& rhs) const noexcept
	{
		return this->isBiggerThan(rhs);
	}

	constexpr bool operator>=(TimeInterval const& rhs) const noexcept
	{
		return *this == rhs || this->isBiggerThan(rhs);
	}

	constexpr bool operator<(TimeInterval const& rhs) const noexcept
	{
		return !(*this >= rhs);
	}

	constexpr bool operator<=(TimeInterval const& rhs) const noexcept
	{
		return *this == rhs || !(*this > rhs);
	}

	constexpr14 void add(TimeInterval const& t) noexcept
	{
		if(t.m_negative == m_negative) {
			// Check for overflow.
			if(isInfinite() || t.isInfinite()
			   || std::numeric_limits<time_t>::max() - m_t.tv_sec - 1 <= t.m_t.tv_sec) {
				m_t.tv_sec = std::numeric_limits<time_t>::max(); // infinite
			} else {
				// Add in same sign direction.
				m_t.tv_sec += t.m_t.tv_sec;
				m_t.tv_nsec += t.m_t.tv_nsec;
				if(m_t.tv_nsec > BILLION) {
					m_t.tv_nsec -= BILLION;
					m_t.tv_sec++;
				}
			}
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

	constexpr14 void sub(TimeInterval const& t) noexcept
	{
		add(TimeInterval(t.ts().tv_sec, t.ts().tv_nsec, !t.isNegative()));
	}

	template <typename T>
	constexpr14 void mul(T x) noexcept
	{
		*this = TimeInterval(s<T>() * x);
	}

	constexpr14 TimeInterval& operator+=(TimeInterval const& rhs) noexcept
	{
		add(rhs);
		return *this;
	}

	constexpr14 TimeInterval operator+(TimeInterval const& rhs) const noexcept
	{
		TimeInterval ti(*this);
		ti += rhs;
		return ti;
	}

	constexpr14 TimeInterval& operator-=(TimeInterval const& rhs) noexcept
	{
		sub(rhs);
		return *this;
	}

	constexpr14 TimeInterval operator-(TimeInterval const& rhs) const noexcept
	{
		TimeInterval ti(*this);
		ti -= rhs;
		return ti;
	}

	constexpr14 TimeInterval operator-() const noexcept
	{
		return TimeInterval(ts().tv_sec, ts().tv_nsec, !isNegative());
	}

	template <typename T>
	constexpr14 TimeInterval& operator*=(T x) noexcept
	{
		mul<T>(x);
		return *this;
	}

	template <typename T>
	constexpr14 TimeInterval operator*(T x) const noexcept
	{
		return TimeInterval(s<T>() * x);
	}

	template <typename T>
	constexpr14 TimeInterval& operator/=(T x) noexcept
	{
		mul<T>((T)1 / x);
		return *this;
	}

	template <typename T>
	constexpr14 TimeInterval operator/(T x) const noexcept
	{
		return TimeInterval(s<T>() / x);
	}

	string str() const
	{
		string res;
		if(isInfinite()) {
			res = "infinity";
			return res;
		}

		if(m_negative)
			res = "-";

#	ifdef ZTH_OS_BAREMETAL
		// Do a simplified print without float formatting support.
		if(m_t.tv_sec >= 60)
			res += format("%u s", (unsigned int)m_t.tv_sec);
		else if(m_t.tv_sec > 0)
			res +=
				format("%u.%03u s", (unsigned int)m_t.tv_sec,
				       (unsigned int)(m_t.tv_nsec / 1000000L));
		else
			res += format("%u us", (unsigned int)m_t.tv_nsec / 1000U);
#	else
		uint64_t d = (uint64_t)(m_t.tv_sec / 3600 / 24);
		time_t rest = m_t.tv_sec - d * 3600 * 24;
		bool doPrint = d > 0;
		if(doPrint)
			res += format("%" PRIu64 "d:", d);

		int h = int(rest / 3600);
		rest -= h * 3600;
		if(doPrint) {
			res += format("%02d:", h);
		} else if(h > 0) {
			res += format("%d:", h);
			doPrint = true;
		}

		int m = int(rest / 60);
		rest -= m * 60;
		if(doPrint) {
			res += format("%02d:", m);
		} else if(m > 0) {
			res += format("%d:", m);
			doPrint = true;
		}

		double sec = (double)rest + (double)m_t.tv_nsec * 1e-9;
		if(doPrint) {
			res += format("%06.3f", sec);
		} else {
			res += format("%g s", sec);
		}
#	endif

		return res;
	}

private:
	struct timespec m_t;
	bool m_negative;
};

template <>
inline cow_string str<TimeInterval const&>(TimeInterval const& value)
{
	return value.str();
}

#	if __cplusplus >= 201103L
/*!
 * \brief Define literals like \c 123_s, which is a #zth::TimeInterval of 123 seconds.
 *
 * You may want to put <tt>using zth::operator""_s;</tt> in your code to simplify using it.
 *
 * \ingroup zth_api_cpp_time
 */
ZTH_EXPORT constexpr14 inline TimeInterval operator"" _s(unsigned long long int x) noexcept
{
	return TimeInterval((time_t)std::min<unsigned long long int>(
		x, (unsigned long long int)std::numeric_limits<time_t>::max()));
}

/*!
 * \brief Define literals like \c 123_ms, which is a #zth::TimeInterval of 123 milliseconds.
 *
 * You may want to put <tt>using zth::operator""_ms;</tt> in your code to simplify using it.
 *
 * \ingroup zth_api_cpp_time
 */
ZTH_EXPORT constexpr14 inline TimeInterval operator"" _ms(unsigned long long int x) noexcept
{
	return TimeInterval(
		(time_t)std::min<unsigned long long int>(
			x / 1000ULL, (unsigned long long int)std::numeric_limits<time_t>::max()),
		((long)x % 1000L) * 1000000L);
}

/*!
 * \brief Define literals like \c 123_us, which is a #zth::TimeInterval of 123 microseconds.
 *
 * You may want to put <tt>using zth::operator""_us;</tt> in your code to simplify using it.
 *
 * \ingroup zth_api_cpp_time
 */
ZTH_EXPORT constexpr14 inline TimeInterval operator"" _us(unsigned long long int x) noexcept
{
	return TimeInterval(
		(time_t)std::min<unsigned long long int>(
			x / 1000000ULL, (unsigned long long int)std::numeric_limits<time_t>::max()),
		((long)x % 1000000L) * 1000L);
}

/*!
 * \brief Define literals like \c 12.3_s, which is a #zth::TimeInterval of 12.3 seconds.
 *
 * You may want to put <tt>using zth::operator""_s;</tt> in your code to simplify using it.
 *
 * \ingroup zth_api_cpp_time
 */
ZTH_EXPORT inline TimeInterval operator"" _s(long double x)
{
	return TimeInterval(x);
}
#	endif // C++11

/*!
 * \brief Convenient wrapper around \c struct \c timespec that contains an absolute timestamp.
 * \ingroup zth_api_cpp_time
 */
class Timestamp {
	ZTH_CLASS_NEW_DELETE(Timestamp)
public:
	// null
	constexpr Timestamp() noexcept
		: m_t()
	{}

	// cppcheck-suppress noExplicitConstructor
	constexpr Timestamp(struct timespec const& ts) noexcept
		: m_t(ts)
	{}

	constexpr14 explicit Timestamp(time_t sec, long nsec = 0) noexcept
		: m_t()
	{
		m_t.tv_sec = sec;
		m_t.tv_nsec = nsec;
	}

	// cppcheck-suppress noExplicitConstructor
	Timestamp(TimeInterval const& ti)
		: m_t()
	{
		*this = now() + ti;
	}

	static Timestamp now()
	{
		Timestamp t;
		int res __attribute__((unused)) = clock_gettime(CLOCK_MONOTONIC, &t.m_t);
		zth_assert(res == 0);
		zth_assert(!t.isNull());
		return t;
	}

	constexpr struct timespec const& ts() const noexcept
	{
		return m_t;
	}
	constexpr operator struct timespec const &() const noexcept
	{
		return ts();
	}

	constexpr bool isBefore(Timestamp const& t) const noexcept
	{
		return ts().tv_sec < t.ts().tv_sec
		       || (ts().tv_sec == t.ts().tv_sec && ts().tv_nsec < t.ts().tv_nsec);
	}

	constexpr bool isAfter(Timestamp const& t) const noexcept
	{
		return t.isBefore(*this);
	}

	bool hasPassed() const noexcept
	{
		return now().isBefore(*this);
	}

	constexpr bool operator==(Timestamp const& rhs) const noexcept
	{
		return ts().tv_nsec == rhs.ts().tv_nsec && ts().tv_sec == rhs.ts().tv_sec;
	}

	constexpr bool operator!=(Timestamp const& rhs) const noexcept
	{
		return !(*this == rhs);
	}
	constexpr bool operator<(Timestamp const& rhs) const noexcept
	{
		return this->isBefore(rhs);
	}
	constexpr bool operator<=(Timestamp const& rhs) const noexcept
	{
		return *this == rhs || this->isBefore(rhs);
	}
	constexpr bool operator>(Timestamp const& rhs) const noexcept
	{
		return rhs.isBefore(*this);
	}
	constexpr bool operator>=(Timestamp const& rhs) const noexcept
	{
		return *this == rhs || rhs.isBefore(*this);
	}

	constexpr14 TimeInterval timeTo(Timestamp const& t) const
	{
		return TimeInterval(t.ts().tv_sec, t.ts().tv_nsec)
		       - TimeInterval(ts().tv_sec, ts().tv_nsec);
	}

	constexpr14 void add(TimeInterval const& dt) noexcept
	{
		TimeInterval t(m_t.tv_sec, m_t.tv_nsec);
		t += dt;
		zth_assert(t.isPositive());
		m_t = t.ts();
	}

	constexpr14 Timestamp& operator+=(TimeInterval const& dt) noexcept
	{
		add(dt);
		return *this;
	}

	constexpr14 Timestamp operator+(TimeInterval const& dt) const noexcept
	{
		Timestamp t(*this);
		return t += dt;
	}

	constexpr14 Timestamp& operator-=(TimeInterval const& dt) noexcept
	{
		add(-dt);
		return *this;
	}

	constexpr14 Timestamp operator-(TimeInterval const& dt) const noexcept
	{
		Timestamp t(*this);
		return t -= dt;
	}

	constexpr14 TimeInterval operator-(Timestamp const& rhs) const noexcept
	{
		return rhs.timeTo(*this);
	}

	static constexpr Timestamp null() noexcept
	{
		return Timestamp();
	}

	constexpr bool isNull() const noexcept
	{
		return m_t.tv_sec == 0 && m_t.tv_nsec == 0;
	}

private:
	struct timespec m_t;
};

#	ifdef ZTH_OS_MAC
// Should be const, but the alias-trick does not work on OSX.
extern Timestamp /* const */ startTime;
#	else
extern Timestamp const startTime;
#	endif

} // namespace zth

#endif // __cplusplus
#endif // ZTH_TIME_H
