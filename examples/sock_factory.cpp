/*
 * SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
 *
 * SPDX-License-Identifier: CC0-1.0
 */

// This example shows how to use C++20 coroutines in Zth.

#include <zth>

#include <cstdio>
#include <random>

static zth::coro::generator<float> fiber_factory(float supply)
{
	// NOLINTNEXTLINE
	std::mt19937 gen{(unsigned)time(nullptr)};
	std::uniform_real_distribution<float> distrib{1, 10};

	while(supply > 0) {
		float x = std::min(supply, distrib(gen));
		printf("Fiber factory: creating %g of fiber\n", (double)x);
		co_yield x;
		supply -= x;
	}

	printf("Fiber factory: no more fiber to create\n");
}

struct Sock {
	explicit Sock(float material_used_)
		: str{zth::format("sock %d (%g)", ++id_next, (double)material_used_)}
		, material_used{material_used_}
	{}

	static inline int id_next = 0;

	zth::string str;
	float material_used;
};

static zth::coro::generator<Sock, float>
sock_factory(zth::coro::generator<float> fiber_generator, float amount_of_fiber)
{
	while(true) {
		float got_fiber = 0;
		while(got_fiber < amount_of_fiber)
			got_fiber += co_await fiber_generator;

		Sock s{amount_of_fiber};
		printf("   Got %g of fiber, creating %s\n", (double)got_fiber, s.str.c_str());
		co_yield s;

		if(got_fiber > amount_of_fiber)
			co_yield got_fiber - amount_of_fiber; // waste
	}
}

static zth::coro::task<int>
packing_socks(zth::coro::generator<Sock, float> big, zth::coro::generator<Sock, float> small)
{
	int sets = 0;

	try {
		while(true) {
			Sock big_left = co_await big.as<Sock>();
			Sock big_right = co_await big.as<Sock>();
			Sock small_left = co_await small.as<Sock>();
			Sock small_right = co_await small.as<Sock>();
			printf("      Packing facility: packed set %d with %s, %s, %s, and %s\n",
			       ++sets, big_left.str.c_str(), big_right.str.c_str(),
			       small_left.str.c_str(), small_right.str.c_str());
		}
	} catch(zth::coro_already_completed const&) {
		// Dispose socks in the pipeline.
		if(big.valid<Sock>())
			big.value<Sock>();
		if(small.valid<Sock>())
			small.value<Sock>();
	}

	co_return sets;
}

int main_fiber(int argc, char** argv)
{
	float supply = 100.F;
	if(argc > 1)
		supply = (float)strtol(argv[1], nullptr, 10);

	auto fiber_generator = fiber_factory(supply);
	fiber_generator.fiber("fiber factory");

	auto big_sock_factory = sock_factory(fiber_generator, 12.5F);
	big_sock_factory.fiber("big sock factory");

	auto small_sock_factory = sock_factory(fiber_generator, 5.F);
	small_sock_factory.setName("small sock factory");
	// The small sock factory is so small, it does not need its own fiber.

	auto waste_collector = [](zth::coro::generator<Sock, float> g) -> zth::coro::task<float> {
		static float total_waste = 0.F;
		while(true) {
			float waste = co_await g.as<float>();
			total_waste += waste;
			printf("   Waste collector: collected %g of waste (total %g)\n",
			       (double)waste, (double)total_waste);
		}

		co_return total_waste;
	};

	zth::join(
		waste_collector(big_sock_factory).fiber("waste collector 1"),
		waste_collector(small_sock_factory).fiber("waste collector 2"),
		packing_socks(big_sock_factory, small_sock_factory).fiber("packing facility"));

	return 0;
}
