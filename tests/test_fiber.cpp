/*
 * SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
 *
 * SPDX-License-Identifier: MPL-2.0
 */

// This file is included by test_fiber98.cpp and test_fiber11.cpp.  The test
// cases are the same, but the C++98-compatible zth::FiberType implementation
// is enforced for testing purposes. They should behave the same, but the C++11
// implementation is more efficient.

#define ZTH_ALLOW_DEPRECATED

#include <zth>

#include <gtest/gtest.h>

TEST(FiberTest, Dummy)
{
	SUCCEED();
}

static void fiber_v() {}
zth_fiber(fiber_v)

TEST(FiberTest, v)
{
	zth_async fiber_v();
	zth::fiber(fiber_v);
	zth::fiber(&fiber_v);

	fiber_v_future f = zth_async fiber_v();
	f.wait();

	*zth_async fiber_v();
	*zth::fiber(fiber_v);
	*zth::fiber(&fiber_v);

	auto fib = zth::fiber(fiber_v);
	while(((zth::Fiber&)fib).state() != zth::Fiber::Dead)
		zth::outOfWork();

	EXPECT_THROW(*fib, zth::fiber_already_dead);

	zth::outOfWork();
}

static int fiber_i()
{
	return 1;
}
zth_fiber(fiber_i)

TEST(FiberTest, i)
{
	zth_async fiber_i();
	zth::fiber(fiber_i);
	zth::fiber(&fiber_i);

	fiber_i_future f = zth_async fiber_i();
	EXPECT_EQ(*f, 1);
	EXPECT_EQ(*zth_async fiber_i(), 1);
	EXPECT_EQ(*zth::fiber(fiber_i), 1);
	EXPECT_EQ(*zth::fiber(&fiber_i), 1);

	zth::outOfWork();
}

static int fiber_ii(int i)
{
	return i + 1;
}
zth_fiber(fiber_ii)

TEST(FiberTest, ii)
{
	// cppcheck-suppress danglingTemporaryLifetime
	zth_async fiber_ii(2);
	zth::fiber(fiber_ii, 2);
	zth::fiber(&fiber_ii, 2);

	fiber_ii_future f = zth_async fiber_ii(3);
	EXPECT_EQ(*f, 4);
	EXPECT_EQ(*zth_async fiber_ii(4), 5);
	EXPECT_EQ(*zth::fiber(fiber_ii, 5), 6);
	EXPECT_EQ(*zth::fiber(&fiber_ii, 6), 7);

	zth::outOfWork();
}

// cppcheck-suppress[constParameter,unmatchedSuppression]
static int fiber_iPi(int* i)
{
	return *i + 1;
}
zth_fiber(fiber_iPi)

TEST(FiberTest, iPi)
{
	int i = 4;
	zth_async fiber_iPi(&i);
	zth::fiber(fiber_iPi, &i);
	zth::fiber(&fiber_iPi, &i);

	fiber_iPi_future f = zth_async fiber_iPi(&i);
	EXPECT_EQ(*f, 5);
	EXPECT_EQ(*zth_async fiber_iPi(&i), 5);
	EXPECT_EQ(*zth::fiber(fiber_iPi, &i), 5);
	EXPECT_EQ(*zth::fiber(&fiber_iPi, &i), 5);

	zth::outOfWork();
}

static int fiber_iRi(int& i)
{
	return ++i;
}
zth_fiber(fiber_iRi)

TEST(FiberTest, iRi)
{
	int i = 5;
	zth::fiber_future<int> f = zth_async fiber_iRi(i);
	EXPECT_EQ(*f, 6);
	EXPECT_EQ(i, 6);

	zth_async fiber_iRi(i);
	zth::fiber(fiber_iRi, i);
	zth::fiber(&fiber_iRi, i);

	i = 10;
	EXPECT_EQ(*zth_async fiber_iRi(i), 11);
	EXPECT_EQ(i, 11);

	zth::outOfWork();
}

static void fiber_viRd(int i, double& d)
{
	d = d * i;
}
zth_fiber(fiber_viRd)

TEST(FiberTest, viRd)
{
	int i = 6;
	double d = 0.1;
	zth::fiber_future<void> f = zth_async fiber_viRd(i, d);
	f.wait();
	EXPECT_DOUBLE_EQ(d, 0.6);

	zth_async fiber_viRd(1, d);
	zth::fiber(fiber_viRd, 2, d);
	zth::fiber(&fiber_viRd, 3, d);

	d = 0.1;
	*zth_async fiber_viRd(4, d);
	EXPECT_DOUBLE_EQ(d, 0.4);

	d = 0.1;
	*zth::fiber(fiber_viRd, 5, d);
	EXPECT_DOUBLE_EQ(d, 0.5);

	zth::outOfWork();
}

static double fiber_did(int i, double d)
{
	return d * i;
}
zth_fiber(fiber_did)

TEST(FiberTest, did)
{
	int i = 7;
	double d = 0.1;
	zth::fiber_future<decltype(fiber_did)> f = zth_async fiber_did(i, d);
	EXPECT_DOUBLE_EQ(*f, 0.7);
	EXPECT_DOUBLE_EQ(*zth::fiber(fiber_did, 2, 0.2), 0.4);
	EXPECT_DOUBLE_EQ(*zth::fiber(&fiber_did, 3, 0.2), 0.6);

	zth_async fiber_did(i, d);

	zth::outOfWork();
}

static void fiber_viRdf(int i, double& d, float f)
{
	d = d * (double)i * (double)f;
}
zth_fiber(fiber_viRdf)

TEST(FiberTest, viRdf)
{
	int i = 8;
	double d = 0.1;
	float f = 2.0f;
	zth::fiber_future<void> r = zth::fiber(fiber_viRdf, i, d, f);
	r.wait();
	EXPECT_DOUBLE_EQ(d, 1.6);

	zth_async fiber_viRdf(i, d, f);
	zth::fiber(fiber_viRdf, i, d, f);
	zth::fiber(&fiber_viRdf, i, d, f);

	zth::outOfWork();
}

static double fiber_didf(int i, double d, float f)
{
	return d * (double)i * (double)f;
}
zth_fiber(fiber_didf)

TEST(FiberTest, didf)
{
	int i = 9;
	double d = 0.1;
	float f = 2.0f;
	fiber_didf_future r = zth_async fiber_didf(i, d, f);
	EXPECT_DOUBLE_EQ(*r, 1.8);
	EXPECT_DOUBLE_EQ(*zth::fiber(fiber_didf, 2, 3.0, 4.F), 24.0);
	EXPECT_DOUBLE_EQ(*zth::fiber(&fiber_didf, 2, 3.0, 5.F), 30.0);

	zth_async fiber_didf(i, d, f);

	zth::outOfWork();
}

static void fiber_viRdfPl(int i, double& d, float f, long* l)
{
	d = d * (double)i * (double)f + (double)*l;
}
zth_fiber(fiber_viRdfPl)

TEST(FiberTest, viRdfPl)
{
	int i = 11;
	double d = 0.1;
	float f = 2.0f;
	long l = 1;
	fiber_viRdfPl_future r = zth_async fiber_viRdfPl(i, d, f, &l);
	r.wait();
	EXPECT_DOUBLE_EQ(d, 3.2);

	zth_async fiber_viRdfPl(i, d, f, &l);
	zth::fiber(fiber_viRdfPl, i, d, f, &l);
	zth::fiber(&fiber_viRdfPl, i, d, f, &l);

	zth::outOfWork();
}

static double fiber_diRdfPl(int i, double& d, float f, long* l)
{
	return d = d * (double)i * (double)f + (double)*l;
}
zth_fiber(fiber_diRdfPl)

TEST(FiberTest, diRdfPl)
{
	int i = 12;
	double d = 0.1;
	float f = 2.0f;
	long l = 1;
	fiber_diRdfPl_future r = zth_async fiber_diRdfPl(i, d, f, &l);
	EXPECT_DOUBLE_EQ(*r, 3.4);
	EXPECT_DOUBLE_EQ(d, 3.4);

	zth_async fiber_diRdfPl(i, d, f, &l);
	zth::fiber(fiber_diRdfPl, i, d, f, &l);
	zth::fiber(&fiber_diRdfPl, i, d, f, &l);

	zth::outOfWork();
}

TEST(FiberTest, direct_v)
{
	using fiber_type = zth::fiber_type<decltype(fiber_v)>;

	fiber_type::factory factory = zth::factory(fiber_v);
	fiber_type::fiber fiber = factory();
	fiber_type::future future = fiber;
	fiber_type::future future2 = fiber;
	future.wait();
	EXPECT_TRUE(future2.valid());

	zth::outOfWork();
}

TEST(FiberTest, direct_v_p)
{
	using fiber_type = zth::fiber_type<decltype(&fiber_v)>;

	fiber_type::factory factory = zth::factory(&fiber_v);
	fiber_type::fiber fiber = factory();
	fiber_type::future future = fiber;
	fiber_type::future future2 = fiber;
	future.wait();
	EXPECT_TRUE(future2.valid());

	zth::outOfWork();
}

TEST(FiberTest, direct_ii)
{
	auto factory = zth::factory(fiber_ii, "fiber_ii()");
	auto future = factory(5).withFuture();
	EXPECT_EQ(*future, 6);

	zth::outOfWork();
}

TEST(FiberTest, direct_iPi)
{
	int i = 4;
	auto f = zth::fiber(fiber_iPi, &i) << zth::asFuture();
	EXPECT_EQ(*f, 5);

	zth::outOfWork();
}

TEST(FiberTest, direct_viRdf)
{
	int i = 8;
	double d = 0.1;
	float f = 2.0f;

	auto future = zth::fiber(fiber_viRdf, i, d, f)
		      << zth::setName("fiber_viRdf()") << zth::asFuture();
	future.wait();
	EXPECT_DOUBLE_EQ(d, 1.6);

	zth_async fiber_viRdf(i, d, f);

	zth::outOfWork();
}

TEST(FiberTest, direct_diRdfPl)
{
	int i = 12;
	double d = 0.1;
	float f = 2.0f;
	long l = 1;
	auto r = zth::factory(fiber_diRdfPl, "fiber_diRdfPl")(i, d, f, &l).withFuture();
	EXPECT_DOUBLE_EQ(r->value(), 3.4);
	EXPECT_DOUBLE_EQ(d, 3.4);

	zth::outOfWork();
}

#ifdef ZTH_FUTURE_EXCEPTION
static void fiber_exception()
{
	throw std::runtime_error("Test exception");
}
zth_fiber(fiber_exception)

TEST(FiberTest, exception)
{
	zth_async fiber_exception();

	fiber_exception_future f = zth_async fiber_exception();
	f.wait();

	EXPECT_TRUE(f.exception());
	EXPECT_NO_THROW(zth::fiber(fiber_exception));
	EXPECT_THROW(*zth::fiber(fiber_exception), std::runtime_error);

	zth::outOfWork();
}

static int fiber_exception_i()
{
	throw std::runtime_error("Test exception");
}
zth_fiber(fiber_exception_i)

TEST(FiberTest, exception_i)
{
	zth_async fiber_exception_i();

	fiber_exception_i_future f = zth_async fiber_exception_i();
	EXPECT_THROW(*f, std::runtime_error);
	EXPECT_NO_THROW(zth::fiber(fiber_exception_i));
	EXPECT_THROW(*zth::fiber(fiber_exception_i), std::runtime_error);

	zth::outOfWork();
}
#endif // ZTH_FUTURE_EXCEPTION

TEST(FiberTest, lambda)
{
	auto lambda = []() {};
	zth::TypedFiberN<decltype(lambda), void, std::tuple<>> f{lambda};
	zth::factory(lambda)();

	int i = 3;
	auto lambda2 = [&](int a) { return i + a; };
	zth::TypedFiberN<decltype(lambda2), void, std::tuple<int>> f2{lambda2};
	auto future = zth::factory(lambda2)(4).withFuture();
	EXPECT_EQ(future->value(), 7);

	i = 4;
	EXPECT_EQ(*(zth::fiber(lambda2, 5) << zth::asFuture()), 9);

	EXPECT_EQ(*(zth::fiber([&]() { return i; }) << zth::asFuture()), 4);
	EXPECT_EQ(*(zth::fiber([&]() { return i + 1; }) << zth::asFuture()), 5);

	struct S {
		int i;
	};
	EXPECT_EQ((zth::fiber([&]() { return S{2}; }) << zth::asFuture())->i, 2);

#ifdef ZTH_FUTURE_EXCEPTION
	EXPECT_NO_THROW(zth::fiber([]() { throw std::runtime_error("Lambda exception"); }));
	EXPECT_THROW(
		*zth::fiber([]() { throw std::runtime_error("Lambda exception"); }),
		std::runtime_error);
#endif // ZTH_FUTURE_EXCEPTION

	zth::outOfWork();
}

TEST(FiberTest, functor)
{
	struct F1 {
		int operator()(int a, int b)
		{
			return a + b;
		}
	};

	F1 f1;
	EXPECT_EQ(*zth::fiber(f1, 5, 6), 11);

	struct F2 {
		int operator()(int a, int b) const noexcept
		{
			return a + b;
		}
	};

	F2 f2;
	EXPECT_EQ(*zth::fiber(f2, 6, 7), 13);
	EXPECT_EQ(*zth::fiber(F2{}, 6, 8), 14);

	std::function<int(int, int)> stdf = [](int a, int b) { return a + b; };
	EXPECT_EQ(*zth::fiber(stdf, 7, 8), 15);

	zth::outOfWork();
}
