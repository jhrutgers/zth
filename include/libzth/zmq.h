#ifndef __ZTH_ZMQ_H
#define __ZTH_ZMQ_H

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
