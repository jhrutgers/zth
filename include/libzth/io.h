#ifndef __ZTH_IO_H
#define __ZTH_IO_H
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

/*!
 * \defgroup zth_api_cpp_io io
 * \ingroup zth_api_cpp
 */
/*!
 * \defgroup zth_api_c_io io
 * \ingroup zth_api_c
 */

#include <libzth/macros.h>
#include <unistd.h>

#if defined(ZTH_HAVE_POLL) || defined(ZTH_HAVE_LIBZMQ)
#  define ZTH_HAVE_POLLER
#endif

#ifdef ZTH_HAVE_POLLER

#  ifndef ZTH_REDIRECT_IO
#    define ZTH_REDIRECT_IO 1
#  endif

#  ifdef ZTH_HAVE_POLL
#    include <poll.h>
#  endif

#  ifdef ZTH_HAVE_LIBZMQ
#    include <zmq.h>
#    include <sys/select.h>
	typedef zmq_pollitem_t zth_pollfd_t;
#  elif defined(ZTH_HAVE_POLL)
#    include <sys/select.h>
	typedef struct pollfd zth_pollfd_t;
#  endif

#  ifdef __cplusplus
namespace zth { namespace io {

	ZTH_EXPORT ssize_t read(int fd, void* buf, size_t count);
	ZTH_EXPORT int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout);
	ZTH_EXPORT int poll(zth_pollfd_t *fds, int nfds, int timeout);
	
} } // namespace
#  endif // __cplusplus

#  if ZTH_REDIRECT_IO
#    ifdef __cplusplus
/*!
 * \copydoc zth::io::read()
 * \details This is a C-wrapper for zth::io::read().
 * \ingroup zth_api_c_io
 */
EXTERN_C ZTH_EXPORT ZTH_INLINE ssize_t zth_read(int fd, void* buf, size_t count) {
	return zth::io::read(fd, buf, count); }

/*!
 * \copydoc zth::io::select()
 * \details This is a C-wrapper for zth::io::select().
 * \ingroup zth_api_c_io
 */
EXTERN_C ZTH_EXPORT ZTH_INLINE int zth_select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout) {
	return zth::io::select(nfds, readfds, writefds, exceptfds, timeout); }

/*!
 * \copydoc zth::io::poll()
 * \details This is a C-wrapper for zth::io::poll().
 * \ingroup zth_api_c_io
 */
EXTERN_C ZTH_EXPORT ZTH_INLINE int zth_poll(zth_pollfd_t *fds, int nfds, int timeout) {
	return zth::io::poll(fds, nfds, timeout); }

#    else // !__cplusplus
ZTH_EXPORT ssize_t zth_read(int fd, void* buf, size_t count);
ZTH_EXPORT int zth_select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout);
ZTH_EXPORT int zth_poll(zth_pollfd_t *fds, int nfds, int timeout);
#    endif // __cplusplus

#    define read		zth_read
#    define select		zth_select
#    define poll		zth_poll
#  endif // ZTH_REDIRECT_IO

#endif // ZTH_HAVE_POLLER
#endif // __ZTH_IO_H
