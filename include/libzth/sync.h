#ifndef ZTH_SYNC_H
#define ZTH_SYNC_H
/*
 * SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
 *
 * SPDX-License-Identifier: MPL-2.0
 */

/*!
 * \defgroup zth_api_cpp_sync sync
 * \ingroup zth_api_cpp
 */
/*!
 * \defgroup zth_api_c_sync sync
 * \ingroup zth_api_c
 */

#include <libzth/macros.h>

#ifdef __cplusplus

#  include <libzth/allocator.h>
#  include <libzth/fiber.h>
#  include <libzth/list.h>
#  include <libzth/util.h>
#  include <libzth/worker.h>

#  include <new>

#  ifdef ZTH_USE_VALGRIND
#    include <valgrind/memcheck.h>
#  endif

#  if __cplusplus >= 201103L && defined(__cpp_exceptions)
#    include <exception>
#    define ZTH_FUTURE_EXCEPTION
#  endif

namespace zth {

class RefCounted {
	ZTH_CLASS_NOCOPY(RefCounted)
public:
	RefCounted() noexcept
		: m_count()
	{}

	virtual ~RefCounted() is_default

	void used() noexcept
	{
		zth_assert(m_count < std::numeric_limits<size_t>::max());
		m_count++;
	}

	void unused()
	{
		zth_assert(m_count > 0);
		if(--m_count == 0)
			delete this;
	}

private:
	size_t m_count;
};

template <typename T>
class SharedPointer {
public:
	// cppcheck-suppress noExplicitConstructor
	constexpr14 SharedPointer(T* object = nullptr) noexcept
		: m_object(object)
	{
		if(m_object)
			m_object->used();
	}

	constexpr14 SharedPointer(SharedPointer const& p) noexcept
		: m_object(p.get())
	{
		if(m_object)
			m_object->used();
	}

	virtual ~SharedPointer()
	{
		reset();
	}

	void reset(T* object = nullptr)
	{
		if(object)
			object->used();
		if(m_object)
			m_object->unused();
		m_object = object;
	}

	SharedPointer& operator=(T* object)
	{
		reset(object);
		return *this;
	}

	SharedPointer& operator=(SharedPointer const& p)
	{
		reset(p.get());
		return *this;
	}

#  if __cplusplus >= 201103L
	constexpr14 SharedPointer(SharedPointer&& p) noexcept
		: m_object()
	{
		*this = std::move(p);
	}

	constexpr14 SharedPointer& operator=(SharedPointer&& p) noexcept
	{
		m_object = p.release();
		return *this;
	}
#  endif

	constexpr T* get() const noexcept
	{
		return m_object;
	}

	constexpr operator T*() const noexcept
	{
		return get();
	}

	constexpr14 T* operator*() const noexcept
	{
		zth_assert(get());
		return get();
	}

	constexpr14 T* operator->() const noexcept
	{
		zth_assert(get());
		return get();
	}

	constexpr14 T* release() noexcept
	{
		T* object = get();
		m_object = nullptr;
		return object;
	}

	operator bool() const noexcept
	{
		return get() != nullptr;
	}

private:
	T* m_object;
};

class Synchronizer : public RefCounted, public UniqueID<Synchronizer> {
	ZTH_CLASS_NEW_DELETE(Synchronizer)
public:
	explicit Synchronizer(cow_string const& name = "Synchronizer")
		: RefCounted()
		, UniqueID(Config::NamedSynchronizer ? name.str() : string())
	{}

#  if __cplusplus >= 201103L
	explicit Synchronizer(cow_string&& name)
		: RefCounted()
		, UniqueID(Config::NamedSynchronizer ? std::move(name).str() : string())
	{}
#  endif

	virtual ~Synchronizer() override
	{
		zth_dbg(sync, "[%s] Destruct", id_str());
		zth_assert(m_queue.empty());
	}

protected:
	void block()
	{
		Worker* w = nullptr;
		Fiber* f = nullptr;
		getContext(&w, &f);

		zth_dbg(sync, "[%s] Block %s", id_str(), f->id_str());
		w->release(*f);
		m_queue.push_back(*f);
		f->nap(Timestamp::null());
		w->schedule();
	}

	/*!
	 * \brief Unblock the specific fiber.
	 * \return \c true if unblocked, \c false when not queued by this Synchronizer.
	 */
	bool unblock(Fiber& f) noexcept
	{
		if(!m_queue.contains(f))
			return false;

		Worker* w = nullptr;
		getContext(&w, nullptr);

		zth_dbg(sync, "[%s] Unblock %s", id_str(), f.id_str());
		m_queue.erase(f);
		f.wakeup();
		w->add(&f);
		return true;
	}

	bool unblockFirst() noexcept
	{
		if(m_queue.empty())
			return false;

		Worker* w = nullptr;
		getContext(&w, nullptr);

		Fiber& f = m_queue.front();
		zth_dbg(sync, "[%s] Unblock %s", id_str(), f.id_str());
		m_queue.pop_front();
		f.wakeup();
		w->add(&f);
		return true;
	}

	bool unblockAll() noexcept
	{
		if(m_queue.empty())
			return false;

		Worker* w = nullptr;
		getContext(&w, nullptr);

		zth_dbg(sync, "[%s] Unblock all", id_str());

		while(!m_queue.empty()) {
			Fiber& f = m_queue.front();
			m_queue.pop_front();
			f.wakeup();
			w->add(&f);
		}
		return true;
	}

	class AlarmClock : public TimedWaitable {
		ZTH_CLASS_NEW_DELETE(AlarmClock)
	public:
		typedef TimedWaitable base;
		AlarmClock(
			Synchronizer& synchronizer, Fiber& fiber, Timestamp const& timeout) noexcept
			: base(timeout)
			, m_synchronizer(synchronizer)
			, m_fiber(fiber)
			, m_rang()
		{}

		virtual ~AlarmClock() override is_default

		virtual bool poll(Timestamp const& now = Timestamp::now()) noexcept override
		{
			if(!base::poll(now))
				return false;

			zth_dbg(sync, "[%s] %s timed out", m_synchronizer.id_str(),
				m_fiber.id_str());
			m_rang = m_synchronizer.unblock(m_fiber);
			return true;
		}

		bool rang() const noexcept
		{
			return m_rang;
		}

	private:
		Synchronizer& m_synchronizer;
		Fiber& m_fiber;
		bool m_rang;
	};

	friend class AlarmClock;

	/*!
	 * \brief Block, with timeout.
	 * \return \c true if unblocked by request, \c false when unblocked by timeout.
	 */
	bool block(Timestamp const& timeout, Timestamp const& now = Timestamp::now())
	{
		if(timeout <= now)
			// Immediate timeout.
			return true;

		return block_(timeout, now);
	}

	/*!
	 * \brief Block, with timeout.
	 * \return \c true if unblocked by request, \c false when unblocked by timeout.
	 */
	bool block(TimeInterval const& timeout, Timestamp const& now = Timestamp::now())
	{
		if(timeout.isNegative() || timeout.isNull())
			// Immediate timeout.
			return true;

		return block_(now + timeout, now);
	}

private:
	/*!
	 * \brief Block, with timeout.
	 * \return \c true if unblocked by request, \c false when unblocked by timeout.
	 */
	bool block_(Timestamp const& timeout, Timestamp const& now)
	{
		Worker* w;
		Fiber* f;
		getContext(&w, &f);

		zth_dbg(sync, "[%s] Block %s with timeout", id_str(), f->id_str());
		w->release(*f);
		m_queue.push_back(*f);
		f->nap(Timestamp::null());

		AlarmClock a(*this, *f, timeout);
		w->waiter().scheduleTask(a);
		w->schedule(nullptr, now);

		w->waiter().unscheduleTask(a);
		return !a.rang();
	}

private:
	List<Fiber> m_queue;
};

/*!
 * \brief Fiber-aware mutex.
 * \ingroup zth_api_cpp_sync
 */
class Mutex : public Synchronizer {
	ZTH_CLASS_NEW_DELETE(Mutex)
public:
	explicit Mutex(cow_string const& name = "Mutex")
		: Synchronizer(name)
		, m_locked()
	{}

#  if __cplusplus >= 201103L
	explicit Mutex(cow_string&& name)
		: Synchronizer(std::move(name))
		, m_locked()
	{}
#  endif

	virtual ~Mutex() override is_default

	void lock()
	{
		while(unlikely(m_locked))
			block();
		m_locked = true;
		zth_dbg(sync, "[%s] Locked", id_str());
	}

	bool trylock() noexcept
	{
		if(m_locked)
			return false;
		m_locked = true;
		zth_dbg(sync, "[%s] Locked", id_str());
		return true;
	}

	void unlock() noexcept
	{
		zth_assert(m_locked);
		zth_dbg(sync, "[%s] Unlocked", id_str());
		m_locked = false;
		unblockFirst();
	}

private:
	bool m_locked;
};

/*!
 * \brief Mutex RAII, that locks and unlocks the mutex automatically.
 * \ingroup zth_api_cpp_sync
 */
class Locked {
	ZTH_CLASS_NEW_DELETE(Locked)
public:
	explicit Locked(Mutex& mutex)
		: m_mutex(&mutex)
	{
		m_mutex->lock();
	}

	~Locked()
	{
		if(m_mutex)
			m_mutex->unlock();
	}

#  if __cplusplus >= 201103L
	Locked(Locked const&) = delete;
	Locked& operator=(Locked const&) = delete;

	Locked(Locked&& l) noexcept
		: m_mutex{}
	{
		*this = std::move(l);
	}

	Locked& operator=(Locked&& l) noexcept
	{
		if(m_mutex)
			m_mutex->unlock();

		m_mutex = l.m_mutex;
		l.m_mutex = nullptr;
		return *this;
	}
#  else	 // Pre C++11
private:
	Locked(Locked const&);
	Locked& operator=(Locked const&);
#  endif // Pre C++11

private:
	Mutex* m_mutex;
};

/*!
 * \brief Fiber-aware semaphore.
 * \ingroup zth_api_cpp_sync
 */
class Semaphore : public Synchronizer {
	ZTH_CLASS_NEW_DELETE(Semaphore)
public:
	explicit Semaphore(size_t init = 0, cow_string const& name = "Semaphore")
		: Synchronizer(name)
		, m_count(init)
	{}

#  if __cplusplus >= 201103L
	Semaphore(size_t init, cow_string&& name)
		: Synchronizer(std::move(name))
		, m_count(init)
	{}
#  endif

	virtual ~Semaphore() override is_default

	void acquire(size_t count = 1)
	{
		size_t remaining = count;
		while(remaining > 0) {
			if(remaining <= m_count) {
				m_count -= remaining;
				if(m_count > 0)
					// There might be another one waiting.
					unblockFirst();
				return;
			} else {
				remaining -= m_count;
				m_count = 0;
				block();
			}
		}
		zth_dbg(sync, "[%s] Acquired %zu", id_str(), count);
	}

	void release(size_t count = 1) noexcept
	{
		zth_assert(m_count + count >= m_count); // ...otherwise it wrapped around, which is
							// probably not want you wanted...

		if(unlikely(m_count + count < m_count))
			// wrapped around, saturate
			m_count = std::numeric_limits<size_t>::max();
		else
			m_count += count;

		zth_dbg(sync, "[%s] Released %zu", id_str(), count);

		if(likely(m_count > 0))
			unblockFirst();
	}

	size_t value() const noexcept
	{
		return m_count;
	}

private:
	size_t m_count;
};

/*!
 * \brief Fiber-aware signal.
 * \ingroup zth_api_cpp_sync
 */
class Signal : public Synchronizer {
	ZTH_CLASS_NEW_DELETE(Signal)
public:
	explicit Signal(cow_string const& name = "Signal")
		: Synchronizer(name)
		, m_signalled()
	{}

#  if __cplusplus >= 201103L
	explicit Signal(cow_string&& name)
		: Synchronizer(std::move(name))
		, m_signalled()
	{}
#  endif

	virtual ~Signal() override is_default

	void wait()
	{
		if(!m_signalled) {
			block();
		} else {
			// Do a yield() here, as one might rely on the signal to block
			// regularly when the signal is used in a loop (see daemon
			// pattern).
			yield();
		}

		if(m_signalled > 0)
			m_signalled--;
	}

	bool wait(Timestamp const& timeout, Timestamp const& now = Timestamp::now())
	{
		if(!m_signalled) {
			if(!block(timeout, now))
				// Timeout.
				return false;
		} else
			yield();

		if(m_signalled > 0)
			m_signalled--;

		// Got signalled.
		return true;
	}

	bool wait(TimeInterval const& timeout, Timestamp const& now = Timestamp::now())
	{
		return wait(now + timeout);
	}

	void signal(bool queue = true, bool queueEveryTime = false) noexcept
	{
		zth_dbg(sync, "[%s] Signal", id_str());
		if(!unblockFirst() && queue && m_signalled >= 0) {
			if(m_signalled == 0 || queueEveryTime)
				m_signalled++;
			zth_assert(m_signalled > 0); // Otherwise, it wrapped around, which is
						     // probably not what you want.
		}
	}

	void signalAll(bool queue = true) noexcept
	{
		zth_dbg(sync, "[%s] Signal all", id_str());
		unblockAll();
		if(queue)
			m_signalled = -1;
	}

	void reset() noexcept
	{
		m_signalled = 0;
	}

private:
	int m_signalled;
};

/*!
 * \brief Fiber-aware future.
 * \ingroup zth_api_cpp_sync
 */
template <typename T = void>
class Future : public Synchronizer {
	ZTH_CLASS_NEW_DELETE(Future)
public:
	typedef T type;

	// cppcheck-suppress uninitMemberVar
	explicit Future(cow_string const& name = "Future")
		: Synchronizer(name)
		, m_valid()
	{
#  ifdef ZTH_USE_VALGRIND
		VALGRIND_MAKE_MEM_NOACCESS(m_data, sizeof(m_data));
#  endif
	}

#  if __cplusplus >= 201103L
	// cppcheck-suppress uninitMemberVar
	explicit Future(cow_string&& name)
		: Synchronizer(std::move(name))
		, m_valid()
	{
#    ifdef ZTH_USE_VALGRIND
		VALGRIND_MAKE_MEM_NOACCESS(m_data, sizeof(m_data));
#    endif
	}
#  endif

	virtual ~Future() override
	{
		if(valid()
#  ifdef ZTH_FUTURE_EXCEPTION
		   && !m_exception
#  endif // ZTH_FUTURE_EXCEPTION
		)
			value().~type();
#  ifdef ZTH_USE_VALGRIND
		VALGRIND_MAKE_MEM_UNDEFINED(m_data, sizeof(m_data));
#  endif
	}

	bool valid() const noexcept
	{
		return m_valid;
	}

	operator bool() const noexcept
	{
		return valid();
	}

	void wait()
	{
		if(!valid())
			block();
	}

	void set()
	{
		if(!set_prepare())
			return;

		new(m_data) type();
		set_finalize();
	}

	void set(type const& value)
	{
		if(!set_prepare())
			return;

		new(m_data) type(value);
		set_finalize();
	}

	Future& operator=(type const& value)
	{
		set(value);
		return *this;
	}

#  if __cplusplus >= 201103L
	void set(type&& value)
	{
		if(!set_prepare())
			return;

		new(m_data) type(std::move(value));
		set_finalize();
	}

	Future& operator=(type&& value)
	{
		set(std::move(value));
		return *this;
	}
#  endif

#  ifdef ZTH_FUTURE_EXCEPTION
	void set(std::exception_ptr exception)
	{
		if(!set_prepare())
			return;

		m_exception = std::move(exception);
		set_finalize();
	}

	Future& operator=(std::exception_ptr value)
	{
		set(std::move(value));
		return *this;
	}

	std::exception_ptr exception() const
	{
		return m_exception;
	}
#  endif // ZTH_FUTURE_EXCEPTION

	type& value() LREF_QUALIFIED
	{
		wait();

#  ifdef ZTH_FUTURE_EXCEPTION
		if(m_exception)
			std::rethrow_exception(m_exception);
#  endif // ZTH_FUTURE_EXCEPTION

		void* p = m_data;
		return *static_cast<type*>(p);
	}

	type const& value() const LREF_QUALIFIED
	{
		wait();

#  ifdef ZTH_FUTURE_EXCEPTION
		if(m_exception)
			std::rethrow_exception(m_exception);
#  endif // ZTH_FUTURE_EXCEPTION

		void const* p = m_data;
		return *static_cast<type const*>(p);
	}

#  if __cplusplus >= 201103L
	type value() &&
	{
		wait();

#    ifdef ZTH_FUTURE_EXCEPTION
		if(m_exception)
			std::rethrow_exception(m_exception);
#    endif // ZTH_FUTURE_EXCEPTION

		void* p = m_data;
		return std::move(*static_cast<type*>(p));
	}
#  endif

	type const* operator*() const
	{
		return &value();
	}

	type* operator*()
	{
		return &value();
	}

	type const* operator->() const
	{
		return &value();
	}

	type* operator->()
	{
		return &value();
	}

private:
	bool set_prepare() noexcept
	{
		zth_assert(!valid());
		if(valid())
			return false;
#  ifdef ZTH_USE_VALGRIND
		VALGRIND_MAKE_MEM_UNDEFINED(m_data, sizeof(m_data));
#  endif
		return true;
	}

	void set_finalize() noexcept
	{
		m_valid = true;
		zth_dbg(sync, "[%s] Set", id_str());
		unblockAll();
	}

private:
	alignas(type) char m_data[sizeof(type)];
	bool m_valid;
#  ifdef ZTH_FUTURE_EXCEPTION
	std::exception_ptr m_exception;
#  endif // ZTH_FUTURE_EXCEPTION
};

template <>
// cppcheck-suppress noConstructor
class Future<void> : public Synchronizer {
	ZTH_CLASS_NEW_DELETE(Future)
public:
	typedef void type;

	explicit Future(cow_string const& name = "Future")
		: Synchronizer(name)
		, m_valid()
	{}

#  if __cplusplus >= 201103L
	explicit Future(cow_string&& name)
		: Synchronizer(std::move(name))
		, m_valid()
	{}
#  endif

	virtual ~Future() override is_default

	bool valid() const noexcept
	{
		return m_valid;
	}

	operator bool() const noexcept
	{
		return valid();
	}

	void wait()
	{
		if(!valid())
			block();
	}

	void set() noexcept
	{
		zth_assert(!valid());
		if(valid())
			return;

		m_valid = true;
		zth_dbg(sync, "[%s] Set", id_str());
		unblockAll();
	}

#  ifdef ZTH_FUTURE_EXCEPTION
	void set(std::exception_ptr exception)
	{
		set();
		m_exception = std::move(exception);
	}

	Future& operator=(std::exception_ptr value)
	{
		set(std::move(value));
		return *this;
	}

	std::exception_ptr exception() const
	{
		return m_exception;
	}
#  endif // ZTH_FUTURE_EXCEPTION

private:
#  ifdef ZTH_FUTURE_EXCEPTION
	std::exception_ptr m_exception;
#  endif // ZTH_FUTURE_EXCEPTION
	bool m_valid;
};

/*!
 * \brief Fiber-aware barrier/gate.
 * \ingroup zth_api_cpp_sync
 */
class Gate : public Synchronizer {
	ZTH_CLASS_NEW_DELETE(Gate)
public:
	explicit Gate(size_t count, cow_string const& name = "Gate")
		: Synchronizer(name)
		, m_count(count)
		, m_current()
	{}

#  if __cplusplus >= 201103L
	Gate(size_t count, cow_string&& name)
		: Synchronizer(std::move(name))
		, m_count(count)
		, m_current()
	{}
#  endif

	virtual ~Gate() override is_default

	bool pass() noexcept
	{
		zth_dbg(sync, "[%s] Pass", id_str());
		if(++m_current >= count()) {
			m_current -= count();
			unblockAll();
			return true;
		} else {
			return false;
		}
	}

	void wait()
	{
		if(!pass())
			block();
	}

	size_t count() const noexcept
	{
		return m_count;
	}
	size_t current() const noexcept
	{
		return m_current;
	}

private:
	size_t const m_count;
	size_t m_current;
};

} // namespace zth

struct zth_mutex_t {
	void* p;
};

/*!
 * \brief Initializes a mutex.
 * \details This is a C-wrapper to create a new zth::Mutex.
 * \ingroup zth_api_c_sync
 */
EXTERN_C ZTH_EXPORT ZTH_INLINE int zth_mutex_init(zth_mutex_t* mutex) noexcept
{
	if(unlikely(!mutex || mutex->p))
		return EINVAL;

	try {
		mutex->p = static_cast<void*>(new zth::Mutex());
		return 0;
	} catch(std::bad_alloc const&) {
		return ENOMEM;
	} catch(...) {
	}
	return EAGAIN;
}

/*!
 * \brief Destroys a mutex.
 * \details This is a C-wrapper to delete a zth::Mutex.
 * \ingroup zth_api_c_sync
 */
EXTERN_C ZTH_EXPORT ZTH_INLINE int zth_mutex_destroy(zth_mutex_t* mutex) noexcept
{
	if(unlikely(!mutex))
		return EINVAL;
	if(unlikely(!mutex->p))
		// Already destroyed.
		return 0;

	delete static_cast<zth::Mutex*>(mutex->p);
	mutex->p = nullptr;
	return 0;
}

/*!
 * \brief Locks a mutex.
 * \details This is a C-wrapper for zth::Mutex::lock().
 * \ingroup zth_api_c_sync
 */
EXTERN_C ZTH_EXPORT ZTH_INLINE int zth_mutex_lock(zth_mutex_t* mutex) noexcept
{
	if(unlikely(!mutex || !mutex->p))
		return EINVAL;

	static_cast<zth::Mutex*>(mutex->p)->lock();
	return 0;
}

/*!
 * \brief Try to lock a mutex.
 * \details This is a C-wrapper for zth::Mutex::trylock().
 * \ingroup zth_api_c_sync
 */
EXTERN_C ZTH_EXPORT ZTH_INLINE int zth_mutex_trylock(zth_mutex_t* mutex) noexcept
{
	if(unlikely(!mutex || !mutex->p))
		return EINVAL;

	return static_cast<zth::Mutex*>(mutex->p)->trylock() ? 0 : EBUSY;
}

/*!
 * \brief Unlock a mutex.
 * \details This is a C-wrapper for zth::Mutex::unlock().
 * \ingroup zth_api_c_sync
 */
EXTERN_C ZTH_EXPORT ZTH_INLINE int zth_mutex_unlock(zth_mutex_t* mutex) noexcept
{
	if(unlikely(!mutex || !mutex->p))
		return EINVAL;

	static_cast<zth::Mutex*>(mutex->p)->unlock();
	return 0;
}

struct zth_sem_t {
	void* p;
};

/*!
 * \brief Initializes a semaphore.
 * \details This is a C-wrapper to create a new zth::Semaphore.
 * \ingroup zth_api_c_sync
 */
EXTERN_C ZTH_EXPORT ZTH_INLINE int zth_sem_init(zth_sem_t* sem, size_t value) noexcept
{
	if(unlikely(!sem || sem->p))
		return EINVAL;

	try {
		sem->p = static_cast<void*>(new zth::Semaphore(value));
		return 0;
	} catch(std::bad_alloc const&) {
		return ENOMEM;
	} catch(...) {
	}
	return EAGAIN;
}

/*!
 * \brief Destroys a semaphore.
 * \details This is a C-wrapper to delete a zth::Semaphore.
 * \ingroup zth_api_c_sync
 */
EXTERN_C ZTH_EXPORT ZTH_INLINE int zth_sem_destroy(zth_sem_t* sem) noexcept
{
	if(unlikely(!sem))
		return EINVAL;
	if(unlikely(!sem->p))
		// Already destroyed.
		return 0;

	delete static_cast<zth::Semaphore*>(sem->p);
	sem->p = nullptr;
	return 0;
}

/*!
 * \brief Returns the value of a semaphore.
 * \details This is a C-wrapper for zth::Semaphore::value().
 * \ingroup zth_api_c_sync
 */
EXTERN_C ZTH_EXPORT ZTH_INLINE int
zth_sem_getvalue(zth_sem_t* __restrict__ sem, size_t* __restrict__ value) noexcept
{
	if(unlikely(!sem || !sem->p || !value))
		return EINVAL;

	*value = static_cast<zth::Semaphore*>(sem->p)->value();
	return 0;
}

#  ifndef EOVERFLOW
#    define EOVERFLOW EAGAIN
#  endif

/*!
 * \brief Increments a semaphore.
 * \details This is a C-wrapper for zth::Mutex::release() of 1.
 * \ingroup zth_api_c_sync
 */
EXTERN_C ZTH_EXPORT ZTH_INLINE int zth_sem_post(zth_sem_t* sem) noexcept
{
	if(unlikely(!sem || !sem->p))
		return EINVAL;

	zth::Semaphore* s = static_cast<zth::Semaphore*>(sem->p);
	if(unlikely(s->value() == std::numeric_limits<size_t>::max()))
		return EOVERFLOW;

	s->release();
	return 0;
}

/*!
 * \brief Decrements (or wait for) a semaphore.
 * \details This is a C-wrapper for zth::Mutex::acquire() of 1.
 * \ingroup zth_api_c_sync
 */
EXTERN_C ZTH_EXPORT ZTH_INLINE int zth_sem_wait(zth_sem_t* sem) noexcept
{
	if(unlikely(!sem || !sem->p))
		return EINVAL;

	static_cast<zth::Semaphore*>(sem->p)->acquire();
	return 0;
}

/*!
 * \brief Try to decrement a semaphore.
 * \details This is a C-wrapper based on zth::Mutex::acquire().
 * \ingroup zth_api_c_sync
 */
EXTERN_C ZTH_EXPORT ZTH_INLINE int zth_sem_trywait(zth_sem_t* sem) noexcept
{
	if(unlikely(!sem || !sem->p))
		return EINVAL;

	zth::Semaphore* s = static_cast<zth::Semaphore*>(sem->p);
	if(unlikely(s->value() == 0))
		return EAGAIN;

	s->acquire();
	return 0;
}

struct zth_cond_t {
	void* p;
};

/*!
 * \brief Initializes a condition.
 * \details This is a C-wrapper to create a new zth::Signal.
 * \ingroup zth_api_c_sync
 */
EXTERN_C ZTH_EXPORT ZTH_INLINE int zth_cond_init(zth_cond_t* cond) noexcept
{
	if(unlikely(!cond || cond->p))
		return EINVAL;

	try {
		cond->p = static_cast<void*>(new zth::Signal());
		return 0;
	} catch(std::bad_alloc const&) {
		return ENOMEM;
	} catch(...) {
	}
	return EAGAIN;
}

/*!
 * \brief Destroys a condition.
 * \details This is a C-wrapper to delete a zth::Signal.
 * \ingroup zth_api_c_sync
 */
EXTERN_C ZTH_EXPORT ZTH_INLINE int zth_cond_destroy(zth_cond_t* cond) noexcept
{
	if(unlikely(!cond))
		return EINVAL;
	if(unlikely(!cond->p))
		// Already destroyed.
		return 0;

	delete static_cast<zth::Signal*>(cond->p);
	cond->p = nullptr;
	return 0;
}

/*!
 * \brief Signals one fiber waiting for the condition.
 * \details This is a C-wrapper for zth::Signal::signal().
 * \ingroup zth_api_c_sync
 */
EXTERN_C ZTH_EXPORT ZTH_INLINE int zth_cond_signal(zth_cond_t* cond) noexcept
{
	if(unlikely(!cond || !cond->p))
		return EINVAL;

	static_cast<zth::Signal*>(cond->p)->signal();
	return 0;
}

/*!
 * \brief Signals all fibers waiting for the condition.
 * \details This is a C-wrapper for zth::Signal::signalAll().
 * \ingroup zth_api_c_sync
 */
EXTERN_C ZTH_EXPORT ZTH_INLINE int zth_cond_broadcast(zth_cond_t* cond) noexcept
{
	if(unlikely(!cond || !cond->p))
		return EINVAL;

	static_cast<zth::Signal*>(cond->p)->signalAll();
	return 0;
}

/*!
 * \brief Wait for a condition.
 * \details This is a C-wrapper for zth::Signal::wait().
 * \ingroup zth_api_c_sync
 */
EXTERN_C ZTH_EXPORT ZTH_INLINE int zth_cond_wait(zth_cond_t* cond) noexcept
{
	if(unlikely(!cond || !cond->p))
		return EINVAL;

	static_cast<zth::Signal*>(cond->p)->wait();
	return 0;
}

struct zth_future_t {
	void* p;
};

typedef zth::Future<uintptr_t> zth_future_t_type;

/*!
 * \brief Initializes a future.
 * \details This is a C-wrapper to create a new zth::Future.
 * \ingroup zth_api_c_sync
 */
EXTERN_C ZTH_EXPORT ZTH_INLINE int zth_future_init(zth_future_t* future) noexcept
{
	if(unlikely(!future || future->p))
		return EINVAL;

	try {
		future->p = static_cast<void*>(new zth_future_t_type());
		return 0;
	} catch(std::bad_alloc const&) {
		return ENOMEM;
	} catch(...) {
	}
	return EAGAIN;
}

/*!
 * \brief Destroys a future.
 * \details This is a C-wrapper to delete a zth::Future.
 * \ingroup zth_api_c_sync
 */
EXTERN_C ZTH_EXPORT ZTH_INLINE int zth_future_destroy(zth_future_t* future) noexcept
{
	if(unlikely(!future))
		return EINVAL;
	if(unlikely(!future->p))
		// Already destroyed.
		return 0;

	delete static_cast<zth_future_t_type*>(future->p);
	future->p = nullptr;
	return 0;
}

/*!
 * \brief Checks if a future was already set.
 * \details This is a C-wrapper for zth::Future::valid().
 * \ingroup zth_api_c_sync
 */
EXTERN_C ZTH_EXPORT ZTH_INLINE int zth_future_valid(zth_future_t* future) noexcept
{
	if(unlikely(!future || !future->p))
		return EINVAL;

	return static_cast<zth_future_t_type*>(future->p)->valid() ? 0 : EAGAIN;
}

/*!
 * \brief Sets a future and signals all waiting fibers.
 * \details This is a C-wrapper for zth::Future::set().
 * \ingroup zth_api_c_sync
 */
EXTERN_C ZTH_EXPORT ZTH_INLINE int zth_future_set(zth_future_t* future, uintptr_t value) noexcept
{
	if(unlikely(!future || !future->p))
		return EINVAL;

	zth_future_t_type* f = static_cast<zth_future_t_type*>(future->p);
	if(f->valid())
		return EAGAIN;

	f->set(value);
	return 0;
}

/*!
 * \brief Wait for and return a future's value.
 * \details This is a C-wrapper for zth::Future::value().
 * \ingroup zth_api_c_sync
 */
EXTERN_C ZTH_EXPORT ZTH_INLINE int
zth_future_get(zth_future_t* __restrict__ future, uintptr_t* __restrict__ value) noexcept
{
	if(unlikely(!future || !future->p || !value))
		return EINVAL;

	*value = static_cast<zth_future_t_type*>(future->p)->value();
	return 0;
}

/*!
 * \brief Wait for a future.
 * \details This is a C-wrapper for zth::Future::wait().
 * \ingroup zth_api_c_sync
 */
EXTERN_C ZTH_EXPORT ZTH_INLINE int zth_future_wait(zth_future_t* future) noexcept
{
	if(unlikely(!future || !future->p))
		return EINVAL;

	static_cast<zth_future_t_type*>(future->p)->wait();
	return 0;
}

struct zth_gate_t {
	void* p;
};

/*!
 * \brief Initializes a gate.
 *
 * If all participants call #zth_gate_wait(), the gate behaves the same as a \c
 * pthread_barrier_t would. \details This is a C-wrapper to create a new
 * zth::Gate.
 *
 * \ingroup zth_api_c_sync
 */
EXTERN_C ZTH_EXPORT ZTH_INLINE int zth_gate_init(zth_gate_t* gate, size_t count) noexcept
{
	if(unlikely(!gate || gate->p))
		return EINVAL;

	try {
		gate->p = static_cast<void*>(new zth::Gate(count));
		return 0;
	} catch(std::bad_alloc const&) {
		return ENOMEM;
	} catch(...) {
	}
	return EAGAIN;
}

/*!
 * \brief Destroys a gate.
 * \details This is a C-wrapper to delete a zth::Gate.
 * \ingroup zth_api_c_sync
 */
EXTERN_C ZTH_EXPORT ZTH_INLINE int zth_gate_destroy(zth_gate_t* gate) noexcept
{
	if(unlikely(!gate))
		return EINVAL;
	if(unlikely(!gate->p))
		// Already destroyed.
		return 0;

	delete static_cast<zth::Gate*>(gate->p);
	gate->p = nullptr;
	return 0;
}

/*!
 * \brief Passes a gate.
 * \details This is a C-wrapper for zth::Gate::pass().
 * \ingroup zth_api_c_sync
 */
EXTERN_C ZTH_EXPORT ZTH_INLINE int zth_gate_pass(zth_gate_t* gate) noexcept
{
	if(unlikely(!gate || !gate->p))
		return EINVAL;

	return static_cast<zth::Gate*>(gate->p)->pass() ? 0 : EBUSY;
}

/*!
 * \brief Wait for a gate.
 * \details This is a C-wrapper for zth::Gate::wait().
 * \ingroup zth_api_c_sync
 */
EXTERN_C ZTH_EXPORT ZTH_INLINE int zth_gate_wait(zth_gate_t* gate) noexcept
{
	if(unlikely(!gate || !gate->p))
		return EINVAL;

	static_cast<zth::Gate*>(gate->p)->wait();
	return 0;
}

#else // !__cplusplus

#  include <stdint.h>

typedef struct {
	void* p;
} zth_mutex_t;

ZTH_EXPORT int zth_mutex_init(zth_mutex_t* mutex);
ZTH_EXPORT int zth_mutex_destroy(zth_mutex_t* mutex);
ZTH_EXPORT int zth_mutex_lock(zth_mutex_t* mutex);
ZTH_EXPORT int zth_mutex_trylock(zth_mutex_t* mutex);
ZTH_EXPORT int zth_mutex_unlock(zth_mutex_t* mutex);

typedef struct {
	void* p;
} zth_sem_t;

ZTH_EXPORT int zth_sem_init(zth_sem_t* sem, size_t value);
ZTH_EXPORT int zth_sem_destroy(zth_sem_t* sem);
ZTH_EXPORT int zth_sem_getvalue(zth_sem_t* __restrict__ sem, size_t* __restrict__ value);
ZTH_EXPORT int zth_sem_post(zth_sem_t* sem);
ZTH_EXPORT int zth_sem_wait(zth_sem_t* sem);
ZTH_EXPORT int zth_sem_trywait(zth_sem_t* sem);

typedef struct {
	void* p;
} zth_cond_t;

ZTH_EXPORT int zth_cond_init(zth_cond_t* cond);
ZTH_EXPORT int zth_cond_destroy(zth_cond_t* cond);
ZTH_EXPORT int zth_cond_signal(zth_cond_t* cond);
ZTH_EXPORT int zth_cond_broadcast(zth_cond_t* cond);
ZTH_EXPORT int zth_cond_wait(zth_cond_t* cond);

typedef struct {
	void* p;
} zth_future_t;

ZTH_EXPORT int zth_future_init(zth_future_t* future);
ZTH_EXPORT int zth_future_destroy(zth_future_t* future);
ZTH_EXPORT int zth_future_valid(zth_future_t* future);
ZTH_EXPORT int zth_future_set(zth_future_t* future, uintptr_t value);
ZTH_EXPORT int zth_future_get(zth_future_t* __restrict__ future, uintptr_t* __restrict__ value);
ZTH_EXPORT int zth_future_wait(zth_future_t* future);

typedef struct {
	void* p;
} zth_gate_t;

ZTH_EXPORT int zth_gate_init(zth_gate_t* gate, size_t count);
ZTH_EXPORT int zth_gate_destroy(zth_gate_t* gate);
ZTH_EXPORT int zth_gate_pass(zth_gate_t* gate);
ZTH_EXPORT int zth_gate_wait(zth_gate_t* gate);

#endif // !__cplusplus
#endif // ZTH_SYNC_H
