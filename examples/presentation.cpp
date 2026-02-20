/*
 * SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <zth>

static_assert(__cplusplus >= 202002L, "This example requires C++20 or later.");
static_assert(ZTH_HAVE_EXCEPTIONS, "This example requires exception support.");

static float random_material()
{
	// NOLINTNEXTLINE
	static std::mt19937 gen{(unsigned)time(nullptr)};
	static std::uniform_real_distribution<float> distrib{1, 10};
	return distrib(gen);
}



// Threaded example.

#include <limits>
#include <list>
#include <mutex>
#include <random>
#include <thread>

static std::mutex mtx;
static std::list<float> fiber_supply;

static void threaded_produce_fiber(int count)
{
	for(int i = 0; i < count; i++) {
		std::this_thread::sleep_for(std::chrono::milliseconds(100));

		float x = random_material();
		printf("Threaded fiber producer: creating %g of fiber\n", (double)x);
		std::lock_guard<std::mutex> lock{mtx};
		fiber_supply.push_back(x);
	}
}

static void threaded_consume_fiber(std::stop_token stoken)
{
	while(!stoken.stop_requested()) {
		float x = std::numeric_limits<float>::quiet_NaN();
		{
			std::lock_guard<std::mutex> lock{mtx};
			if(!fiber_supply.empty()) {
				x = fiber_supply.front();
				fiber_supply.pop_front();
			}
		}

		if(!std::isnan(x)) {
			printf("Threaded fiber consumer: consuming %g of fiber\n", (double)x);
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
		} else {
			std::this_thread::sleep_for(std::chrono::milliseconds(150));
		}
	}
}

static void threaded_example()
{
	std::thread producer{threaded_produce_fiber, 100};
	std::jthread consumer{threaded_consume_fiber};
	producer.join();
	consumer.request_stop();
	consumer.join();
}



// Fibered example

static void fibered_produce_fiber(int count)
{
	for(int i = 0; i < count; i++) {
		zth::nap(std::chrono::milliseconds(100));

		float x = random_material();
		printf("Fibered fiber producer: creating %g of fiber\n", (double)x);
		fiber_supply.push_back(x);
	}
}

static void fibered_consume_fiber()
{
	while(true) {
		if(fiber_supply.empty()) {
			zth::nap(std::chrono::milliseconds(150));
			continue;
		}

		float x = fiber_supply.front();
		fiber_supply.pop_front();

		printf("Fibered fiber consumer: consuming %g of fiber\n", (double)x);
		zth::nap(std::chrono::milliseconds(50));
	}
}

static void fibered_example()
{
	auto producerFiber = zth::fiber(fibered_produce_fiber, 100);
	auto consumerFiber = zth::fiber(fibered_consume_fiber);
	*producerFiber;
	consumerFiber.cancel();
	*consumerFiber;
}



// Coro example

static zth::coro::generator<float> coro_produce_fiber(int count)
{
	for(int i = 0; i < count; i++) {
		float x = random_material();
		printf("Coro fiber producer: creating %g of fiber\n", (double)x);
		co_yield x;
	}
}

static zth::coro::task<> coro_consume_fiber(zth::coro::generator<float> producer)
{
	while(true) {
		float x = co_await producer;

		printf("Coro fiber consumer: consuming %g of fiber\n", (double)x);
		zth::nap(std::chrono::milliseconds(50));
	}
}

static void coro_example()
{
	auto producer = coro_produce_fiber(100);
	auto consumer = coro_consume_fiber(producer);

	try {
		consumer.run();
	} catch(const zth::coro_already_completed&) {
		// Done.
	}
}



// Main

int main()
{
	printf("Running threaded example...\n");
	threaded_example();

	printf("Running fibered example...\n");
	fibered_example();

	printf("Running coro example...\n");
	coro_example();

	return 0;
}
