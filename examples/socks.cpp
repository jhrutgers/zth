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

static void nap_a_bit()
{
	// NOLINTNEXTLINE(concurrency-mt-unsafe)
	zth::nap(drand48());
}

struct Sock {
	enum Side { Left, Right };
	Sock(int i, Side side)
		: str(zth::format("%s sock %d", side == Left ? "left" : "right", i))
	{}

	zth::string str;
};

class Socks : public zth::RefCounted {
	ZTH_CLASS_NOCOPY(Socks)
public:
	explicit Socks(int i)
		: left(i, Sock::Left)
		, right(i, Sock::Right)
		, str(zth::format("socks %d", i))
	{}

	virtual ~Socks() noexcept override
	{
		printf("Release %s\n", str.c_str());
	}

	Sock left;
	Sock right;
	zth::string str;
};

static void washSock(Sock& sock)
{
	nap_a_bit();

	printf("    Wash %s\n", sock.str.c_str());
	nap_a_bit();

	printf("    Iron %s\n", sock.str.c_str());
	nap_a_bit();
}

static Socks* useSocks(Socks& socks)
{
	nap_a_bit();

	printf("  Wear %s\n", socks.str.c_str());

	// Done wearing, go wash both socks.
	{
		zth::joiner j(zth::fiber(washSock, socks.left), zth::fiber(washSock, socks.right));

		nap_a_bit();
		printf("  I don't do the dirty work for %s\n", socks.str.c_str());

		// j is out of scope here, so this is the point where the join actually happens.
	}

	printf("  Fold %s\n", socks.str.c_str());
	nap_a_bit();
	return &socks;
}

static void takeSocks(int count)
{
	std::list<zth::fiber_future<Socks*> /**/> allSocks;

	for(int i = 1; i <= count; i++) {
		Socks* socks = new Socks(i);
		socks->used();
		printf("Take %s\n", socks->str.c_str());

		// Give the pair of socks to someone else to use.
		allSocks.push_back(zth::fiber(useSocks, *socks));

		zth::nap(0.5);
	}

	// Store the socks in the same order as they were worn.
	for(decltype(allSocks.begin()) it = allSocks.begin(); it != allSocks.end(); ++it) {
		// *it is a SharedReference<Future<Socks*> >
		Socks* socks = **it;
		printf("Store %s\n", socks->str.c_str());
		socks->unused();
	}
}

int main_fiber(int argc, char** argv)
{
	int count = 10;
	if(argc > 1)
		count = (int)strtol(argv[1], nullptr, 0);

	// NOLINTNEXTLINE(concurrency-mt-unsafe)
	srand48((long)time(nullptr));

	zth::fiber(takeSocks, count);
	return 0;
}
