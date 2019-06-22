#include <zth>

#include <cstdio>
using namespace std;

#ifdef ZTH_OS_WINDOWS
#  define srand48(seed)	srand(seed)
#  define drand48()		((double)rand() / (double)RAND_MAX)
#endif

struct Sock {
	enum Side { Left, Right };
	Sock(int i, Side side) : i(i), side(side), other(), str(zth::format("%s sock %d", side == Left ? "left" : "right", i)) {}

	zth::Future<> done;
	int i;
	Side side;
	Sock* other;
	std::string str;
};

struct Socks {
	Socks(int i) : left(i, Sock::Left), right(i, Sock::Right), str(zth::format("socks %d", i)) {
		left.other = &right; right.other = &left; }

	Sock left;
	Sock right;
	std::string str;
};

void takeSocks(int count);
Socks* wearSocks(Socks& socks);
void washSock(Sock& sock);
make_fibered(takeSocks, wearSocks, washSock)

void takeSocks(int count) {
	std::list<wearSocks_future> allSocks;

	for(int i = 1; i <= count; i++) {
		Socks* socks = new Socks(i);
		printf("Take %s\n", socks->str.c_str());

		allSocks.push_back(async wearSocks(*socks));

		zth::nap(0.5);
	}

	// Store the socks in the same order as they were worn.
	for(decltype(allSocks.begin()) it = allSocks.begin(); it != allSocks.end(); ++it) {
		// *it is a SharedPointer<Future<Socks*>>
		Socks* socks = ***it;
		printf("Store %s\n", socks->str.c_str());
		delete socks;
	}
}

Socks* wearSocks(Socks& socks) {
	zth::nap(drand48());
	printf("Wear %s\n", socks.str.c_str());

	// Done wearing, go wash both socks.
	async washSock(socks.left);		// in parallel
	washSock(socks.right);			// done right here and now

	// Wait till the washing sequence has completed.
	socks.left.done.wait();

	printf("Fold %s\n", socks.str.c_str());
	zth::nap(drand48());
	return &socks;
}

void washSock(Sock& sock) {
	zth::nap(drand48());

	printf("Wash %s\n", sock.str.c_str());
	zth::nap(drand48());

	printf("Iron %s\n", sock.str.c_str());
	zth::nap(drand48());

	sock.done.set();
}

int main() {
	srand48(time(NULL));
	zth::Worker w;
	async takeSocks(10);
	w.run();
}

