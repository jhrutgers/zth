#ifndef ZTH_EXCEPTION_H
#define ZTH_EXCEPTION_H
/*
 * SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifdef __cplusplus

namespace zth {

/*!
 * \brief Base exception class for Zth exceptions.
 */
struct exception {};

/*!
 * \brief Exception thrown when an operation cannot be performed as there is no fiber context.
 */
struct not_in_fiber : public exception {};

/*!
 * \brief Exception thrown when an operation cannot be performed, as the fiber is already dead.
 */
struct fiber_already_dead : public exception {};

/*!
 * \brief Exception thrown when an operation cannot be performed due to an invalid coroutine state.
 */
struct coro_invalid_state : public exception {};

/*!
 * \brief Exception thrown when a coroutine has already completed.
 */
struct coro_already_completed : public coro_invalid_state {};

/*!
 * \brief Wrapper for an errno.
 */
struct errno_exception : public exception {
	explicit errno_exception(int err) noexcept
		: error(err)
	{}

	int error;
};

} // namespace zth

#endif // __cplusplus
#endif // ZTH_EXCEPTION_H
