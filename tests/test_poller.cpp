/*
 * SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#include <zth>

#include <gtest/gtest.h>
#include <unistd.h>

// NOLINTNEXTLINE(misc-unused-using-decls)
using zth::operator""_ms;

// NOLINTBEGIN(cert-err58-cpp)
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

	char buf = 0;
	EXPECT_EQ(read(pipefd[0], &buf, 1), 1);
	EXPECT_EQ(buf, '1');

	close(pipefd[0]);
	close(pipefd[1]);
}

static void read_fiber(int fd)
{
	char buf = 0;

	// Fiber-aware blocking read.
	zth::io::read(fd, &buf, 1);
	close(fd);
}
zth_fiber(read_fiber)

TEST(Poller, FiberedPipeWrite)
{
	int pipefd[]{-1, -1};
	ASSERT_EQ(pipe(pipefd), 0);

	zth_async read_fiber(pipefd[0]);

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

	zth_async write_fiber(pipefd[1]);

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
	(void)rc;

	char buf[4];

	rc = zmq_recv(socket, buf, sizeof(buf), 0);

	if(rc < 0) {
		printf("ZMQ error: %s\n", zth::err(errno).c_str());
		// Continue anyway, such that the test can terminate properly.
		rc = 0;
	}

	zth_assert((size_t)rc < sizeof(buf));

	for(int i = 0; i < rc; i++)
		buf[i]++; // NOLINT

	rc = zmq_send(socket, buf, (size_t)rc, 0);
	zth_assert(rc > 0);
	(void)rc;

	zmq_close(socket);
}

TEST(Poller, Zmq)
{
	zth::fiber(zmq_fiber);

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
// NOLINTEND(cert-err58-cpp)
