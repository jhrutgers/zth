/*
 * SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#include <zth>

#include <exception>
#include <future>

#include <gtest/gtest.h>

static void fiber_d(zth::promise<int> p)
{
	p.set_value(42);
}
zth_fiber(fiber_d)

TEST(Promise, d)
{
	zth::promise<int> p;
	auto f = p.get_future();
	zth_async fiber_d(std::move(p));

	auto i = f.get();
	EXPECT_EQ(i, 42);
}

static void fiber_exception(zth::promise<int> p)
{
	p.set_exception(std::make_exception_ptr(std::runtime_error("Test exception")));
}
zth_fiber(fiber_exception)

TEST(Promise, exception)
{
	zth::promise<int> p;
	auto f = p.get_future();
	zth_async fiber_exception(std::move(p));

	EXPECT_THROW(f.get(), std::runtime_error);
}

static void fiber_async() {}

TEST(Future, async)
{
	auto future = std::async(zth::launch::awaitable, fiber_async);
	EXPECT_EQ(future.valid(), true);
	future.wait();

	auto future2 = std::async(zth::launch::detached, fiber_async);
	EXPECT_EQ(future2.valid(), false);
	EXPECT_THROW(future2.get(), std::future_error);

	auto future3 = std::async(zth::launch::awaitable, []() { return 3; });
	EXPECT_EQ(future3.get(), 3);
}

TEST(Future, fromPointer)
{
	auto* f = new zth::Future<int>();

	zth::SharedPointer<zth::Future<int>> sp{f};
	zth::SharedReference<zth::Future<int>> sr{f};

	EXPECT_FALSE(sp->valid());
	EXPECT_FALSE(sr.get().valid());

	f->set(1);
	EXPECT_TRUE(sp->valid());
	EXPECT_EQ(sp->value(), 1);
	EXPECT_EQ(**sp, 1);
	EXPECT_TRUE(sr.get().valid());
	EXPECT_EQ(sr.get().value(), 1);
	EXPECT_EQ(*sr, 1);

	zth::shared_future<int> stdsf;
	EXPECT_FALSE(stdsf.valid());
	EXPECT_THROW(stdsf.get(), std::future_error);

	zth::future<int> stdf;
	EXPECT_FALSE(stdf.valid());
	EXPECT_THROW(stdf.get(), std::future_error);

	auto* f2 = new zth::Future<int>();
	sp = f2;
	f2->set(2);
	EXPECT_EQ(**sp, 2);
}

TEST(Future, fromSharedPointer)
{
	// NOLINTBEGIN(clang-analyzer-cplusplus.NewDelete)
	auto* f = new zth::Future<int>();
	zth::SharedPointer<zth::Future<int>> sp{f};
	EXPECT_EQ(f->refs(), 1);

	zth::SharedReference<zth::Future<int>> sr{sp};
	zth::shared_future<int> stdsf{sp};
	zth::future<int> stdf{sp};
	EXPECT_TRUE(stdsf.valid());
	EXPECT_TRUE(stdf.valid());
	EXPECT_EQ(f->refs(), 4);

	f->set(1);
	EXPECT_TRUE(sp->valid());
	EXPECT_TRUE(sr.get().valid());
	EXPECT_EQ(stdsf.get(), 1);
	EXPECT_EQ(stdf.get(), 1);
	EXPECT_EQ(f->refs(), 4);

	auto* f2 = new zth::Future<int>();
	sp = f2;
	EXPECT_EQ(f->refs(), 3);
	EXPECT_EQ(f2->refs(), 1);
	sr = sp;
	stdsf = sp;
	EXPECT_EQ(f->refs(), 1);
	EXPECT_EQ(f2->refs(), 3);

	f2->set(2);
	EXPECT_EQ(stdsf.get(), 2);
	// NOLINTEND(clang-analyzer-cplusplus.NewDelete)
}

TEST(Future, fromSharedReference)
{
	auto* f = new zth::Future<int>();
	zth::SharedReference<zth::Future<int>> sr{f};

	zth::SharedPointer<zth::Future<int>> sp{sr};
	zth::shared_future<int> stdsf{sr};
	zth::future<int> stdf{sr};
	EXPECT_TRUE(stdsf.valid());
	EXPECT_TRUE(stdf.valid());

	f->set(1);
	EXPECT_TRUE(sp->valid());
	EXPECT_TRUE(sr.get().valid());
	EXPECT_EQ(stdsf.get(), 1);
	EXPECT_EQ(stdf.get(), 1);
}

TEST(Future, moveSharedPointer)
{
	auto* f = new zth::Future<int>();
	f->used();

	{
		zth::SharedPointer<zth::Future<int>> sp{f};
		zth::SharedReference<zth::Future<int>> sr{std::move(sp)};
		EXPECT_EQ(f->refs(), 2);
	}

	{
		zth::SharedPointer<zth::Future<int>> sp{f};
		zth::SharedReference<zth::Future<int>> sr;
		sr = std::move(sp);
		EXPECT_EQ(f->refs(), 2);
	}

	{
		zth::SharedPointer<zth::Future<int>> sp{f};
		zth::shared_future<int> stdsf{std::move(sp)};
		EXPECT_EQ(f->refs(), 2);
		(void)stdsf;
	}

	{
		zth::SharedPointer<zth::Future<int>> sp{f};
		zth::shared_future<int> stdsf;
		stdsf = std::move(sp);
		EXPECT_EQ(f->refs(), 2);
		(void)stdsf;
	}

	{
		zth::SharedPointer<zth::Future<int>> sp{f};
		zth::future<int> stdf{std::move(sp)};
		EXPECT_EQ(f->refs(), 2);
		(void)stdf;
	}

	{
		zth::SharedPointer<zth::Future<int>> sp{f};
		zth::future<int> stdf;
		stdf = std::move(sp);
		EXPECT_EQ(f->refs(), 2);
		(void)stdf;
	}

	EXPECT_TRUE(f->unused());
}

TEST(Future, moveSharedReference)
{
	auto* f = new zth::Future<int>();
	f->used();

	{
		zth::SharedReference<zth::Future<int>> sr{f};
		zth::SharedPointer<zth::Future<int>> sp{std::move(sr)};
		EXPECT_EQ(f->refs(), 2);
	}

	{
		zth::SharedReference<zth::Future<int>> sr{f};
		zth::SharedPointer<zth::Future<int>> sp;
		sp = std::move(sr);
		EXPECT_EQ(f->refs(), 2);
	}

	{
		zth::SharedReference<zth::Future<int>> sr{f};
		zth::shared_future<int> stdsf{std::move(sr)};
		EXPECT_EQ(f->refs(), 2);
		(void)stdsf;
	}

	{
		zth::SharedReference<zth::Future<int>> sr{f};
		zth::shared_future<int> stdsf;
		stdsf = std::move(sr);
		EXPECT_EQ(f->refs(), 2);
		(void)stdsf;
	}

	{
		zth::SharedReference<zth::Future<int>> sr{f};
		zth::future<int> stdf{std::move(sr)};
		EXPECT_EQ(f->refs(), 2);
		(void)stdf;
	}

	{
		zth::SharedReference<zth::Future<int>> sr{f};
		zth::future<int> stdf;
		stdf = std::move(sr);
		EXPECT_EQ(f->refs(), 2);
		(void)stdf;
	}

	EXPECT_TRUE(f->unused());
}
