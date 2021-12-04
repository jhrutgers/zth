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

static void v()
{
}
zth_fiber(v)

TEST(FiberTest, v)
{
	async v();

	v_future f = async v();
	f->wait();
}

static int i()
{
	return 1;
}
zth_fiber(i)

TEST(FiberTest, i)
{
	async i();

	i_future f = async i();
	EXPECT_EQ(f->value(), 1);
}

static int ii(int i)
{
	return i + 1;
}
zth_fiber(ii)

TEST(FiberTest, ii)
{
	async ii(2);

	ii_future f = async ii(3);
	EXPECT_EQ(f->value(), 4);
}

static int iPi(int* i)
{
	return *i + 1;
}
zth_fiber(iPi)

TEST(FiberTest, iPi)
{
	int i = 4;
	async iPi(&i);

	iPi_future f = async iPi(&i);
	EXPECT_EQ(f->value(), 5);
}

static int iRi(int& i)
{
	return ++i;
}
zth_fiber(iRi)

TEST(FiberTest, iRi)
{
	int i = 5;
	iRi_future f = async iRi(i);
	EXPECT_EQ(f->value(), 6);
	EXPECT_EQ(i, 6);

	async iRi(i);
}

static void viRd(int i, double& d)
{
	d = d * i;
}
zth_fiber(viRd)

TEST(FiberTest, viRd)
{
	int i = 6;
	double d = 0.1;
	viRd_future f = async viRd(i, d);
	f->wait();
	EXPECT_DOUBLE_EQ(d, 0.6);

	async viRd(i, d);
}

static double did(int i, double d)
{
	return d * i;
}
zth_fiber(did)

TEST(FiberTest, did)
{
	int i = 7;
	double d = 0.1;
	did_future f = async did(i, d);
	EXPECT_DOUBLE_EQ(f->value(), 0.7);

	async did(i, d);
}

static void viRdf(int i, double& d, float f)
{
	d = d * i * f;
}
zth_fiber(viRdf)

TEST(FiberTest, viRdf)
{
	int i = 8;
	double d = 0.1;
	float f = 2.0f;
	viRdf_future r = async viRdf(i, d, f);
	r->wait();
	EXPECT_DOUBLE_EQ(d, 1.6);

	async viRdf(i, d, f);
}

static double didf(int i, double d, float f)
{
	return d * i * f;
}
zth_fiber(didf)

TEST(FiberTest, didf)
{
	int i = 9;
	double d = 0.1;
	float f = 2.0f;
	didf_future r = async didf(i, d, f);
	EXPECT_DOUBLE_EQ(r->value(), 1.8);

	async didf(i, d, f);
}

static void viRdfPl(int i, double& d, float f, long* l)
{
	d = d * i * f + *l;
}
zth_fiber(viRdfPl)

TEST(FiberTest, viRdfPl)
{
	int i = 11;
	double d = 0.1;
	float f = 2.0f;
	long l = 1;
	viRdfPl_future r = async viRdfPl(i, d, f, &l);
	r->wait();
	EXPECT_DOUBLE_EQ(d, 3.2);

	async viRdfPl(i, d, f, &l);
}

static double diRdfPl(int i, double& d, float f, long* l)
{
	return d = d * i * f + *l;
}
zth_fiber(diRdfPl)

TEST(FiberTest, diRdfPl)
{
	int i = 12;
	double d = 0.1;
	float f = 2.0f;
	long l = 1;
	diRdfPl_future r = async diRdfPl(i, d, f, &l);
	EXPECT_DOUBLE_EQ(r->value(), 3.4);
	EXPECT_DOUBLE_EQ(d, 3.4);

	async diRdfPl(i, d, f, &l);
}
