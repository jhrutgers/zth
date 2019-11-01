#include <zth>

void main_fiber(int argc, char** argv) {
	while(true) {
		printf("on\n");
		zth::nap(1);
		printf("off\n");
		zth::nap(1);
	}
}
