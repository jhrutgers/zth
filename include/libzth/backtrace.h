#ifndef ZTH_BACKTRACE_H
#define ZTH_BACKTRACE_H
/*
 * SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#include <libzth/macros.h>

#include <libzth/util.h>

#ifdef __cplusplus

#  include <libzth/allocator.h>
#  include <libzth/config.h>
#  include <libzth/time.h>

namespace zth {

class Fiber;

namespace impl {

class NoBacktrace {
	ZTH_CLASS_NEW_DELETE(NoBacktrace)
public:
	typedef void* bt_type;

	explicit NoBacktrace(size_t skip = 0, size_t maxDepth = 128) noexcept
	{
		(void)skip;
		(void)maxDepth;
	}

	Fiber* fiber() const noexcept
	{
		return nullptr;
	}

	uint64_t fiberId() const noexcept
	{
		return 0;
	}

	bt_type bt() const noexcept
	{
		return nullptr;
	}

	bool truncated() const noexcept
	{
		return true;
	}

	Timestamp t0() const noexcept
	{
		return Timestamp();
	}

	Timestamp t1() const noexcept
	{
		return Timestamp();
	}

	void printPartial(size_t start, ssize_t end = -1, int color = -1) const
	{
		(void)start;
		(void)end;
		(void)color;
	}

	void print(int color = -1) const
	{
		(void)color;
	}

	void printDelta(Backtrace const& other, int color = -1) const
	{
		(void)other;
		(void)color;
	}
};

/*!
 * \brief Save a backtrace.
 *
 * Use the type #zth::Backtrace instead.
 *
 * \ingroup zth_api_cpp_util
 */
class Backtrace {
	ZTH_CLASS_NEW_DELETE(Backtrace)
public:
	typedef vector_type<void*>::type bt_type;

	explicit Backtrace(size_t skip = 0, size_t maxDepth = 128) noexcept;

	Fiber* fiber() const noexcept
	{
		return m_fiber;
	}

	uint64_t fiberId() const noexcept
	{
		return m_fiberId;
	}

	bt_type const& bt() const noexcept
	{
		return m_bt;
	}

	bt_type& bt() noexcept
	{
		return m_bt;
	}

	bool truncated() const noexcept
	{
		return m_truncated;
	}

	void truncated(bool set) noexcept
	{
		m_truncated = set;
	}

	Timestamp const& t0() const noexcept
	{
		return m_t0;
	}

	Timestamp const& t1() const noexcept
	{
		return m_t1;
	}

	void printPartial(size_t start, ssize_t end = -1, int color = -1) const;
	void print(int color = -1) const;
	void printDelta(Backtrace const& other, int color = -1) const;

private:
	Timestamp m_t0;
	Timestamp m_t1;
	Fiber* m_fiber;
	uint64_t m_fiberId;
	bt_type m_bt;
	bool m_truncated;
};

template <bool Enable = Config::EnableBacktrace>
struct PickBacktrace {
	typedef Backtrace type;
};

template <>
struct PickBacktrace<false> {
	typedef NoBacktrace type;
};

} // namespace impl

/*!
 * \brief Save a backtrace.
 * \ingroup zth_api_cpp_util
 */
typedef impl::PickBacktrace<>::type Backtrace;

} // namespace zth
#endif // __cplusplus
#endif // ZTH_BACKTRACE_H
