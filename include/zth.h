#ifndef ZTH_H
#define ZTH_H
/*
 * SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
 *
 * SPDX-License-Identifier: MPL-2.0
 */

/*!
 * \defgroup zth_api_cpp C++ API
 * \brief C++ interface to Zth (but all \ref zth_api_c functions are available as well).
 */
/*!
 * \defgroup zth_api_c C API
 * \brief C interface to Zth.
 */
/*!
 * \defgroup zth_api_rust Rust API
 * \brief Rust wrapper for Zth.
 * \see <a href="rust/zth/index.html">Rust documentation</a>
 */

#include <libzth/macros.h>

#include <libzth/allocator.h>
#include <libzth/async.h>
#include <libzth/backtrace.h>
#include <libzth/config.h>
#include <libzth/coro.h>
#include <libzth/exception.h>
#include <libzth/fiber.h>
#include <libzth/fsm14.h>
#include <libzth/future.h>
#include <libzth/init.h>
#include <libzth/io.h>
#include <libzth/perf.h>
#include <libzth/poller.h>
#include <libzth/sync.h>
#include <libzth/time.h>
#include <libzth/util.h>
#include <libzth/version.h>
#include <libzth/waiter.h>
#include <libzth/worker.h>
#include <libzth/zmq.h>

#ifdef __cplusplus
#  ifndef ZTH_INLINE_EMIT
namespace zth {
ZTH_INIT_CALL_(checkConfig, checkConfig();)
static inline checkConfig__init const checkConfig__init_;
} // namespace zth
#  endif // !ZTH_INLINE_EMIT
#endif	 // __cplusplus

#endif // ZTH_H
