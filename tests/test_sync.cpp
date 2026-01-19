/*
 * SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#include <zth>

#include <gtest/gtest.h>

TEST(Sync, Mutex)
{
	zth::Mutex m;
	m.lock();

	auto f = zth::fiber([&]() { zth::Locked l{m}; });

	for(int i = 0; i < 10; i++)
		zth::outOfWork();

	m.unlock();
	*f;
}

TEST(Sync, Mutex_C)
{
	zth_mutex_t mutex{};
	EXPECT_EQ(zth_mutex_init(&mutex), 0);
	EXPECT_EQ(zth_mutex_lock(&mutex), 0);

	auto f = zth::fiber([&]() {
		zth_mutex_lock(&mutex);
		zth_mutex_unlock(&mutex);
	});

	for(int i = 0; i < 10; i++)
		zth::outOfWork();

	EXPECT_EQ(zth_mutex_unlock(&mutex), 0);
	*f;
	EXPECT_EQ(zth_mutex_destroy(&mutex), 0);
}

TEST(Sync, Semaphore)
{
	zth::Semaphore s;

	zth::fiber([&]() {
		s.acquire(3);
		s.release(10);
	});

	zth::fiber([&]() { s.release(1); });
	zth::fiber([&]() { s.release(2); });

	s.acquire(10);
	zth::Semaphore done;

	s.release(1);

	for(int i = 0; i < 10; i++)
		zth::fiber([&]() {
			s.acquire();
			done.release();
		});

	zth::outOfWork();
	zth::fiber([&]() { s.release(3); });
	zth::outOfWork();
	zth::outOfWork();
	zth::outOfWork();
	zth::fiber([&]() { s.release(3); });
	zth::fiber([&]() { s.release(3); });

	done.acquire(10);
}

TEST(Sync, Semaphore_C)
{
	zth_sem_t a{};
	zth_sem_t b{};
	EXPECT_EQ(zth_sem_init(&a, 0), 0);
	EXPECT_EQ(zth_sem_init(&b, 0), 0);

	zth::fiber([&]() {
		zth_sem_wait(&a);
		zth_sem_post(&b);
	});

	zth::outOfWork();
	zth::outOfWork();
	EXPECT_EQ(zth_sem_post(&a), 0);
	EXPECT_EQ(zth_sem_wait(&b), 0);

	EXPECT_EQ(zth_sem_destroy(&a), 0);
	EXPECT_EQ(zth_sem_destroy(&b), 0);
}

TEST(Sync, Signal)
{
	zth::Signal s;
	zth::fiber([&]() { s.signal(); });
	s.wait();

	s.signal();
	*zth::fiber([&]() { s.wait(); });
}

TEST(Sync, Signal_C)
{
	zth_cond_t cond{};
	EXPECT_EQ(zth_cond_init(&cond), 0);

	zth::fiber([&]() { zth_cond_signal(&cond); });

	EXPECT_EQ(zth_cond_wait(&cond), 0);
	EXPECT_EQ(zth_cond_destroy(&cond), 0);
}

TEST(Sync, Future_C)
{
	zth_future_t f{};
	EXPECT_EQ(zth_future_init(&f), 0);

	zth::fiber([&]() { zth_future_set(&f, 1); });

	uintptr_t value = 0;
	EXPECT_EQ(zth_future_get(&f, &value), 0);
	EXPECT_EQ(value, 1);
	EXPECT_EQ(zth_future_destroy(&f), 0);
}

TEST(Sync, Gate)
{
	zth::Gate g{4};

	for(int i = 0; i < 3; i++)
		zth::fiber([&]() { g.wait(); });

	g.wait();
}

TEST(Sync, Gate_C)
{
	zth_gate_t g{};
	EXPECT_EQ(zth_gate_init(&g, 3), 0);

	for(int i = 0; i < 2; i++)
		zth::fiber([&]() { zth_gate_wait(&g); });

	EXPECT_EQ(zth_gate_wait(&g), 0);
	EXPECT_EQ(zth_gate_destroy(&g), 0);
}

TEST(Sync, Mailbox)
{
	zth::Mailbox<int> mb;

	zth::fiber([&]() {
		for(int i = 0; i < 10; i++)
			mb.put(i);
	});

	zth::fiber([&]() {
		for(int i = 0; i < 3; i++)
			mb.put(std::make_exception_ptr(std::runtime_error("Test exception")));
	});

	int values = 0;
	int exceptions = 0;
	for(int i = 0; i < 13; i++) {
		try {
			int v = mb.take();
			EXPECT_EQ(v, values);
			values++;
		} catch(std::exception const& e) {
			EXPECT_STREQ(e.what(), "Test exception");
			exceptions++;
		}
	}
	EXPECT_EQ(values, 10);
	EXPECT_EQ(exceptions, 3);
}

TEST(Sync, Mailbox_C)
{
	zth_mailbox_t mb{};
	EXPECT_EQ(zth_mailbox_init(&mb), 0);

	zth::fiber([&]() {
		for(int i = 0; i < 10; i++)
			zth_mailbox_put(&mb, (uintptr_t)i);
	});

	int values = 0;
	for(int i = 0; i < 10; i++) {
		uintptr_t v = 0;
		EXPECT_EQ(zth_mailbox_take(&mb, &v), 0);
		EXPECT_EQ(v, (uintptr_t)values);
		values++;
	}
	EXPECT_EQ(values, 10);

	EXPECT_EQ(zth_mailbox_destroy(&mb), 0);
}
