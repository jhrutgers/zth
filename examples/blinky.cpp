#include <zth>

void main_fiber(int argc, char** argv) {
	while(true) {
		printf("on");
		zth::nap(1);
		printf("off");
		zth::nap(1);
	}
}
