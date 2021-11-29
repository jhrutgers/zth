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
#include <libzth/waiter.h>

#include <bitset>
#include <memory>
#include <map>
#include <set>
#include <stdexcept>
#include <vector>

#if __cplusplus >= 201103L
#  include <functional>
#  include <initializer_list>
#endif

#if defined(ZTH_HAVE_POLL) || defined(ZTH_HAVE_LIBZMQ)
#  define ZTH_HAVE_POLLER
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
		enum EventsFlags {
			PollInIndex, PollOutIndex, PollErrIndex, PollPriIndex, PollHupIndex, FlagCount,

			PollIn = 1U << PollInIndex, PollOut = 1U << PollOutIndex,
			PollErr = 1U << PollErrIndex, PollPri = 1U << PollPriIndex, PollHup = 1U << PollHupIndex,
		};

		typedef std::bitset<FlagCount> Events;

		constexpr explicit Pollable(Events e, void* user = nullptr) noexcept
			: user_data(user), events(e), revents()
		{}

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
		constexpr PollableFd(int fd, Events e, void* user = nullptr) noexcept
			: Pollable(e, user)
#ifdef ZTH_HAVE_LIBZMQ
			, socket()
#endif
			, fd(fd)
		{}

#ifdef ZTH_HAVE_LIBZMQ
		constexpr PollableFd(void* socket, Events e, void* user = nullptr) noexcept
			: Pollable(e, user), socket(socket), fd()
		{}

		void* socket;
#endif
		int fd;
	};



	//////////////////////////////////////////////
	// Poller base classes
	//

	/*!
	 * \brief Abstract base class of a poller.
	 * \ingroup zth_api_cpp_poller
	 */
	class PollerInterface : public UniqueID<PollerInterface> {
	public:

		/*!
		 * \brief Dtor.
		 */
		virtual ~PollerInterface() is_default

		/*!
		 * \brief Add a pollable object.
		 *
		 * Once a pollable is added, do not modify its properties, except for
		 * \c user_data.
		 *
		 * \return 0 on success, otherwise an errno
		 */
		virtual int add(Pollable& p) = 0;

#if __cplusplus >= 201103L
		int add(std::initializer_list<std::reference_wrapper<Pollable>> l);
#endif

		/*!
		 * \brief Remove a pollable object.
		 * \return 0 on success, otherwise an errno
		 */
		virtual int remove(Pollable& p) noexcept = 0;

		/*!
		 * \brief Reserve memory to add more pollables.
		 *
		 * \exception std::bad_alloc when allocation fails
		 */
		virtual void reserve(size_t more) = 0;

		/*!
		 * \brief Checks if there is any pollable registered.
		 */
		virtual bool empty() const noexcept = 0;
	};

	class PollerClientBase : public PollerInterface {
	public:
		virtual ~PollerClientBase() override is_default

		/*!
		 * \brief Result of #poll().
		 */
		typedef small_vector<Pollable*> Result;

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
		virtual Result const& poll(int timeout_ms = -1) noexcept = 0;

		/*!
		 * \brief Indicate that the given pollable got an event.
		 */
		virtual void event(Pollable& p) noexcept = 0;
	};

	class PollerServerBase : public PollerInterface {
	public:
		typedef PollerClientBase Client;
		virtual ~PollerServerBase() override is_default
		virtual int poll(int timeout_ms) noexcept = 0;
		virtual int migrateTo(PollerServerBase& p) = 0;

		/*!
		 * \brief Add a Pollable, that belongs to a given client.
		 *
		 * \p client can be \c nullptr.
		 *
		 * \return 0 on success, otherwise an errno
		 */
		virtual int add(Pollable& p, Client* client) = 0;

		/*!
		 * \brief Remove the given Pollable, belonging to the given client.
		 *
		 * \p client must match the one that was specified during #add().
		 *
		 * \return 0 on success, otherwise an errno
		 */
		virtual int remove(Pollable& p, Client* client) = 0;
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
	 * The subclass must implement \c init() to initialize this thing, and
	 * \c doPoll() to perform the actual poll.
	 *
	 * \tparam Item_ the item to be used as the actual poll thing
	 * \tparam Allocator the allocator type to use for dynamic memory allocations
	 */
	template <typename PollItem_>
	class PollerServer : public PollerServerBase {
	public:
		typedef PollerServerBase base;
		using base::Client;

		PollerServer() is_default

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

		virtual ~PollerServer() override
		{
			// Call clear() in the subclass. The virtual deinit() will be called.
			zth_assert(empty());
		}

		virtual int migrateTo(PollerServerBase& p) override
		{
			p.reserve(m_metaItems.size());
			size_t added = 0;

			for(size_t i = 0; i < m_metaItems.size(); i++) {
				MetaItem const& m = m_metaItems[i];

				int res = p.add(*m.pollable, m.client);
				if(res) {
					// Rollback
					for(size_t j = i; j > 0; j--)
						p.remove(*m_metaItems[j - 1U].pollable, m_metaItems[j - 1U].client);

					return res;
				}

				added++;
			}

			clear();

			// Really release all memory. This object is probably not used
			// anymore. Swap to local variables, which will release all memory
			// when going out of scope.
			decltype(m_metaItems) dummyMeta;
			m_metaItems.swap(dummyMeta);

			decltype(m_pollItems) dummyPoll;
			m_pollItems.swap(dummyPoll);

			return 0;
		}

		virtual void reserve(size_t more) override
		{
			m_metaItems.reserve(m_metaItems.size() + more);
			m_pollItems.reserve(m_metaItems.capacity());
		}

		int add(Pollable& p, Client* client) final
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
			} __catch(...) {
				// Rollback.
				m_metaItems.pop_back();
				return ENOMEM;
			}

			if(client)
				zth_dbg(io, "[%s] added pollable %p for client %s", this->id_str(), &p, client->id_str());
			else
				zth_dbg(io, "[%s] added pollable %p", this->id_str(), &p);

			currentWorker().waiter().wakeup();
			return 0;
		}

		int add(Pollable& p) final
		{
			return add(p, nullptr);
		}

		int remove(Pollable& p, Client* client) noexcept final
		{
			size_t count = m_pollItems.size();

			size_t i;
			for(i = count; i > 0; i--) {
				MetaItem& m = m_metaItems[i - 1u];
				if(m.pollable == &p && m.client == client)
					break;
			}

			if(i == 0u)
				return ESRCH;

			i--;

			if(i < count - 1u) {
				// Not removing the last element, fill the gap.
				if(i < m_pollItems.size()) {
					deinit(*m_metaItems[i].pollable, m_pollItems[i]);
					if(i < m_pollItems.size() - 1u) {
#if __cplusplus >= 201103L
						m_pollItems[i] = std::move(m_pollItems.back());
#else
						m_pollItems[i] = m_pollItems.back();
#endif
					}
					m_pollItems.pop_back();
				}

#if __cplusplus >= 201103L
				m_metaItems[i] = std::move(m_metaItems.back());
#else
				m_metaItems[i] = m_metaItems.back();
#endif
				m_metaItems.pop_back();
			} else {
				// Drop the last element.
				remove(1u);
			}

			zth_dbg(io, "[%s] removed pollable %p", this->id_str(), &p);
			return 0;
		}

		/*!
		 * \brief Remove the last added Pollables.
		 */
		void remove(size_t last) noexcept
		{
			if(last >= m_metaItems.size()) {
				clear();
			} else {
				for(size_t i = m_metaItems.size() - last; i < m_pollItems.size(); i++)
					deinit(*m_metaItems[i].pollable, m_pollItems[i]);

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
			for(size_t i = 0; i < m_pollItems.size(); i++)
				deinit(*m_metaItems[i].pollable, m_pollItems[i]);

			m_metaItems.clear();
			m_pollItems.clear();
		}

		bool empty() const noexcept final
		{
			return m_metaItems.empty();
		}

		virtual int poll(int timeout_ms) noexcept override
		{
			int res = 0;

			if((res = mirror()))
				return res;

			if(m_metaItems.empty()) {
				return EINVAL;
			}

			zth_dbg(io, "[%s] polling %u items for %d ms", this->id_str(), (unsigned)m_metaItems.size(), timeout_ms);
			return doPoll(timeout_ms, m_pollItems);
		}


	protected:
		typedef PollItem_ PollItem;
		typedef std::vector<PollItem, typename Config::Allocator<PollItem>::type> PollItemList;

		/*!
		 * \brief Register an \c revents for the PollItem at the given index.
		 *
		 * This function should be called by \c doPoll().
		 */
		void event(Pollable::Events revents, size_t index) noexcept
		{
			zth_assert(index < m_metaItems.size());

			MetaItem const& m = m_metaItems[index];
			m.pollable->revents = revents;

			if(revents.any()) {
				zth_dbg(io, "[%s] pollable %p got event 0x%lx", this->id_str(), m.pollable, revents.to_ulong());
				if(m.client)
					m.client->event(*m.pollable);
			}
		}

	private:
		/*!
		 * \brief Initialize a PollItem, given a Pollable.
		 * \return 0 on success, otherwise an errno
		 */
		virtual int init(Pollable const& p, PollItem& item) noexcept = 0;

		/*!
		 * \brief Cleanup a PollItem.
		 */
		virtual void deinit(Pollable const& UNUSED_PAR(p), PollItem& UNUSED_PAR(item)) noexcept {}

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
				if((res = init(*m_metaItems[i].pollable, m_pollItems[i]))) {
					m_pollItems.resize(i);
					break;
				}

			return res;
		}

	private:
		/*!
		 * \brief Meta data for a Pollable.
		 */
		struct MetaItem {
			Pollable* pollable;
			Client* client;
		};

		typedef std::vector<MetaItem, typename Config::Allocator<MetaItem>::type> MetaItemList;

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
	};


#ifdef ZTH_HAVE_LIBZMQ
	// By default, use zmq_poll(). It allows polling everything poll() can do,
	// including all ZeroMQ sockets.

	/*!
	 * \brief A PollerServer that uses \c zmq_poll().
	 */
	class ZmqPoller : public PollerServer<zmq_pollitem_t> {
	public:
		typedef PollerServer<zmq_pollitem_t> base;

		ZmqPoller();
		virtual ~ZmqPoller() override;

	private:
		virtual int init(Pollable const& p, zmq_pollitem_t& item) noexcept override;
		virtual int doPoll(int timeout_ms, typename base::PollItemList& items) noexcept override;
	};

	typedef ZmqPoller DefaultPollerServer;

#elif defined(ZTH_HAVE_POLL)
	// If we don't have ZeroMQ, use the OS's poll() instead.

	/*!
	 * \brief A PollerServer that uses the OS's \c poll().
	 */
	class PollPoller : public PollerServer<struct pollfd> {
	public:
		typedef PollerServer<struct pollfd> base;

		PollPoller();
		virtual ~PollPoller() override;

	private:
		virtual int init(Pollable const& p, struct pollfd& item) noexcept override;
		virtual int doPoll(int timeout_ms, base::PollItemsList& items) noexcept override;
	};

	typedef PollPoller DefaultPollerServer;

#else
	// No poller available.
	// Do provide a PollerServer, but it can only return an error upon a poll.

	/*!
	 * \brief Dummy poller that cannot poll.
	 */
	class NoPoller final : public PollerServer<int> {
	public:
		typedef PollerServer<int> base;

		NoPoller();
		~NoPoller() final;

	protected:
		int init(Pollable& p, int& item) noexcept final;
		int doPoll(int timeout_ms, base::PollItemsList& items) noexcept final;
	};

	typedef NoPoller DefaultPollerServer;
#endif // No poller





	//////////////////////////////////////////////
	// PollerClient
	//

	/*!
	 * \brief The poller to be used by a fiber.
	 *
	 * All poll requests are forwarded to the PollerServer, as managed by the
	 * current Worker's Waiter.
	 */
	class PollerClient : public PollerClientBase {
		typedef small_vector<Pollable*> Pollables;

	public:
		typedef PollerClientBase base;
		using base::Result;

		PollerClient();

#if __cplusplus >= 201103L
		PollerClient(std::initializer_list<std::reference_wrapper<Pollable>> l);
#endif

		virtual ~PollerClient() override;

		virtual void reserve(size_t more) override;
		virtual int add(Pollable& p) override;
		using base::add;
		virtual int remove(Pollable& p) noexcept override;

		virtual Result const& poll(int timeout_ms = -1) noexcept override;
		virtual void event(Pollable& p) noexcept override;

		bool empty() const noexcept final;

	private:
		/*! \brief The fiber that waits for this Poller.  */
		TimedWaitable m_wait;

		/*! \brief All %Pollables to poll. */
		Pollables m_pollables;

		/*! \brief The poll() result, populated by event(). */
		Result m_result;
	};

	// The default Poller to use.
	typedef PollerClient Poller;

	template <typename P>
	int poll(P pollable, int timeout_ms = -1)
	{
		__try {
			Poller poller;
			poller.add(pollable);

			while(true) {
				Poller::Result const& result = poller.poll(timeout_ms);

				if(result.empty()) {
					switch(errno) {
					case EINTR:
					case EAGAIN:
						if(timeout_ms == -1)
							// Retry.
							continue;
						ZTH_FALLTHROUGH
					default:
						// Error.
						return errno;
					}
				}

				if((result[0]->revents & result[0]->events).any()) {
					// Got it.
					return 0;
				}

				// Hmm, there is another reason we returned...
				return EIO;
			}
		} __catch(...) {
			return ENOMEM;
		}
	}
} // namespace

#endif // __cplusplus
#endif // ZTH_POLLER_H
