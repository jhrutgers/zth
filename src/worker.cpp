#include <zth>

#include <cstring>

namespace zth {

ZTH_TLS_DEFINE(Worker*, currentWorker_, NULL)

void worker_global_init() {
	zth_dbg(banner, "%s", banner());
}
INIT_CALL(worker_global_init)

int Runnable::run() {
	Worker* w = Worker::currentWorker();
	if(unlikely(!w))
		return EAGAIN;

	Fiber* f = new Fiber(&Runnable::entry_, (void*)this);
	int res;
	if(unlikely((res = fiberHook(*f)))) {
		// Rollback.
		delete f;
		return res;
	}

	w->add(f);
	return 0;
}

} // namespace
