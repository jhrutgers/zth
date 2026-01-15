/*
 * SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#include <zth>

#include <gtest/gtest.h>

TEST(Sync, Signal)
{
	zth::Signal s;
	zth::fiber([&]() { s.signal(); });
	s.wait();
}
