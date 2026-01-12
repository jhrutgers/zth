/*
 * SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#define UNW_LOCAL_ONLY

#include <libzth/macros.h>

#include <libzth/allocator.h>

#ifdef ZTH_OS_MAC
#  ifndef _BSD_SOURCE
#    define _BSD_SOURCE
#  endif
#endif

#include <libzth/perf.h>
#include <libzth/worker.h>

#if __cplusplus < 201103L
#  include <inttypes.h>
#else
#  include <cinttypes>
#endif

#include <cstdlib>
#include <fcntl.h>
#include <map>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#if !defined(ZTH_OS_WINDOWS) && !defined(ZTH_OS_BAREMETAL)
#  include <cxxabi.h>
#  include <dlfcn.h>
#  include <execinfo.h>
#endif

#ifdef ZTH_HAVE_LIBUNWIND
#  include <libunwind.h>
#endif

namespace zth {

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
ZTH_TLS_DEFINE(perf_eventBuffer_type*, perf_eventBuffer, nullptr)

class PerfFiber : public Runnable {
	ZTH_CLASS_NOCOPY(PerfFiber)
	ZTH_CLASS_NEW_DELETE(PerfFiber)
public:
	explicit PerfFiber(Worker* worker)
		: m_worker(worker)
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
			(void)fclose(m_vcdd);
			m_vcdd = nullptr;
		}

		if(m_vcd) {
			(void)fclose(m_vcd);
			m_vcd = nullptr;
		}

		if(perf_eventBuffer) {
			for(size_t i = 0; i < perf_eventBuffer->size(); i++)
				(*perf_eventBuffer)[i].release();

			delete_alloc(perf_eventBuffer);
			perf_eventBuffer = nullptr;
		}
	}

	void flushEventBuffer()
	{
		zth_assert(Config::EnablePerfEvent);

		Fiber* f = fiber();
		size_t spareRoom = eventBuffer().capacity() - eventBuffer().size();
		if(unlikely(!f || f == m_worker->currentFiber() || spareRoom < 3)) {
			// Do it right here right now.
			processEventBuffer();
			return;
		}

		if(likely(spareRoom > 3)) {
			// Wakeup and do the processing later on.
			m_worker->resume(*f);
			return;
		}

		// Almost full, but enough room to signal that we are going to process the buffer.
		Fiber const* currentFiber_ = m_worker->currentFiber();
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

			m_worker->suspend(*this);
		}

		// Will never get here. The fiber is never properly stopped, but just killed when
		// the Worker terminates.
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
			TimeInterval t = Timestamp(e.t) - startTime;
			char const* s = nullptr;

			switch(e.type) {
			case PerfEvent<>::FiberName:
				if(e.str) {
					for(char* p = e.str; *p; ++p) {
						if(*p < 33 || *p > 126)
							*p = '_';
					}
					string const& id = vcdId(e.fiber);
					if(fprintf(m_vcd,
						   "$var wire 1 %s \\#%s_%s $end\n$var real 0 %s! "
						   "\\#%s_%s/log $end\n",
						   id.c_str(), str(e.fiber).c_str(), e.str,
						   id.c_str(), str(e.fiber).c_str(), e.str)
					   <= 0) {
						res = errno;
						goto write_error;
					}
				} else {
					string const& id = vcdId(e.fiber);
					if(fprintf(m_vcd,
						   "$var wire 1 %s \\#%s_Fiber $end\n$var real 0 "
						   "%s! \\#%s_Fiber_log $end\n",
						   id.c_str(), str(e.fiber).c_str(), id.c_str(),
						   str(e.fiber).c_str())
					   <= 0) {
						res = errno;
						goto write_error;
					}
				}
				break;

			case PerfEvent<>::FiberState: {
				char x = '-';
				bool doCleanup = false;
				switch(e.fiberState) {
				case Fiber::New:
					x = 'L';
					break;
				case Fiber::Ready:
					x = '0';
					break;
				case Fiber::Running:
					x = '1';
					break;
				case Fiber::Waiting:
					x = 'W';
					break;
				case Fiber::Suspended:
					x = 'Z';
					break;
				case Fiber::Dead:
					x = 'X';
					break;
				default:
					x = '-';
					doCleanup = true;
				}

				static_assert(sizeof(unsigned) >= 4, "");

				if(fprintf(m_vcdd, "#%s%09u\n%c%s\n", str(t.ts().tv_sec).c_str(),
					   (unsigned)t.ts().tv_nsec, x, vcdId(e.fiber).c_str())
				   <= 0) {
					res = errno;
					goto write_error;
				}

				if(doCleanup)
					vcdIdRelease(e.fiber);

				break;
			}

			case PerfEvent<>::Log: {
				if(true) { // NOLINT
					s = e.c_str;
				} else {
				case PerfEvent<>::Marker:
					s = e.str;
				}
				if(!s)
					break;

				static_assert(sizeof(unsigned) >= 4, "");

				if(fprintf(m_vcdd, "#%s%09u\ns", str(t.ts().tv_sec).c_str(),
					   (unsigned)t.ts().tv_nsec)
				   <= 0) {
					res = errno;
					goto write_error;
				}

				char const* chunkStart = s;
				char const* chunkEnd = chunkStart;
				int len = 0;
				while(chunkEnd[1]) {
					if(*chunkEnd < 33 || *chunkEnd > 126) {
						if(fprintf(m_vcdd, "%.*s\\x%02x", len, chunkStart,
							   (unsigned)(unsigned char)*chunkEnd)
						   < 0) {
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

				if(fprintf(m_vcdd, "%s %s!\n", chunkStart, vcdId(e.fiber).c_str())
				   <= 0) {
					res = errno;
					goto write_error;
				}
			}

			case PerfEvent<>::Nothing:
			default:; // ignore
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
		char const* vcdFilename =
			snprintf(vcdFilename_, sizeof(vcdFilename_), "zth.%d.vcd", (int)getpid())
					<= 0
				? vcdFilename_
				: "zth.vcd";

		m_vcd = fopen(vcdFilename, "w");
		if(!m_vcd) {
			res = errno;
			(void)fprintf(
				stderr, "Cannot open VCD file %s; %s\n", vcdFilename,
				err(res).c_str());
			goto rollback;
		}

#ifdef ZTH_OS_WINDOWS
		// tmpfile() on WIN32 is broken
		m_vcdd = fopen("zth.vcdd", "w+");
#else
		m_vcdd = tmpfile();
#endif
		if(!(m_vcdd)) {
			(void)fprintf(
				stderr, "Cannot open temporary VCD data file; %s\n",
				err(errno).c_str());
			goto rollback;
		}

		{
			time_t now = -1;
			if(time(&now) != -1) {
#if defined(ZTH_OS_LINUX) || defined(ZTH_OS_MAC)
				char dateBuf[128];
				char const* strnow = ctime_r(&now, dateBuf);
#else
				// Possibly not thread-safe.
				char const* strnow = ctime(&now);
#endif
				if(strnow)
					if(fprintf(m_vcd, "$date %s$end\n", strnow) < 0)
						goto write_error;
			}
		}

		if(fprintf(m_vcd,
			   "$version %s $end\n$timescale 1 ns $end\n$scope module top $end\n",
			   banner())
		   < 0)
			goto write_error;

		(void)fprintf(stderr, "Using %s for perf VCD output\n", vcdFilename);
		return 0;

write_error:
		(void)fprintf(stderr, "Cannot write %s", vcdFilename);
		res = EIO;
rollback:
		if(m_vcdd) {
			(void)fclose(m_vcdd);
			m_vcdd = nullptr;
		}
		if(m_vcd) {
			(void)fclose(m_vcd);
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

		(void)fseek(m_vcdd, 0, SEEK_SET);

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

		(void)fclose(m_vcdd);
		m_vcdd = nullptr;
		(void)fclose(m_vcd);
		m_vcd = nullptr;
		return 0;

read_error:
		fprintf(stderr, "Cannot read temporary VCD data file\n");
		return res ? res : EIO;
write_error:
		fprintf(stderr, "Cannot write VCD file\n");
		return res ? res : EIO;
	}

	string const& vcdId(uint64_t fiber)
	{
		std::pair<decltype(m_vcdIds.begin()), bool> i =
			m_vcdIds.insert(std::make_pair(fiber, string()));
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

		// NOLINTNEXTLINE(cppcoreguidelines-avoid-do-while)
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
	Worker* m_worker;
	FILE* m_vcd;
	FILE* m_vcdd;
	map_type<uint64_t, string>::type m_vcdIds;
};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
ZTH_TLS_STATIC(PerfFiber*, perfFiber, nullptr);

int perf_init()
{
	if(!Config::EnablePerfEvent || !zth_config(DoPerfEvent))
		return 0;

	perf_eventBuffer = new_alloc<perf_eventBuffer_type>();
	perf_eventBuffer->reserve(Config::PerfEventBufferSize);

	Worker* worker = nullptr;
	getContext(&worker, nullptr);
	perfFiber = new PerfFiber(worker);
	return perfFiber->run();
}

void perf_deinit()
{
	if(!Config::EnablePerfEvent) {
		zth_assert(!perfFiber);
		return;
	}

	if(perfFiber) {
		delete perfFiber;
		perfFiber = nullptr;
	}
}

void perf_flushEventBuffer() noexcept
{
	if(!Config::EnablePerfEvent)
		return;

	try {
		if(likely(perfFiber))
			perfFiber->flushEventBuffer();
	} catch(...) {
		// Something is wrong. Disable perf.
		perf_deinit();
	}
}

extern "C" void context_entry(Context* context);

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
Backtrace::Backtrace(size_t UNUSED_PAR(skip), size_t UNUSED_PAR(maxDepth))
	: m_t0(Timestamp::now())
	, m_fiber()
	, m_fiberId()
	, m_truncated(true)
{
	Worker const* worker = Worker::instance();
	m_fiber = worker ? worker->currentFiber() : nullptr;
	// NOLINTNEXTLINE(cppcoreguidelines-prefer-member-initializer)
	m_fiberId = m_fiber ? m_fiber->id() : 0;

#ifdef ZTH_HAVE_LIBUNWIND
	unw_context_t uc;

	if(unw_getcontext(&uc))
		return;

	unw_cursor_t cursor;
	unw_init_local(&cursor, &uc);
	size_t depth = 0;
	for(size_t i = 0; i < skip && unw_step(&cursor) > 0; i++)
		;

	unw_proc_info_t pip;

	while(unw_step(&cursor) > 0 && depth < maxDepth) {
		// cppcheck-suppress knownConditionTrueFalse
		if(depth == 0) {
			unw_word_t sp = 0;
			unw_get_reg(&cursor, UNW_REG_SP, &sp);
		}

		unw_word_t ip = 0;
		unw_get_reg(&cursor, UNW_REG_IP, &ip);
		m_bt.push_back(reinterpret_cast<void*>(ip));

		if(unw_get_proc_info(&cursor, &pip) == 0
		   && pip.start_ip == reinterpret_cast<unw_word_t>(&context_entry))
			// Stop here, as we might get segfaults when passing the context_entry
			// functions.
			break;
	}

	m_truncated = depth == maxDepth;
#elif defined(ZTH_OS_MAC) && defined(ZTH_ARCH_ARM64)
	// macOS on ARM64 does not seem to support backtrace() in combination with ucontext.
	m_truncated = true;
#elif !defined(ZTH_OS_WINDOWS) && !defined(ZTH_OS_BAREMETAL)
	m_bt.resize(maxDepth);
	m_bt.resize((size_t)backtrace(m_bt.data(), (int)maxDepth));
	//  NOLINTNEXTLINE(cppcoreguidelines-prefer-member-initializer)
	m_truncated = m_bt.size() == maxDepth;
#endif

	m_t1 = Timestamp::now();
}

void Backtrace::printPartial(
	size_t UNUSED_PAR(start), ssize_t UNUSED_PAR(end), int UNUSED_PAR(color)) const
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
		if(len > 0 && (size_t)len < sizeof(atos)) {
			fflush(nullptr);
			atosf = popen(atos, "w");
		}
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
			char* demangled =
				abi::__cxa_demangle(info.dli_sname, nullptr, nullptr, &status);
			if(status == 0 && demangled) {
#  ifdef ZTH_OS_MAC
				log_color(
					color, "%s%-3zd 0x%0*" PRIxPTR " %s + %" PRIuPTR "\n",
					color >= 0 ? ZTH_DBG_PREFIX : "", i, (int)sizeof(void*) * 2,
					reinterpret_cast<uintptr_t>(bt()[i]), demangled,
					reinterpret_cast<uintptr_t>(bt()[i])
						- reinterpret_cast<uintptr_t>(info.dli_saddr));
#  else
				log_color(
					color, "%s%-3zd %s(%s+0x%" PRIxPTR ") [0x%" PRIxPTR "]\n",
					color >= 0 ? ZTH_DBG_PREFIX : "", i, info.dli_fname,
					demangled,
					reinterpret_cast<uintptr_t>(bt()[i])
						- reinterpret_cast<uintptr_t>(info.dli_saddr),
					reinterpret_cast<uintptr_t>(bt()[i]));
#  endif

				free(demangled); // NOLINT
				continue;
			}
		}

		if(syms)
			log_color(
				color, "%s%-3zd %s\n", color >= 0 ? ZTH_DBG_PREFIX : "", i,
				syms[i - start]);
		else
			log_color(
				color, "%s%-3zd 0x%0*" PRIxPTR "\n",
				color >= 0 ? ZTH_DBG_PREFIX : "", i, (int)sizeof(void*) * 2,
				reinterpret_cast<uintptr_t>(bt()[i]));
	}

	if(syms)
		free(syms); // NOLINT

#  ifdef ZTH_OS_MAC
	if(atosf)
		pclose(atosf);
#  endif
#endif
}

void Backtrace::print(int UNUSED_PAR(color)) const
{
#if !defined(ZTH_OS_WINDOWS) && !defined(ZTH_OS_BAREMETAL)
	log_color(
		color, "%sBacktrace of fiber %p #%" PRIu64 ":\n", color >= 0 ? ZTH_DBG_PREFIX : "",
		m_fiber, m_fiberId);
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

	if(other.fiberId() != fiberId() || other.truncated() || truncated()) {
		log_color(color, "%sExecuted from:\n", color >= 0 ? ZTH_DBG_PREFIX : "");
		other.print(color);
		log_color(color, "%sto:\n", color >= 0 ? ZTH_DBG_PREFIX : "");
		print(color);
		log_color(color, "%stook %s\n", color >= 0 ? ZTH_DBG_PREFIX : "", dt.str().c_str());
		return;
	}

	// Find common base
	ssize_t common = 0;
	{
		Backtrace::bt_type const& this_bt = bt();
		Backtrace::bt_type const& other_bt = other.bt();
		size_t max_common = std::min(this_bt.size(), other_bt.size());
		while((size_t)common < max_common
		      && this_bt[this_bt.size() - 1 - (size_t)common]
				 == other_bt[other_bt.size() - 1 - (size_t)common])
			common++;
	}

	log_color(
		color, "%sExecution from fiber %p #%s:\n", color >= 0 ? ZTH_DBG_PREFIX : "",
		m_fiber, str(m_fiberId).c_str());
	other.printPartial(0, -common - 1, color);
	log_color(color, "%sto:\n", color >= 0 ? ZTH_DBG_PREFIX : "");
	printPartial(0, -common - 1, color);
	if(common > 0) {
		log_color(color, "%shaving in common:\n", color >= 0 ? ZTH_DBG_PREFIX : "");
		other.printPartial(other.bt().size() - (size_t)common, color);
	}
	log_color(color, "%stook %s\n", color >= 0 ? ZTH_DBG_PREFIX : "", dt.str().c_str());
}

} // namespace zth
