#include <zth>
#include <signal.h>

static sig_atomic_t volatile stop = 0;

static int check(int res) {
	if(res < 0) {
		int e = errno;
		printf("%s\n", zth::err(errno).c_str());
		errno = e;
	}
	return res;
}

void server() {
	void* responder = zth_zmq_socket(ZMQ_REP);
	int rc __attribute__((unused)) = zmq_bind(responder, "inproc://hello");
	zth_assert(!rc);

	while(true) {
		char buffer[10] = {};

		switch(check(zmq_recv(responder, buffer, sizeof(buffer), 0))) {
		case 0:
			printf("Stopping server...\n");
			check(zmq_send(responder, NULL, 0, 0));
			// fall-through
		case -1:
			zmq_close(responder);
			return;
		}

		printf("Received %s\n", buffer);
		zth::nap(1);          //  Do some 'work'
		check(zmq_send(responder, (void*)"World", 5, 0));
	}
}
zth_fiber(server)

void client(int messages) {
	printf("Connecting to hello world server...\n");
	void* requester = zth_zmq_socket(ZMQ_REQ);
	zmq_connect(requester, "inproc://hello");

	while(messages > 0 && !stop) {
		char buffer[10];
		printf("Sending Hello %d...\n", messages);
		check(zmq_send(requester, (void*)"Hello", 5, 0));
		check(zmq_recv(requester, buffer, sizeof(buffer), 0));
		printf("Received World %d\n", messages);
		messages--;
	}

	// Terminator.
	printf("Stopping client...\n");
	check(zmq_send(requester, NULL, 0, 0));
	check(zmq_recv(requester, NULL, 0, 0));
	zmq_close(requester);
}
zth_fiber(client)

static void handler(int sig) {
	char const* msg = "got interrupted\n";
	for(ssize_t len = strlen(msg), c = 1; c > 0 && len > 0; c = write(fileno(stderr), msg, len), len -= c, msg += c);
	stop = 1;
}

void main_fiber(int argc, char** argv) {
	printf("zmq main fiber\n");

#ifdef ZTH_OS_WINDOWS
	signal(SIGINT, handler);
#else
	struct sigaction sa;
	sa.sa_flags = 0;
	sigemptyset(&sa.sa_mask);
	sa.sa_handler = handler;
	if(sigaction(SIGINT, &sa, NULL) == -1)
		fprintf(stderr, "sigaction() failed; %s", zth::err(errno).c_str());
#endif

	int messages = 10;
	if(argc > 1) {
		messages = atoi(argv[1]);
		if(messages == 0)
			messages = 1;
	}

	async server();
	async client(messages);
}

