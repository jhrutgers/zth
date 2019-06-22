#include <zth>
#include <sys/select.h>
#include <unistd.h>

void job(int length) {
	printf("job %d started\n", length);
	zth::nap(0.1 * (double)length);
	printf("job %d finished\n", length);
}
make_fibered(job)

void employer() {
	char buf[128];
	int offset = 0;
	fd_set fds;

	printf("Enter jobs: ");
	fflush(stdout);

	while(true) {
		zth::nap(0.5);
		FD_ZERO(&fds);
		FD_SET(0, &fds);
		struct timeval tv = {};

		switch(select(1, &fds, NULL, NULL, &tv)) {
		case 1: {
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
		case 0:
			// timeout
			break;
		default:
			printf("select() returned error %d\n", errno);
			return;
		}
	}
}
make_fibered(employer)

int main() {
	zth::Worker w;
	async employer();
	w.run();
}
