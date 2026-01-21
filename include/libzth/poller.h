#ifndef ZTH_POLLER_H
#define ZTH_POLLER_H
/*
 * SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
 *
 * SPDX-License-Identifier: MPL-2.0
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

#  include <libzth/allocator.h>
#  include <libzth/waiter.h>
#  include <libzth/worker.h>

#  include <bitset>
#  include <memory>
#  include <stdexcept>
#  include <vector>

#  if __cplusplus >= 201103L
#    include <functional>
#    include <initializer_list>
#  endif

#  if defined(ZTH_HAVE_POLL) || defined(ZTH_HAVE_LIBZMQ)
#    define ZTH_HAVE_POLLER
#  endif

#  if defined(ZTH_HAVE_LIBZMQ)
#    include <zmq.h>
#  elif defined(ZTH_HAVE_POLL)
#    include <poll.h>
#  endif

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
	/*!
	 * \brief Flags to be used with #events and #revents.
	 */
	enum EventsFlags {
		PollInIndex,
		PollOutIndex,
		PollErrIndex,
		PollPriIndex,
		PollHupIndex,
		FlagCount,
	};

	/*! \brief Type of #events and #revents. */
	typedef std::bitset<FlagCount> Events;

	// unsigned long can be converted implicitly to bitset.
	static const unsigned long PollIn = 1UL << PollInIndex;
	static const unsigned long PollOut = 1UL << PollOutIndex;
	static const unsigned long PollErr = 1UL << PollErrIndex;
	static const unsigned long PollPri = 1UL << PollPriIndex;
	static const unsigned long PollHup = 1UL << PollHupIndex;

	/*!
	 * \brief Ctor.
	 */
	constexpr explicit Pollable(Events const& e, void* user = nullptr) noexcept
		: user_data(user)
		, events(e)
		, revents()
	{}

	/*!
	 * \brief User data.
	 *
	 * This can be changed, even after adding a Pollable to a Poller.
	 */
	void* user_data;

	/*!
	 * \brief Events to poll.
	 *
	 * Do not change these events after adding the Pollable to a Poller.
	 *
	 * Not every PollerServer may support all flags.
	 */
	Events events;

	/*!
	 * \brief Returned events by a poll.
	 *
	 * Do not set manually, let the Poller do that.
	 */
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
	/*!
	 * \brief Ctor for a file descriptor.
	 *
	 * Do not pass ZeroMQ sockets as a file descriptor.
	 * Use the socket \c void* for that.
	 */
	constexpr PollableFd(int f, Events const& e, void* user = nullptr) noexcept
		: Pollable(e, user)
#  ifdef ZTH_HAVE_LIBZMQ
		, socket()
#  endif // ZTH_HAVE_LIBZMQ
		, fd(f)
	{}

#  ifdef ZTH_HAVE_LIBZMQ
	/*!
	 * \brief Ctor for a ZeroMQ socket.
	 */
	constexpr PollableFd(void* s, Events const& e, void* user = nullptr) noexcept
		: Pollable(e, user)
		, socket(s)
		, fd()
	{}

	/*!
	 * \brief The ZeroMQ socket.
	 *
	 * If set, it #fd is ignored.
	 * Do not change the value after adding the Pollable to a Poller.
	 */
	void* socket;
#  endif // ZTH_HAVE_LIBZMQ

	/*!
	 * \brief The file descriptor.
	 *
	 * If #socket is set, this field is ignored.
	 * Do not change the value after adding the Pollable to a Poller.
	 */
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
	virtual ~PollerInterface() noexcept override is_default

	/*!
	 * \brief Add a pollable object.
	 *
	 * Once a pollable is added, do not modify its properties, except for
	 * \c user_data.
	 *
	 * \return 0 on success, otherwise an errno
	 */
	virtual int add(Pollable& p) noexcept = 0;

#  if __cplusplus >= 201103L
	int add(std::initializer_list<std::reference_wrapper<Pollable>> l) noexcept;
#  endif

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

/*!
 * \brief The abstract base class of a Poller client.
 *
 * A call to #poll() forwards all registered Pollables to the server.  When
 * a Pollable has an event, the server passes it to the client via
 * #event().
 *
 * \ingroup zth_api_cpp_poller
 */
class PollerClientBase : public PollerInterface {
public:
	virtual ~PollerClientBase() noexcept override is_default

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

/*!
 * \brief Abstract base class of a Poller server.
 * \ingroup zth_api_cpp_poller
 */
class PollerServerBase : public PollerInterface {
public:
	typedef PollerClientBase Client;

	virtual ~PollerServerBase() noexcept override is_default

	/*!
	 * \brief Poll.
	 *
	 * The Pollables with events are not saved in the server,
	 * but these are forwarded to the registered Client.
	 *
	 * \return 0 when a Pollable returned an event, otherwise an errno
	 */
	virtual int poll(int timeout_ms) noexcept = 0;

	/*!
	 * \brief Move all registered Pollables to another server.
	 * \return 0 on success, otherwise an errno
	 */
	virtual int migrateTo(PollerServerBase& p) noexcept = 0;

	/*!
	 * \brief Add a Pollable, that belongs to a given client.
	 *
	 * \p client can be \c nullptr.
	 *
	 * \return 0 on success, otherwise an errno
	 */
	virtual int add(Pollable& p, Client* client) noexcept = 0;

	using PollerInterface::add;

	/*!
	 * \brief Remove the given Pollable, belonging to the given client.
	 *
	 * \p client must match the one that was specified during #add().
	 *
	 * \return 0 on success, otherwise an errno
	 */
	virtual int remove(Pollable& p, Client* client) noexcept = 0;

	using PollerInterface::remove;
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
 * \c doPoll() to perform the actual poll. Implement \c deinit() to do
 * cleanup of that thing before destruction.
 *
 * \tparam PollItem_ the item to be used as the actual poll thing
 * \ingroup zth_api_cpp_poller
 */
template <typename PollItem_>
class PollerServer : public PollerServerBase {
public:
	typedef PollerServerBase base;
	using base::Client;

	/*!
	 * \brief Ctor.
	 */
	PollerServer() is_default

#  if __cplusplus >= 201103L
	PollerServer(PollerServer const&) = delete;
	void operator=(PollerServer const&) = delete;
	PollerServer(PollerServer&& p) = delete;
	void operator=(PollerServer&& p) = delete;
#  else
private:
	PollerServer(PollerServer const&);
	PollerServer& operator=(PollerServer const&);

public:
#  endif

	virtual ~PollerServer() noexcept override
	{
		// Call clear() in the subclass. The virtual deinit() will be called.
		zth_assert(empty());
	}

	virtual int migrateTo(PollerServerBase& p) noexcept override
	{
		try {
			p.reserve(m_metaItems.size());
		} catch(...) {
			return ENOMEM;
		}

		for(size_t i = 0; i < m_metaItems.size(); i++) {
			MetaItem const& m = m_metaItems[i];

			int res = p.add(*m.pollable, m.client);
			if(res) {
				// Rollback
				for(size_t j = i; j > 0; j--)
					p.remove(
						*m_metaItems[j - 1U].pollable,
						m_metaItems[j - 1U].client);

				return res;
			}
		}

		clear();

		// Really release all memory. This object is probably not used
		// anymore.
		m_metaItems.clear_and_release();
		m_pollItems.clear_and_release();

		return 0;
	}

	virtual void reserve(size_t more) override
	{
		m_metaItems.reserve(m_metaItems.size() + more);
		m_pollItems.reserve(m_metaItems.capacity());
	}

	int add(Pollable& p, Client* client) noexcept final
	{
		try {
			MetaItem m = {&p, client};
			m_metaItems.push_back(m);
		} catch(...) {
			return ENOMEM;
		}

		try {
			// Reserve space, but do not initialize yet.
			m_pollItems.reserve(m_metaItems.size());
		} catch(...) {
			// Rollback.
			m_metaItems.pop_back();
			return ENOMEM;
		}

		if(client)
			zth_dbg(io, "[%s] added pollable %p for client %s", this->id_str(), &p,
				client->id_str());
		else
			zth_dbg(io, "[%s] added pollable %p", this->id_str(), &p);

		zth_assert(m_metaItems.size() >= m_pollItems.size());
		currentWorker().waiter().wakeup();
		return 0;
	}

	int add(Pollable& p) noexcept final
	{
		return add(p, nullptr);
	}

	// cppcheck-suppress[constParameter,constParameterPointer]
	int remove(Pollable& p, Client* client) noexcept final
	{
		size_t count = m_metaItems.size();

		size_t i;
		for(i = count; i > 0; i--) {
			MetaItem const& m = m_metaItems[i - 1U];
			if(m.pollable == &p && m.client == client)
				break;
		}

		if(i == 0U)
			return ESRCH;

		i--;

		if(i < count - 1U) {
			// Not removing the last element, fill the gap.
			if(i < m_pollItems.size()) {
				deinit(*m_metaItems[i].pollable, m_pollItems[i]);
				if(i < m_pollItems.size() - 1U) {
#  if __cplusplus >= 201103L
					m_pollItems[i] = std::move(m_pollItems.back());
#  else
					m_pollItems[i] = m_pollItems.back();
#  endif
				}
				m_pollItems.pop_back();
			}

#  if __cplusplus >= 201103L
			m_metaItems[i] = std::move(m_metaItems.back());
#  else
			m_metaItems[i] = m_metaItems.back();
#  endif
			m_metaItems.pop_back();
		} else {
			// Drop the last element.
			remove(1U);
		}

		zth_dbg(io, "[%s] removed pollable %p", this->id_str(), &p);
		zth_assert(m_metaItems.size() >= m_pollItems.size());
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

		zth_dbg(io, "[%s] polling %u items for %d ms", this->id_str(),
			(unsigned)m_metaItems.size(), timeout_ms);
		return doPoll(timeout_ms, m_pollItems);
	}


protected:
	typedef PollItem_ PollItem;

	/*! \brief Type of list of poll things. */
	typedef small_vector<PollItem, 4> PollItemList;

	/*!
	 * \brief Register an \c revents for the PollItem at the given index.
	 *
	 * This function should be called by \c doPoll().
	 * Events are forwarded to the client registered with the Pollable.
	 */
	// cppcheck-suppress passedByValue
	void event(Pollable::Events revents, size_t index) noexcept
	{
		zth_assert(index < m_metaItems.size());

		MetaItem const& m = m_metaItems[index];
		m.pollable->revents = revents;

		if(revents.any()) {
			zth_dbg(io, "[%s] pollable %p got event 0x%lx", this->id_str(), m.pollable,
				revents.to_ulong());
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

		zth_assert(m_metaItems.size() == m_pollItems.size());
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

	typedef small_vector<MetaItem, PollItemList::prealloc> MetaItemList;

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


#  ifdef ZTH_HAVE_LIBZMQ
// By default, use zmq_poll(). It allows polling everything poll() can do,
// including all ZeroMQ sockets.

/*!
 * \brief A PollerServer that uses \c zmq_poll().
 * \ingroup zth_api_cpp_poller
 */
class ZmqPoller : public PollerServer<zmq_pollitem_t> {
	ZTH_CLASS_NEW_DELETE(ZmqPoller)
public:
	typedef PollerServer<zmq_pollitem_t> base;

	ZmqPoller();
	virtual ~ZmqPoller() noexcept override;

private:
	virtual int init(Pollable const& p, zmq_pollitem_t& item) noexcept override;
	virtual int doPoll(int timeout_ms, base::PollItemList& items) noexcept override;
};

typedef ZmqPoller DefaultPollerServer;

#  elif defined(ZTH_HAVE_POLL)
// If we don't have ZeroMQ, use the OS's poll() instead.

/*!
 * \brief A PollerServer that uses the OS's \c poll().
 * \ingroup zth_api_cpp_poller
 */
class PollPoller : public PollerServer<struct pollfd> {
	ZTH_CLASS_NEW_DELETE(PollPoller)
public:
	typedef PollerServer<struct pollfd> base;

	PollPoller();
	virtual ~PollPoller() override;

private:
	virtual int init(Pollable const& p, struct pollfd& item) noexcept override;
	virtual int doPoll(int timeout_ms, base::PollItemList& items) noexcept override;
};

typedef PollPoller DefaultPollerServer;

#  else
// No poller available.
// Do provide a PollerServer, but it can only return an error upon a poll.

/*!
 * \brief Dummy poller that cannot poll.
 * \ingroup zth_api_cpp_poller
 */
class NoPoller final : public PollerServer<int> {
	ZTH_CLASS_NEW_DELETE(NoPoller)
public:
	typedef PollerServer<int> base;

	NoPoller();
	~NoPoller() final;

protected:
	int init(Pollable const& p, int& item) noexcept final;
	int doPoll(int timeout_ms, base::PollItemList& items) noexcept final;
};

typedef NoPoller DefaultPollerServer;
#  endif // No poller

/*!
 * \typedef DefaultPollerServer
 * \brief The poller server, by default instantiated by the #zth::Waiter.
 * \ingroup zth_api_cpp_poller
 */


//////////////////////////////////////////////
// PollerClient
//

/*!
 * \brief The poller to be used by a fiber.
 *
 * All poll requests are forwarded to the PollerServer, as managed by the
 * current Worker's #zth::Waiter.
 *
 * \ingroup zth_api_cpp_poller
 */
class PollerClient : public PollerClientBase {
	ZTH_CLASS_NEW_DELETE(PollerClient)

	typedef small_vector<Pollable*> Pollables;

public:
	typedef PollerClientBase base;
	using base::Result;

	PollerClient();

#  if __cplusplus >= 201103L
	// cppcheck-suppress noExplicitConstructor
	PollerClient(std::initializer_list<std::reference_wrapper<Pollable>> l);
#  endif

	virtual ~PollerClient() noexcept override;

	virtual void reserve(size_t more) override;
	virtual int add(Pollable& p) noexcept override;
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

/*!
 * \brief The default Poller to use.
 * \ingroup zth_api_cpp_poller
 */
typedef PollerClient Poller;

/*!
 * \brief Fiber-aware \c %poll() for a single pollable thing.
 * \ingroup zth_api_cpp_poller
 */
template <typename P>
int poll(P pollable, int timeout_ms = -1)
{
	try {
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
	} catch(...) {
	}
	return ENOMEM;
}

} // namespace zth
#endif // __cplusplus
#endif // ZTH_POLLER_H
