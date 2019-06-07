#ifndef __ZTH_PERF_H
#define __ZTH_PERF_H
#ifdef __cplusplus

#include <libzth/config.h>
#include <list>

namespace zth {

	std::list<void*> backtrace(size_t depth = 32);
	void print_backtrace(size_t depth = 32);

} // namespace
#endif
#endif // __ZTH_PERF_H
