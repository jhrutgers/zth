/*
 * SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
 *
 * SPDX-License-Identifier: MPL-2.0
 */

// This file is included by test_fiber98.cpp and test_fiber11.cpp.  The test
// cases are the same, but the C++98-compatible zth::FiberType implementation
// is enforced for testing purposes. They should behave the same, but the C++11
// implementation is more efficient.

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
	async fiber_v();

	fiber_v_future f = async fiber_v();
	f->wait();
}

static int fiber_i()
{
	return 1;
}
zth_fiber(fiber_i)

TEST(FiberTest, i)
{
	async fiber_i();

	fiber_i_future f = async fiber_i();
	EXPECT_EQ(f->value(), 1);
}

static int fiber_ii(int i)
{
	return i + 1;
}
zth_fiber(fiber_ii)

TEST(FiberTest, ii)
{
	async fiber_ii(2);

	fiber_ii_future f = async fiber_ii(3);
	EXPECT_EQ(f->value(), 4);
}

// cppcheck-suppress constParameter
static int fiber_iPi(int* i)
{
	return *i + 1;
}
zth_fiber(fiber_iPi)

TEST(FiberTest, iPi)
{
	int i = 4;
	async fiber_iPi(&i);

	fiber_iPi_future f = async fiber_iPi(&i);
	EXPECT_EQ(f->value(), 5);
}

static int fiber_iRi(int& i)
{
	return ++i;
}
zth_fiber(fiber_iRi)

TEST(FiberTest, iRi)
{
	int i = 5;
	fiber_iRi_future f = async fiber_iRi(i);
	EXPECT_EQ(f->value(), 6);
	EXPECT_EQ(i, 6);

	async fiber_iRi(i);
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
	fiber_viRd_future f = async fiber_viRd(i, d);
	f->wait();
	EXPECT_DOUBLE_EQ(d, 0.6);

	async fiber_viRd(i, d);
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
	fiber_did_future f = async fiber_did(i, d);
	EXPECT_DOUBLE_EQ(f->value(), 0.7);

	async fiber_did(i, d);
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
	fiber_viRdf_future r = async fiber_viRdf(i, d, f);
	r->wait();
	EXPECT_DOUBLE_EQ(d, 1.6);

	async fiber_viRdf(i, d, f);
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
	fiber_didf_future r = async fiber_didf(i, d, f);
	EXPECT_DOUBLE_EQ(r->value(), 1.8);

	async fiber_didf(i, d, f);
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
	fiber_viRdfPl_future r = async fiber_viRdfPl(i, d, f, &l);
	r->wait();
	EXPECT_DOUBLE_EQ(d, 3.2);

	async fiber_viRdfPl(i, d, f, &l);
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
	fiber_diRdfPl_future r = async fiber_diRdfPl(i, d, f, &l);
	EXPECT_DOUBLE_EQ(r->value(), 3.4);
	EXPECT_DOUBLE_EQ(d, 3.4);

	async fiber_diRdfPl(i, d, f, &l);
}

TEST(FiberTest, direct_v)
{
	using fiber_type = zth::fiber_type<decltype(&fiber_v)>;

	fiber_type::factory factory = zth::fiber(fiber_v);
	fiber_type::fiber fiber = factory();
	fiber_type::future future = fiber;
	fiber_type::future future2 = fiber.withFuture();
	future->wait();
	EXPECT_TRUE(future2->valid());
}

TEST(FiberTest, direct_ii)
{
	auto factory = zth::fiber(fiber_ii, "fiber_ii()");
	auto future = factory(5).withFuture();
	EXPECT_EQ(future->value(), 6);
}

TEST(FiberTest, direct_iPi)
{
	int i = 4;
	auto f = zth::fiber(fiber_iPi)(&i).withFuture();
	EXPECT_EQ(f->value(), 5);
}

TEST(FiberTest, direct_viRdf)
{
	int i = 8;
	double d = 0.1;
	float f = 2.0f;

	auto future =
		(zth::fiber(fiber_viRdf)(i, d, f) << zth::setName("fiber_viRdf()")).withFuture();
	future->wait();
	EXPECT_DOUBLE_EQ(d, 1.6);

	async fiber_viRdf(i, d, f);
}

TEST(FiberTest, direct_diRdfPl)
{
	int i = 12;
	double d = 0.1;
	float f = 2.0f;
	long l = 1;
	auto r = zth::fiber(fiber_diRdfPl, "fiber_diRdfPl")(i, d, f, &l).withFuture();
	EXPECT_DOUBLE_EQ(r->value(), 3.4);
	EXPECT_DOUBLE_EQ(d, 3.4);
}
