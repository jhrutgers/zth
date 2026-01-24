/*
 * SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <zth>

// C++20 introduces coroutines. Zth provides support to run coroutines within fibers.
//
// The main difference between fibers and coroutines is that fibers are eagerly running--they are
// active things.  Coroutines are lazy or passive--they only run when they are awaited or resumed.
// Coroutines are effectively functions that hold their state while they are suspended. They can be
// shared between fibers.

// There are two main coroutine types provided by Zth: zth::coro::task and zth::coro::generator.
// A task is just something that runs till completion.
static zth::coro::task<int> world()
{
	printf("World!!1\n");
	co_return 5;
}

static zth::coro::task<int> hello()
{
	printf("Hello\n");
	// Suspend hello() and wait till world() returns.
	int world_result = co_await world();

	printf("Back in hello(), world_result = %d\n", world_result);
	co_return world_result * 2;
}

// A generator is something that produces a sequence of values via co_yield.
static zth::coro::generator<int> count_to(int n)
{
	for(int i = 1; i <= n; i++)
		co_yield i;
}

int main_fiber(int /*argc*/, char** /*argv*/)
{
	// Run the coroutine, until completion.
	int result = hello().run();
	printf("Result from hello(): %d\n", result);

	// Use the generator to get a sequence of values.
	printf("Counting to 5:\n");
	for(auto value : count_to(5))
		printf("  %d\n", value);

	// You can also start the generator in a fiber...
	auto generator = count_to(3);
	generator.fiber("counter");

	// ...and co_await values from it in another fiber.
	auto awaiter = [](zth::coro::generator<int> g) -> zth::coro::task<> {
		printf("Counting to 3 in fiber:\n");
		for(int i = 0; i < 3; i++) {
			int x = co_await g;
			printf("  %d\n", x);
		}
	};
	*awaiter(generator).fiber("awaiter");

	// ...or share a generated between two fibers. The generator coro is now executed within the
	// fiber that does a co_await on it.
	auto awaiters = [](zth::coro::generator<int> g) -> zth::coro::task<> {
		while(true) {
			printf("%s: %d\n", zth::currentFiber().name().c_str(), co_await g);
			// Be nice for the other fiber.
			zth::outOfWork();
		}
	};
	auto gen10 = count_to(10);
	zth::join(awaiters(gen10).fiber("f1"), awaiters(gen10).fiber("f2"));

	return 0;
}
