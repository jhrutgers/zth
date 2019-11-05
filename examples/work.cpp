#include <zth>
#ifndef ZTH_OS_WINDOWS
#  include <sys/select.h>
#endif
#include <signal.h>
#include <unistd.h>

static void job(int length) {
	printf("job %d started\n", length);
	zth::nap(0.1 * (double)length);
	printf("job %d finished\n", length);
}
zth_fiber(job)

#ifndef ZTH_OS_BAREMETAL
static void handler(int sig) {
	char const* msg = "got interrupted\n";
	for(ssize_t len = strlen(msg), c = 1; c > 0 && len > 0; c = write(fileno(stderr), msg, len), len -= c, msg += c);
}
#endif

static void employer() {
	char buf[128];
	int offset = 0;

#ifndef ZTH_OS_BAREMETAL
#  ifdef ZTH_OS_WINDOWS
	signal(SIGINT, handler);
#  else
	struct sigaction sa;
	sa.sa_flags = 0;
	sigemptyset(&sa.sa_mask);
	sa.sa_handler = handler;
	if(sigaction(SIGINT, &sa, NULL) == -1)
		fprintf(stderr, "sigaction() failed; %s", zth::err(errno).c_str());
#  endif
#endif

	printf("Enter jobs: ");
	fflush(stdout);

	while(true) {
		// Do a blocking read.
		ssize_t cnt = read(0, &buf[offset], sizeof(buf) - offset - 1);
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
zth_fiber(employer)

void main_fiber(int argc, char** argv) {
	employer();
}

