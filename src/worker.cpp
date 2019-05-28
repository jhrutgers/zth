#include <zth>

namespace zth {

pthread_key_t Worker::m_currentWorker;

void worker_global_init() __attribute__((constructor));
void worker_global_init() {
	zth_dbg(banner, "%s", banner());

	int res = pthread_key_create(&Worker::m_currentWorker, NULL);
	if(res)
		zth_abort("Cannot create Worker's key; error %d", res);
}

} // namespace
