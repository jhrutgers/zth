#include <libzth/worker.h>
#include <libzth/async.h>

namespace zth {

ZTH_TLS_DEFINE(Worker*, currentWorker_, NULL)

void worker_global_init() {
	zth_dbg(banner, "%s", banner());
}
INIT_CALL(worker_global_init)

} // namespace

__attribute__((weak)) void main_fiber(int argc, char** argv) {}
make_fibered(main_fiber);

__attribute__((weak)) int main(int argc, char** argv) {
	zth::Worker w;
	async main_fiber(argc, argv);
	w.run();
	return 0;
}

