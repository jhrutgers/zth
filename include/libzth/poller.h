#ifndef ZTH_POLLER_H
#define ZTH_POLLER_H
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
 * \defgroup zth_api_cpp_poller poller
 * \ingroup zth_api_cpp
 * \brief Fiber-aware poller
 *
 * In order to wait for multiple file descriptors, one could use \c poll().
 * However, when \c poll() suspends, it suspends not only the current fiber,
 * but the whole application.
 *
 * Zth's poller implementation is a fiber-aware, extendable poller
 * implementation, that uses either \c zmq_poll() or the OS's \c poll().
 * However, it has the following limitation: in Windows, only WSA2 or ZeroMQ
 * sockets can be polled; the provided file descriptor must be one of those.
 * Other fds, like a normal file or stdin, are not supported.  For POSIX-like
 * systems, any fd can be used.
 */

#include <libzth/macros.h>

#ifdef __cplusplus

#include <libzth/worker.h>

#include <memory>
#include <map>
#include <vector>

#if __cplusplus >= 201103L
#  include <initializer_list>
#endif

namespace zth {

	//////////////////////////////////////////////
	// Pollable
	//

	/*!
	 * \brief A pollable thing.
	 *
	 * Only file descriptors (or even only sockets in Windows) are supported by
	 * Zth.  Use #zth::PollableFd instead.  The generic Pollable class exists
	 * for extending by your application.
	 *
	 * If you implement more Pollables, you must provide and register your own
	 * PollerServer.
	 *
	 * \ingroup zth_api_cpp_poller
	 */
	struct Pollable {
	public:
		enum Events { PollIn = 1, PollOut = 2, PollErr = 4, PollPri = 8, PollHup = 16 };

		void* user_data;
		Events events;
		Events revents;
	};

	/*!
	 * \brief A pollable file descriptor.
	 *
	 * In Windows, the fd must be either a WSA2 or ZeroMQ socket.
	 *
	 * \ingroup zth_api_cpp_poller
	 */
	struct PollableFd : public Pollable {
		int fd;
	};



	//////////////////////////////////////////////
	// Poller base classes
	//

	/*!
	 * \brief Abstract base class of a poller.
	 * \tparam Allocator the allocator type to use for dynamic memory allocations
	 * \ingroup zth_api_cpp_poller
	 */
	template <template <typename> typename Allocator = allocator_traits>
	class PollerBase {
	public:
		/*!
		 * \brief Result of #poll().
		 */
		typedef std::vector<Pollable*, Allocator<Pollable*>::allocator_type> Result;

		/*!
		 * \brief Dtor.
		 */
		virtual ~PollerBase() is_default

		/*!
		 * \brief Add a pollable object.
		 *
		 * Once a pollable is added, do not modify its properties, except for
		 * \c user_data.
		 *
		 * \return 0 on success, otherwise an errno
		 */
		virtual int add(Pollable& p) = 0;

		/*!
		 * \brief Remove a pollable object.
		 * \return 0 on success, otherwise an errno
		 */
		virtual int remove(Pollable& p) noexcept = 0;

		/*!
		 * \brief Poll.
		 *
		 * When \p timeout_ms is -1, \c poll() blocks until at least one of the
		 * pollables got an event.  When \p timeout_ms is 0, \c poll() never
		 * blocks.  Otherwise, \c poll() blocks at most for \p timeout_ms and
		 * return with a timeout, or return earlier when a pollable got an
		 * event.
		 *
		 * \return The registered pollables that have an event set.
		 *         If the result is empty, \c errno is set to the error.
		 */
		virtual Result const& poll(int timeout_ms) noexcept = 0;
	};

	/*!
	 * \brief Base class of the actual %Poller to be used by fibers.
	 *
	 * This client connects to a PollerServer, which is part of the Waiter.
	 */
	template <template <typename> typename Allocator = allocator_traits>
	class PollerClientBase : public PollerBase<Allocator> {
	public:
		/*!
		 * \brief Indicate that the given pollable got an event.
		 */
		virtual void event(Pollable& p) noexcept = 0;
	};



	//////////////////////////////////////////////
	// PollerServers
	//

	/*!
	 * \brief %Poller to be executed by the Waiter.
	 *
	 * Per-fiber Poller clients can forward their poll request to the server.
	 *
	 * \c %poll() typically uses an array of things to do the actual poll on,
	 * such as \c struct \c pollfd. This class manages the administration of
	 * Pollables to these things.
	 *
	 * The subclass must implement #init() to initialize this thing, and
	 * #doPoll() to perform the actual poll.
	 *
	 * \tparam Item_ the item to be used as the actual poll thing
	 * \tparam Allocator the allocator type to use for dynamic memory allocations
	 */
	template <typename PollItem_, template <typename> typename Allocator = allocator_traits>
	class PollerServer : public PollerBase<Allocator> {
	public:
		typedef PollerBase<Allocator> base;
		typedef PollerClientBase<Allocator> Client;
		using typename base::Result;

	protected:
		typedef PollItem_ PollItem;
		typedef std::vector<PollItem, Allocator<PollItem>::allocator_type> PollItemList;

	private:
		/*!
		 * \brief Meta data for a Pollable.
		 */
		struct MetaItem {
			Pollable* pollable;
			Client* client;
		};

		typedef std::vector<MetaItem, Allocator<MetaItem>::allocator_type> MetaItemList;

		/*!
		 * \brief Initialize a PollItem, given a Pollable.
		 * \return 0 on success, otherwise an errno
		 */
		virtual int init(Pollable const& p, PollItem& item) noexcept = 0;

		/*!
		 * \brief Do the actual poll.
		 *
		 * For every element in \c items, the #event() should be called if
		 * there are \c revents.
		 *
		 * \param timeout_ms the timeout for the polling
		 * \param meta the list of MetaItems, of which the indices matches with the \p items
		 * \param items the items, as initialized by #init().
		 * \return 0 on success, otherwise an errno
		 */
		virtual int doPoll(int timeout_ms, PollItemList& items) noexcept = 0;

	protected:
		/*!
		 * \brief Register an \c revents for the PollItem at the given index.
		 *
		 * This function should be called by #doPoll().
		 */
		void event(Pollable::Events revents, size_t index) noexcept
		{
			zth_assert(index < m_metaItems.size());

			MetaItem const& m = m_metaItems[i];
			m.pollable->revents = revents;

			if(revents) {
				m_result.push_back(m.pollable);
				if(m.client)
					m.client->event(*m.pollable);
			}
		}

	public:
		virtual ~PollerServer() override is_default

		template <template <typename> typename A>
		int migrateTo(PollerBase<A>& p)
		{
			p.reserve(m_metaItems.size());
			size_t added = 0;

			for(size_t i = 0; i < m_metaItems.size(); i++) {
				MetaItem const* m = m_metaItems[i];

				int res = p.add(m.pollable, m.client);
				if(res) {
					// Rollback
					p.remove(added);
					return res;
				}

				added++;
			}

			clear();
			return 0;
		}

#if __cplusplus >= 201103L
		PollerServer(PollerServer const&) = delete;
		void operator=(PollerServer const&) = delete;
		PollerServer(PollerServer&& p) = delete;
		void operator=(PollerServer&& p) = delete;
#else
	private:
		PollerServer(PollerServer const&);
		void operator=(PollerServer const&);
	public:
#endif

		/*!
		 * \brief Reserve storage for the given number of additional Pollables to handle.
		 */
		void reserve(size_t more)
		{
			m_metaItems.reserve(m_metaItems.size() + more);
			m_pollItems.reserve(m_metaItems.capacity());
			m_result.reserve(m_metaItems.capacity());
		}

		/*!
		 * \brief Add a Pollable, that belongs to a given client.
		 *
		 * \p client can be \c nullptr.
		 *
		 * \return 0 on success, otherwise an errno
		 */
		int add(Pollable& p, Client* client)
		{
			__try {
				MetaItem m = { &p, client };
				m_metaItems.push_back(m);
			} __catch(...) {
				return ENOMEM;
			}

			__try {
				// Reserve space, but do not initialize yet.
				m_pollItems.reserve(m_metaItems.size());
				m_result.reserve(m_metaItems.size());
			} __catch(...) {
				// Rollback.
				m_metaItems.pop_back();
				return ENOMEM;
			}

			return 0;
		}

		int add(Pollable& p) final
		{
			return add(p, nullptr);
		}

		/*!
		 * \brief Remove the given Pollable, belonging to the given client.
		 *
		 * \p client must match the one that was specified during #add().
		 *
		 * \return 0 on success, otherwise an errno
		 */
		int remove(Pollable& p, Client* client) noexcept
		{
			size_t count = m_pollableItems.size();

			size_t i;
			for(i = count; i > 0; i--) {
				MetaItem& m = m_metaItems[i - 1u];
				if(m.pollable == &p && m.client == client)
					break;
			}

			if(i == 0u)
				return ESRCH;

			i--;
			int res = 0;

			if(i < count - 1u) {
				// Not removing the last element, fill the gap.
#ifdef __cplusplus >= 201103L
				m_metaItems[i] = std::move(m_metaItems[count - 1u]);
#else
				m_metaItems[i] = m_metaItems[count - 1u];
#endif
				if(i < m_pollItems.size())
					res = init(*m_metaItems[i].pollable, m_pollItems[i]);
			}

			// Drop the last element.
			remove(1u);
			return res;
		}

		/*!
		 * \brief Remove the last added Pollables.
		 */
		void remove(size_t last) noexcept
		{
			if(last >= m_metaItems.size()) {
				clear();
			} else {
				m_metaItems.resize(m_metaItems.size() - last);
				if(m_pollItems.size() > m_metaItems.size())
					m_pollItems.resize(m_metaItems.size());
			}
		}

		int remove(Pollable& p) noexcept final
		{
			return remove(p, nullptr);
		}

		/*!
		 * \brief Clear all Pollables.
		 *
		 * It does not release the allocated memory.
		 */
		void clear() noexcept
		{
			m_metaItems.clear();
			m_pollItems.clear();
		}

		virtual Result const& poll(int timeout_ms) noexcept override
		{
			m_result.clear();
			if((errno = mirror()))
				return m_result;

			if(m_metaItems.empty()) {
				errno = EINVAL;
				return m_result;
			}

			int res = doPoll(timeout_ms, m_pollItems);
			// m_result should be populated now by calls to event() by doPoll().

			if(m_result.empty() && !res)
				res = EAGAIN;

			errno = res;
			return m_result;
		}

	protected:
		/*!
		 * \brief Make sure all PollItems are initialized.
		 * \return 0 on success, otherwise an errno
		 * \see #m_pollItems.
		 */
		int mirror() noexcept
		{
			int res = 0;

			size_t doInit = m_pollItems.size();
			m_pollItems.resize(m_metaItems.size());

			for(size_t i = doInit; i < m_metaItems.size(); i++)
				if((res = init(*m_metaItems[i].pollable, m_pollItems[i])))
					break;

			return 0;
		}

	private:
		/*!
		 * \brief All registered Pollables.
		 */
		MetaItemList m_metaItems;

		/*!
		 * \brief All items used by #doPoll().
		 *
		 * This list is lazily initialized. All entries in this list are in
		 * sync with #m_metaItems, but the list may be shorter than
		 * #m_metaItems.  #mirror() fixes that.
		 */
		PollItemList m_pollItems;

		/*!
		 * \brief The list of Pollables that have events to be processed.
		 *
		 * This is a subset of #m_metaItems.
		 */
		Result m_result;
	};


#ifdef ZTH_HAVE_LIBZMQ
	// By default, use zmq_poll(). It allows polling everything poll() can do,
	// including all ZeroMQ sockets.

	/*!
	 * \brief A PollerServer that uses \c zmq_poll().
	 */
	template <template <typename> typename Allocator = allocator_traits>
	class ZmqPoller : public PollerServer<zmq_pollitem_t, Allocator> {
	public:
		typedef PollerServer<zmq_pollitem_t, Allocator> base;

		virtual ~ZmqPoller() override is_default

	private:
		virtual int init(Pollable const& p, zmq_pollitem_t& item) noexcept override
		{
			// Zth only has PollableFds, so this is safe.
			PollableFd const& pfd = static_cast<PollableFd const&>(p);

			item.socket = nullptr;
			item.fd = pfd.fd;
			item.events = pfd.events;
			return 0;
		}

		virtual int doPoll(int timeout_ms, base::PollItemList& items) noexcept override
		{
			int res = zmq_poll(&items[0], items.size(), timeout_ms);
			if(res < 0)
				return errno;
			if(res == 0)
				// Timeout.
				return 0;

			for(size_t i = 0; res > 0 && i < items.size(); i++) {
				zmq_pollitem_t const& item = items[i];
				Pollable::Events revents = 0;

				if(item.revents) {
					res--;

					if(item.revents & ZMQ_POLLIN)
						revents |= Pollable::PollIn;
					if(item.revents & ZMQ_POLLOUT)
						revents |= Pollable::PollOut;
					if(item.revents & ZMQ_POLLERR)
						revents |= Pollable::PollErr;
				}

				event(revents, i);
			}

			return 0;
		}
	};

	typedef ZmqPoller<> DefaultPoller;

#elif defined(ZTH_HAVE_POLL)
	// If we don't have ZeroMQ, use the OS's poll() instead.

	/*!
	 * \brief A PollerServer that uses the OS's \c poll().
	 */
	template <template <typename> typename Allocator = allocator_traits>
	class PollPoller : public PollerServer<struct pollfd, Allocator> {
	public:
		typedef PollerServer<struct pollfd, Allocator> base;

		virtual ~PollPoller() override is_default

	private:
		virtual int init(Pollable const& p, struct pollfd& item) noexcept override
		{
			// Zth only has PollableFds, so this is safe.
			PollableFd const& pfd = static_cast<PollableFd const&>(p);

			item.fd = p.fd;
			item.events = p.events;
			return 0;
		}

		virtual int doPoll(int timeout_ms, base::PollItemsList& items) noexcept override
		{
			int res = ::poll(&items[0], items.size(), timeout_ms);
			if(res < 0)
				return errno;
			if(res == 0)
				// Timeout.
				return 0;

			for(size_t i = 0; res > 0 && i < items.size(); i++) {
				struct pollfd const& item = items[i];
				Pollable::Events revents = 0;

				if(item.revents) {
					res--;

					if(item.revents & POLLIN)
						revents |= Pollable::PollIn;
					if(item.revents & POLLOUT)
						revents |= Pollable::PollOut;
					if(item.revents & POLLERR)
						revents |= Pollable::PollErr;
					if(item.revents & POLLPRI)
						revents |= Pollable::PollPri;
					if(item.revents & POLLHUP)
						revents |= Pollable::PollHup;
				}

				event(revents, i);
			}

			return 0;
		}
	};

	typedef PollPoller<> DefaultPoller;

#else
	// No poller available.
	// Do provide a PollerServer, but it can only return an error upon a poll.

	/*!
	 * \brief Dummy poller that cannot poll.
	 */
	template <template <typename> typename Allocator = allocator_traits>
	class NoPoller : public PollerServer<int, Allocator> {
	public:
		typedef PollerServer<int, Allocator> base;

		virtual ~NoPoller() override is_default

	protected:
		virtual int init(Pollable& UNUSED_PAR(p), int& UNUSED_PAR(item)) override
		{
			return EINVAL;
		}

		virtual int doPoll(int timeout_ms, base::PollItemsList& items) noexcept override
		{
			return ENOSYS;
		}
	};

	typedef NoPoller<> DefaultPoller;
#endif // No poller





	//////////////////////////////////////////////
	// PollerClient
	//

	/*!
	 * \brief The poller to be used by a fiber.
	 *
	 * All poll requests are forwarded to the PollServer, as managed by the
	 * current Worker's Waiter.
	 */
	template <template <typename> typename Allocator = allocator_traits>
	class PollerClient : public PollerClientBase<Allocator> {
		typedef std::set<Pollable*, Allocator<Pollable*>::allocator_type> Pollables;

	public:
		typedef PollerClientBase<Allocator> base;
		using base::Result;

		constexpr PollerBase() noexcept
			: m_fiber()
		{}

#if __cplusplus >= 201103L
		Poller(std::initializer_list<Pollable&> l)
			: m_fiber()
		{
			m_pollables.insert(l);
			m_result.reserve(m_pollables.size());
		}
#endif

		virtual ~PollerClient() override is_default

		virtual int add(Pollable& p) override
		{
			__try {
				m_result.reserve(m_pollables.size() + 1u);
				return m_pollables.insert(&p).second ? 0 : EALREADY;
			} __catch(...) {
				return ENOMEM;
			}
		}

		virtual int remove(Pollable& p) noexcept override
		{
			return m_pollables.erase(&p) ? 0 : EAGAIN;
		}

		virtual Result const& poll(int timeout_ms) noexcept override
		{
			m_result.clear();

			if(m_pollables.empty()) {
				errno = EINVAL;
				return m_result;
			}

			PollerBase<Allocator>* p = currentWorker().waiter().poller();
			if(!p) {
				errno = ESRCH;
				return m_result;
			}

			m_fiber = &currentFiber();

			// Add all our pollables to the server.
			for(Pollables::iterator it = m_pollables.begin(); it != m_pollables.end(); ++it)
				p->add(**it, this);

			// Go do the poll.
			if(timeout_ms == 0) {
				// Do it immediately, in the current fiber's context.
				p->poll(0);
			} else if(timeout_ms < 0) {
				// Wait for the event callback.
				suspend();
			} else {
				TimedWaitable w(Timestamp::now() + TimeInterval(timeout_ms / 1000L, (timeout_ms * 1000000L) % 1000000000L));
				w.setFiber(m_fiber);
				scheduleTask(w);
				// Wait for the event callback or a timeout.
				suspend();
				unscheduleTask(w);
			}

			// Remove our pollables from the server.
			for(Pollables::iterator it = m_pollables.rbegin(); it != m_pollables.rend(); ++it)
				p->remove(**it, this);

			// m_result got populated by event().
			errno = m_result.empty() ? EAGAIN : 0;
			return m_result;
		}

		virtual void event(Pollable& p) noexcept override
		{
			m_result.push_back(&p);

			zth_assert(m_fiber);
			resume(*m_fiber);
		}

	private:
		/*!
		 * \brief The fiber that waits for this Poller.
		 */
		Fiber* m_fiber;

		/*!
		 * \brief All %Pollables to poll.
		 */
		Pollables m_pollables;

		/*!
		 * \brief The poll() result, populated by event().
		 */
		Result m_result;
	};

	// The default Poller to use.
	typedef PollerClient<> Poller;
} // namespace
#endif // __cplusplus
#endif // ZTH_POLLER_H
