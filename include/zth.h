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

#include <libzth/macros.h>
#include <libzth/allocator.h>
#include <libzth/async.h>
#include <libzth/config.h>
#include <libzth/fiber.h>
#include <libzth/fsm.h>
#include <libzth/fsm14.h>
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

#endif // ZTH_H
