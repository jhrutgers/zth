#include <zth>

using namespace zth;

#if 0
struct State {
	typedef enum { init, s1, s2, end } type;
};
template <> cow_string zth::str<State::type>(State::type value) { return zth::str((int)value); }
#else
namespace State {
	static char const* init = "init";
	static char const* s1 = "s1";
	static char const* s2 = "s2";
	static char const* end = "end";
	typedef char const* type;
}
#endif

typedef FsmCallback<State::type> Fsm_type;
Fsm_type::Description desc = {
	State::init,
		guards::always, State::s1,
		guards::end,
	State::s1,
		guards::timeout_s<1>, State::s2,
		guards::end,
	State::s2,
		guards::timeout_s<2>, State::end,
		guards::end,
	State::end,
		guards::end,
};

void cb(Fsm_type& fsm) {
	printf("state = %s\n", str(fsm.state()).c_str());
}

Fsm_type fsm(desc, &cb);

void main_fiber(int argc, char** argv) {
	printf("desc = %p\n", desc);
	fsm.run();
}

