#include <zth>

#ifndef ZTH_OS_WINDOWS
#  include <execinfo.h>
#endif

#define UNW_LOCAL_ONLY
#include <libunwind.h>

namespace zth {

std::list<void*> backtrace(size_t depth) {
	std::list<void*> res;

#ifndef ZTH_OS_WINDOWS
	void* buf[depth];
	int cnt = ::backtrace(buf, (int)depth);

	for(int i = 0; i < cnt; i++)
		res.push_back(buf[cnt]);
#endif
	printf("%d\n", cnt);

	return res;
}

void print_backtrace(size_t depth) {
	std::list<void*> res = backtrace(depth);

	size_t i = 0;
	for(std::list<void*>::const_iterator it = res.cbegin(); it != res.cend(); ++it, i++)
		zth_log("%2zu: %p\n", i, *it);

	unw_cursor_t cursor; unw_context_t uc;
	unw_word_t ip, sp;

	unw_getcontext(&uc);
	unw_init_local(&cursor, &uc);
	while (unw_step(&cursor) > 0) {
		unw_get_reg(&cursor, UNW_REG_IP, &ip);
		unw_get_reg(&cursor, UNW_REG_SP, &sp);
		printf ("ip = %lx, sp = %lx\n", (long) ip, (long) sp);
	}
}

} // namespace
