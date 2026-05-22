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

UniqueID<Fiber> const& currentFiberID() noexcept;

/*!
 * \brief Save a backtrace.
 * \ingroup zth_api_cpp_util
 */
class Backtrace {
	ZTH_CLASS_NEW_DELETE(Backtrace)
public:
	typedef vector_type<void*>::type bt_type;

	explicit Backtrace(size_t skip = 0, size_t maxDepth = 128);
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

	bool truncated() const noexcept
	{
		return m_truncated;
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

} // namespace zth
#endif // __cplusplus
#endif // ZTH_BACKTRACE_H
