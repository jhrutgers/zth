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

#  if __cplusplus >= 201703L
#    include <tuple>
#  endif

namespace zth {

template <typename Impl, typename T>
class SharedPointerOps {
public:
	constexpr14 T& operator*() const noexcept
	{
		zth_assert(impl().get());
		// cppcheck-suppress nullPointerRedundantCheck
		return *impl().get();
	}

	constexpr14 T* operator->() const noexcept
	{
		zth_assert(impl().get());
		return impl().get();
	}

private:
	Impl const& impl() const noexcept
	{
		return static_cast<Impl const&>(*this);
	}
};

template <typename Impl>
class SharedPointerOps<Impl, void> {
public:
};

template <typename T>
class SharedPointer : public SharedPointerOps<SharedPointer<T>, T> {
public:
	typedef T type;

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

	~SharedPointer() noexcept
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

	SharedPointer& operator=(SharedPointer const& p) noexcept
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
		reset();
		m_object = p.release();
		return *this;
	}
#  endif

	constexpr type* get() const noexcept
	{
		return m_object;
	}

	constexpr operator type*() const noexcept
	{
		return get();
	}

	constexpr14 type* release() noexcept
	{
		type* object = get();
		m_object = nullptr;
		return object;
	}

	operator bool() const noexcept
	{
		return get() != nullptr;
	}

private:
	type* m_object;
};

template <typename Impl, typename T>
class SharedReferenceOps {
public:
	constexpr14 T& get() const noexcept
	{
		zth_assert(impl().valid());
		return *impl().m_object.get();
	}

	constexpr operator T&() const noexcept
	{
		return get();
	}

	constexpr14 decltype(**static_cast<T*>(nullptr)) operator*() const
	{
		return *get();
	}

	constexpr14 decltype(static_cast<T*>(nullptr)->operator->()) operator->() const noexcept
	{
		return get().operator->();
	}

private:
	Impl const& impl() const noexcept
	{
		return static_cast<Impl const&>(*this);
	}
};

template <typename Impl>
class SharedReferenceOps<Impl, void> {
public:
};

template <typename T>
class SharedReference : public SharedReferenceOps<SharedReference<T>, T> {
public:
	typedef SharedPointer<T> SharedPointer_type;
	typedef typename SharedPointer_type::type type;
	friend class SharedReferenceOps<SharedReference<T>, T>;

	constexpr14 SharedReference() noexcept
		: m_object()
	{}

	// cppcheck-suppress noExplicitConstructor
	constexpr14 SharedReference(type& object) noexcept
		: m_object(&object)
	{}

	constexpr14 SharedReference(SharedReference const& p) noexcept
		: m_object(p.m_object)
	{}

	// cppcheck-suppress noExplicitConstructor
	constexpr14 SharedReference(SharedPointer_type const& p) noexcept
		: m_object(p)
	{}

	~SharedReference() noexcept is_default

	void reset()
	{
		m_object.reset();
	}

	SharedReference& operator=(SharedReference const& p) noexcept
	{
		m_object = p.m_object;
		return *this;
	}

#  if __cplusplus >= 201103L
	// cppcheck-suppress noExplicitConstructor
	constexpr14 SharedReference(SharedPointer_type&& p) noexcept
		: m_object(std::move(p))
	{}

	constexpr14 SharedReference(SharedReference&& p) noexcept
		: m_object()
	{
		*this = std::move(p);
	}

	constexpr14 SharedReference& operator=(SharedReference&& p) noexcept
	{
		m_object = std::move(p.m_object);
		return *this;
	}

	constexpr14 operator SharedPointer<type>() && noexcept
	{
		return std::move(m_object);
	}
#  endif

	constexpr14 operator SharedPointer_type() LREF_QUALIFIED noexcept
	{
		return m_object;
	}

	constexpr14 bool valid() const noexcept
	{
		return m_object.get();
	}

private:
	SharedPointer_type m_object;
};

class SynchronizerBase : public RefCounted, public UniqueID<SynchronizerBase> {
	ZTH_CLASS_NEW_DELETE(SynchronizerBase)
protected:
	explicit SynchronizerBase(cow_string const& name = "Synchronizer")
		: UniqueID(Config::NamedSynchronizer ? name.str() : string())
	{}

#  if __cplusplus >= 201103L
	explicit SynchronizerBase(cow_string&& name)
		: UniqueID(Config::NamedSynchronizer ? std::move(name).str() : string())
	{}
#  endif

public:
	virtual ~SynchronizerBase() noexcept override
	{
		zth_dbg(sync, "[%s] Destruct", id_str());
	}

protected:
	typedef List<> queue_type;

	virtual queue_type& queue(size_t q = 0) = 0;

	queue_type::iterator enqueue(Listable& item, size_t q = 0) noexcept
	{
		return queue(q).push_back(item);
	}

	void block(size_t q = 0)
	{
		Worker& w = currentWorker();
		Fiber* f = w.currentFiber();
		if(unlikely(!f))
			zth_throw(not_in_fiber());

		zth_dbg(sync, "[%s] Block %s", id_str(), f->id_str());
		w.release(*f);
		enqueue(*f, q);
		f->nap(Timestamp::null());
		w.schedule();
	}

	/*!
	 * \brief Unblock the specific fiber.
	 * \return \c true if unblocked, \c false when not queued by this Synchronizer.
	 */
	bool unblock(Fiber& f, size_t q = 0, bool prio = false) noexcept
	{
		queue_type& queue_ = queue(q);
		if(!queue_.contains(f))
			return false;

		wakeup(f, queue_.erase(f), prio, q);
		return true;
	}

	Listable* unblockFirst(size_t q = 0, bool prio = false) noexcept
	{
		queue_type& queue_ = queue(q);
		if(queue_.empty())
			return nullptr;

		Listable& l = queue_.front();
		wakeup(l, queue_.pop_front(), prio, q);
		return &l;
	}

	bool unblockAll(size_t q = 0, bool prio = false) noexcept
	{
		queue_type& queue_ = queue(q);
		if(queue_.empty())
			return false;

		while(!queue_.empty()) {
			Listable& f = queue_.front();
			wakeup(f, queue_.pop_front(), prio, q);
		}
		return true;
	}

	virtual void
	wakeup(Listable& item, queue_type::user_type user, bool prio, size_t UNUSED_PAR(q)) noexcept
	{
		// Synchronizer only handles Fibers. Other types could be implemented in a derived
		// class based on the user parameter.
		zth_assert(user == queue_type::user_type());
		Fiber& f = static_cast<Fiber&>(item);

		Worker& w = currentWorker();

		zth_dbg(sync, "[%s] Unblock %s", id_str(), f.id_str());
		f.wakeup();
		w.add(&f, prio);
	}

	class AlarmClock : public TimedWaitable {
		ZTH_CLASS_NEW_DELETE(AlarmClock)
	public:
		typedef TimedWaitable base;
		AlarmClock(
			SynchronizerBase& synchronizer, size_t q, Fiber& fiber,
			Timestamp const& timeout) noexcept
			: base(timeout)
			, m_synchronizer(synchronizer)
			, m_q(q)
			, m_fiber(fiber)
			, m_rang()
		{}

		virtual ~AlarmClock() noexcept override is_default

		virtual bool poll(Timestamp const& now = Timestamp::now()) noexcept override
		{
			if(!base::poll(now))
				return false;

			zth_dbg(sync, "[%s] %s timed out", m_synchronizer.id_str(),
				m_fiber.id_str());
			m_rang = m_synchronizer.unblock(m_fiber, m_q);
			return true;
		}

		bool rang() const noexcept
		{
			return m_rang;
		}

	private:
		SynchronizerBase& m_synchronizer;
		size_t m_q;
		Fiber& m_fiber;
		bool m_rang;
	};

	friend class AlarmClock;

	/*!
	 * \brief Block, with timeout.
	 * \return \c true if unblocked by request, \c false when unblocked by timeout.
	 */
	bool
	blockUntil(Timestamp const& timeout, Timestamp const& now = Timestamp::now(), size_t q = 0)
	{
		if(timeout <= now)
			// Immediate timeout.
			return true;

		return block_(timeout, now, q);
	}

	/*!
	 * \brief Block, with timeout.
	 * \return \c true if unblocked by request, \c false when unblocked by timeout.
	 */
	bool
	blockFor(TimeInterval const& timeout, Timestamp const& now = Timestamp::now(), size_t q = 0)
	{
		if(timeout.isNegative() || timeout.isNull())
			// Immediate timeout.
			return true;

		return block_(now + timeout, now, q);
	}

private:
	/*!
	 * \brief Block, with timeout.
	 * \return \c true if unblocked by request, \c false when unblocked by timeout.
	 */
	bool block_(Timestamp const& timeout, Timestamp const& now, size_t q = 0)
	{
		Worker& w = currentWorker();
		Fiber* f = w.currentFiber();
		if(unlikely(!f))
			zth_throw(not_in_fiber());

		zth_dbg(sync, "[%s] Block %s with timeout", id_str(), f->id_str());
		w.release(*f);
		queue(q).push_back(*f);
		f->nap(Timestamp::null());

		AlarmClock a(*this, q, *f, timeout);
		w.waiter().scheduleTask(a);
		w.schedule(nullptr, now);

		w.waiter().unscheduleTask(a);
		return !a.rang();
	}
};

template <size_t Size = 1>
class Synchronizer : public SynchronizerBase {
	ZTH_CLASS_NEW_DELETE(Synchronizer)
public:
	typedef SynchronizerBase base;

	Synchronizer() is_default

	explicit Synchronizer(cow_string const& name)
		: SynchronizerBase(name)
	{}

#  if __cplusplus >= 201103L
	explicit Synchronizer(cow_string&& name)
		: SynchronizerBase(std::move(name))
	{}
#  endif // C++11

	virtual ~Synchronizer() noexcept override
	{
		for(size_t i = 0; i < Size; i++)
			zth_assert(m_queue[i].empty());
	}

protected:
	typedef base::queue_type queue_type;

	virtual queue_type& queue(size_t q) final
	{
		zth_assert(q < Size);
		return m_queue[q];
	}

private:
	queue_type m_queue[Size];
};

/*!
 * \brief Fiber-aware mutex.
 * \ingroup zth_api_cpp_sync
 */
class Mutex : public Synchronizer<> {
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

	virtual ~Mutex() noexcept override is_default

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

	bool trylock(Timestamp const& timeout)
	{
		while(unlikely(m_locked))
			if(!blockUntil(timeout))
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
class Semaphore : public Synchronizer<> {
	ZTH_CLASS_NEW_DELETE(Semaphore)
public:
	typedef unsigned int count_type;

	explicit Semaphore(count_type init = 0, cow_string const& name = "Semaphore")
		: Synchronizer(name)
		, m_count(init)
		, m_unblockAll()
	{}

#  if __cplusplus >= 201103L
	Semaphore(count_type init, cow_string&& name)
		: Synchronizer(std::move(name))
		, m_count(init)
		, m_unblockAll()
	{}
#  endif

	virtual ~Semaphore() noexcept override is_default

	void acquire(count_type count = 1)
	{
		while(m_count < count) {
			if(count > 1)
				m_unblockAll = true;

			block();
		}

		m_count -= count;
		zth_dbg(sync, "[%s] Acquired %u", id_str(), count);

		if(m_count > 0 && !m_unblockAll) {
			// There is more to acquire. Wake the next in line.
			unblockFirst();
		}
	}

	void release(count_type count = 1) noexcept
	{
		zth_assert(m_count + count >= m_count); // ...otherwise it wrapped around, which is
							// probably not want you wanted...

		if(unlikely(m_count + count < m_count))
			// wrapped around, saturate
			m_count = std::numeric_limits<count_type>::max();
		else
			m_count += count;

		zth_dbg(sync, "[%s] Released %u", id_str(), count);

		if(likely(m_count > 0)) {
			if(!m_unblockAll) {
				// As some fibers may be waiting for just one unit, and others for
				// multiple, let them all try to acquire what they need.
				unblockFirst();
			} else if(!unblockAll()) {
				// Nobody was waiting anymore.
				m_unblockAll = false;
			}
		}
	}

	count_type value() const noexcept
	{
		return m_count;
	}

private:
	count_type m_count;
	bool m_unblockAll;
};

/*!
 * \brief Fiber-aware signal.
 * \ingroup zth_api_cpp_sync
 */
class Signal : public Synchronizer<> {
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

	virtual ~Signal() noexcept override is_default

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
			if(!blockUntil(timeout, now))
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
		return wait(now + timeout, now);
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

template <typename T>
class Optional {
	ZTH_CLASS_NEW_DELETE(Optional)
public:
	typedef T type;

	// cppcheck-suppress[uninitMemberVar,noExplicitConstructor]
	Optional() noexcept
		: m_valid()
	{
		init();
	}

	~Optional() noexcept
	{
		deinit();
	}

	bool valid() const noexcept
	{
		return m_valid
#  ifdef ZTH_FUTURE_EXCEPTION
		       || m_exception
#  endif // ZTH_FUTURE_EXCEPTION
			;
	}

	operator bool() const noexcept
	{
		return valid();
	}

	void reset() noexcept
	{
		unuse();
	}

	// cppcheck-suppress[uninitMemberVar,noExplicitConstructor]
	Optional(type const& value)
		: m_valid()
	{
		init();
		set(value);
	}

	void set(type const& value = type()) noexcept
	{
		unuse();
		use();
		new(m_data) type(value);
		m_valid = true;
	}

	Optional& operator=(type const& value) noexcept
	{
		set(value);
		return *this;
	}

#  if __cplusplus >= 201103L
	// cppcheck-suppress[uninitMemberVar,noExplicitConstructor]
	Optional(type&& value) noexcept
		: m_valid{true}
	{
		use();
		new(m_data) type{std::move(value)};
	}

	void set(type&& value) noexcept
	{
		unuse();
		use();
		new(m_data) type{std::move(value)};
		m_valid = true;
	}

	Optional& operator=(type&& value) noexcept
	{
		set(std::move(value));
		return *this;
	}
#  endif // C++11

#  ifdef ZTH_FUTURE_EXCEPTION
	// cppcheck-suppress uninitMemberVar
	Optional(std::exception_ptr exc) noexcept
		: m_valid()
		, m_exception(exc)
	{
		init();
	}

	void set(std::exception_ptr exception) noexcept
	{
		unuse();
		m_exception = std::move(exception);
	}

	Optional& operator=(std::exception_ptr value) noexcept
	{
		set(value);
		return *this;
	}

	type& value() LREF_QUALIFIED
	{
		zth_assert(valid());

		if(m_exception)
			std::rethrow_exception(m_exception);

		void* p = m_data;
		return *static_cast<type*>(p);
	}

	type const& value() const LREF_QUALIFIED
	{
		zth_assert(valid());

		if(m_exception)
			std::rethrow_exception(m_exception);

		void const* p = m_data;
		return *static_cast<type const*>(p);
	}

	std::exception_ptr exception() const noexcept
	{
		return m_exception;
	}

#    if __cplusplus >= 201103L
	type&& value() &&
	{
		zth_assert(valid());

		if(m_exception)
			std::rethrow_exception(m_exception);

		void* p = m_data;
		return std::move(*static_cast<type*>(p));
	}
#    endif // C++11
#  else	   // !ZTH_FUTURE_EXCEPTION
	type& value() LREF_QUALIFIED noexcept
	{
		zth_assert(valid());

		void* p = m_data;
		return *static_cast<type*>(p);
	}

	type const& value() const LREF_QUALIFIED noexcept
	{
		zth_assert(valid());

		void const* p = m_data;
		return *static_cast<type const*>(p);
	}

#    if __cplusplus >= 201103L
	type&& value() && noexcept
	{
		zth_assert(valid());

		void* p = m_data;
		return std::move(*static_cast<type*>(p));
	}
#    endif // C++11
#  endif   // !ZTH_FUTURE_EXCEPTION

private:
	void init() noexcept
	{
#  ifdef ZTH_USE_VALGRIND
		VALGRIND_MAKE_MEM_NOACCESS(m_data, sizeof(m_data));
#  endif
	}

	void use() noexcept
	{
#  ifdef ZTH_USE_VALGRIND
		VALGRIND_MAKE_MEM_UNDEFINED(m_data, sizeof(m_data));
#  endif
	}

	void unuse() noexcept
	{
#  ifdef ZTH_FUTURE_EXCEPTION
		if(m_exception)
			m_exception = nullptr;
#  endif // ZTH_FUTURE_EXCEPTION

		if(m_valid) {
			value().~type();
			m_valid = false;
#  ifdef ZTH_USE_VALGRIND
			VALGRIND_MAKE_MEM_NOACCESS(m_data, sizeof(m_data));
#  endif
		}
	}

	void deinit() noexcept
	{
		if(m_valid) {
			value().~type();
#  ifdef ZTH_USE_VALGRIND
			VALGRIND_MAKE_MEM_UNDEFINED(m_data, sizeof(m_data));
#  endif
		}
	}

private:
	alignas(type) char m_data[sizeof(type)];
	bool m_valid;
#  ifdef ZTH_FUTURE_EXCEPTION
	std::exception_ptr m_exception;
#  endif // ZTH_FUTURE_EXCEPTION
};

template <>
class Optional<void> {
	ZTH_CLASS_NEW_DELETE(Optional)
public:
	typedef void type;

	// cppcheck-suppress noExplicitConstructor
	Optional(bool set = false) noexcept
		: m_set(set)
	{}

	bool valid() const noexcept
	{
		return m_set
#  ifdef ZTH_FUTURE_EXCEPTION
		       || m_exception
#  endif // ZTH_FUTURE_EXCEPTION
			;
	}

	void reset() noexcept
	{
		m_set = false;
#  ifdef ZTH_FUTURE_EXCEPTION
		m_exception = nullptr;
#  endif // ZTH_FUTURE_EXCEPTION
	}

	void set() noexcept
	{
		m_set = true;
	}

#  ifdef ZTH_FUTURE_EXCEPTION
	Optional(std::exception_ptr exc) noexcept
		: m_exception(exc)
		, m_set()
	{}

	void set(std::exception_ptr exception) noexcept
	{
		m_exception = std::move(exception);
	}

	void value() const
	{
		zth_assert(valid());

		if(m_exception)
			std::rethrow_exception(m_exception);
	}

	std::exception_ptr exception() const noexcept
	{
		return m_exception;
	}
#  else	 // !ZTH_FUTURE_EXCEPTION
	void value() noexcept
	{
		zth_assert(valid());
	}
#  endif // !ZTH_FUTURE_EXCEPTION

private:
#  ifdef ZTH_FUTURE_EXCEPTION
	std::exception_ptr m_exception;
#  endif // ZTH_FUTURE_EXCEPTION
	bool m_set;
};

/*!
 * \brief Fiber-aware future.
 * \ingroup zth_api_cpp_sync
 */
template <typename T = void>
class Future : public Synchronizer<> {
	ZTH_CLASS_NEW_DELETE(Future)
public:
	typedef T type;
	typedef type& indirection_type;
	typedef type* member_type;

	explicit Future(cow_string const& name = "Future")
		: Synchronizer(name)
	{}

#  if __cplusplus >= 201103L
	explicit Future(cow_string&& name)
		: Synchronizer{std::move(name)}
	{}
#  endif

	virtual ~Future() noexcept override is_default

	bool valid() const noexcept
	{
		return m_value.valid();
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

	void set(type const& value = type()) noexcept
	{
		set_prepare();
		m_value.set(value);
		set_finalize();
	}

	Future& operator=(type const& value) noexcept
	{
		set(value);
		return *this;
	}

#  if __cplusplus >= 201103L
	void set(type&& value) noexcept
	{
		set_prepare();
		m_value.set(std::move(value));
		set_finalize();
	}

	Future& operator=(type&& value) noexcept
	{
		set(std::move(value));
		return *this;
	}
#  endif

#  ifdef ZTH_FUTURE_EXCEPTION
	void set(std::exception_ptr exception) noexcept
	{
		set_prepare();
		m_value.set(std::move(exception));
		set_finalize();
	}

	Future& operator=(std::exception_ptr value) noexcept
	{
		set(std::move(value));
		return *this;
	}

	std::exception_ptr exception() const noexcept
	{
		return m_value.exception();
	}
#  endif // ZTH_FUTURE_EXCEPTION

	type& value() LREF_QUALIFIED
	{
		wait();
		return m_value.value();
	}

	indirection_type operator*() LREF_QUALIFIED
	{
		return value();
	}

	member_type operator->()
	{
		return &value();
	}

#  if __cplusplus >= 201103L
	type&& value() &&
	{
		wait();
		return m_value.value();
	}

	type&& operator*() &&
	{
		return value();
	}
#  endif

private:
	void set_prepare() noexcept
	{
		zth_assert(!valid());
	}

	void set_finalize() noexcept
	{
		zth_dbg(sync, "[%s] Set", id_str());
		unblockAll();
	}

private:
	Optional<type> m_value;
};

template <>
// cppcheck-suppress noConstructor
class Future<void> : public Synchronizer<> {
	ZTH_CLASS_NEW_DELETE(Future)
public:
	typedef void type;
	typedef void indirection_type;
	typedef void member_type;

	explicit Future(cow_string const& name = "Future")
		: Synchronizer(name)
	{}

#  if __cplusplus >= 201103L
	explicit Future(cow_string&& name)
		: Synchronizer(std::move(name))
	{}
#  endif

	virtual ~Future() noexcept override is_default

	bool valid() const noexcept
	{
		return m_value.valid();
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
		set_prepare();
		m_value.set();
		set_finalize();
	}

#  ifdef ZTH_FUTURE_EXCEPTION
	void set(std::exception_ptr exception) noexcept
	{
		set_prepare();
		m_value.set(std::move(exception));
		set_finalize();
	}

	Future& operator=(std::exception_ptr value) noexcept
	{
		set(std::move(value));
		return *this;
	}

	std::exception_ptr exception() const noexcept
	{
		return m_value.exception();
	}
#  endif // ZTH_FUTURE_EXCEPTION

	void value()
	{
		wait();
		m_value.value();
	}

	indirection_type operator*()
	{
		value();
	}

	member_type operator->()
	{
		value();
	}

private:
	void set_prepare() noexcept
	{
		zth_assert(!valid());
	}

	void set_finalize() noexcept
	{
		zth_dbg(sync, "[%s] Set", id_str());
		unblockAll();
	}

private:
	Optional<void> m_value;
};

ZTH_STRUCTURED_BINDING_FORWARDING_NS(Future)

/*!
 * \brief Fiber-aware mailbox.
 * \ingroup zth_api_cpp_sync
 */
template <typename T>
class Mailbox : public Synchronizer<2> {
	ZTH_CLASS_NEW_DELETE(Mailbox)
protected:
	enum { Queue_Put = 0, Queue_Take = 1 };

public:
	typedef T type;
	typedef void* assigned_type;

	explicit Mailbox(cow_string const& name = "Mailbox")
		: Synchronizer(name)
		, m_assigned()
	{}

#  if __cplusplus >= 201103L
	explicit Mailbox(cow_string&& name)
		: Synchronizer{std::move(name)}
		, m_assigned()
	{}
#  endif // C++11

	virtual ~Mailbox() noexcept override is_default

	bool valid() const noexcept
	{
		return m_value.valid();
	}

	operator bool() const noexcept
	{
		return valid();
	}

	bool can_put() const noexcept
	{
		return !valid();
	}

	bool can_take() const noexcept
	{
		return valid(static_cast<Listable*>(&currentFiber()));
	}

protected:
	bool valid(assigned_type assigned) const noexcept
	{
		return valid() && (!m_assigned || m_assigned == assigned);
	}

public:
	void wait()
	{
		wait(static_cast<Listable*>(&currentFiber()));
	}

protected:
	virtual void wait(assigned_type assigned)
	{
		while(!valid(assigned))
			block((size_t)Queue_Take);
	}

public:
	void wait_empty()
	{
		while(valid())
			block((size_t)Queue_Put);
	}

	void put(type const& value)
	{
		put_prepare();
		m_value.set(value);
		put_finalize();
	}

	Mailbox& operator=(type const& value)
	{
		put(value);
		return *this;
	}

#  if __cplusplus >= 201103L
	void put(type&& value)
	{
		put_prepare();
		m_value.set(std::move(value));
		put_finalize();
	}

	Mailbox& operator=(type&& value)
	{
		put(std::move(value));
		return *this;
	}
#  endif

#  ifdef ZTH_FUTURE_EXCEPTION
	void put(std::exception_ptr exception)
	{
		put_prepare();
		m_value.set(std::move(exception));
		put_finalize();
	}

	Mailbox& operator=(std::exception_ptr value)
	{
		put(std::move(value));
		return *this;
	}

	std::exception_ptr exception() const noexcept
	{
		return m_value.exception();
	}
#  endif // ZTH_FUTURE_EXCEPTION

	type take()
	{
		return take(static_cast<Listable*>(&zth::currentFiber()));
	}

protected:
	type take(assigned_type assigned)
	{
		take_prepare(assigned);

#  ifdef ZTH_FUTURE_EXCEPTION
		std::exception_ptr exc = m_value.exception();
		if(exc) {
			take_finalize();
			std::rethrow_exception(exc);
		}
#  endif // ZTH_FUTURE_EXCEPTION

		type v = m_value.value();
		take_finalize();
		return v;
	}

	virtual void
	wakeup(Listable& item, queue_type::user_type user, bool prio, size_t q) noexcept final
	{
		if(q == Queue_Take)
			m_assigned = wakeup(item, user, prio);
		else
			base::wakeup(item, user, prio, q);
	}

	virtual assigned_type wakeup(Listable& item, queue_type::user_type user, bool prio) noexcept
	{
		base::wakeup(item, user, prio, Queue_Take);
		return &item;
	}

private:
	void put_prepare()
	{
		wait_empty();
	}

	void put_finalize() noexcept
	{
		zth_dbg(sync, "[%s] Put", id_str());
		unblockFirst(Queue_Take);
	}

	void take_prepare(assigned_type assigned)
	{
		wait(assigned);
	}

	void take_finalize() noexcept
	{
		zth_dbg(sync, "[%s] Take", id_str());
		m_value.reset();
		unblockFirst(Queue_Put);
	}

private:
	Optional<type> m_value;
	assigned_type m_assigned;
};

/*!
 * \brief Fiber-aware barrier/gate.
 * \ingroup zth_api_cpp_sync
 */
class Gate : public Synchronizer<> {
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

	virtual ~Gate() noexcept override is_default

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

	if(value > std::numeric_limits<zth::Semaphore::count_type>::max())
		return EDOM;

	try {
		sem->p = static_cast<void*>(new zth::Semaphore((zth::Semaphore::count_type)value));
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

	static_cast<zth::Signal*>(cond->p)->signalAll(false);
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

struct zth_mailbox_t {
	void* p;
};

typedef zth::Mailbox<uintptr_t> zth_mailbox_t_type;

/*!
 * \brief Initializes a mailbox.
 * \details This is a C-wrapper to create a new zth::Mailbox.
 * \ingroup zth_api_c_sync
 */
EXTERN_C ZTH_EXPORT ZTH_INLINE int zth_mailbox_init(zth_mailbox_t* mailbox) noexcept
{
	if(unlikely(!mailbox || mailbox->p))
		return EINVAL;

	try {
		mailbox->p = static_cast<void*>(new zth_mailbox_t_type());
		return 0;
	} catch(std::bad_alloc const&) {
		return ENOMEM;
	} catch(...) {
	}
	return EAGAIN;
}

/*!
 * \brief Destroys a mailbox.
 * \details This is a C-wrapper to delete a zth::Mailbox.
 * \ingroup zth_api_c_sync
 */
EXTERN_C ZTH_EXPORT ZTH_INLINE int zth_mailbox_destroy(zth_mailbox_t* mailbox) noexcept
{
	if(unlikely(!mailbox))
		return EINVAL;
	if(unlikely(!mailbox->p))
		// Already destroyed.
		return 0;

	delete static_cast<zth_mailbox_t_type*>(mailbox->p);
	mailbox->p = nullptr;
	return 0;
}

/*!
 * \brief Checks if a mailbox contains data to take.
 * \details This is a C-wrapper for zth::Mailbox::can_take().
 * \ingroup zth_api_c_sync
 */
EXTERN_C ZTH_EXPORT ZTH_INLINE int zth_mailbox_can_take(zth_mailbox_t* mailbox) noexcept
{
	if(unlikely(!mailbox || !mailbox->p))
		return EINVAL;

	return static_cast<zth_mailbox_t_type*>(mailbox->p)->can_take() ? 0 : EAGAIN;
}

/*!
 * \brief Checks if a mailbox is empty and can accept data.
 * \details This is a C-wrapper for zth::Mailbox::can_put().
 * \ingroup zth_api_c_sync
 */
EXTERN_C ZTH_EXPORT ZTH_INLINE int zth_mailbox_can_put(zth_mailbox_t* mailbox) noexcept
{
	if(unlikely(!mailbox || !mailbox->p))
		return EINVAL;

	return static_cast<zth_mailbox_t_type*>(mailbox->p)->can_put() ? 0 : EAGAIN;
}

/*!
 * \brief Puts a value into the mailbox.
 * \details This is a C-wrapper for zth::Mailbox::put(). It is blocking when there is already a
 *          value in the mailbox.
 * \ingroup zth_api_c_sync
 */
EXTERN_C ZTH_EXPORT ZTH_INLINE int zth_mailbox_put(zth_mailbox_t* mailbox, uintptr_t value) noexcept
{
	if(unlikely(!mailbox || !mailbox->p))
		return EINVAL;

	zth_mailbox_t_type* m = static_cast<zth_mailbox_t_type*>(mailbox->p);
	m->put(value);
	return 0;
}

/*!
 * \brief Wait for and return a mailbox's value.
 * \details This is a C-wrapper for zth::Mailbox::take().
 * \ingroup zth_api_c_sync
 */
EXTERN_C ZTH_EXPORT ZTH_INLINE int
zth_mailbox_take(zth_mailbox_t* __restrict__ mailbox, uintptr_t* __restrict__ value) noexcept
{
	if(unlikely(!mailbox || !mailbox->p || !value))
		return EINVAL;

	*value = static_cast<zth_mailbox_t_type*>(mailbox->p)->take();
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

typedef struct {
	void* p;
} zth_mailbox_t;

ZTH_EXPORT int zth_mailbox_init(zth_mailbox_t* mailbox);
ZTH_EXPORT int zth_mailbox_destroy(zth_mailbox_t* mailbox);
ZTH_EXPORT int zth_mailbox_can_take(zth_mailbox_t* mailbox);
ZTH_EXPORT int zth_mailbox_can_put(zth_mailbox_t* mailbox);
ZTH_EXPORT int zth_mailbox_put(zth_mailbox_t* mailbox, uintptr_t value);
ZTH_EXPORT int zth_mailbox_take(zth_mailbox_t* __restrict__ mailbox, uintptr_t* __restrict__ value);

#endif // !__cplusplus
#endif // ZTH_SYNC_H
