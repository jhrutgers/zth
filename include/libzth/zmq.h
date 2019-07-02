#ifndef __ZTH_ZMQ_H
#define __ZTH_ZMQ_H
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

#include <libzth/macros.h>

#ifdef ZTH_HAVE_LIBZMQ
#  include <zmq.h>

#  ifndef ZTH_REDIRECT_ZMQ
#    define ZTH_REDIRECT_ZMQ 1
#  endif

#  ifdef __cplusplus
namespace zth { namespace zmq {

	void* zmq_context();
	void *zmq_socket(int type);
	int zmq_msg_send(zmq_msg_t *msg, void *socket, int flags);
	int zmq_msg_recv(zmq_msg_t *msg, void *socket, int flags);
	int zmq_send(void *socket, void *buf, size_t len, int flags);
	int zmq_recv(void *socket, void *buf, size_t len, int flags);
	int zmq_send_const(void *socket, void *buf, size_t len, int flags);

} } // namespace

#  endif // __cplusplus

#  if ZTH_REDIRECT_ZMQ
ZTH_EXPORT_INLINE_CPPONLY void* zth_zmq_socket(int type)
	ZTH_EXPORT_INLINE_CPPONLY_IMPL({ return zth::zmq::zmq_socket(type); })
ZTH_EXPORT_INLINE_CPPONLY int zth_zmq_msg_send(zmq_msg_t *msg, void *socket, int flags)
	ZTH_EXPORT_INLINE_CPPONLY_IMPL({ return zth::zmq::zmq_msg_send(msg, socket, flags); })
ZTH_EXPORT_INLINE_CPPONLY int zth_zmq_msg_recv(zmq_msg_t *msg, void *socket, int flags)
	ZTH_EXPORT_INLINE_CPPONLY_IMPL({ return zth::zmq::zmq_msg_recv(msg, socket, flags); })
ZTH_EXPORT_INLINE_CPPONLY int zth_zmq_send(void *socket, void *buf, size_t len, int flags)
	ZTH_EXPORT_INLINE_CPPONLY_IMPL({ return zth::zmq::zmq_send(socket, buf, len, flags); })
ZTH_EXPORT_INLINE_CPPONLY int zth_zmq_recv(void *socket, void *buf, size_t len, int flags)
	ZTH_EXPORT_INLINE_CPPONLY_IMPL({ return zth::zmq::zmq_recv(socket, buf, len, flags); })
ZTH_EXPORT_INLINE_CPPONLY int zth_zmq_send_const(void *socket, void *buf, size_t len, int flags)
	ZTH_EXPORT_INLINE_CPPONLY_IMPL({ return zth::zmq::zmq_send_const(socket, buf, len, flags); })

#    define zmq_socket		zth_zmq_socket
#    define zmq_msg_send	zth_zmq_msg_send
#    define zmq_msg_recv	zth_zmq_msg_recv
#    define zmq_send		zth_zmq_send
#    define zmq_recv		zth_zmq_recv
#    define zmq_send_const	zth_zmq_send_const
#  endif // ZTH_REDIRECT_ZMQ

#endif // ZTH_HAVE_LIBZMQ
#endif // __ZTH_ZMQ_H
