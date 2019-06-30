#ifndef __ZTH_IO_H
#define __ZTH_IO_H

#include <libzth/macros.h>

#ifndef ZTH_REDIRECT_IO
#  define ZTH_REDIRECT_IO 1
#endif

#include <unistd.h>

#ifdef ZTH_HAVE_POLL
#  include <poll.h>
#endif

#ifdef ZTH_HAVE_LIBZMQ
#  include <zmq.h>
#  include <sys/select.h>
	typedef zmq_pollitem_t zth_pollfd_t;
#elif defined(ZTH_HAVE_POLL)
#  include <sys/select.h>
	typedef struct pollfd zth_pollfd_t;
#endif

#ifdef __cplusplus
namespace zth { namespace io {

#  if defined(ZTH_HAVE_POLL) || defined(ZTH_HAVE_LIBZMQ)
	ssize_t read(int fd, void* buf, size_t count);
	int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout);
	int poll(zth_pollfd_t *fds, int nfds, int timeout);
#  endif
	
} } // namespace
#endif // __cplusplus

#if ZTH_REDIRECT_IO
ZTH_EXPORT_INLINE_CPPONLY ssize_t zth_read(int fd, void* buf, size_t count)
	ZTH_EXPORT_INLINE_CPPONLY_IMPL({ return zth::io::read(fd, buf, count); })
ZTH_EXPORT_INLINE_CPPONLY int zth_select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout)
	ZTH_EXPORT_INLINE_CPPONLY_IMPL({ return zth::io::select(nfds, readfds, writefds, exceptfds, timeout); })
ZTH_EXPORT_INLINE_CPPONLY int zth_poll(zth_pollfd_t *fds, int nfds, int timeout)
	ZTH_EXPORT_INLINE_CPPONLY_IMPL({ return zth::io::poll(fds, nfds, timeout); })

#  define read		zth_read
#  define select	zth_select
#  define poll		zth_poll
#endif // ZTH_REDIRECT_IO

#endif // __ZTH_IO_H
