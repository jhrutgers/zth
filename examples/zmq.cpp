#include <zth>
#include <signal.h>

static sig_atomic_t volatile stop = 0;

void server() {
	void* responder = zmq_socket(ZMQ_REP);
	int rc = zmq_bind(responder, "inproc://hello");
	zth_assert(!rc);

	while(!stop) {
		char buffer [10];
		if(zmq_recv(responder, buffer, 10, 0) == -1)
			break;
		printf("Received Hello\n");
		zth::nap(1);          //  Do some 'work'
		zmq_send(responder, (void*)"World", 5, 0);
	}
	
	zmq_close(responder);
}
zth_fiber(server)

void client() {
	printf("Connecting to hello world server...\n");
	void* requester = zmq_socket(ZMQ_REQ);
	zmq_connect(requester, "inproc://hello");

	for(int request_nbr = 0; request_nbr != 10 && !stop; request_nbr++) {
		char buffer [10];
		printf("Sending Hello %d...\n", request_nbr);
		zmq_send(requester, (void*)"Hello", 5, 0);
		zmq_recv(requester, buffer, 10, 0);
		printf("Received World %d\n", request_nbr);
	}

	zmq_close(requester);
}
zth_fiber(client)

static void handler(int sig) {
	char const* msg = "got interrupted\n";
	write(fileno(stderr), msg, strlen(msg));
	stop = 1;
}

void main_fiber(int argc, char** argv) {
	printf("zmq main fiber\n");

	struct sigaction sa;
	sa.sa_flags = 0;
	sigemptyset(&sa.sa_mask);
	sa.sa_handler = handler;
	if(sigaction(SIGINT, &sa, NULL) == -1)
		fprintf(stderr, "sigaction() failed; %s", zth::err(errno).c_str());

	async server();
	async client();
}

