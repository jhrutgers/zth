#ifndef ZTH_IO_H
#define ZTH_IO_H
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
 * \defgroup zth_api_cpp_io io
 * \ingroup zth_api_cpp
 */
/*!
 * \defgroup zth_api_c_io io
 * \ingroup zth_api_c
 */

#include <libzth/macros.h>
#include <libzth/poller.h>

#include <unistd.h>

#if defined(ZTH_HAVE_POLLER)
#	if !defined(ZTH_OS_WINDOWS)
#		if defined(__cplusplus)
namespace zth {
namespace io {

ZTH_EXPORT ssize_t read(int fd, void* buf, size_t count);
ZTH_EXPORT ssize_t write(int fd, void const* buf, size_t count);

} // namespace io
} // namespace zth

/*!
 * \copydoc zth::io::read()
 * \details This is a C-wrapper for zth::io::read().
 * \ingroup zth_api_c_io
 */
EXTERN_C ZTH_EXPORT ZTH_INLINE ssize_t zth_read(int fd, void* buf, size_t count)
{
	return zth::io::read(fd, buf, count);
}

/*!
 * \copydoc zth::io::write()
 * \details This is a C-wrapper for zth::io::write().
 * \ingroup zth_api_c_io
 */
EXTERN_C ZTH_EXPORT ZTH_INLINE ssize_t zth_write(int fd, void const* buf, size_t count)
{
	return zth::io::write(fd, buf, count);
}

#		else  // !__cplusplus
ZTH_EXPORT ssize_t zth_read(int fd, void* buf, size_t count);
ZTH_EXPORT ssize_t zth_write(int fd, void const* buf, size_t count);
#		endif // __cplusplus
#	endif	       // !ZTH_OS_WINDOWS
#endif		       // ZTH_HAVE_POLLER

#endif // ZTH_IO_H
