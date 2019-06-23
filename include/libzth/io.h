#ifndef __ZTH_IO_H
#define __ZTH_IO_H
#ifdef __cplusplus

#include <libzth/macros.h>
#include <unistd.h>

namespace zth { namespace io {

	ssize_t read(int fd, void* buf, size_t count);
	
} } // namespace
#endif // __cplusplus
#endif // __ZTH_IO_H
