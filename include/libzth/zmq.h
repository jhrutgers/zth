#ifndef ZTH_ZMQ_H
#define ZTH_ZMQ_H
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
 * \defgroup zth_api_cpp_zmq 0MQ
 * \ingroup zth_api_cpp
 */
/*!
 * \defgroup zth_api_c_zmq 0MQ
 * \ingroup zth_api_c
 */

#include <libzth/macros.h>

#ifdef ZTH_HAVE_LIBZMQ
#  include <zmq.h>

#  ifndef ZTH_REDIRECT_ZMQ
#    define ZTH_REDIRECT_ZMQ 1
#  endif

#  ifdef __cplusplus
namespace zth { namespace zmq {

	void* zmq_context();
	void* zmq_socket(int type);
	int zmq_msg_send(zmq_msg_t *msg, void *socket, int flags);
	int zmq_msg_recv(zmq_msg_t *msg, void *socket, int flags);
	int zmq_send(void *socket, void *buf, size_t len, int flags);
	int zmq_recv(void *socket, void *buf, size_t len, int flags);
	int zmq_send_const(void *socket, void *buf, size_t len, int flags);

} } // namespace

#  endif // __cplusplus

#  if ZTH_REDIRECT_ZMQ
#    ifdef __cplusplus
/*!
 * \copydoc zth::zmq::zmq_context()
 * \details This is a C-wrapper for zth::zmq::zmq_context().
 * \ingroup zth_api_c_zmq
 */
EXTERN_C ZTH_EXPORT ZTH_INLINE void* zth_zmq_context() {
	return zth::zmq::zmq_context(); }

/*!
 * \copydoc zth::zmq::zmq_socket()
 * \details This is a C-wrapper for zth::zmq::zmq_socket().
 * \ingroup zth_api_c_zmq
 */
EXTERN_C ZTH_EXPORT ZTH_INLINE void* zth_zmq_socket(int type) {
	return zth::zmq::zmq_socket(type); }

/*!
 * \copydoc zth::zmq::zmq_msg_send()
 * \details This is a C-wrapper for zth::zmq::zmq_msg_send().
 * \ingroup zth_api_c_zmq
 */
EXTERN_C ZTH_EXPORT ZTH_INLINE int zth_zmq_msg_send(zmq_msg_t *msg, void *socket, int flags) {
	return zth::zmq::zmq_msg_send(msg, socket, flags); }

/*!
 * \copydoc zth::zmq::zmq_msg_recv()
 * \details This is a C-wrapper for zth::zmq::zmq_msg_recv().
 * \ingroup zth_api_c_zmq
 */
EXTERN_C ZTH_EXPORT ZTH_INLINE int zth_zmq_msg_recv(zmq_msg_t *msg, void *socket, int flags) {
	return zth::zmq::zmq_msg_recv(msg, socket, flags); }

/*!
 * \copydoc zth::zmq::zmq_send()
 * \details This is a C-wrapper for zth::zmq::zmq_send().
 * \ingroup zth_api_c_zmq
 */
EXTERN_C ZTH_EXPORT ZTH_INLINE int zth_zmq_send(void *socket, void *buf, size_t len, int flags) {
	return zth::zmq::zmq_send(socket, buf, len, flags); }

/*!
 * \copydoc zth::zmq::zmq_recv()
 * \details This is a C-wrapper for zth::zmq::zmq_recv().
 * \ingroup zth_api_c_zmq
 */
EXTERN_C ZTH_EXPORT ZTH_INLINE int zth_zmq_recv(void *socket, void *buf, size_t len, int flags) {
	return zth::zmq::zmq_recv(socket, buf, len, flags); }

/*!
 * \copydoc zth::zmq::zmq_send_const()
 * \details This is a C-wrapper for zth::zmq::zmq_send_const().
 * \ingroup zth_api_c_zmq
 */
EXTERN_C ZTH_EXPORT ZTH_INLINE int zth_zmq_send_const(void *socket, void *buf, size_t len, int flags) {
	return zth::zmq::zmq_send_const(socket, buf, len, flags); }

#    else // !__cplusplus
ZTH_EXPORT void* zth_zmq_socket(int type);
ZTH_EXPORT int zth_zmq_msg_send(zmq_msg_t *msg, void *socket, int flags);
ZTH_EXPORT int zth_zmq_msg_recv(zmq_msg_t *msg, void *socket, int flags);
ZTH_EXPORT int zth_zmq_send(void *socket, void *buf, size_t len, int flags);
ZTH_EXPORT int zth_zmq_recv(void *socket, void *buf, size_t len, int flags);
ZTH_EXPORT int zth_zmq_send_const(void *socket, void *buf, size_t len, int flags);
#    endif // __cplusplus

#    define zmq_ctx_new		zth_zmq_context
#    define zmq_ctx_term(c)
#    define zmq_socket(c,t)	zth_zmq_socket(t)
#    define zmq_msg_send	zth_zmq_msg_send
#    define zmq_msg_recv	zth_zmq_msg_recv
#    define zmq_send		zth_zmq_send
#    define zmq_recv		zth_zmq_recv
#    define zmq_send_const	zth_zmq_send_const
#  endif // ZTH_REDIRECT_ZMQ

#endif // ZTH_HAVE_LIBZMQ
#endif // ZTH_ZMQ_H
