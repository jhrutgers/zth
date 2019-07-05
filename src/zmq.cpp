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

#define ZTH_REDIRECT_ZMQ 0

#include <libzth/macros.h>
#ifdef ZTH_HAVE_LIBZMQ

#  include <libzth/zmq.h>
#  include <libzth/waiter.h>
#  include <libzth/worker.h>

namespace zth { namespace zmq {

static void zmq_global_deinit() {
	zth_dbg(zmq, "destroy context");
	zmq_ctx_term(zmq_context());
}

static void* zmq_global_init() {
	int major, minor, patch;
	zmq_version(&major, &minor, &patch);
	zth_dbg(banner, "0MQ version is %d.%d.%d", major, minor, patch);

	zth_dbg(zmq, "new context");
	void* zmq_ctx = zmq_ctx_new();

	if(!zmq_ctx)
		zth_abort("0MQ context creation failed; %s", err(errno).c_str());

	// Only do the deinit this when 0MQ was actually used.
	atexit(zmq_global_deinit);
	return zmq_ctx;
}

void* zmq_context() {
	static void* zmq_ctx = zmq_global_init();
	return zmq_ctx;
}
	
void* zmq_socket(int type) {
	return ::zmq_socket(zmq_context(), type);
}

int zmq_msg_send(zmq_msg_t *msg, void *socket, int flags) {
	int res = ::zmq_msg_send(msg, socket, flags | ZMQ_DONTWAIT);
	int err = res == -1 ? zmq_errno() : 0;
	if(err != EAGAIN || (flags & ZMQ_DONTWAIT))
		return res;
	
	zth_dbg(zmq, "[%s] zmq_msg_send(%p) hand-off", zth::currentFiber().str().c_str(), socket);

	Await1Fd w(socket, ZMQ_POLLOUT);

	if((errno = zth::currentWorker().waiter().waitFd(w)))
		return -1;
	
	return ::zmq_msg_send(msg, socket, flags | ZMQ_DONTWAIT);
}

int zmq_msg_recv(zmq_msg_t *msg, void *socket, int flags) {
	int res = ::zmq_msg_recv(msg, socket, flags | ZMQ_DONTWAIT);
	int err = res == -1 ? zmq_errno() : 0;
	if(err != EAGAIN || (flags & ZMQ_DONTWAIT))
		return res;
	
	zth_dbg(zmq, "[%s] zmq_msg_recv(%p) hand-off", zth::currentFiber().str().c_str(), socket);

	Await1Fd w(socket, ZMQ_POLLIN);

	if((errno = zth::currentWorker().waiter().waitFd(w)))
		return -1;
	
	return ::zmq_msg_recv(msg, socket, flags | ZMQ_DONTWAIT);
}

int zmq_send(void *socket, void *buf, size_t len, int flags) {
	int res = ::zmq_send(socket, buf, len, flags | ZMQ_DONTWAIT);
	int err = res == -1 ? zmq_errno() : 0;
	if(err != EAGAIN || (flags & ZMQ_DONTWAIT))
		return res;
	
	zth_dbg(zmq, "[%s] zmq_send(%p) hand-off", zth::currentFiber().str().c_str(), socket);

	Await1Fd w(socket, ZMQ_POLLOUT);

	if((errno = zth::currentWorker().waiter().waitFd(w)))
		return -1;
	
	return ::zmq_send(socket, buf, len, flags | ZMQ_DONTWAIT);
}

int zmq_recv(void *socket, void *buf, size_t len, int flags) {
	int res = ::zmq_recv(socket, buf, len, flags | ZMQ_DONTWAIT);
	int err = res == -1 ? zmq_errno() : 0;
	if(err != EAGAIN || (flags & ZMQ_DONTWAIT))
		return res;
	
	zth_dbg(zmq, "[%s] zmq_recv(%p) hand-off", zth::currentFiber().str().c_str(), socket);

	Await1Fd w(socket, ZMQ_POLLIN);

	if((errno = zth::currentWorker().waiter().waitFd(w)))
		return -1;
	
	return ::zmq_recv(socket, buf, len, flags | ZMQ_DONTWAIT);
}

int zmq_send_const(void *socket, void *buf, size_t len, int flags) {
	int res = ::zmq_send_const(socket, buf, len, flags | ZMQ_DONTWAIT);
	int err = res == -1 ? zmq_errno() : 0;
	if(err != EAGAIN || (flags & ZMQ_DONTWAIT))
		return res;
	
	zth_dbg(zmq, "[%s] zmq_send_const(%p) hand-off", zth::currentFiber().str().c_str(), socket);

	Await1Fd w(socket, ZMQ_POLLOUT);

	if((errno = zth::currentWorker().waiter().waitFd(w)))
		return -1;
	
	return ::zmq_send_const(socket, buf, len, flags | ZMQ_DONTWAIT);
}

} } // namespace
#else
static int no_zmq __attribute__((unused));
#endif // ZTH_HAVE_LIBZMQ
