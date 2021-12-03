// Not for Windows, as Windows cannot poll() stdin.

#include <zth>
#include <sys/select.h>
#include <csignal>
#include <unistd.h>

static void job(int length)
{
	static int id = 0;
	int job_id = ++id;

	printf("job %d: starting  %d.%d s\n", job_id, length / 10, length % 10);
	zth::nap(0.1 * (double)length);
	printf("job %d: finished  %d.%d s\n", job_id, length / 10, length % 10);
}
zth_fiber(job)

#ifndef ZTH_OS_BAREMETAL
static void handler(int /*sig*/)
{
	char const* msg = "Got interrupted. Ignored. Press Ctrl+D to stop.\n";
	for(ssize_t len = strlen(msg), c = 1; c > 0 && len > 0; c = write(fileno(stderr), msg, len), len -= c, msg += c);
}
#endif

static void employer()
{
#ifndef ZTH_OS_BAREMETAL
	struct sigaction sa = {};
	sa.sa_flags = 0;
	sigemptyset(&sa.sa_mask);
	sa.sa_handler = handler;
	if(sigaction(SIGINT, &sa, NULL) == -1)
		fprintf(stderr, "sigaction() failed; %s", zth::err(errno).c_str());
#endif

	printf("Enter job lengths in 0.1 s units: ");
	fflush(stdout);

	char buf[128];
	size_t len = 0;

	while(true) {
		// Do a blocking read.
		ssize_t cnt = zth::io::read(0, &buf[len], sizeof(buf) - len - 1u);

		if(cnt == 0) {
			// stdin was closed.
			printf("Stopped issuing jobs\n");
			return;
		} else if(cnt < 0) {
			switch(errno) {
			case EINTR:
				// Just interrupted. Try again.
				break;
			case EAGAIN:
#if EAGAIN != EWOULDBLOCK
			case EWOULDBLOCK:
#endif
				fprintf(stderr, "Unexpected non-blocking read\n");
				return;
			default:
				fprintf(stderr, "Cannot read stdin; %s", zth::err(errno).c_str());
				return;
			}
		}

		len += cnt;
		zth_assert(len < sizeof(buf));

		// Make sure the string is terminated.
		buf[len] = 0;

		// Go parse the buffer.
		size_t totalScanned = 0;
		char* s = buf;

		while(true) {
			zth::yield();

			int work = 0;
			int scanned = 0;

			switch(sscanf(s, "%d%n", &work, &scanned)) {
			case 1:
				if(totalScanned + scanned < len) {
					// More chars are in the buffer (at least a whitespace).
					s += scanned;
					totalScanned += scanned;

					// Start job.
					async job(work);
					continue;
				}
				// fall-through
			default:
				if(totalScanned == 0 && len == sizeof(buf) - 1u) {
					// Buffer does not make sense.
					totalScanned = len;
				}
			}
			break;
		}

		if((len -= totalScanned))
			memmove(buf, &buf[totalScanned], len);

		buf[len] = '\0';
	}
}
zth_fiber(employer)

void main_fiber(int /*argc*/, char** /*argv*/)
{
	employer();
}

