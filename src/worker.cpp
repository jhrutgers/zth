#include <zth>

#include <cstring>

namespace zth {

pthread_key_t Worker::m_currentWorker;

void worker_global_init() __attribute__((constructor));
void worker_global_init() {
	zth_dbg(banner, "%s", banner());

	int res = pthread_key_create(&Worker::m_currentWorker, NULL);
	if(res)
		zth_abort("Cannot create Worker's key; %s (error %d)", strerror(res), res);
}

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
