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

#include <libzth/config.h>
#include <libzth/io.h>
#include <libzth/poller.h>
#include <libzth/util.h>
#include <libzth/worker.h>
#include <libzth/zmq.h>

#if defined(ZTH_HAVE_POLLER) && !defined(ZTH_OS_WINDOWS)
#  include <fcntl.h>

namespace zth { namespace io {

/*!
 * \brief Like normal \c %read(), but forwards the \c %poll() to the #zth::Waiter in case it would block.
 * \ingroup zth_api_cpp_io
 */
ssize_t read(int fd, void* buf, size_t count) {
	perf_syscall("read()");

	int flags = fcntl(fd, F_GETFL);
	if(unlikely(flags == -1))
		return -1; // with errno set

	// NOLINTNEXTLINE(hicpp-signed-bitwise)
	if((flags & O_NONBLOCK)) {
		zth_dbg(io, "[%s] read(%d) non-blocking", currentFiber().str().c_str(), fd);
		// Just do the call.
		return ::read(fd, buf, count);
	}

	if((errno = zth::poll(PollableFd(fd, Pollable::PollIn), -1)))
		return -1;

	// Got data to read.
	zth_dbg(io, "[%s] read(%d)", currentFiber().str().c_str(), fd);
	return ::read(fd, buf, count);
}

/*!
 * \brief Like normal \c %write(), but forwards the \c %poll() to the #zth::Waiter in case it would block.
 * \ingroup zth_api_cpp_io
 */
ssize_t write(int fd, void const* buf, size_t count) {
	perf_syscall("write()");

	int flags = fcntl(fd, F_GETFL);
	if(unlikely(flags == -1))
		return -1; // with errno set

	// NOLINTNEXTLINE(hicpp-signed-bitwise)
	if((flags & O_NONBLOCK)) {
		zth_dbg(io, "[%s] write(%d) non-blocking", currentFiber().str().c_str(), fd);
		// Just do the call.
		return ::write(fd, buf, count);
	}

	if((errno = zth::poll(PollableFd(fd, Pollable::PollOut), -1)))
		return -1;

	// Go write the data.
	zth_dbg(io, "[%s] write(%d)", currentFiber().str().c_str(), fd);
	return ::write(fd, buf, count);
}

} } // namespace
#else
static int no_io __attribute__((unused));
#endif // ZTH_HAVE_POLLER
