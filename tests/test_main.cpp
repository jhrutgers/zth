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

#include <gtest/gtest.h>

#include <zth>

namespace {

TEST(Zth, DummySuccess)
{
	SUCCEED();
}

#if 0
TEST(Zth, DummyFail)
{
	FAIL();
}
#endif

} // namespace

static int test_main()
{
	return RUN_ALL_TESTS();
}
zth_fiber(test_main)

int main(int argc, char** argv)
{
	int res = 0;
	testing::InitGoogleTest(&argc, argv);
	zth_preinit();
	{
		zth::Worker w;
		test_main_future f = async test_main();
		w.run();
		res = f->value();
	}
	zth_postdeinit();
	return res;
}

