#ifndef ZTH_H
#define ZTH_H
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
 * \defgroup zth_api_cpp C++ API
 * \brief C++ interface to Zth (but all \ref zth_api_c functions are available as well).
 */
/*!
 * \defgroup zth_api_c C API
 * \brief C interface to Zth.
 */

#include <libzth/macros.h>
#include <libzth/init.h>
#include <libzth/zmq.h>
#include <libzth/config.h>
#include <libzth/util.h>
#include <libzth/allocator.h>
#include <libzth/time.h>
#include <libzth/version.h>
#include <libzth/fiber.h>
#include <libzth/worker.h>
#include <libzth/poller.h>
#include <libzth/io.h>
#include <libzth/waiter.h>
#include <libzth/sync.h>
#include <libzth/async.h>
#include <libzth/perf.h>
#include <libzth/fsm.h>

// You probably don't need these headers in your application.
//#include <libzth/regs.h>

#endif // ZTH_H
