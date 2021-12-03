#include <zth>

void main_fiber(int /*argc*/, char** /*argv*/)
{
	zth::PeriodicWakeUp w(1);

	while(true) {
		printf("on\n");
		w();
		printf("off\n");
		w();
	}
}
