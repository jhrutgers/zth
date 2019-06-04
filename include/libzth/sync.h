#ifndef __ZTH_SYNC_H
#define __ZTH_SYNC_H
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
	class Synchronizer {
	public:
		Synchronizer() {}
		virtual ~Synchronizer() {}
		virtual char const* name() const { return "Synchronizer"; }

	protected:
		void block() {
			Worker* w = Worker::currentWorker();
			if(unlikely(!w))
				return;

			Fiber* f = w->currentFiber();
			if(unlikely(!f))
				return;

			zth_dbg(sync, "[%s (%p)] Block %s (%p)", name(), this, f->name().c_str(), f);
			w->release(*f);
			m_queue.push_back(*f);
			f->nap(Timestamp::null());
			w->schedule();
		}

		void unblockFirst() {
			Worker* w = Worker::currentWorker();
			if(unlikely(!w))
				return;

			if(m_queue.empty())
				return;

			Fiber& f = m_queue.front();
			zth_dbg(sync, "[%s (%p)] Unblock %s (%p)", name(), this, f.name().c_str(), &f);
			m_queue.pop_front();
			f.wakeup();
			w->add(&f);
		}

		void unblockAll() {
			Worker* w = Worker::currentWorker();
			if(unlikely(!w))
				return;

			zth_dbg(sync, "[%s (%p)] Unblock all", name(), this);

			while(!m_queue.empty()) {
				Fiber& f = m_queue.front();
				m_queue.pop_front();
				f.wakeup();
				w->add(&f);
			}
		}

	private:
		List<Fiber> m_queue;
	};

	class Mutex : public Synchronizer {
	public:
		Mutex() : m_locked() {}
		virtual ~Mutex() {}
		virtual char const* name() const { return "Mutex"; }

		void lock() {
			if(unlikely(m_locked))
				block();
			zth_assert(!m_locked);
			m_locked = true;
		}

		bool trylock() {
			if(m_locked)
				return false;
			m_locked = true;
			return true;
		}

		void unlock() {
			zth_assert(m_locked);
			m_locked = false;
			unblockFirst();
		}

	private:
		bool m_locked;
	};

	class Semaphore : public Synchronizer {
	public:
		Semaphore(size_t init = 0) : m_count(init) {} 
		virtual ~Semaphore() {}
		virtual char const* name() const { return "Semaphore"; }

		void acquire(size_t count = 1) {
			while(count > 0) {
				if(count <= m_count) {
					m_count -= count;
					return;
				} else {
					count -= m_count;
					m_count = 0;
					block();
				}
			}
		}

		void release(size_t count = 1) {
			if((m_count += count) > 0)
				unblockFirst();
		}

	private:
		size_t m_count;
	};

	class Signal : public Synchronizer {
	public:
		Signal() {}
		virtual ~Signal() {}
		virtual char const* name() const { return "Signal"; }
		void wait() { block(); }
		void signal() { unblockFirst(); }
		void signalAll() { unblockAll(); }
	};

	template <typename T>
	class Future : public Synchronizer {
	public:
		typedef T type;
		Future() : m_valid() {
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
		virtual char const* name() const { return "Future"; }

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
			unblockAll();
		}
		Future& operator=(type const& value) { set(value); return *this; }

		type& value() { wait(); return *reinterpret_cast<type*>(m_data); }
		type const& value() const { wait(); return *reinterpret_cast<type const*>(m_data); }
		operator type const&() const { return value(); }
		operator type&() { return value(); }
		type const* operator*() const { return &value(); }
		type* operator*() { return &value(); }
		type const* operator->() const { return &value(); }
		type* operator->() { return &value(); }
	private:
		char m_data[sizeof(type)];
		bool m_valid;
	};
	
	template <>
	class Future<void> : public Synchronizer {
	public:
		typedef void type;
		Future() : m_valid() {}
		virtual ~Future() {}
		virtual char const* name() const { return "Future"; }

		bool valid() const { return m_valid; }
		operator bool() const { return valid(); }

		void wait() { if(!valid()) block(); }
		void set() {
			zth_assert(!valid());
			if(valid())
				return;
			m_valid = true;
			unblockAll();
		}

	private:
		bool m_valid;
	};

} // namespace

#endif // __cplusplus
#endif // __ZTH_SYNC_H
