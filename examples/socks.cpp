/*
 * SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <zth>

#include <cstdio>
using namespace std;

#ifdef ZTH_OS_WINDOWS
#  define srand48(seed) srand(seed)
#  define drand48()	((double)rand() / (double)RAND_MAX)
#endif

struct Sock {
	enum Side { Left, Right };
	Sock(int i, Side side)
		: i(i)
		, side(side)
		, other()
		, str(zth::format("%s sock %d", side == Left ? "left" : "right", i))
	{}

	zth::Future<> done;
	int i;
	Side side;
	Sock* other;
	zth::string str;
};

struct Socks {
	explicit Socks(int i)
		: left(i, Sock::Left)
		, right(i, Sock::Right)
		, str(zth::format("socks %d", i))
	{
		left.other = &right;
		right.other = &left;
	}

	Sock left;
	Sock right;
	zth::string str;
};

void takeSocks(int count);
Socks* wearSocks(Socks& socks);
void washSock(Sock& sock);
zth_fiber(takeSocks, wearSocks, washSock)

void takeSocks(int count)
{
	std::list<wearSocks_future> allSocks;

	for(int i = 1; i <= count; i++) {
		Socks* socks = new Socks(i);
		printf("Take %s\n", socks->str.c_str());

#if __cplusplus >= 201103L
		allSocks.emplace_back(async wearSocks(*socks));
#else
		allSocks.push_back(async wearSocks(*socks));
#endif

		zth::nap(0.5);
	}

	// Store the socks in the same order as they were worn.
	for(decltype(allSocks.begin()) it = allSocks.begin(); it != allSocks.end(); ++it) {
		// *it is a SharedPointer<Future<Socks*> >
		Socks* socks = (**it)->value();
		printf("Store %s\n", socks->str.c_str());
		delete socks;
	}
}

Socks* wearSocks(Socks& socks)
{
	// NOLINTNEXTLINE(concurrency-mt-unsafe)
	zth::nap(drand48());
	printf("Wear %s\n", socks.str.c_str());

	// Done wearing, go wash both socks.
	async washSock(socks.left); // in parallel
	washSock(socks.right);	    // done right here and now

	// Wait till the washing sequence has completed.
	socks.left.done.wait();

	printf("Fold %s\n", socks.str.c_str());
	// NOLINTNEXTLINE(concurrency-mt-unsafe)
	zth::nap(drand48());
	return &socks;
}

void washSock(Sock& sock)
{
	// NOLINTNEXTLINE(concurrency-mt-unsafe)
	zth::nap(drand48());

	printf("Wash %s\n", sock.str.c_str());
	// NOLINTNEXTLINE(concurrency-mt-unsafe)
	zth::nap(drand48());

	printf("Iron %s\n", sock.str.c_str());
	// NOLINTNEXTLINE(concurrency-mt-unsafe)
	zth::nap(drand48());

	sock.done.set();
}

int main(int argc, char** argv)
{
	int count = 10;
	if(argc > 1)
		count = atoi(argv[1]);

	// NOLINTNEXTLINE(concurrency-mt-unsafe)
	srand48((long)time(nullptr));
	zth::Worker w;
	async takeSocks(count);
	w.run();
}
