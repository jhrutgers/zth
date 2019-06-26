#ifndef __ZTH_IO_H
#define __ZTH_IO_H
#ifdef __cplusplus

#include <libzth/macros.h>

#ifdef ZTH_HAVE_POLL
#  include <poll.h>
#  include <unistd.h>
#  include <sys/select.h>
#endif

namespace zth { namespace io {

#ifdef ZTH_HAVE_POLL
	extern ssize_t (*real_read)(int,void*,size_t);
	extern int (*real_select)(int,fd_set*,fd_set*,fd_set*,struct timeval*);
	extern int (*real_poll)(struct pollfd*,nfds_t,int);

	ssize_t read(int fd, void* buf, size_t count);
	int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout);
	int poll(struct pollfd *fds, nfds_t nfds, int timeout);
#endif
	
} } // namespace
#endif // __cplusplus
#endif // __ZTH_IO_H
