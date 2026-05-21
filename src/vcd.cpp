/*
 * SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#include <libzth/perf.h>

#include <libzth/allocator.h>
#include <libzth/fiber.h>
#include <libzth/worker.h>

#include <cstdlib>
#include <cstring>
#include <set>
#include <unistd.h>

namespace zth {

void perf_time(Timestamp const& t = Timestamp());
void perf_dt(Timestamp const& t = Timestamp());

enum PerfEvent {
	PerfEventTerminate,
	PerfEventTime,
	PerfEventTimeDelta,
	PerfEventMarker,
	PerfEventLog,
	PerfEventFiber,
	PerfEventFiberState,
};

class PerfBuffer final {
	ZTH_CLASS_NOCOPY(PerfBuffer)
public:
	typedef void* Known;
	typedef set_type<Known>::type KnownSet;

	PerfBuffer() noexcept
		: m_buffer()
		, m_size()
		, m_running()
		, m_done()
		, m_dump()
	{}

	~PerfBuffer() noexcept
	{
		deinit();
	}

	bool enabled() const noexcept
	{
		return m_buffer;
	}

	static constexpr size_t capacity() noexcept
	{
		return Config::PerfEventBufferSize;
	}

	int init() noexcept
	{
		if(!m_buffer) {
			m_buffer = allocate_noexcept<char>(capacity());
			if(!m_buffer)
				return ENOMEM;
		}

		release();
		return 0;
	}

	void deinit() noexcept
	{
		m_size = 0;

		if(m_buffer) {
			deallocate<char>((char*)m_buffer, capacity());
			m_buffer = nullptr;
		}
	}

	size_t size() const noexcept
	{
		return std::min(capacity(), (size_t)m_size);
	}

	size_t space() const noexcept
	{
		if(!m_buffer)
			return 0;

		zth_assert(size() < capacity());
		return capacity() - size() - 1;
	}

	char const* data() noexcept
	{
		return (char const*)m_buffer;
	}

	char volatile* reserve(size_t s) noexcept
	{
		if(!running())
			return nullptr;

		if(space() < s) {
			stop();
			return nullptr;
		}

		size_t end =
#if GCC_VERSION < 40802L
			__sync_add_and_fetch(&m_size, s);
#else
			__atomic_add_fetch(&m_size, s, __ATOMIC_RELAXED);
#endif

		size_t start = end - s;
		char volatile* p = &m_buffer[start];

		if(unlikely(end >= capacity())) {
			// Hit a race. Terminate the buffer.
			if(start < capacity())
				*p = 0;
			return nullptr;
		}

		return p;
	}

	void release() noexcept
	{
		stop();
		m_size = 0;

		if(Config::EnableThreads) {
			// In case of threads, fill the buffer with 0, such that even in case of a
			// race during reserve()/append, the buffer is always terminated properly.
			// Without threads, an IRQ can still do async append, but that will always
			// complete before the main context can resume again. So, it is not possible
			// to see a partially filled buffer in that case.
			memset((void*)m_buffer, 0, capacity());
		}

		m_known.clear();
	}

	Timestamp const& t() const noexcept
	{
		return m_t;
	}

	void t(Timestamp const& t) noexcept
	{
		m_t = t;
	}

	void dump_callback(zth_perf_dump_callback_t* f) noexcept
	{
		m_dump = f;
	}

	zth_perf_dump_callback_t* dump_callback() const noexcept
	{
		return m_dump;
	}

	void done_callback(zth_perf_done_callback_t* f) noexcept
	{
		m_done = f;
	}

	zth_perf_done_callback_t* done_callback() const noexcept
	{
		return m_done;
	}

	void start(zth_perf_done_callback_t* f = nullptr) noexcept
	{
		if(m_running)
			return;
		if(!m_buffer)
			return;

		done_callback(f);
		m_running = true;
		perf_time();

		zth_dbg(perf, "[%s] start", currentWorker().id_str());
	}

	void stop() noexcept;

	bool running() const noexcept
	{
		return m_running;
	}

	bool knows(void* x) const noexcept
	{
		return m_known.find(x) != m_known.end();
	}

	void know(void* x) noexcept
	{
		try {
			m_known.insert(x);
		} catch(std::bad_alloc const&) { // NOLINT
						 // Ignore.
		}
	}

private:
	char volatile* m_buffer;
	size_t volatile m_size;
	bool volatile m_running;
	Timestamp m_t;
	KnownSet m_known;
	zth_perf_done_callback_t* m_done;
	zth_perf_dump_callback_t* m_dump;
};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
ZTH_TLS_STATIC(PerfBuffer*, perf_buffer, nullptr)

/*!
 * \brief Starts recording perf events.
 *
 * Recording can only be started when perf is enabled.
 *
 * \ingroup zth_api_cpp_perf
 */
void perf_start(zth_perf_done_callback_t* f)
{
	if(perf_buffer)
		perf_buffer->start(f);
}

/*!
 * \brief Stops recording perf events.
 *
 * Recording is automatically stopped when the buffer is full.
 *
 * \ingroup zth_api_cpp_perf
 */
void perf_stop()
{
	if(perf_buffer)
		perf_buffer->stop();
}

void PerfBuffer::stop() noexcept
{
	if(perf_buffer != this)
		// Called from another thread?
		return;

	if(!m_running)
		return;

	m_running = false;
	zth_dbg(perf, "[%s] stop", currentWorker().id_str());

	if(done_callback())
		done_callback()();
}

typedef char leb128_buf_t[9];

static size_t leb128_encode(leb128_buf_t& buf, uint64_t x)
{
	size_t len = 0;

	while(true) {
		buf[len] = (char)(x & 0x7FU);
		x >>= 7U;
		if(!x)
			return len + 1U;
		buf[len] = (char)((unsigned)buf[len] | 0x80U);
		len++;
	}
}

static size_t leb128_len(char const* buf)
{
	size_t len = 1;
	for(; *buf & 0x80; buf++, len++)
		;
	return len;
}

/*!
 * \brief Put a full timestamp into the perf output.
 *
 * This is the basis for successive time deltas.
 */
void perf_time(Timestamp const& t)
{
	if(!perf_buffer)
		return;

	Timestamp now;
	Timestamp const& t_ = t.isNull() ? (now = Timestamp::now()) : t;

	struct timespec const& ts = t_.ts();
	leb128_buf_t s;
	size_t s_len = leb128_encode(s, (uint64_t)ts.tv_sec);
	leb128_buf_t ns;
	size_t ns_len = leb128_encode(ns, (uint64_t)ts.tv_nsec);
	char volatile* p = perf_buffer->reserve(s_len + ns_len + 1);
	if(!p)
		return;

	memcpy((void*)(p + 1), s, s_len);
	memcpy((void*)(p + 1 + s_len), ns, ns_len);
	*p = PerfEventTime;
	perf_buffer->t(t_);
}

/*!
 * \brief Put a delta timestamp into the perf output.
 */
void perf_dt(Timestamp const& t)
{
	if(!perf_buffer)
		return;

	Timestamp now;
	Timestamp const& t_ = t.isNull() ? (now = Timestamp::now()) : t;

	Timestamp const& t0 = perf_buffer->t();
	TimeInterval d = t_ - t0;
	if(d.hasPassed())
		// We only go forward in time.
		return;

	if(d.ts().tv_sec) {
		// Substantially long ago. Emit full timestamp.
		perf_time(t_);
		return;
	}

	leb128_buf_t buf;
	size_t len = leb128_encode(buf, (uint64_t)d.ts().tv_nsec);

	char volatile* p = perf_buffer->reserve(len + 1);
	if(!p)
		return;

	memcpy((void*)(p + 1), buf, len);
	*p = PerfEventTimeDelta;
	perf_buffer->t(t_);
}

/*!
 * \brief Put a string marker into the perf output.
 * \ingroup zth_api_cpp_perf
 */
void perf_mark(char const* m, Timestamp const& t)
{
	perf_dt(t);

	zth_perf_async_handle_t h = {perf_buffer};
	perf_mark_async(m, &h);
}

/*!
 * \brief Returns the handle of the current thread's perf buffer.
 * \see perf_mark_async()
 * \ingroup zth_api_cpp_perf
 */
void perf_async_handle(zth_perf_async_handle_t* handle)
{
	if(!handle)
		return;

	handle->p = (void*)perf_buffer;
}

/*!
 * \brief Async-/thread-safe #perf_mark().
 *
 * This function performs #perf_mark() on another thread's perf event buffer, of which its handle is
 * retrieved by the owner thread using #perf_async_handle().
 *
 * \ingroup zth_api_cpp_perf
 */
void perf_mark_async(char const* marker, zth_perf_async_handle_t* handle)
{
	if(!handle || !handle->p)
		return;

	PerfBuffer* pb = static_cast<PerfBuffer*>(handle->p);
	char volatile* p = pb->reserve(sizeof(marker) + 1);
	if(!p)
		return;

	// NOLINTNEXTLINE(bugprone-multi-level-implicit-pointer-conversion)
	memcpy((void*)(p + 1), (void*)&marker, sizeof(marker));
	*p = PerfEventMarker;
}

/*!
 * \brief Put a formatted log string into the perf output.
 * \ingroup zth_api_cpp_perf
 */
void perf_log(char const* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	perf_logv(fmt, args);
	va_end(args);
}

/*!
 * \brief Put a formatted log string into the perf output.
 * \ingroup zth_api_cpp_perf
 */
void perf_log(Timestamp const& t, char const* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	perf_logv(fmt, args, t);
	va_end(args);
}

/*!
 * \brief Put a formatted log string into the perf output.
 * \ingroup zth_api_cpp_perf
 */
void perf_logv(char const* fmt, va_list args, Timestamp const& t)
{
	if(!perf_buffer || !perf_buffer->running())
		return;

	va_list args2;
	va_copy(args2, args);
	int len = vsnprintf(nullptr, 0, fmt, args2);
	va_end(args2);

	if(len < 0) {
		perf_mark(fmt, t);
		return;
	}

	perf_dt(t);
	char volatile* p = perf_buffer->reserve((size_t)len + 2);
	if(!p)
		return;

	int len2 = vsnprintf((char*)p + 1, (size_t)len + 1U, fmt, args);
	zth_assert(len == len2);
	(void)len2;

	p[0] = (char)PerfEventLog;
}

/*!
 * \brief Write fiber ID/name to the perf buffer.
 *
 * This is used to identify fibers IDs later on.
 */
void perf_fiber(Fiber& f)
{
	if(!perf_buffer || !perf_buffer->running())
		return;

	perf_buffer->know(&f);

	leb128_buf_t id;
	size_t id_len = leb128_encode(id, f.id());

	char const* name = f.name().c_str();
	size_t name_len = f.name().size() + 1;

	char volatile* p = perf_buffer->reserve(id_len + name_len + 1);
	if(!p)
		return;

	memcpy((char*)p + 1, id, id_len);
	memcpy((char*)p + 1 + id_len, name, name_len);
	p[0] = PerfEventFiber;

	perf_fiber_state(f);
}

/*!
 * \brief Record the current fiber state.
 */
void perf_fiber_state(Fiber& f, int state, Timestamp const& t)
{
	if(!perf_buffer || !perf_buffer->running())
		return;

	perf_dt(t);

	if(!perf_buffer->knows(&f))
		perf_fiber(f);

	leb128_buf_t id;
	size_t id_len = leb128_encode(id, f.id());

	char volatile* p = perf_buffer->reserve(id_len + 2);
	if(!p)
		return;

	memcpy((char*)p + 1, id, id_len);
	p[0] = PerfEventFiberState;
	if(state < 0)
		state = (int)f.state();
	zth_assert(state >= 0 && state < 0x100);
	p[1 + id_len] = (char)state;
}

/*!
 * \brief Passes collected perf data to \p f.
 *
 * The data (pointer to a buffer and its length) passed to \p f can be converted to VCD later on.
 * Write the data to file or stdout, for example, for later processing.
 *
 * The internal buffer is erased afterwards.
 *
 * Typical workflow:
 *
 * - Call #perf_start() when event collection is required.
 * - Wait till the callback to #perf_start() is invoked.
 * - Call #perf_dump() to convert the buffer to be sent to the PC.
 * - On the PC (offline), process the buffer into VCD.
 *
 * \ingroup zth_api_cpp_perf
 */
void perf_dump(zth_perf_dump_callback_t* f)
{
	if(!perf_buffer)
		return;

	char const* p = perf_buffer->data();
	if(!p)
		return;

	char const* end = p + perf_buffer->size();

	if(!f)
		goto done;

	while(p < end) {
		switch(p[0]) {
		case PerfEventTerminate:
			goto done;
		case PerfEventTime: {
			size_t len = 1 + leb128_len(p + 1);
			len += leb128_len(p + len);
			f(p, len);
			p += len;
			break;
		}
		case PerfEventTimeDelta: {
			size_t len = leb128_len(p + 1) + 1U;
			f(p, len);
			p += len;
			break;
		}
		case PerfEventMarker: {
			// Convert the passed pointer to an actual string.
			// NOLINTNEXTLINE
			char const* __attribute__((aligned(1)))* s = (char const**)(void*)(p + 1);
			size_t len = strlen(*s);
			char e = PerfEventLog;
			f(&e, 1);
			f(*s, len + 1U); // including \0
			p += sizeof(*s) + 1U;
			break;
		}
		case PerfEventLog: {
			size_t len = strlen(p + 1);
			f(p, len + 2U); // including *p and \0
			p += len + 2U;
			break;
		}
		case PerfEventFiber: {
			size_t len = 1 + leb128_len(p + 1U);
			len += strlen(p + len) + 1U; // including \0
			f(p, len);
			p += len;
			break;
		}
		case PerfEventFiberState: {
			size_t len = 1 + leb128_len(p + 1) + 1U;
			f(p, len);
			p += len;
			break;
		}
		default:
			// Unknown. Abort.
			goto done;
		}
	}
done:
	perf_buffer->release();
}

static void perf_continue()
{
	zth_dbg(perf, "[%s] dump", currentWorker().id_str());

	zth_assert(perf_buffer && perf_buffer->enabled() && !perf_buffer->running());

	zth_perf_dump_callback_t* f = perf_buffer->dump_callback();
	zth_assert(f);

	Timestamp now = Timestamp::now();
	char id = PerfEventTime;
	f(&id, 1);

	leb128_buf_t buf;
	f(buf, leb128_encode(buf, (uint64_t)now.ts().tv_sec));
	f(buf, leb128_encode(buf, (uint64_t)now.ts().tv_nsec));

	id = PerfEventLog;
	f(&id, 1);
	f("perf_dump", 10);

	perf_dump(f);

	if(perf_buffer->done_callback() == perf_continue)
		perf_start(perf_continue);
}

/*!
 * \brief Setup the system to automatically perform perf event recording, dumping, and resuming.
 *
 * Typical workflow:
 *
 * - Make sure Config::DoPerfEvent is set (optionally via environment).
 * - Call perf_run().
 * - When the buffer is full, the callback function is called, as if #perf_dump() was invoked.
 * - The buffer is cleared and recording is resumed automatically.
 *
 * \ingroup zth_api_cpp_perf
 */
void perf_run(zth_perf_dump_callback_t* f)
{
	if(!perf_buffer || !perf_buffer->enabled())
		return;
	if(!f)
		return;
	if(!zth_config(DoPerfEvent))
		return;

	perf_buffer->dump_callback(f);
	perf_start(perf_continue);
}

/*!
 * \brief Abort a #perf_run().
 */
void perf_abort()
{
	if(!perf_buffer)
		return;

	zth_perf_done_callback_t* f = perf_buffer->done_callback();
	perf_buffer->done_callback(nullptr);
	perf_buffer->stop();
	if(f)
		f();
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
ZTH_TLS_STATIC(FILE*, perf_dump_file, nullptr)

static void perf_run_dump_callback(void const* buf, size_t len)
{
	if(!perf_dump_file)
		return;
	if(!buf || !len)
		return;

	if(fwrite(buf, len, 1, perf_dump_file) != 1) {
		zth_log("Cannot write to perf dump file");
		(void)fclose(perf_dump_file);
		perf_dump_file = nullptr;
		if(perf_buffer)
			perf_buffer->dump_callback(nullptr);
		perf_abort();
	}
}

/*!
 * \brief Like #perf_run(), but dump the contents to file immediately.
 * \ingroup zth_api_cpp_perf
 */
void perf_run_dump(char const* path)
{
	if(!path) {
		// NOLINTNEXTLINE(concurrency-mt-unsafe)
		path = getenv("ZTH_PERF_FILE");

		string p =
			format("%s.%u-%u.perf", path ? path : "zth", (unsigned)getpid(),
			       (unsigned)currentWorker().id());

		perf_run_dump(p.c_str());
		return;
	}

	if(perf_dump_file) {
		(void)fclose(perf_dump_file);
		perf_dump_file = nullptr;
	}

	perf_dump_file = fopen(path, "wb");
	if(!perf_dump_file) {
		string e = err(errno);
		zth_log("Cannot open perf dump file %s; %s", path, e.c_str());
		return;
	}

	zth_dbg(perf, "[%s] perf dump to %s", currentWorker().id_str(), path);
	perf_run(perf_run_dump_callback);
}

/*!
 * \brief Initializes the per-thread perf event buffer.
 */
int perf_init()
{
	if(!Config::EnablePerfEvent)
		return 0;

	if(!perf_buffer) {
		try {
			perf_buffer = new_alloc<PerfBuffer>();
		} catch(std::bad_alloc const&) {
			return 0;
		}
	}

	perf_buffer->init();

	if(zth_config(DoPerfEvent))
		perf_run_dump();

	return 0;
}

void perf_deinit()
{
	if(perf_buffer) {
		perf_abort();
		perf_buffer->release();
		perf_buffer->deinit();

		delete_alloc(perf_buffer);
		perf_buffer = nullptr;
	}
}

} // namespace zth
