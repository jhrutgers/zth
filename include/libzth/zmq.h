#ifndef __ZTH_ZMQ_H
#define __ZTH_ZMQ_H

#include <libzth/macros.h>

#ifdef ZTH_HAVE_LIBZMQ
#include <zmq.h>

#ifdef __cplusplus
namespace zth { namespace zmq {

	void* zmq_context();
	void *zmq_socket(int type);
	int zmq_msg_send(zmq_msg_t *msg, void *socket, int flags);
	int zmq_msg_recv(zmq_msg_t *msg, void *socket, int flags);
	int zmq_send(void *socket, void *buf, size_t len, int flags);
	int zmq_recv(void *socket, void *buf, size_t len, int flags);
	int zmq_send_const(void *socket, void *buf, size_t len, int flags);

} } // namespace

//using namespace zth::zmq;

#endif // __cplusplus
#endif // ZTH_HAVE_LIBZMQ
#endif // __ZTH_ZMQ_H
