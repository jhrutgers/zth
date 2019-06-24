#include <zth>
#include <sys/select.h>
#include <signal.h>
#include <unistd.h>

void job(int length) {
	printf("job %d started\n", length);
	zth::nap(0.1 * (double)length);
	printf("job %d finished\n", length);
}
make_fibered(job)

static void handler(int sig) {
	char const* msg = "got interrupted\n";
	write(fileno(stderr), msg, strlen(msg));
}

void employer() {
	char buf[128];
	int offset = 0;

	struct sigaction sa;
	sa.sa_flags = 0;
	sigemptyset(&sa.sa_mask);
	sa.sa_handler = handler;
	if(sigaction(SIGINT, &sa, NULL) == -1)
		fprintf(stderr, "sigaction() failed; %s", zth::err(errno).c_str());

	printf("Enter jobs: ");
	fflush(stdout);

	while(true) {
		// Do a blocking read.
		ssize_t cnt = zth::io::read(0, &buf[offset], sizeof(buf) - offset - 1);
		if(cnt <= 0) {
			printf("Couldn't read stdin\n");
			return;
		}
		buf[offset + cnt] = 0;

		int totalScanned = 0;
		char* s = buf;
		while(true) {
			zth::yield();

			char work[10];
			int scanned;
			char dummy;
			int res = sscanf(s, "%9s %n%c", work, &scanned, &dummy);
			//printf("sscanf(\"%s\") returned %d\n", s, res);
			if(res != 2)
				break;
			async job(atoi(work));

			totalScanned += scanned;
			s += scanned;
		}
		offset += cnt - totalScanned;
		if(offset > 0)
			memmove(buf, &buf[totalScanned], offset);

		//printf("sscanf(): cnt=%zd tot=%d off=%d\n", cnt, totalScanned, offset);
	}
}
make_fibered(employer)

int main() {
	zth::Worker w;
	async employer();
	w.run();
}
