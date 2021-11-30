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

#include "gtest/gtest.h"

#include <unistd.h>
#include <zth>

using zth::operator""_ms;

TEST(Poller, Dummy)
{
	SUCCEED();
}

TEST(Poller, Nothing)
{
	zth::Poller p;
	auto const& result = p.poll();
	EXPECT_EQ(errno, EINVAL);
	EXPECT_TRUE(result.empty());
}

#if defined(ZTH_HAVE_POLLER) && (defined(ZTH_OS_LINUX) || defined(ZTH_OS_MAC))
TEST(Poller, PipeBasic)
{
	// pipefd[0] is read end, pipefd[1] is write end
	int pipefd[]{-1, -1};
	ASSERT_EQ(pipe(pipefd), 0);

	// Nothing to read.
	EXPECT_EQ(zth::poll(zth::PollableFd(pipefd[0], zth::Pollable::PollIn), 0), EAGAIN);

	// With 100 ms timeout.
	EXPECT_EQ(zth::poll(zth::PollableFd(pipefd[0], zth::Pollable::PollIn), 100), EAGAIN);

	ASSERT_EQ(write(pipefd[1], "1", 1), 1);

	// Expect something to read.
	EXPECT_EQ(zth::poll(zth::PollableFd(pipefd[0], zth::Pollable::PollIn), 0), 0);
	EXPECT_EQ(zth::poll(zth::PollableFd(pipefd[0], zth::Pollable::PollIn), 100), 0);
	EXPECT_EQ(zth::poll(zth::PollableFd(pipefd[0], zth::Pollable::PollIn), -1), 0);

	char buf;
	EXPECT_EQ(read(pipefd[0], &buf, 1), 1);
	EXPECT_EQ(buf, '1');

	close(pipefd[0]);
	close(pipefd[1]);
}

static void read_fiber(int fd)
{
	char buf;

	// Fiber-aware blocking read.
	zth::io::read(fd, &buf, 1);
	close(fd);
}
zth_fiber(read_fiber)

TEST(Poller, FiberedPipeWrite)
{
	int pipefd[]{-1, -1};
	ASSERT_EQ(pipe(pipefd), 0);

	async read_fiber(pipefd[0]);

	zth::nap(100_ms);
	EXPECT_EQ(zth::io::write(pipefd[1], "2", 1), 1);
	close(pipefd[1]);
}

static void write_fiber(int fd)
{
	zth::nap(100_ms);
	// Fiber-aware blocking write.
	zth::io::write(fd, "3", 1);
	close(fd);
}
zth_fiber(write_fiber)

TEST(Poller, FiberedPipeRead)
{
	int pipefd[]{-1, -1};
	ASSERT_EQ(pipe(pipefd), 0);

	async write_fiber(pipefd[1]);

	char buf = 0;
	EXPECT_EQ(zth::io::read(pipefd[0], &buf, 1), 1);
	EXPECT_EQ(buf, '3');

	close(pipefd[0]);
}
#endif // Linux or Mac

#if defined(ZTH_HAVE_POLLER) && defined(ZTH_HAVE_LIBZMQ)
static void zmq_fiber()
{
	void* socket = zmq_socket(zth_zmq_context(), ZMQ_REP);
	int rc = zmq_bind(socket, "inproc://test_poller");
	zth_assert(rc == 0);

	char buf[4];

	rc = zmq_recv(socket, buf, sizeof(buf), 0);
	zth_assert(rc >= 0);
	zth_assert((size_t)rc < sizeof(buf));

	for(size_t i = 0; i < rc; i++)
		buf[i]++;

	rc = zmq_send(socket, buf, rc, 0);
	zth_assert(rc > 0);

	zmq_close(socket);
}
zth_fiber(zmq_fiber)

TEST(Poller, Zmq)
{
	zmq_fiber_future f = async zmq_fiber();

	void* socket = zmq_socket(zth_zmq_context(), ZMQ_REQ);
	ASSERT_EQ(zmq_connect(socket, "inproc://test_poller"), 0);

	char buf[] = "123";
	ASSERT_EQ(zmq_send(socket, buf, strlen(buf), 0), 3);

	EXPECT_EQ(zmq_recv(socket, buf, sizeof(buf), 0), 3);
	EXPECT_STREQ(buf, "234");

	zmq_close(socket);
}
#endif // ZmqPoller

TEST(Poller, Migrate)
{
	zth::PollableFd fd(0, zth::Pollable::PollIn);

	zth::DefaultPollerServer s1;
	EXPECT_TRUE(s1.empty());

	zth::DefaultPollerServer s2;
	EXPECT_TRUE(s2.empty());

	EXPECT_EQ(s1.add(fd), 0);
	EXPECT_FALSE(s1.empty());
	EXPECT_TRUE(s2.empty());

	s1.migrateTo(s2);
	EXPECT_TRUE(s1.empty());
	EXPECT_FALSE(s2.empty());

	EXPECT_EQ(s2.remove(fd), 0);
}

