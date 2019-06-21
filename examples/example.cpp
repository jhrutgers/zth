#include <zth>

#include <cstdio>
using namespace std;

#ifdef ZTH_OS_WINDOWS
#  define srand48(seed)	srand(seed)
#  define drand48()		((double)rand() / (double)RAND_MAX)
#endif

zth::Semaphore sem;
zth::Future<int> future;

void fiber1(void*)
{
	printf("fiber 1 %" PRIu64 "\n", 0ULL);
	zth::outOfWork();
	sem.acquire(1);
	printf("fiber 1\n");
	future = 42;
}

void fiber2(void*)
{
	printf("fiber 2\n");
	zth::nap(1);
	sem.release(1);
//	zth::yield();
	printf("fiber 2\n");
	printf("future = %d\n", future.value());
}

int fiber4(int i) {
	printf("fiber 4\n");
//	for(int i = 0; i < 1000; i++)
		zth::outOfWork();
	return i + 4;
}
make_fibered(fiber4)

void fiber3() {
	printf("fiber 3\n");
	fiber4_future f = async fiber4(2);
//	for(int i = 0; i < 1000; i++)
		zth::outOfWork();
	zth_perfmark("abc zz");
	printf("got from fiber 4: %d\n", f->value());
}
make_fibered(fiber3)

void washSock(int i) {
	if(i < 0)
		printf("wash left %d\n", -i);
	else
		printf("wash right %d\n", i);

	zth::nap(drand48());
}
make_fibered(washSock)

void wearSocks(int i) {
	printf("wear %d\n", i);
	washSock_future left = async washSock(-i);
	washSock_future right = async washSock(i);
	left->wait();
	right->wait();
	printf("washed %d\n", i);
}
make_fibered(wearSocks)

void socks() {
	srand48(time(NULL));
	for(int i = 1; i <= 10; i++) {
		zth::nap(drand48());
		async wearSocks(i);
	}
}
make_fibered(socks)

struct Int : public zth::Listable<Int> {
	Int(int value) : value(value) {}
	bool operator<(Int const& rhs) const { return value < rhs.value; }
	std::string str() const { return zth::format("%d", value); }
	int value;
};

int main()
{
#if 0
	for(double s1 = -5.0; s1 < 5; s1 += 0.1)
		for(double s2 = -5.0; s2 < 5; s2 += 0.1) {
			zth::TimeInterval ti1 = s1;
			zth::TimeInterval ti2 = s2;
			{
				double exp = ti1.s() + ti2.s();
				double s = (ti1 + ti2).s();
				if(std::fabs(exp - s) > 0.0001)
					printf("%6g + %6g = %6g (exp %6g)\n", s1, s2, s, exp);
			}
			{
				double exp = ti1.s() - ti2.s();
				double s = (ti1 - ti2).s();
				if(std::fabs(exp - s) > 0.0001)
					printf("%6g - %6g = %6g (exp %6g)\n", s1, s2, s, exp);
			}
		}

	Int i1 = 1;
	Int i2 = 2;
	Int i4 = 3;
	Int i3 = 3;
	Int i5 = 5;
	Int i6 = 6;
	Int i7 = 7;
	zth::SortedList<Int> list;
	list.insert(i2);
	list.insert(i3);
	list.insert(i4);
	list.insert(i1);
	list.insert(i5);
	list.insert(i6);
	list.insert(i7);
	list.erase(i7);
#endif

	zth::Worker w;
//	w.add(new zth::Fiber(&fiber1));
//	w.add(new zth::Fiber(&fiber2));

//	async fiber3();
	async socks();
	w.run();
}

