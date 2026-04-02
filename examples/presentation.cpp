/*
 * SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <zth>

#include <random>

static_assert(__cplusplus >= 202002L, "This example requires C++20 or later.");
static_assert(ZTH_HAVE_EXCEPTIONS, "This example requires exception support.");

/*! \brief Generate a random amount of strand within [1, 10]. */
static float some_strand()
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
#include <thread>

static std::mutex mtx;
static std::list<float> strand_supply;

/*! \brief A thread producing \p count pieces of strand. */
static void threaded_produce_strand(int count)
{
	for(int i = 0; i < count; i++) {
		std::this_thread::sleep_for(std::chrono::milliseconds(100));

		float x = some_strand();
		printf("Threaded producer: creating %g of strand\n", (double)x);
		std::lock_guard<std::mutex> lock{mtx};
		strand_supply.push_back(x);
	}
}

/*! \brief A thread consuming pieces of strand. */
static void threaded_consume_strand(std::stop_token const& stoken, float& rope)
{
	while(!stoken.stop_requested()) {
		float x = std::numeric_limits<float>::quiet_NaN();
		{
			std::lock_guard<std::mutex> lock{mtx};
			if(!strand_supply.empty()) {
				x = strand_supply.front();
				strand_supply.pop_front();
			}
		}

		if(!std::isnan(x)) {
			printf("Threaded consumer: consuming %g of strand\n", (double)x);
			rope += x * 0.1F;
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
		} else {
			std::this_thread::sleep_for(std::chrono::milliseconds(150));
		}
	}
}

static void threaded_example()
{
	float rope = 0.0F;
	std::thread producer{threaded_produce_strand, 100};
	std::jthread consumer{threaded_consume_strand, std::ref(rope)};
	producer.join();
	consumer.request_stop();
	consumer.join();
	printf("Threads produced %g of rope.\n", (double)rope);
}



// Fibered example

/*! \brief A fiber producing \p count pieces of strand. */
static void fibered_produce_strand(int count)
{
	for(int i = 0; i < count; i++) {
		zth::nap(std::chrono::milliseconds(100));

		float x = some_strand();
		printf("Fibered producer: creating %g of strand\n", (double)x);
		strand_supply.push_back(x);
	}
}

/*! \brief A fiber consuming pieces of strand. */
static void fibered_consume_strand(float& rope)
{
	while(true) {
		while(strand_supply.empty())
			zth::nap(std::chrono::milliseconds(150));

		float x = strand_supply.front();
		strand_supply.pop_front();

		printf("Fibered consumer: consuming %g of strand\n", (double)x);
		rope += x * 0.1F;
		zth::nap(std::chrono::milliseconds(50));
	}
}

static void fibered_example()
{
	float rope = 0.0F;
	auto producerFiber = zth::fiber(fibered_produce_strand, 100);
	auto consumerFiber = zth::fiber(fibered_consume_strand, rope);
	*producerFiber;
	consumerFiber.cancel();
	printf("Fibers produced %g of rope.\n", (double)rope);
}



// Coro example

/*! \brief A generator producing \p count pieces of strand. */
static zth::coro::generator<float> coro_produce_strand(int count)
{
	for(int i = 0; i < count; i++) {
		float x = some_strand();
		printf("Coro producer: creating %g of strand\n", (double)x);
		co_yield x;
	}
}

/*! \brief A task consuming pieces of strand. */
static zth::coro::task<float> coro_consume_strand(zth::coro::generator<float> producer)
{
	float rope = 0.0F;

	try {
		while(true) {
			float x = co_await producer;

			printf("Coro consumer: consuming %g of strand\n", (double)x);
			rope += x * 0.1F;
			zth::nap(std::chrono::milliseconds(50));
		}
	} catch(zth::coro_already_completed const&) {
		co_return rope;
	}
}

static void coro_example()
{
	auto producer = coro_produce_strand(100);
	auto consumer = coro_consume_strand(producer);
	float x = consumer.run();
	printf("Coroutines produced %g of rope.\n", (double)x);
}



// Main

int main_fiber(int argc [[maybe_unused]], char** argv [[maybe_unused]])
{
	printf("Running threaded example...\n");
	threaded_example();

	printf("Running fibered example...\n");
	fibered_example();

	printf("Running coro example...\n");
	coro_example();

	return 0;
}
