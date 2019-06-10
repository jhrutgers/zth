#ifndef _BSD_SOURCE
#  define _BSD_SOURCE
#endif
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
		, m_vcd()
		, m_vcdd()
	{
		zth_assert(worker);
	}

	virtual ~PerfFiber() {
		if(!processEventBuffer())
			finalizeVCD();

		if(m_vcdd) {
			fclose(m_vcdd);
			m_vcdd = NULL;
		}

		if(m_vcd) {
			fclose(m_vcd);
			m_vcd = NULL;
		}

		delete perf_eventBuffer;
		perf_eventBuffer = NULL;
	}

	void flushEventBuffer() {
		Fiber* f = fiber();
		if(f && f->state() >= Fiber::Ready && f->state() < Fiber::Dead) {
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
		if(startVCD())
			return;

		while(true) {
			if(processEventBuffer())
				return;

			m_worker.suspend(*this);
		}

		// Will never get here. The fiber is never properly stopped, but just killed when the Worker terminates.
	}

	int processEventBuffer() {
		if(unlikely(!m_vcdd))
			return EAGAIN;

		int res = 0;

		decltype(eventBuffer()) eb = eventBuffer();
		size_t eb_size = eb.size();

		for(size_t i = 0; i < eb_size; i++) {
			PerfEvent const& e = eb[i];

			if(fprintf(m_vcdd, "#%" PRIu64 "%09ld\n%p -> %d\n", (uint64_t)e.t.ts().tv_sec, e.t.ts().tv_nsec, e.fiber, e.fiberState.state) <= 0) {
				res = errno;
				goto write_error;
			}
		}

	done:
		eb.clear();
		return 0;

	write_error:
		fprintf(stderr, "Cannot write VCD file; %s\n", err(res).c_str());
		if(!res)
			res = EIO;
		goto done;
	}

	int startVCD() {
		int res = 0;
		char vcdFilename_[32];
		char const* vcdFilename = snprintf(vcdFilename_, sizeof(vcdFilename_), "zth.%d.vcd", (int)getpid()) <= 0 ? vcdFilename_ : "zth.vcd";

		if(!(m_vcd = fopen(vcdFilename, "w"))) {
			res = errno;
			fprintf(stderr, "Cannot open VCD file %s; %s\n", vcdFilename, err(res).c_str());
			goto rollback;
		}

		if(!(m_vcdd = tmpfile())) {
			fprintf(stderr, "Cannot open temporary VCD data file; %s\n", err(errno).c_str());
			goto rollback;
		}

		time_t now;
		if(time(&now) != -1) {
#if defined(ZTH_OS_LINUX) || defined(ZTH_OS_MAC)
			char dateBuf[128];
			char* strnow = ctime_r(&now, dateBuf);
#else
			// Possibly not thread-safe.
			char* strnow = ctime(&now);
#endif
			if(strnow)
				if(fprintf(m_vcd, "$date %s$end\n", strnow) < 0)
					goto write_error;
		}

		if(fprintf(m_vcd, "$version %s $end\n$timescale 1 ns $end\n$scope module top $end\n", banner()) < 0)
			goto write_error;

		fprintf(stderr, "Using %s for perf VCD output\n", vcdFilename);
		return 0;

	write_error:
		fprintf(stderr, "Cannot write %s", vcdFilename);
		res = EIO;
	rollback:
		if(m_vcdd) {
			fclose(m_vcdd);
			m_vcdd = NULL;
		}
		if(m_vcd) {
			fclose(m_vcd);
			m_vcd = NULL;
		}
		return res ? res : EIO;
	}

	int finalizeVCD() {
		if(!m_vcd || !m_vcdd)
			return 0;

		int res = 0;
		if(fprintf(m_vcd, "$upscope $end\n$enddefinitions $end\n") < 0)
			goto write_error;

		rewind(m_vcdd);
		char buf[1024];
		size_t cnt;
		while((cnt = fread(buf, 1, sizeof(buf), m_vcdd)) > 0)
			if(fwrite(buf, cnt, 1, m_vcd) != 1)
				goto write_error;

		if(ferror(m_vcdd))
			goto read_error;

		zth_assert(feof(m_vcdd));
		zth_assert(!ferror(m_vcd));

		fclose(m_vcdd);
		m_vcdd = NULL;
		fclose(m_vcd);
		m_vcd = NULL;
		return 0;

	read_error:
		fprintf(stderr, "Cannot read temporary VCD data file\n");
		return res ? res : EIO;
	write_error:
		fprintf(stderr, "Cannot write VCD file\n");
		return res ? res : EIO;
	}

private:
	Worker& m_worker;
	FILE* m_vcd;
	FILE* m_vcdd;
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
