#ifndef _BSD_SOURCE
#  define _BSD_SOURCE
#endif

#define UNW_LOCAL_ONLY

#include <zth>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <map>

#ifndef ZTH_OS_WINDOWS
#  include <execinfo.h>
#  include <dlfcn.h>
#  include <cxxabi.h>
#endif

#ifdef ZTH_HAVE_LIBUNWIND
#include <libunwind.h>
#endif

namespace zth {

ZTH_TLS_DEFINE(std::vector<PerfEvent<> >*, perf_eventBuffer, NULL)
	
class PerfFiber : public Runnable {
public:
	PerfFiber(Worker* worker)
		: m_worker(*worker)
		, m_vcd()
		, m_vcdd()
	{
		zth_assert(worker);
		startVCD();
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
		assert(Config::EnablePerfEvent);

		Fiber* f = fiber();
		size_t spareRoom = eventBuffer().capacity() - eventBuffer().size();
		if(unlikely(!f || f == m_worker.currentFiber() || spareRoom < 2)) {
			// Do it right here right now.
			processEventBuffer();
			return;
		}

		if(likely(spareRoom > 2)) {
			// Wakeup and do the processing later on.
			m_worker.resume(*f);
			return;
		}

		// Almost full, but enough room to signal that we are going to process the buffer.
		Fiber* currentFiber = m_worker.currentFiber();
		if(unlikely(!currentFiber)) {
			// Huh? Do it here anyway.
			processEventBuffer();
			return;
		}

		// Record a 'context switch' to f...
		PerfEvent<> e;
		e.t = Timestamp::now();
		e.fiber = currentFiber->id();
		e.type = PerfEvent<>::FiberState;
		e.fiberState = Fiber::Ready;
		eventBuffer().push_back(e);

		e.fiber = f->id();
		e.fiberState = Fiber::Running;
		eventBuffer().push_back(e);

		zth_dbg(perf, "Emergency processEventBuffer()");
		processEventBuffer();

		// ...and switch back.
		e.t = Timestamp::now();
		e.fiber = f->id();
		e.fiberState = f->state();
		eventBuffer().push_back(e);

		e.fiber = currentFiber->id();
		e.fiberState = currentFiber->state();
		eventBuffer().push_back(e);
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
		if(!m_vcd)
			return;

		while(true) {
			if(processEventBuffer())
				return;

			m_worker.suspend(*this);
		}

		// Will never get here. The fiber is never properly stopped, but just killed when the Worker terminates.
	}

	int processEventBuffer() {
		int res = 0;
		decltype(eventBuffer()) eb = eventBuffer();
		size_t eb_size = eb.size();

		if(unlikely(!m_vcdd || !m_vcd)) {
			res = EAGAIN;
			goto done;
		}

		for(size_t i = 0; i < eb_size; i++) {
			PerfEvent<> const& e = eb[i];
			TimeInterval t = e.t - startTime;
			char const* str = NULL;

			switch(e.type) {
			case PerfEvent<>::FiberName:
				if(e.str) {
					for(char* p = e.str; *p; ++p) {
						if(*p < 33 || *p > 126)
							*p = '_';
					}
					std::string const& id = vcdId(e.fiber);
					if(fprintf(m_vcd, "$var wire 1 %s \\#%" PRIu64 "_%s $end\n$var real 0 %s! \\#%" PRIu64 "_%s/log $end\n",
						id.c_str(), e.fiber, e.str, id.c_str(), e.fiber, e.str) <= 0)
					{
						res = errno;
						goto write_error;
					}
				} else {
					std::string const& id = vcdId(e.fiber);
					if(fprintf(m_vcd, "$var wire 1 %s %" PRIu64 "_Fiber $end\n$var real 0 %s! %" PRIu64 "_Fiber_log $end\n",
						id.c_str(), e.fiber, id.c_str(), e.fiber) <= 0)
					{
						res = errno;
						goto write_error;
					}
				}
				break;

			case PerfEvent<>::FiberState:
			{
				char x;
				bool doCleanup = false;
				switch(e.fiberState) {
				case Fiber::New:		x = 'L'; break;
				case Fiber::Ready:		x = '0'; break;
				case Fiber::Running:	x = '1'; break;
				case Fiber::Waiting:	x = 'W'; break;
				case Fiber::Suspended:	x = 'Z'; break;
				case Fiber::Dead:		x = 'X'; break;
				default:
					x = '-';
					doCleanup = true;
				}

				if(fprintf(m_vcdd, "#%" PRIu64 "%09ld\n%c%s\n", (uint64_t)t.ts().tv_sec, t.ts().tv_nsec, x, vcdId(e.fiber).c_str()) <= 0) {
					res = errno;
					goto write_error;
				}

				if(doCleanup)
					vcdIdRelease(e.fiber);

				break;
			}

			case PerfEvent<>::Log:
			{
				if(true) {
					str = e.c_str;
				} else {
			case PerfEvent<>::Marker:
					str = e.str;
				}
				if(!str)
					break;

				if(fprintf(m_vcdd, "#%" PRIu64 "%09ld\ns", (uint64_t)t.ts().tv_sec, t.ts().tv_nsec) <= 0) {
					res = errno;
					goto write_error;
				}

				char const* chunkStart = str;
				char const* chunkEnd = chunkStart;
				int len = 0;
				while(chunkEnd[1]) {
					if(*chunkEnd < 33 || *chunkEnd > 126) {
						if(fprintf(m_vcdd, "%.*s\\x%02hhx", len, chunkStart, *chunkEnd) < 0) {
							res = errno;
							goto write_error;
						}
						chunkStart = chunkEnd = chunkEnd + 1;
						len = 0;
					} else {
						chunkEnd++;
						len++;
					}
				}

				if(fprintf(m_vcdd, "%s %s!\n", chunkStart, vcdId(e.fiber).c_str()) <= 0) {
					res = errno;
					goto write_error;
				}
			}
			
			default:
				; // ignore
			}
		}

	done:
		for(size_t i = 0; i < eb_size; i++)
			eb[i].release();
		eb.clear();
		return res;

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

#ifdef ZTH_OS_WINDOWS
		// tmpfile() on WIN32 is broken
		m_vcdd = fopen("zth.vcdd", "w+");
#else
		m_vcdd = tmpfile();
#endif
		if(!(m_vcdd)) {
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

	std::string const& vcdId(uint64_t fiber) {
		std::pair<decltype(m_vcdIds.begin()),bool> i = m_vcdIds.insert(std::make_pair(fiber, std::string()));
		if(likely(!i.second))
			// Already in the map. Return the value.
			return i.first->second;

		// Just added with empty string. Generate a proper VCD identifier.
		// Identifier is a string of ASCII 34-126. Use 33 (!) as special character.
		// Do a radix-93 convert.
		uint64_t id = fiber;
		char buf[16]; // the string cannot be longer than 10 chars, as 93^10 > 2^64.
		char* s = &buf[sizeof(buf) - 1];
		*s = 0;

		do {
			*--s = id % 93 + 34;
			id /= 93;
		} while(id);

		return i.first->second = s;
	}

	void vcdIdRelease(uint64_t fiber) {
		decltype(m_vcdIds.begin()) it = m_vcdIds.find(fiber);
		if(it != m_vcdIds.end())
			m_vcdIds.erase(it);
	}

private:
	Worker& m_worker;
	FILE* m_vcd;
	FILE* m_vcdd;
	std::map<uint64_t,std::string> m_vcdIds;
};

ZTH_TLS_STATIC(PerfFiber*, perfFiber, NULL);

int perf_init() {
	if(!Config::EnablePerfEvent)
		return 0;

	perf_eventBuffer = new std::vector<PerfEvent<> >();
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
	if(likely(perfFiber))
		perfFiber->flushEventBuffer();
}

Backtrace::Backtrace(size_t skip)
	: m_depth()
{
	Worker* worker = Worker::currentWorker();
	m_fiber = worker ? worker->currentFiber() : NULL;
	m_fiberId = m_fiber ? m_fiber->id() : 0;

#ifdef ZTH_HAVE_LIBUNWIND
	unw_context_t uc;

	if(unw_getcontext(&uc))
		return;

	unw_cursor_t cursor;
	unw_init_local(&cursor, &uc);
	size_t depth = 0;
	for(size_t i = 0; i < skip && unw_step(&cursor) > 0; i++);

	while (unw_step(&cursor) > 0 && depth < maxDepth) {
		if(depth == 0) {
			unw_word_t sp;
			unw_get_reg(&cursor, UNW_REG_SP, &sp);
			m_sp = (void*)sp;
		}

		unw_word_t ip;
		unw_get_reg(&cursor, UNW_REG_IP, &ip);
		m_bt[depth++] = (void*)ip;
	}

	m_depth = depth;
#endif
}

void Backtrace::print() const {
#ifndef ZTH_OS_WINDOWS
#ifdef ZTH_OS_MAC
	char atos[256];
	FILE* atosf = NULL;
	if(Config::Debug) {
		int len = snprintf(atos, sizeof(atos), "atos -p %d\n", getpid());
		if(len > 0 && (size_t)len < sizeof(atos))
			atosf = popen(atos, "w");
	}
#endif
	
	printf("Backtrace of fiber %p #%" PRIu64 ":\n", m_fiber, m_fiberId);
	fflush(stdout);

	char** syms =
#ifdef ZTH_OS_MAC
		!atosf ? NULL :
#endif
		backtrace_symbols(m_bt, (int)m_depth);

	for(size_t i = 0; i < m_depth; i++) {
#ifdef ZTH_OS_MAC
		if(atosf) {
			fprintf(atosf, "%p\n", m_bt[i]);
			continue;
		}
#endif

		Dl_info info;
		if(dladdr(m_bt[i], &info)) {
			int status;
			char* demangled = abi::__cxa_demangle(info.dli_sname, NULL, 0, &status);
			if(status == 0 && demangled) {
				printf("%-3zd 0x%0*" PRIxPTR " %s + %" PRIuPTR "\n", i, (int)sizeof(void*) * 2, (uintptr_t)m_bt[i], 
					demangled, (uintptr_t)m_bt[i] - (uintptr_t)info.dli_saddr);
				
				free(demangled);
				continue;
			}
		}

		if(syms)
			printf("%s\n", syms[i]);
		else
			printf("%-3zd 0x%0*" PRIxPTR "\n", i, (int)sizeof(void*) * 2, (uintptr_t)m_bt[i]);
	}
	
	if(syms)
		free(syms);

#ifdef ZTH_OS_MAC
	if(atosf)
		pclose(atosf);
#endif

	if(m_depth == maxDepth)
		printf("<truncated>\n");
	else
		printf("<end>\n");
#endif
}

} // namespace
