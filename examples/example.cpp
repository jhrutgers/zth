#include <zth>

#include <cstdio>
using namespace std;

void* fiber1(void*)
{
	printf("fiber 1\n");
	zth::yield();
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
	zth::Worker w;
	w.add(new zth::Fiber(&fiber1));
	w.add(new zth::Fiber(&fiber2));
	w.run();
}

