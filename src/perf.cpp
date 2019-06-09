#include <zth>

#ifndef ZTH_OS_WINDOWS
#  include <execinfo.h>
#endif

#define UNW_LOCAL_ONLY
#include <libunwind.h>

namespace zth {

ZTH_TLS_DEFINE(std::vector<PerfEvent>, perf_eventBuffer)
	
class PerfFiber : public Runnable {
public:
	PerfFiber(Worker* worker)
		: m_worker(worker)
	{
		perf_eventBuffer.reserve(Config::PerfEventBufferSize);
	}

	virtual ~PerfFiber() {}

	void flushEventBuffer() {
		Fiber* f = fiber();
		if(f) {
			m_worker->resume(*f);
			m_worker->schedule(f);
		}
	}

	decltype(perf_eventBuffer)& eventBuffer() const { return perf_eventBuffer; }

protected:
	virtual int fiberHook(Fiber& f) {
		f.setName("zth::PerfFiber");
		f.suspend();
		return Runnable::fiberHook(f);
	}

	virtual void entry() {
		m_vcdBufferContent = 0;

		int vcd = open("zth.vcd", O_WRONLY);
		if(!vcd) {
			fprintf(stderr, "Cannot open VCD file; error %d\n", errno);
			return;
		}

		while(true) {
			decltype(eventBuffer()) eb = eventBuffer();
			size_t eb_size = eb.size();
			for(size_t i = 0; i < eb_size; i++) {
				PerfEvent const& e = eb[i];
				int len = snprintf(m_vcdBuffer + m_vcdBufferContent, sizeof(m_vcdBuffer) - m_vcdBufferContent,
					"%s: %s -> %d\n", e.t.toString().c_str(), e.fiber.name().c_str(), e.fiberstate.state);

				m_vcdBufferContent += len;
				zth_assert(m_vcdBufferContent < sizeof(m_vcdBufferContent));

				if(m_vcdBufferContent > Config::PerfVCDFileBuffer) {
					// Overflow, do a blocking write right now.
					write(vcd, m_vcdBuffer, m_vcdBufferContent);
				}
			}
		}

	write_error:
		close(vcd);
	}

private:
	Worker& m_worker;
	char m_vcdBuffer[Config::PerfVCDFileBuffer + 0x100];
	size_t m_vcdBufferContent;
};

ZTH_TLS_STATIC(PerfFiber*, perfFiber);

void perf_init() {
	Worker* worker;
	getContext(&worker, NULL);
	perfFiber = new PerfFiber(worker);
	worker->add(perfFiber);
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
