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

#include <libzth/poller.h>

namespace zth {


//////////////////////////////////////////////
// PollerInterface
//

#if __cplusplus >= 201103L
/*!
 * \brief Add pollable objects.
 *
 * \return 0 on success, otherwise an errno
 */
int PollerInterface::add(std::initializer_list<std::reference_wrapper<Pollable>> l) noexcept
{
	__try {
		reserve(l.size());
	} __catch(...) {
		return ENOMEM;
	}

	int res = 0;
	size_t count = 0;

	for(Pollable& p : l) {
		if((res = add(p))) {
			// Rollback.
			for(auto const* it = l.begin(); it != l.end() && count > 0; ++it, count--)
				remove(*it);
			break;
		}

		// Success.
		count++;
	}

	return res;
}
#endif // C++11



#ifdef ZTH_HAVE_LIBZMQ
//////////////////////////////////////////////
// ZmqPoller
//

ZmqPoller::ZmqPoller()
{
	this->setName("zth::ZmqPoller");
}

ZmqPoller::~ZmqPoller() is_default

int ZmqPoller::init(Pollable const& p, zmq_pollitem_t& item) noexcept
{
	// Zth only has PollableFds, so casting is safe.
	// NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast)
	PollableFd const& pfd = static_cast<PollableFd const&>(p);

	item.socket = pfd.socket;
	item.fd = pfd.fd;

	item.events = 0;
	if((pfd.events.test(Pollable::PollInIndex)))
		item.events |= ZMQ_POLLIN;
	if((pfd.events.test(Pollable::PollOutIndex)))
		item.events |= ZMQ_POLLOUT;
	return 0;
}

int ZmqPoller::doPoll(int timeout_ms, typename base::PollItemList& items) noexcept
{
	zth_assert(items.size() <= (size_t)std::numeric_limits<int>::max());
	int res = zmq_poll(&items[0], (int)items.size(), timeout_ms);
	if(res < 0)
		return zmq_errno();
	if(res == 0)
		// Timeout.
		return 0;

	for(size_t i = 0; res > 0 && i < items.size(); i++) {
		zmq_pollitem_t const& item = items[i];
		Pollable::Events revents = 0;

		if(item.revents) {
			res--;

			if(item.revents & ZMQ_POLLIN) // NOLINT(hicpp-signed-bitwise)
				revents |= Pollable::PollIn;
			if(item.revents & ZMQ_POLLOUT) // NOLINT(hicpp-signed-bitwise)
				revents |= Pollable::PollOut;
			if(item.revents & ZMQ_POLLERR) // NOLINT(hicpp-signed-bitwise)
				revents |= Pollable::PollErr;
		}

		this->event(revents, i);
	}

	return 0;
}

#elif defined(ZTH_HAVE_POLL)
//////////////////////////////////////////////
// PollPoller
//

PollPoller::PollPoller()
{
	setName("zth::PollPoller");
}

PollPoller::~PollPoller()
{
	clear();
}

int PollPoller::init(Pollable const& p, struct pollfd& item) noexcept
{
	// Zth only has PollableFds, so this is safe.
	PollableFd const& pfd = static_cast<PollableFd const&>(p);

	item.fd = pfd.fd;

	item.events = 0;
	if((pfd.events.test(Pollable::PollInIndex)))
		item.events |= POLLIN;
	if((pfd.events.test(Pollable::PollOutIndex)))
		item.events |= POLLOUT;
	if((pfd.events.test(Pollable::PollPriIndex)))
		item.events |= POLLPRI;
	if((pfd.events.test(Pollable::PollHupIndex)))
		item.events |= POLLHUP;

	return 0;
}

int PollPoller::doPoll(int timeout_ms, base::PollItemsList& items) noexcept
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


#else
//////////////////////////////////////////////
// NoPoller
//

NoPoller::NoPoller()
{
	setName("zth::NoPoller");
}

NoPoller::~NoPoller() is_default

int NoPoller::init(Pollable& UNUSED_PAR(p), int& UNUSED_PAR(item)) noexcept
{
	return EINVAL;
}

int NoPoller::doPoll(int timeout_ms, base::PollItemsList& items) noexcept
{
	return ENOSYS;
}

#endif // Poller method




//////////////////////////////////////////////
// PollerClient
//

PollerClient::PollerClient()
{
	this->setName("zth::PollerClient");
}

#if __cplusplus >= 201103L
PollerClient::PollerClient(std::initializer_list<std::reference_wrapper<Pollable>> l)
{
	errno = add(l);

#  ifdef __cpp_exceptions
	switch(errno) {
	case 0: break;
	case ENOMEM: throw std::bad_alloc();
	default: throw std::runtime_error("");
	}
#  endif
}
#endif

PollerClient::~PollerClient() is_default

void PollerClient::reserve(size_t more)
{
	m_pollables.reserve(m_pollables.size() + more);
	m_result.reserve(m_pollables.capacity());
}

int PollerClient::add(Pollable& p) noexcept
{
	__try {
		m_result.reserve(m_pollables.size() + 1u);
		m_pollables.push_back(&p);
		zth_dbg(io, "[%s] poll %p for events 0x%lu", id_str(), &p, p.events.to_ulong());
		return 0;
	} __catch(...) {
		return ENOMEM;
	}
}

int PollerClient::remove(Pollable& p) noexcept
{
	for(size_t i = m_pollables.size(); i > 0; i--) {
		Pollable*& pi = m_pollables[i - 1u];
		if(pi == &p) {
			pi = m_pollables.back();
			m_pollables.pop_back();
			return 0;
		}
	}

	return EAGAIN;
}

PollerClient::Result const& PollerClient::poll(int timeout_ms) noexcept
{
	m_result.clear();

	if(m_pollables.empty()) {
		errno = EINVAL;
		return m_result;
	}

	Waiter& waiter = currentWorker().waiter();
	PollerServerBase& p = waiter.poller();

	// Add all our pollables to the server.
	for(size_t i = 0; i < m_pollables.size(); i++)
		p.add(*m_pollables[i], this);

	// Go do the poll.
	zth_dbg(io, "[%s] polling %u items for %d ms", id_str(), (unsigned)m_pollables.size(), timeout_ms);

	// First try, in the current fiber's context.
	m_wait = TimedWaitable();
	int res = p.poll(0);

	if(!m_result.empty()) { // NOLINT(bugprone-branch-clone)
		// Completed already.
	} else if(res && res != EAGAIN) {
		// Completed with an error.
	} else if(timeout_ms == 0) {
		// Just tried. Got nothing, apparently.
	} else if(timeout_ms < 0) {
		// Wait for the event callback.
		zth_dbg(io, "[%s] hand-off to server", id_str());
		m_wait.setFiber(currentFiber());
		suspend();
	} else {
		zth_dbg(io, "[%s] hand-off to server with timeout", id_str());
		m_wait = TimedWaitable(Timestamp::now() + TimeInterval(timeout_ms / 1000L, (timeout_ms * 1000000L) % 1000000000L));
		waitUntil(m_wait);
	}

	// Remove our pollables from the server.
	for(size_t i = m_pollables.size(); i > 0; i--)
		p.remove(*m_pollables[i - 1u], this);

	// m_result got populated by event().
	if(m_result.empty() && !res)
		res = EAGAIN;

	if(res)
		zth_dbg(io, "[%s] poll returned %s", id_str(), err(res).c_str());
	else
		zth_dbg(io, "[%s] poll returned %u pollable(s)", id_str(), (unsigned)m_result.size());

	errno = res;
	return m_result;
}

void PollerClient::event(Pollable& p) noexcept
{
	m_result.push_back(&p);

	if(!m_wait.timeout().isNull()) {
		// Managed by the waiter
		currentWorker().waiter().wakeup(m_wait);
	} else if(m_wait.hasFiber()) {
		// Just suspended.
		resume(m_wait.fiber());
	}
}

bool PollerClient::empty() const noexcept
{
	return m_pollables.empty();
}

} // namespace
