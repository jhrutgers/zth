#include <zth>
#include <libzth/fsm.h>

#if 0
enum { StateInitial, StateA, StateB };

void foo() {
	
	Fsm<> fsm;

	switch(fsm.eval()) {
	case StateInitial:
		if(fsm.entry()) {
			printf("entry\n");
			fsm.setTimeout(0.1);
		}

		print("in state\n");
		if(fsm.timedout())
			fsm.transition(StateA);

		if(fsm.exit()) {
			printf("exit\n");
		}
		break;
	default:;
	}
}

#endif

using namespace zth;

enum State { stateInit, stateEnd };
typedef FsmCallback<State> Fsm_type;
Fsm_type::Description desc = {
	stateInit,
		guards::always, stateEnd,
		guards::end,
	stateEnd,
		guards::end,
};

void cb(Fsm_type& fsm) {
	printf("state = %d\n", (int)fsm.state());
}

Fsm_type fsm(desc, &cb);

int main() {
	printf("desc = %p\n", desc);
	fsm.eval();
}

