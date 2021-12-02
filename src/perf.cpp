/*
 * Zth (libzth), a cooperative userspace multitasking library.
 * Copyright (C) 2019-2021  Jochem Rutgers
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#define UNW_LOCAL_ONLY

#include <libzth/macros.h>

#ifdef ZTH_OS_MAC
#  ifndef _BSD_SOURCE
#    define _BSD_SOURCE
#  endif
#endif

#include <libzth/perf.h>
#include <libzth/worker.h>

#include <unistd.h>
#include <cstdlib>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <map>

#if !defined(ZTH_OS_WINDOWS) && !defined(ZTH_OS_BAREMETAL)
#  include <execinfo.h>
#  include <dlfcn.h>
#  include <cxxabi.h>
#endif

#ifdef ZTH_HAVE_LIBUNWIND
#  include <libunwind.h>
#endif

namespace zth {

ZTH_TLS_DEFINE(perf_eventBuffer_type*, perf_eventBuffer, nullptr)

class PerfFiber : public Runnable {
	ZTH_CLASS_NOCOPY(PerfFiber)
public:
	explicit PerfFiber(Worker* worker)
		: m_worker(*worker)
		, m_vcd()
		, m_vcdd()
	{
		zth_assert(worker);
		startVCD();
	}

	virtual ~PerfFiber() override
	{
		if(!processEventBuffer())
			finalizeVCD();

		if(m_vcdd) {
			fclose(m_vcdd);
			m_vcdd = nullptr;
		}

		if(m_vcd) {
			fclose(m_vcd);
			m_vcd = nullptr;
		}

		if(perf_eventBuffer) {
			for(size_t i = 0; i < perf_eventBuffer->size(); i++)
				(*perf_eventBuffer)[i].release();

			delete perf_eventBuffer;
			perf_eventBuffer = nullptr;
		}
	}

	void flushEventBuffer()
	{
		zth_assert(Config::EnablePerfEvent);

		Fiber* f = fiber();
		size_t spareRoom = eventBuffer().capacity() - eventBuffer().size();
		if(unlikely(!f || f == m_worker.currentFiber() || spareRoom < 3)) {
			// Do it right here right now.
			processEventBuffer();
			return;
		}

		if(likely(spareRoom > 3)) {
			// Wakeup and do the processing later on.
			m_worker.resume(*f);
			return;
		}

		// Almost full, but enough room to signal that we are going to process the buffer.
		Fiber* currentFiber_ = m_worker.currentFiber();
		if(unlikely(!currentFiber_)) {
			// Huh? Do it here anyway.
			processEventBuffer();
			return;
		}

		// Record a 'context switch' to f...
		PerfEvent<> e;
		e.t = Timestamp::now();
		e.fiber = f->id();
		e.type = PerfEvent<>::Marker;
		e.c_str = "Emerg flush";
		eventBuffer().push_back(e);

		e.fiber = currentFiber_->id();
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

		e.fiber = currentFiber_->id();
		e.fiberState = currentFiber_->state();
		eventBuffer().push_back(e);
	}

	static perf_eventBuffer_type& eventBuffer()
	{
		zth_assert(perf_eventBuffer);
		return *perf_eventBuffer;
	}

protected:
	virtual int fiberHook(Fiber& f) override
	{
		f.setName("zth::PerfFiber");
		return Runnable::fiberHook(f);
	}

	virtual void entry() override
	{
		if(!m_vcd)
			return;

		while(true) {
			if(processEventBuffer())
				return;

			m_worker.suspend(*this);
		}

		// Will never get here. The fiber is never properly stopped, but just killed when the Worker terminates.
	}

	int processEventBuffer()
	{
		int res = 0;
		perf_eventBuffer_type& eb = eventBuffer();
		size_t eb_size = eb.size();

		if(unlikely(!m_vcdd || !m_vcd)) {
			res = EAGAIN;
			goto done;
		}

		for(size_t i = 0; i < eb_size; i++) {
			PerfEvent<> const& e = eb[i];
			TimeInterval t = e.t - startTime;
			char const* s = nullptr;

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
				char x = '-';
				bool doCleanup = false;
				switch(e.fiberState) {
				case Fiber::New:	x = 'L'; break;
				case Fiber::Ready:	x = '0'; break;
				case Fiber::Running:	x = '1'; break;
				case Fiber::Waiting:	x = 'W'; break;
				case Fiber::Suspended:	x = 'Z'; break;
				case Fiber::Dead:	x = 'X'; break;
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
				if(true) { // NOLINT
					s = e.c_str;
				} else {
			case PerfEvent<>::Marker:
					s = e.str;
				}
				if(!s)
					break;

				if(fprintf(m_vcdd, "#%" PRIu64 "%09ld\ns", (uint64_t)t.ts().tv_sec, t.ts().tv_nsec) <= 0) {
					res = errno;
					goto write_error;
				}

				char const* chunkStart = s;
				char const* chunkEnd = chunkStart;
				int len = 0;
				while(chunkEnd[1]) {
					if(*chunkEnd < 33 || *chunkEnd > 126) {
						if(fprintf(m_vcdd, "%.*s\\x%02hhx", len, chunkStart, (unsigned char)*chunkEnd) < 0) {
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

	int startVCD()
	{
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

		{
			time_t now = -1;
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
			m_vcdd = nullptr;
		}
		if(m_vcd) {
			fclose(m_vcd);
			m_vcd = nullptr;
		}
		return res ? res : EIO;
	}

	int finalizeVCD()
	{
		if(!m_vcd || !m_vcdd)
			return 0;

		int res = 0;
		if(fprintf(m_vcd, "$upscope $end\n$enddefinitions $end\n") < 0)
			goto write_error;

		rewind(m_vcdd);

		{
			char buf[1024];
			size_t cnt = 0;
			while((cnt = fread(buf, 1, sizeof(buf), m_vcdd)) > 0)
				if(fwrite(buf, cnt, 1, m_vcd) != 1)
					goto write_error;
		}

		if(ferror(m_vcdd))
			goto read_error;

		zth_assert(feof(m_vcdd));
		zth_assert(!ferror(m_vcd));

		fclose(m_vcdd);
		m_vcdd = nullptr;
		fclose(m_vcd);
		m_vcd = nullptr;
		return 0;

	read_error:
		fprintf(stderr, "Cannot read temporary VCD data file\n");
		return res ? res : EIO;
	write_error:
		fprintf(stderr, "Cannot write VCD file\n");
		return res ? res : EIO;
	}

	std::string const& vcdId(uint64_t fiber)
	{
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
			*--s = (char)(id % 93 + 34);
			id /= 93;
		} while(id);

		return i.first->second = s;
	}

	void vcdIdRelease(uint64_t fiber)
	{
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

ZTH_TLS_STATIC(PerfFiber*, perfFiber, nullptr);

int perf_init()
{
	if(!zth_config(DoPerfEvent))
		return 0;

	perf_eventBuffer = new std::vector<PerfEvent<> >();
	perf_eventBuffer->reserve(Config::PerfEventBufferSize);

	Worker* worker = nullptr;
	getContext(&worker, nullptr);
	perfFiber = new PerfFiber(worker);
	return perfFiber->run();
}

void perf_deinit()
{
	if(perfFiber) {
		delete perfFiber;
		perfFiber = nullptr;
	}
}

void perf_flushEventBuffer()
{
	if(likely(perfFiber))
		perfFiber->flushEventBuffer();
}

extern "C" void context_entry(Context* context);

Backtrace::Backtrace(size_t UNUSED_PAR(skip), size_t UNUSED_PAR(maxDepth))
	: m_t0(Timestamp::now())
{
	Worker* worker = Worker::currentWorker();
	m_fiber = worker ? worker->currentFiber() : nullptr;
	m_fiberId = m_fiber ? m_fiber->id() : 0;
	m_truncated = true;

#ifdef ZTH_HAVE_LIBUNWIND
	unw_context_t uc;

	if(unw_getcontext(&uc))
		return;

	unw_cursor_t cursor;
	unw_init_local(&cursor, &uc);
	size_t depth = 0;
	for(size_t i = 0; i < skip && unw_step(&cursor) > 0; i++);

	unw_proc_info_t pip;

	while (unw_step(&cursor) > 0 && depth < maxDepth) {
		if(depth == 0) {
			unw_word_t sp = 0;
			unw_get_reg(&cursor, UNW_REG_SP, &sp);
		}

		unw_word_t ip = 0;
		unw_get_reg(&cursor, UNW_REG_IP, &ip);
		m_bt.push_back(reinterpret_cast<void*>(ip));

		if(unw_get_proc_info(&cursor, &pip) == 0 && pip.start_ip == reinterpret_cast<unw_word_t>(&context_entry))
			// Stop here, as we might get segfaults when passing the context_entry functions.
			break;
	}

	m_truncated = depth == maxDepth;
#elif !defined(ZTH_OS_WINDOWS) && !defined(ZTH_OS_BAREMETAL)
	m_bt.resize(maxDepth);
	m_bt.resize((size_t)backtrace(&m_bt[0], (int)maxDepth));
	m_truncated = m_bt.size() == maxDepth;
#endif

	m_t1 = Timestamp::now();
}

void Backtrace::printPartial(size_t UNUSED_PAR(start), ssize_t UNUSED_PAR(end), int UNUSED_PAR(color)) const
{
#if !defined(ZTH_OS_WINDOWS) && !defined(ZTH_OS_BAREMETAL)
	if(bt().empty())
		return;
	if(end < 0) {
		if(-end > (ssize_t)bt().size())
			return;
		end = (ssize_t)bt().size() + end;
	}
	if(start > (size_t)end)
		return;

#  ifdef ZTH_OS_MAC
	FILE* atosf = nullptr;
	if(Config::Debug) {
		char atos[256];
		int len = snprintf(atos, sizeof(atos), "atos -p %d\n", getpid());
		if(len > 0 && (size_t)len < sizeof(atos))
			atosf = popen(atos, "w");
	}
#  endif

	char** syms =
#  ifdef ZTH_OS_MAC
		!atosf ? nullptr :
#  endif
		backtrace_symbols(&bt()[start], (int)((size_t)end - start + 1));

	for(size_t i = start; i <= (size_t)end; i++) {
#  ifdef ZTH_OS_MAC
		if(atosf) {
			fprintf(atosf, "%p\n", bt()[i]);
			continue;
		}
#  endif

		Dl_info info;
		if(dladdr(bt()[i], &info)) {
			int status = -1;
			char* demangled = abi::__cxa_demangle(info.dli_sname, nullptr, nullptr, &status);
			if(status == 0 && demangled) {
#  ifdef ZTH_OS_MAC
				log_color(color, "%s%-3zd 0x%0*" PRIxPTR " %s + %" PRIuPTR "\n", color >= 0 ? ZTH_DBG_PREFIX : "",
					i, (int)sizeof(void*) * 2, reinterpret_cast<uintptr_t>(bt()[i]),
					demangled, reinterpret_cast<uintptr_t>(bt()[i]) - reinterpret_cast<uintptr_t>(info.dli_saddr));
#  else
				log_color(color, "%s%-3zd %s(%s+0x%" PRIxPTR ") [0x%" PRIxPTR "]\n", color >= 0 ? ZTH_DBG_PREFIX : "",
					i, info.dli_fname, demangled, reinterpret_cast<uintptr_t>(bt()[i]) - reinterpret_cast<uintptr_t>(info.dli_saddr),
					reinterpret_cast<uintptr_t>(bt()[i]));
#  endif

				free(demangled);
				continue;
			}
		}

		if(syms)
			log_color(color, "%s%-3zd %s\n", color >= 0 ? ZTH_DBG_PREFIX : "", i, syms[i - start]);
		else
			log_color(color, "%s%-3zd 0x%0*" PRIxPTR "\n", color >= 0 ? ZTH_DBG_PREFIX : "", i,
				(int)sizeof(void*) * 2, reinterpret_cast<uintptr_t>(bt()[i]));
	}

	if(syms)
		free(syms);

#  ifdef ZTH_OS_MAC
	if(atosf)
		pclose(atosf);
#  endif
#endif
}

void Backtrace::print(int UNUSED_PAR(color)) const
{
#if !defined(ZTH_OS_WINDOWS) && !defined(ZTH_OS_BAREMETAL)
	log_color(color, "%sBacktrace of fiber %p #%" PRIu64 ":\n", color >= 0 ? ZTH_DBG_PREFIX : "", m_fiber, m_fiberId);
	if(!bt().empty())
		printPartial(0, (ssize_t)bt().size() - 1, color);

	if(truncated())
		log_color(color, "%s<truncated>\n", color >= 0 ? ZTH_DBG_PREFIX : "");
	else
		log_color(color, "%s<end>\n", color >= 0 ? ZTH_DBG_PREFIX : "");
#endif
}

void Backtrace::printDelta(Backtrace const& other, int color) const
{
	// Make sure other was first.
	if(other.t0() > t0()) {
		other.printDelta(*this, color);
		return;
	}

	TimeInterval dt = t0() - other.t1();

	if(other.fiberId() != fiberId() ||
		other.truncated() || truncated())
	{
		log_color(color, "%sExecuted from:\n", color >= 0 ? ZTH_DBG_PREFIX : "");
		other.print(color);
		log_color(color, "%sto:\n", color >= 0 ? ZTH_DBG_PREFIX : "");
		print(color);
		log_color(color, "%stook %s\n", color >= 0 ? ZTH_DBG_PREFIX : "", dt.str().c_str());
		return;
	}

	// Find common base
	ssize_t common = 0;
	while(bt()[bt().size() - 1 - (size_t)common] == other.bt()[other.bt().size() - 1 - (size_t)common])
		common++;

	log_color(color, "%sExecution from fiber %p #%" PRIu64 ":\n", color >= 0 ? ZTH_DBG_PREFIX : "", m_fiber, m_fiberId);
	other.printPartial(0, -common - 1, color);
	log_color(color, "%sto:\n", color >= 0 ? ZTH_DBG_PREFIX : "");
	printPartial(0, -common - 1, color);
	if(common > 0) {
		log_color(color, "%shaving in common:\n", color >= 0 ? ZTH_DBG_PREFIX : "");
		other.printPartial(other.bt().size() - (size_t)common, color);
	}
	log_color(color, "%stook %s\n", color >= 0 ? ZTH_DBG_PREFIX : "", dt.str().c_str());
}

} // namespace
