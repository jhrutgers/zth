#include <zth>

#include <cstdio>
using namespace std;

void fiber1(void*)
{
	printf("fiber 1\n");
	zth::outOfWork();
	printf("fiber 1\n");
}

void fiber2(void*)
{
	printf("fiber 2\n");
	zth::nap(1);
//	zth::yield();
	printf("fiber 2\n");
}

struct Int : public zth::Listable<Int> {
	Int(int value) : value(value) {}
	bool operator<(Int const& rhs) const { return value < rhs.value; }
	std::string str() const { return zth::format("%d", value); }
	int value;
};

int main()
{
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

	zth::Worker w;
	w.add(new zth::Fiber(&fiber1));
	w.add(new zth::Fiber(&fiber2));
	w.run();
}

