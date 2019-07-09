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

#define ZTH_REDIRECT_IO 0
#include <libzth/config.h>
#include <libzth/io.h>
#include <libzth/util.h>
#include <libzth/worker.h>
#include <libzth/zmq.h>

#ifdef ZTH_HAVE_POLLER

#  include <alloca.h>
#  include <fcntl.h>
#  ifndef POLLIN_SET
#    define POLLIN_SET (/*POLLRDBAND |*/ POLLIN | /*POLLHUP |*/ POLLERR)
#  endif
#  ifndef POLLOUT_SET
#    define POLLOUT_SET (/*POLLWRBAND |*/ POLLOUT | POLLERR)
#  endif
#  ifndef POLLEX_SET
#    define POLLEX_SET (POLLPRI)
#  endif

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

	if((flags & O_NONBLOCK)) {
		zth_dbg(io, "[%s] read(%d) non-blocking", currentFiber().str().c_str(), fd);
		// Just do the call.
		return ::read(fd, buf, count);
	}

	zth_pollfd_t fds = {};
	fds.fd = fd;
	fds.events = POLLIN_SET;
#  ifdef ZTH_HAVE_LIBZMQ
	switch(::zmq_poll(&fds, 1, 0))
#  else
	switch(::poll(&fds, 1, 0))
#  endif
	{
	case 0: {
		// No data, so the read() would block.
		// Forward our request to the Waiter.
		zth_dbg(io, "[%s] read(%d) hand-off", currentFiber().str().c_str(), fd);
		AwaitFd w(&fds, 1);
		if(currentWorker().waiter().waitFd(w)) {
			// Got some error.
			errno = w.error();
			return -1;
		}
		// else: fall-through to go read the data.
	}
	case 1:
		// Got data to read.
		zth_dbg(io, "[%s] read(%d)", currentFiber().str().c_str(), fd);
		return ::read(fd, buf, count);
	
	default:
		// Huh?
		zth_assert(false);
		errno = EINVAL;
		// fall-through
	case -1:
		// Error. Return with errno set.
		return -1;
	}
}

/*!
 * \brief Like normal \c %select(), but forwards the \c %poll() to the #zth::Waiter in case it would block.
 * \ingroup zth_api_cpp_io
 */
int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout) {
	perf_syscall("select()");

	if(nfds < 0) {
		errno = EINVAL;
		return -1;
	}
	if(nfds == 0)
		return 0;
	if(timeout && timeout->tv_sec == 0 && timeout->tv_usec == 0)
		// Non-blocking call.
		return ::select(nfds, readfds, writefds, exceptfds, timeout);

	zth_dbg(io, "[%s] select(%d) hand-off", currentFiber().str().c_str(), nfds);

	// Convert select() to poll()
	zth_pollfd_t* fds_heap = NULL;
	zth_pollfd_t* fds = NULL;
	if(nfds < 16)
		fds = (zth_pollfd_t*)alloca(sizeof(zth_pollfd_t*) * nfds * 3);
	else if(!(fds = fds_heap = (zth_pollfd_t*)malloc(sizeof(zth_pollfd_t*) * nfds * 3))) {
		errno = ENOMEM;
		return -1;
	}

	int nfds_poll = 0;
	for(int fd = 0; fd < nfds; fd++) {
		if(readfds)
			if(FD_ISSET(fd, readfds)) {
				fds[nfds_poll].fd = fd;
				fds[nfds_poll].events = POLLIN_SET;
				nfds_poll++;
			}
		if(writefds)
			if(FD_ISSET(fd, writefds)) {
				fds[nfds_poll].fd = fd;
				fds[nfds_poll].events = POLLOUT_SET;
				nfds_poll++;
			}
		if(exceptfds)
			if(FD_ISSET(fd, exceptfds)) {
				fds[nfds_poll].fd = fd;
				fds[nfds_poll].events = POLLEX_SET;
				nfds_poll++;
			}
	}

	int res = 0;
	Timestamp t;
	if(timeout)
		t = Timestamp::now() + TimeInterval(timeout->tv_sec, (long)timeout->tv_usec * 1000L);

	AwaitFd w(fds, nfds_poll, t);
	if(currentWorker().waiter().waitFd(w))
		// Some error;
		goto done;

	// Success, convert poll() outcome to select() outcome
	if(readfds)
		FD_ZERO(readfds);
	if(writefds)
		FD_ZERO(writefds);
	if(exceptfds)
		FD_ZERO(exceptfds);
	
	for(int i = 0; i < nfds_poll; i++) {
		if(readfds && (fds[i].revents & POLLIN_SET)) {
			FD_SET(i, readfds);
			res++;
		}
		if(writefds && (fds[i].revents & POLLOUT_SET)) {
			FD_SET(i, writefds);
			res++;
		}
		if(exceptfds && (fds[i].revents & POLLEX_SET)) {
			FD_SET(i, exceptfds);
			res++;
		}
	}

done:
	if(fds_heap)
		free(fds_heap);
	if(w.error()) {
		errno = w.error();
		return -1;
	}
	return res;
}

/*!
 * \brief Like normal \c %poll(), but forwards the \c %poll() to the #zth::Waiter in case it would block.
 * \ingroup zth_api_cpp_io
 */
int poll(zth_pollfd_t *fds, int nfds, int timeout) {
	perf_syscall("poll()");

	if(timeout == 0)
		// Non-blocking poll.
#  ifdef ZTH_HAVE_LIBZMQ
		return ::zmq_poll(fds, nfds, 0);
#  else
		return ::poll(fds, (nfds_t)nfds, 0);
#  endif
	
	zth_dbg(io, "[%s] poll(%d) hand-off", currentFiber().str().c_str(), (int)nfds);
	Timestamp t;
	if(timeout > 0)
		t = Timestamp::now() + TimeInterval(timeout / 1000, (long)(timeout % 1000) * 1000000L);
	
	AwaitFd w(fds, nfds, t);
	if(currentWorker().waiter().waitFd(w)) {
		// Some error;
		errno = w.error();
		return -1;
	}

	return w.result();
}

} } // namespace
#else
static int no_io __attribute__((unused));
#endif // ZTH_HAVE_POLLER
