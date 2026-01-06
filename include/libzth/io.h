#ifndef ZTH_IO_H
#define ZTH_IO_H
/*
 * SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
 *
 * SPDX-License-Identifier: MPL-2.0
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
#  if !defined(ZTH_OS_WINDOWS)
#    if defined(__cplusplus)
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

#    else  // !__cplusplus
ZTH_EXPORT ssize_t zth_read(int fd, void* buf, size_t count);
ZTH_EXPORT ssize_t zth_write(int fd, void const* buf, size_t count);
#    endif // __cplusplus
#  endif   // !ZTH_OS_WINDOWS
#endif	   // ZTH_HAVE_POLLER

#endif // ZTH_IO_H
