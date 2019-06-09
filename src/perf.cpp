#include <zth>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifndef ZTH_OS_WINDOWS
#  include <execinfo.h>
#endif

#define UNW_LOCAL_ONLY
#include <libunwind.h>

namespace zth {

ZTH_TLS_DEFINE(std::vector<PerfEvent>*, perf_eventBuffer, NULL)
	
class PerfFiber : public Runnable {
public:
	PerfFiber(Worker* worker)
		: m_worker(*worker)
		, m_vcd(-1)
		, m_vcdBuffer()
	{
		zth_assert(worker);
	}

	virtual ~PerfFiber() {
		processEventBuffer();

		if(m_vcdBuffer) {
			free(m_vcdBuffer);
			m_vcdBuffer = NULL;
		}

		if(m_vcd >= 0) {
			close(m_vcd);
			m_vcd = -1;
		}

		delete perf_eventBuffer;
		perf_eventBuffer = NULL;
	}

	void flushEventBuffer() {
		Fiber* f = fiber();
		if(f) {
			m_worker.resume(*f);
			m_worker.schedule(f);
		}
	}

	static decltype(*perf_eventBuffer)& eventBuffer() { 
		zth_assert(perf_eventBuffer);
		return *perf_eventBuffer;
	}

protected:
	virtual int fiberHook(Fiber& f) {
		f.setName("zth::PerfFiber");
		return Runnable::fiberHook(f);
	}

	virtual void entry() {
		m_vcdBufferSize = Config::PerfEventBufferThresholdToTriggerVCDWrite * 16;
		if(!(m_vcdBuffer = (char*)malloc(m_vcdBufferSize))) {
			fprintf(stderr, "Cannot alloc perf buffer; %s\n", err(ENOMEM).c_str());
			return;
		}

		char filename_[32];
		char const* filename = snprintf(filename_, sizeof(filename_), "zth.%d.vcd", (int)getpid()) <= 0 ? filename_ : "zth.vcd";
		if((m_vcd = open(filename, O_CREAT | O_TRUNC | O_WRONLY, 0666)) < 0) {
			fprintf(stderr, "Cannot open VCD file %s; %s\n", filename, err(errno).c_str());
			goto rollback_malloc;
		}

		while(true) {
			if(processEventBuffer())
				goto write_error;

			m_worker.suspend(*this);
		}

	write_error:
		if(m_vcd >= 0) {
			close(m_vcd);
			m_vcd = -1;
		}
	rollback_malloc:
		if(m_vcdBuffer) {
			free(m_vcdBuffer);
			m_vcdBuffer = NULL;
		}
	}

	int processEventBuffer() {
		if(unlikely(m_vcd < 0))
			return EAGAIN;

		int res = 0;

		decltype(eventBuffer()) eb = eventBuffer();
		size_t eb_size = eb.size();
		size_t vcdBufferContent = 0;

		for(size_t i = 0; i < eb_size; i++) {
			PerfEvent const& e = eb[i];
			int len;

			do {
				size_t vcdBufferRemaining = m_vcdBufferSize - vcdBufferContent;

				len = snprintf(m_vcdBuffer + vcdBufferContent, vcdBufferRemaining,
					"%" PRIu64 ".%09ld: %s -> %d\n", (uint64_t)e.t.ts().tv_sec, e.t.ts().tv_nsec, e.fiber->name().c_str(), e.fiberState.state);

				if(likely(len < (int)vcdBufferRemaining))
					break;

				m_vcdBufferSize *= 2;
				zth_dbg(perf, "[Worker %p] Realloc buffer to 0x%zx", &m_worker, m_vcdBufferSize);
				char* newVcdBuffer = (char*)realloc(m_vcdBuffer, m_vcdBufferSize);
				if(!newVcdBuffer) {
					res = ENOMEM;
					fprintf(stderr, "Cannot realloc perf buffer; %s\n", err(ENOMEM).c_str());
					goto error;
				}
				m_vcdBuffer = newVcdBuffer;
			} while(true);

			vcdBufferContent += len;
			zth_assert(vcdBufferContent < m_vcdBufferSize);
		}

		// Dump current buffer to file. This call may block. That is OK, even though other fibers are stalled.
		// This way, doing perf operations does not influence the performance numbers of other fibers.
		// In the vcd file, the overhead of this write() will show up.
		if(likely(vcdBufferContent > 0)) {
			zth_dbg(perf, "[Worker %p] Dump %zu bytes of VCD data", &m_worker, vcdBufferContent);
			for(size_t offset = 0, remaining = vcdBufferContent; remaining > 0; ) {
				ssize_t cnt = write(m_vcd, m_vcdBuffer + offset, remaining);

				if(cnt < 0) {
					res = errno;
					fprintf(stderr, "Cannot write VCD file; %s\n", err(res).c_str());
					goto error;
				}

				zth_assert((size_t)cnt <= remaining);
				offset += (size_t)cnt;
				remaining -= (size_t)cnt;
			}
			vcdBufferContent = 0;
		}

	done:
		eb.clear();
		return 0;
	error:
		if(!res)
			res = EIO;
		goto done;
	}

private:
	Worker& m_worker;
	int m_vcd;
	char* m_vcdBuffer;
	size_t m_vcdBufferSize;
};

ZTH_TLS_STATIC(PerfFiber*, perfFiber, NULL);

int perf_init() {
	perf_eventBuffer = new std::vector<PerfEvent>();
	perf_eventBuffer->reserve(Config::PerfEventBufferSize);

	Worker* worker;
	getContext(&worker, NULL);
	perfFiber = new PerfFiber(worker);
	return perfFiber->run();
}

void perf_deinit() {
	if(perfFiber) {
		delete perfFiber;
		perfFiber = NULL;
	}
}

void perf_flushEventBuffer() {
	zth_assert(perfFiber);
	perfFiber->flushEventBuffer();
}

std::list<void*> backtrace(size_t depth) {
	std::list<void*> res;

#ifndef ZTH_OS_WINDOWS
	void* buf[depth];
	int cnt = ::backtrace(buf, (int)depth);

	for(int i = 0; i < cnt; i++)
		res.push_back(buf[cnt]);
#endif
	printf("%d\n", cnt);

	return res;
}

void print_backtrace(size_t depth) {
	std::list<void*> res = backtrace(depth);

	size_t i = 0;
	for(std::list<void*>::const_iterator it = res.cbegin(); it != res.cend(); ++it, i++)
		zth_log("%2zu: %p\n", i, *it);

	unw_cursor_t cursor; unw_context_t uc;
	unw_word_t ip, sp;

	unw_getcontext(&uc);
	unw_init_local(&cursor, &uc);
	while (unw_step(&cursor) > 0) {
		unw_get_reg(&cursor, UNW_REG_IP, &ip);
		unw_get_reg(&cursor, UNW_REG_SP, &sp);
		printf ("ip = %lx, sp = %lx\n", (long) ip, (long) sp);
	}
}

} // namespace
