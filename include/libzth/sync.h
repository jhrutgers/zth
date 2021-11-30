#ifndef ZTH_SYNC_H
#define ZTH_SYNC_H
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

/*!
 * \defgroup zth_api_cpp_sync sync
 * \ingroup zth_api_cpp
 */
/*!
 * \defgroup zth_api_c_sync sync
 * \ingroup zth_api_c
 */

#ifdef __cplusplus

#include <libzth/list.h>
#include <libzth/fiber.h>
#include <libzth/worker.h>
#include <libzth/util.h>

#include <new>

#ifdef ZTH_USE_VALGRIND
#  include <valgrind/memcheck.h>
#endif

namespace zth {

	class RefCounted {
	public:
		RefCounted() : m_count() {}
		virtual ~RefCounted() {}

		void used() {
			zth_assert(m_count < std::numeric_limits<size_t>::max());
			m_count++;
		}

		void unused() {
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
		explicit SharedPointer(RefCounted* object = NULL) : m_object() { reset(object); }
		SharedPointer(SharedPointer const& p) : m_object() { *this = p; }
		virtual ~SharedPointer() { reset(); }

		void reset(RefCounted* object = NULL) {
			if(object)
				object->used();
			if(m_object)
				m_object->unused();
			m_object = object;
		}

		SharedPointer& operator=(RefCounted* object) { reset(object); return *this; }
		SharedPointer& operator=(SharedPointer const& p) { reset(p.get()); return *this; }

		T* get() const { return m_object ? static_cast<T*>(m_object) : NULL; }
		operator T*() const { return get(); }
		T* operator*() const { zth_assert(get()); return get(); }
		T* operator->() const { zth_assert(get()); return get(); }

		T* release() {
			T* object = get();
			m_object = NULL;
			return object;
		}

	private:
		RefCounted* m_object;
	};

	class Synchronizer : public RefCounted, public UniqueID<Synchronizer> {
	public:
		explicit Synchronizer(char const* name = "Synchronizer") : RefCounted(), UniqueID(Config::NamedSynchronizer ? name : NULL) {}
		virtual ~Synchronizer() {
			zth_dbg(sync, "[%s] Destruct", id_str());
			zth_assert(m_queue.empty());
		}

	protected:
		void block() {
			Worker* w;
			Fiber* f;
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
		bool unblock(Fiber& f) {
			if(!m_queue.contains(f))
				return false;

			Worker* w;
			getContext(&w, NULL);

			zth_dbg(sync, "[%s] Unblock %s", id_str(), f.id_str());
			m_queue.erase(f);
			f.wakeup();
			w->add(&f);
			return true;
		}

		bool unblockFirst() {
			if(m_queue.empty())
				return false;

			Worker* w;
			getContext(&w, NULL);

			Fiber& f = m_queue.front();
			zth_dbg(sync, "[%s] Unblock %s", id_str(), f.id_str());
			m_queue.pop_front();
			f.wakeup();
			w->add(&f);
			return true;
		}

		bool unblockAll() {
			if(m_queue.empty())
				return false;

			Worker* w;
			getContext(&w, NULL);

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
		public:
			typedef TimedWaitable base;
			AlarmClock(Synchronizer& synchronizer, Fiber& fiber, Timestamp const& timeout)
				: base(timeout)
				, m_synchronizer(synchronizer)
				, m_fiber(fiber)
				, m_rang()
			{}

			virtual ~AlarmClock() {}

			virtual bool poll(Timestamp const& now = Timestamp::now()) {
				if(!base::poll(now))
					return false;

				zth_dbg(sync, "[%s] %s timed out", m_synchronizer.id_str(), m_fiber.id_str());
				m_rang = m_synchronizer.unblock(m_fiber);
				return true;
			}

			bool rang() const { return m_rang; }

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
		bool block(Timestamp const& timeout, Timestamp const& now = Timestamp::now()) {
			if(timeout <= now)
				// Immediate timeout.
				return true;

			return block_(timeout, now);
		}

		/*!
		 * \brief Block, with timeout.
		 * \return \c true if unblocked by request, \c false when unblocked by timeout.
		 */
		bool block(TimeInterval const& timeout, Timestamp const& now = Timestamp::now()) {
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
		bool block_(Timestamp const& timeout, Timestamp const& now) {
			Worker* w;
			Fiber* f;
			getContext(&w, &f);

			zth_dbg(sync, "[%s] Block %s with timeout", id_str(), f->id_str());
			w->release(*f);
			m_queue.push_back(*f);
			f->nap(Timestamp::null());

			AlarmClock a(*this, *f, timeout);
			w->waiter().scheduleTask(a);
			w->schedule(NULL, now);

			w->waiter().unscheduleTask(a);
			return !a.rang();
		}

	private:
		List<Fiber> m_queue;
	};

	/*!
	 * \ingroup zth_api_cpp_sync
	 */
	class Mutex : public Synchronizer {
	public:
		explicit Mutex(char const* name = "Mutex") : Synchronizer(name), m_locked() {}
		virtual ~Mutex() {}

		void lock() {
			while(unlikely(m_locked))
				block();
			m_locked = true;
			zth_dbg(sync, "[%s] Locked", id_str());
		}

		bool trylock() {
			if(m_locked)
				return false;
			m_locked = true;
			zth_dbg(sync, "[%s] Locked", id_str());
			return true;
		}

		void unlock() {
			zth_assert(m_locked);
			zth_dbg(sync, "[%s] Unlocked", id_str());
			m_locked = false;
			unblockFirst();
		}

	private:
		bool m_locked;
	};

	/*!
	 * \ingroup zth_api_cpp_sync
	 */
	class Semaphore : public Synchronizer {
	public:
		Semaphore(size_t init = 0, char const* name = "Semaphore") : Synchronizer(name), m_count(init) {}
		virtual ~Semaphore() {}

		void acquire(size_t count = 1) {
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

		void release(size_t count = 1) {
			zth_assert(m_count + count >= m_count); // ...otherwise it wrapped around, which is probably not want you wanted...

			if(unlikely(m_count + count < m_count))
				// wrapped around, saturate
				m_count = std::numeric_limits<size_t>::max();
			else
				m_count += count;

			zth_dbg(sync, "[%s] Released %zu", id_str(), count);

			if(likely(m_count > 0))
				unblockFirst();
		}

		size_t value() const { return m_count; }

	private:
		size_t m_count;
	};

	/*!
	 * \ingroup zth_api_cpp_sync
	 */
	class Signal : public Synchronizer {
	public:
		explicit Signal(char const* name = "Signal") : Synchronizer(name) , m_signalled() {}
		virtual ~Signal() {}

		void wait() {
			if(!m_signalled)
				block();
			else
				// Do a yield() here, as one might rely on the signal to block
				// regularly when the signal is used in a loop (see daemon
				// pattern).
				yield();

			if(m_signalled > 0)
				m_signalled--;
		}

		bool wait(Timestamp const& timeout, Timestamp const& now = Timestamp::now()) {
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

		bool wait(TimeInterval const& timeout, Timestamp const& now = Timestamp::now()) {
			return wait(now + timeout);
		}

		void signal(bool queue = true, bool queueEveryTime = false) {
			zth_dbg(sync, "[%s] Signal", id_str());
			if(!unblockFirst() && queue && m_signalled >= 0) {
				if(m_signalled == 0 || queueEveryTime)
					m_signalled++;
				zth_assert(m_signalled > 0); // Otherwise, it wrapped around, which is probably not what you want.
			}
		}

		void signalAll(bool queue = true) {
			zth_dbg(sync, "[%s] Signal all", id_str());
			unblockAll();
			if(queue)
				m_signalled = -1;
		}

		void reset() {
			m_signalled = 0;
		}

	private:
		int m_signalled;
	};

	/*!
	 * \ingroup zth_api_cpp_sync
	 */
	template <typename T = void>
	class Future : public Synchronizer {
	public:
		typedef T type;
		// cppcheck-suppress uninitMemberVar
		explicit Future(char const* name = "Future") : Synchronizer(name), m_valid() {
#ifdef ZTH_USE_VALGRIND
			VALGRIND_MAKE_MEM_NOACCESS(m_data, sizeof(m_data));
#endif
		}
		virtual ~Future() {
			if(valid())
				value().~type();
#ifdef ZTH_USE_VALGRIND
			VALGRIND_MAKE_MEM_UNDEFINED(m_data, sizeof(m_data));
#endif
		}

		bool valid() const { return m_valid; }
		operator bool() const { return valid(); }

		void wait() { if(!valid()) block(); }
		void set(type const& value = type()) {
			zth_assert(!valid());
			if(valid())
				return;
#ifdef ZTH_USE_VALGRIND
			VALGRIND_MAKE_MEM_UNDEFINED(m_data, sizeof(m_data));
#endif
			new(m_data) type(value);
			m_valid = true;
			zth_dbg(sync, "[%s] Set", id_str());
			unblockAll();
		}
		Future& operator=(type const& value) { set(value); return *this; }

		type& value() { wait(); char* p = m_data; return *reinterpret_cast<type*>(p); }
		type const& value() const { wait(); char const* p = m_data; return *reinterpret_cast<type const*>(p); }
		operator type const&() const { return value(); }
		operator type&() { return value(); }
		type const* operator*() const { return &value(); }
		type* operator*() { return &value(); }
		type const* operator->() const { return &value(); }
		type* operator->() { return &value(); }
	private:
		char m_data[sizeof(type)] __attribute__((aligned(8)));
		bool m_valid;
	};

	template <>
	// cppcheck-suppress noConstructor
	class Future<void> : public Synchronizer {
	public:
		typedef void type;
		explicit Future(char const* name = "Future") : Synchronizer(name), m_valid() {}
		virtual ~Future() {}

		bool valid() const { return m_valid; }
		operator bool() const { return valid(); }

		void wait() { if(!valid()) block(); }
		void set() {
			zth_assert(!valid());
			if(valid())
				return;
			m_valid = true;
			zth_dbg(sync, "[%s] Set", id_str());
			unblockAll();
		}

	private:
		bool m_valid;
	};

	class Gate : public Synchronizer {
	public:
		explicit Gate(size_t count, char const* name = "Gate")
			: Synchronizer(name), m_count(count), m_current() {}
		virtual ~Gate() {}

		bool pass() {
			zth_dbg(sync, "[%s] Pass", id_str());
			if(++m_current >= count()) {
				m_current -= count();
				unblockAll();
				return true;
			} else {
				return false;
			}
		}

		void wait() {
			if(!pass())
				block();
		}

		size_t count() const { return m_count; }
		size_t current() const { return m_current; }

	private:
		size_t const m_count;
		size_t m_current;
	};

} // namespace

struct zth_mutex_t { void* p; };

/*!
 * \brief Initializes a mutex.
 * \details This is a C-wrapper to create a new zth::Mutex.
 * \ingroup zth_api_c_sync
 */
EXTERN_C ZTH_EXPORT ZTH_INLINE int zth_mutex_init(zth_mutex_t* mutex) {
	if(unlikely(!mutex || mutex->p))
		return EINVAL;

	mutex->p = (void*)new zth::Mutex();
	return 0;
}

/*!
 * \brief Destroys a mutex.
 * \details This is a C-wrapper to delete a zth::Mutex.
 * \ingroup zth_api_c_sync
 */
EXTERN_C ZTH_EXPORT ZTH_INLINE int zth_mutex_destroy(zth_mutex_t* mutex) {
	if(unlikely(!mutex))
		return EINVAL;
	// cppcheck-suppress nullPointerRedundantCheck
	if(unlikely(!mutex->p))
		// Already destroyed.
		return 0;

	delete reinterpret_cast<zth::Mutex*>(mutex->p);
	mutex->p = NULL;
	return 0;
}

/*!
 * \brief Locks a mutex.
 * \details This is a C-wrapper for zth::Mutex::lock().
 * \ingroup zth_api_c_sync
 */
EXTERN_C ZTH_EXPORT ZTH_INLINE int zth_mutex_lock(zth_mutex_t* mutex) {
	if(unlikely(!mutex || !mutex->p))
		return EINVAL;

	reinterpret_cast<zth::Mutex*>(mutex->p)->lock();
	return 0;
}

/*!
 * \brief Try to lock a mutex.
 * \details This is a C-wrapper for zth::Mutex::trylock().
 * \ingroup zth_api_c_sync
 */
EXTERN_C ZTH_EXPORT ZTH_INLINE int zth_mutex_trylock(zth_mutex_t* mutex) {
	if(unlikely(!mutex || !mutex->p))
		return EINVAL;

	return reinterpret_cast<zth::Mutex*>(mutex->p)->trylock() ? 0 : EBUSY;
}

/*!
 * \brief Unlock a mutex.
 * \details This is a C-wrapper for zth::Mutex::unlock().
 * \ingroup zth_api_c_sync
 */
EXTERN_C ZTH_EXPORT ZTH_INLINE int zth_mutex_unlock(zth_mutex_t* mutex) {
	if(unlikely(!mutex || !mutex->p))
		return EINVAL;

	reinterpret_cast<zth::Mutex*>(mutex->p)->unlock();
	return 0;
}

struct zth_sem_t { void* p; };

/*!
 * \brief Initializes a semaphore.
 * \details This is a C-wrapper to create a new zth::Semaphore.
 * \ingroup zth_api_c_sync
 */
EXTERN_C ZTH_EXPORT ZTH_INLINE int zth_sem_init(zth_sem_t* sem, size_t value) {
	if(unlikely(!sem || sem->p))
		return EINVAL;

	sem->p = (void*)new zth::Semaphore(value);
	return 0;
}

/*!
 * \brief Destroys a semaphore.
 * \details This is a C-wrapper to delete a zth::Semaphore.
 * \ingroup zth_api_c_sync
 */
EXTERN_C ZTH_EXPORT ZTH_INLINE int zth_sem_destroy(zth_sem_t* sem) {
	if(unlikely(!sem))
		return EINVAL;
	// cppcheck-suppress nullPointerRedundantCheck
	if(unlikely(!sem->p))
		// Already destroyed.
		return 0;

	delete reinterpret_cast<zth::Semaphore*>(sem->p);
	sem->p = NULL;
	return 0;
}

/*!
 * \brief Returns the value of a semaphore.
 * \details This is a C-wrapper for zth::Semaphore::value().
 * \ingroup zth_api_c_sync
 */
EXTERN_C ZTH_EXPORT ZTH_INLINE int zth_sem_getvalue(zth_sem_t *__restrict__ sem, size_t *__restrict__ value) {
	if(unlikely(!sem || !sem->p || !value))
		return EINVAL;

	*value = reinterpret_cast<zth::Semaphore*>(sem->p)->value();
	return 0;
}

#ifndef EOVERFLOW
#  define EOVERFLOW EAGAIN
#endif

/*!
 * \brief Increments a semaphore.
 * \details This is a C-wrapper for zth::Mutex::release() of 1.
 * \ingroup zth_api_c_sync
 */
EXTERN_C ZTH_EXPORT ZTH_INLINE int zth_sem_post(zth_sem_t* sem) {
	if(unlikely(!sem || !sem->p))
		return EINVAL;

	zth::Semaphore* s = reinterpret_cast<zth::Semaphore*>(sem->p);
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
EXTERN_C ZTH_EXPORT ZTH_INLINE int zth_sem_wait(zth_sem_t* sem) {
	if(unlikely(!sem || !sem->p))
		return EINVAL;

	reinterpret_cast<zth::Semaphore*>(sem->p)->acquire();
	return 0;
}

/*!
 * \brief Try to decrement a semaphore.
 * \details This is a C-wrapper based on zth::Mutex::acquire().
 * \ingroup zth_api_c_sync
 */
EXTERN_C ZTH_EXPORT ZTH_INLINE int zth_sem_trywait(zth_sem_t* sem) {
	if(unlikely(!sem || !sem->p))
		return EINVAL;

	zth::Semaphore* s = reinterpret_cast<zth::Semaphore*>(sem->p);
	if(unlikely(s->value() == 0))
		return EAGAIN;

	s->acquire();
	return 0;
}

struct zth_cond_t { void* p; };

/*!
 * \brief Initializes a condition.
 * \details This is a C-wrapper to create a new zth::Signal.
 * \ingroup zth_api_c_sync
 */
EXTERN_C ZTH_EXPORT ZTH_INLINE int zth_cond_init(zth_cond_t* cond) {
	if(unlikely(!cond || cond->p))
		return EINVAL;

	cond->p = (void*)new zth::Signal();
	return 0;
}

/*!
 * \brief Destroys a condition.
 * \details This is a C-wrapper to delete a zth::Signal.
 * \ingroup zth_api_c_sync
 */
EXTERN_C ZTH_EXPORT ZTH_INLINE int zth_cond_destroy(zth_cond_t* cond) {
	if(unlikely(!cond))
		return EINVAL;
	// cppcheck-suppress nullPointerRedundantCheck
	if(unlikely(!cond->p))
		// Already destroyed.
		return 0;

	delete reinterpret_cast<zth::Signal*>(cond->p);
	cond->p = NULL;
	return 0;
}

/*!
 * \brief Signals one fiber waiting for the condition.
 * \details This is a C-wrapper for zth::Signal::signal().
 * \ingroup zth_api_c_sync
 */
EXTERN_C ZTH_EXPORT ZTH_INLINE int zth_cond_signal(zth_cond_t* cond) {
	if(unlikely(!cond || !cond->p))
		return EINVAL;

	reinterpret_cast<zth::Signal*>(cond->p)->signal();
	return 0;
}

/*!
 * \brief Signals all fibers waiting for the condition.
 * \details This is a C-wrapper for zth::Signal::signalAll().
 * \ingroup zth_api_c_sync
 */
EXTERN_C ZTH_EXPORT ZTH_INLINE int zth_cond_broadcast(zth_cond_t* cond) {
	if(unlikely(!cond || !cond->p))
		return EINVAL;

	reinterpret_cast<zth::Signal*>(cond->p)->signalAll();
	return 0;
}

/*!
 * \brief Wait for a condition.
 * \details This is a C-wrapper for zth::Signal::wait().
 * \ingroup zth_api_c_sync
 */
EXTERN_C ZTH_EXPORT ZTH_INLINE int zth_cond_wait(zth_cond_t* cond) {
	if(unlikely(!cond || !cond->p))
		return EINVAL;

	reinterpret_cast<zth::Signal*>(cond->p)->wait();
	return 0;
}

struct zth_future_t { void* p; };
typedef zth::Future<uintptr_t> zth_future_t_type;

/*!
 * \brief Initializes a future.
 * \details This is a C-wrapper to create a new zth::Future.
 * \ingroup zth_api_c_sync
 */
EXTERN_C ZTH_EXPORT ZTH_INLINE int zth_future_init(zth_future_t* future) {
	if(unlikely(!future || future->p))
		return EINVAL;

	future->p = (void*)new zth_future_t_type();
	return 0;
}

/*!
 * \brief Destroys a future.
 * \details This is a C-wrapper to delete a zth::Future.
 * \ingroup zth_api_c_sync
 */
EXTERN_C ZTH_EXPORT ZTH_INLINE int zth_future_destroy(zth_future_t* future) {
	if(unlikely(!future))
		return EINVAL;
	// cppcheck-suppress nullPointerRedundantCheck
	if(unlikely(!future->p))
		// Already destroyed.
		return 0;

	delete reinterpret_cast<zth_future_t_type*>(future->p);
	future->p = NULL;
	return 0;
}

/*!
 * \brief Checks if a future was already set.
 * \details This is a C-wrapper for zth::Future::valid().
 * \ingroup zth_api_c_sync
 */
EXTERN_C ZTH_EXPORT ZTH_INLINE int zth_future_valid(zth_future_t* future) {
	if(unlikely(!future || !future->p))
		return EINVAL;

	return reinterpret_cast<zth_future_t_type*>(future->p)->valid() ? 0 : EAGAIN;
}

/*!
 * \brief Sets a future and signals all waiting fibers.
 * \details This is a C-wrapper for zth::Future::set().
 * \ingroup zth_api_c_sync
 */
EXTERN_C ZTH_EXPORT ZTH_INLINE int zth_future_set(zth_future_t* future, uintptr_t value) {
	if(unlikely(!future || !future->p))
		return EINVAL;

	zth_future_t_type* f = reinterpret_cast<zth_future_t_type*>(future->p);
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
EXTERN_C ZTH_EXPORT ZTH_INLINE int zth_future_get(zth_future_t *__restrict__ future, uintptr_t *__restrict__ value) {
	if(unlikely(!future || !future->p || !value))
		return EINVAL;

	*value = reinterpret_cast<zth_future_t_type*>(future->p)->value();
	return 0;
}

/*!
 * \brief Wait for a future.
 * \details This is a C-wrapper for zth::Future::wait().
 * \ingroup zth_api_c_sync
 */
EXTERN_C ZTH_EXPORT ZTH_INLINE int zth_future_wait(zth_future_t* future) {
	if(unlikely(!future || !future->p))
		return EINVAL;

	reinterpret_cast<zth_future_t_type*>(future->p)->wait();
	return 0;
}

struct zth_gate_t { void* p; };

/*!
 * \brief Initializes a gate.
 * \details If all participants call #zth_gate_wait(), the gate behaves the same as a \c pthread_barrier_t would.
 * \details This is a C-wrapper to create a new zth::Gate.
 * \ingroup zth_api_c_sync
 */
EXTERN_C ZTH_EXPORT ZTH_INLINE int zth_gate_init(zth_gate_t* gate, size_t count) {
	if(unlikely(!gate || gate->p))
		return EINVAL;

	gate->p = (void*)new zth::Gate(count);
	return 0;
}

/*!
 * \brief Destroys a gate.
 * \details This is a C-wrapper to delete a zth::Gate.
 * \ingroup zth_api_c_sync
 */
EXTERN_C ZTH_EXPORT ZTH_INLINE int zth_gate_destroy(zth_gate_t* gate) {
	if(unlikely(!gate))
		return EINVAL;
	// cppcheck-suppress nullPointerRedundantCheck
	if(unlikely(!gate->p))
		// Already destroyed.
		return 0;

	delete reinterpret_cast<zth::Gate*>(gate->p);
	gate->p = NULL;
	return 0;
}

/*!
 * \brief Passes a gate.
 * \details This is a C-wrapper for zth::Gate::pass().
 * \ingroup zth_api_c_sync
 */
EXTERN_C ZTH_EXPORT ZTH_INLINE int zth_gate_pass(zth_gate_t* gate) {
	if(unlikely(!gate || !gate->p))
		return EINVAL;

	return reinterpret_cast<zth::Gate*>(gate->p)->pass() ? 0 : EBUSY;
}

/*!
 * \brief Wait for a gate.
 * \details This is a C-wrapper for zth::Gate::wait().
 * \ingroup zth_api_c_sync
 */
EXTERN_C ZTH_EXPORT ZTH_INLINE int zth_gate_wait(zth_gate_t* gate) {
	if(unlikely(!gate || !gate->p))
		return EINVAL;

	reinterpret_cast<zth::Gate*>(gate->p)->wait();
	return 0;
}

#else // !__cplusplus

typedef struct { void* p; } zth_mutex_t;
ZTH_EXPORT int zth_mutex_init(zth_mutex_t* mutex);
ZTH_EXPORT int zth_mutex_destroy(zth_mutex_t* mutex);
ZTH_EXPORT int zth_mutex_lock(zth_mutex_t* mutex);
ZTH_EXPORT int zth_mutex_trylock(zth_mutex_t* mutex);
ZTH_EXPORT int zth_mutex_unlock(zth_mutex_t* mutex);

typedef struct { void* p; } zth_sem_t;
ZTH_EXPORT int zth_sem_init(zth_sem_t* sem, size_t value);
ZTH_EXPORT int zth_sem_destroy(zth_sem_t* sem);
ZTH_EXPORT int zth_sem_getvalue(zth_sem_t *__restrict__ sem, size_t *__restrict__ value);
ZTH_EXPORT int zth_sem_post(zth_sem_t* sem);
ZTH_EXPORT int zth_sem_wait(zth_sem_t* sem);
ZTH_EXPORT int zth_sem_trywait(zth_sem_t* sem);

typedef struct { void* p; } zth_cond_t;
ZTH_EXPORT int zth_cond_init(zth_cond_t* cond);
ZTH_EXPORT int zth_cond_destroy(zth_cond_t* cond);
ZTH_EXPORT int zth_cond_signal(zth_cond_t* cond);
ZTH_EXPORT int zth_cond_broadcast(zth_cond_t* cond);
ZTH_EXPORT int zth_cond_wait(zth_cond_t* cond);

typedef struct { void* p; } zth_future_t;
ZTH_EXPORT int zth_future_init(zth_future_t* future);
ZTH_EXPORT int zth_future_destroy(zth_future_t* future);
ZTH_EXPORT int zth_future_valid(zth_future_t* future);
ZTH_EXPORT int zth_future_set(zth_future_t* future, uintptr_t value);
ZTH_EXPORT int zth_future_get(zth_future_t *__restrict__ future, uintptr_t *__restrict__ value);
ZTH_EXPORT int zth_future_wait(zth_future_t* future);

typedef struct { void* p; } zth_gate_t;
ZTH_EXPORT int zth_gate_init(zth_gate_t* gate, size_t count);
ZTH_EXPORT int zth_gate_destroy(zth_gate_t* gate);
ZTH_EXPORT int zth_gate_pass(zth_gate_t* gate);
ZTH_EXPORT int zth_gate_wait(zth_gate_t* gate);

#endif // __cplusplus
#endif // ZTH_SYNC_H
