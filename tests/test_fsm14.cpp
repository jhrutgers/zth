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

#include <libzth/fsm14.h>
#include <zth>

#include <gtest/gtest.h>

TEST(Fsm14, a)
{
	using namespace zth::fsm;

	// clang-format off
	constexpr auto transitions = compile(
		"a"_S	>>= "b",
		"b"	>>= "end"_S,
		"end" + stop
	);
	// clang-format on

	transitions.dump();
}

#if 0
namespace ns {

	using namespace fsm;

static int i = 42;
static bool positive()
{
	return i > 0;
}
static constexpr auto big_i = guard(positive);
static constexpr auto print = action([&](){ printf("p %d\n", i); });

struct MyFsm : public Fsm {
	int i = 1;
	bool flag = true;

	virtual void enter() override
	{
		printf("enter\n");
		Fsm::enter();
	}

	bool inci2()
	{
		printf("%d\n", i++);
		return i < 15;
	}

	bool haveFlag()
	{
		bool res = flag;
		flag = false;
		return res;
	}
};

static constexpr auto inci = guard([](MyFsm& fsm) {
		printf("%d\n", fsm.i++);
		return fsm.i < 10;
	});

template <int value>
constexpr auto incif_v = guard([](MyFsm& fsm) {
	printf("%d\n", fsm.i++);
	return fsm.i < value;
});

static constexpr auto inci2 = guard(&MyFsm::inci2);

static constexpr auto haveFlag = guard(&MyFsm::haveFlag);

static constexpr auto s = compile(
	"initial"_S						>>= "state",
	"state"				/ nothing	>>= "a",
	"state"		+ big_i / print		>>= "state",
	"a"			+ entry / print,
	"a"			+ incif_v<8>		>>= "a",
	"a"			+ inci2,
	"a"								>>= "b"_S,
	"b"			+ never				>>= "b",
				+ always			>>= "d",
	"c"			+ never,
	"d"			+ haveFlag / push	>>= "pushed",
				+ always			>>= "c",
	"pushed"			/ pop
);

} // ns

int main()
{


//	printf("en %s\n", s.enabled() ? "yes" : "no");
//	ns::s.dump();
	ns::MyFsm f;
	ns::s.init(f).run();
}
#endif
