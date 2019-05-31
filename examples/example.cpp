#include <zth>

#include <cstdio>
using namespace std;

void* fiber1(void*)
{
	printf("fiber 1\n");
	zth::outOfWork();
	printf("fiber 1\n");
	return NULL;
}

void* fiber2(void*)
{
	printf("fiber 2\n");
	zth::yield();
	printf("fiber 2\n");
	return NULL;
}

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

	zth::Worker w;
	w.add(new zth::Fiber(&fiber1));
	w.add(new zth::Fiber(&fiber2));
	w.run();
}

