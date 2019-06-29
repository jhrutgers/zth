#include <libzth/fiber.h>
#include <libzth/worker.h>

namespace zth {

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

