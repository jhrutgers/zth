#include <zth>

// Choose your State type...
#ifdef NDEBUG
// State description with enums, which is as efficient as strings, but does not emit the string names in the final binary.
struct State {
	typedef enum { init, peek, blink_on, blink_off, red_wait, red, amber, green, end } type;
};
// Do provide a str() specialization for this type.
namespace zth { template <> cow_string str<State::type>(State::type value) { return str((int)value); } }
#else
// Nicely named states, which eases debugging.
namespace State {
	static char const* init = "init";
	static char const* peek = "peek";
	static char const* blink_on = "blink on";
	static char const* blink_off = "blink off";
	static char const* red_wait = "red (wait)";
	static char const* red = "red (ready)";
	static char const* amber = "amber";
	static char const* green = "green";
	static char const* end = "end";
	typedef char const* type;
}
#endif

enum Input { traffic };
namespace zth { template <> cow_string str<Input>(Input value) { return str((int)value); } }

typedef zth::FsmCallback<State::type, zth::Timestamp&, Input> Fsm_type;

template <typename Fsm>
static zth::TimeInterval tooLongGreen(Fsm& fsm) { return zth::TimeInterval(10) - (zth::Timestamp::now() - fsm.template as<Fsm_type>().callbackArg()); }

Fsm_type::Description desc = {
	State::init,
		zth::guards::always,					State::blink_on,
		zth::guards::end,
	State::peek,
		zth::guards::peek<Fsm_type,traffic>,	State::amber,
		zth::guards::always,					State::blink_on,
		zth::guards::end,
	State::blink_on,
		zth::guards::timeout_s<1>,				State::blink_off,
		zth::guards::end,
	State::blink_off,
		zth::guards::timeout_s<1>,				State::peek,
		zth::guards::end,
	State::red_wait,
		zth::guards::timeout_s<2>,				State::red,
		zth::guards::end,
	State::red,
		zth::guards::input<Fsm_type,traffic>,	State::green,
		zth::guards::timeout_s<10>,				State::blink_off,
		zth::guards::end,
	State::green,
		tooLongGreen,							State::amber,
		zth::guards::input<Fsm_type,traffic>,	State::green,
		zth::guards::timeout_s<3>,				State::amber,
		zth::guards::end,
	State::amber,
		zth::guards::timeout_s<2>,				State::red_wait,
		zth::guards::end,
	State::end,
		zth::guards::end,
};

void cb(Fsm_type& fsm, zth::Timestamp& green) {
	if(fsm.state() == State::red && fsm.exit() && fsm.next() == State::green)
		green = zth::Timestamp::now();

	if(!fsm.entry())
		return;

	// I know, switch(fsm.state()) is nicer, but then the State must be some integral type...
#ifdef ZTH_OS_WINDOWS
	// No ANSI terminal support.
	if(fsm.state() == State::blink_on || fsm.state() == State::amber)
		printf("amber\n");
	else if(fsm.state() == State::red_wait)
		printf("red\n");
	else if(fsm.state() == State::green)
		printf("green\n");
	else if(fsm.state() == State::blink_off)
		printf("(off)\n");
#else
	if(fsm.state() == State::init) {
		printf("\x1b[0m ________ \n");
		printf("/        \\\n");
		printf("|   ..   |\n");
		printf("|   ..   |\n");
		printf("|        |\n");
		printf("|   ..   |\n");
		printf("|   ..   |\n");
		printf("|        |\n");
		printf("|   ..   |\n");
		printf("|   ..   |\n");
		printf("\\________/\n");
		printf("    ||    \n");
		printf("    ||    \n");
		printf("    ||    \n");
		printf("    ||    \n");
		if(!zth_config(EnableDebugPrint))
			printf("\x1b[15A\x1b[s");
	} else if(fsm.state() == State::blink_on || fsm.state() == State::amber) {
		if(!zth_config(EnableDebugPrint))
			printf("\n\x1b[u");
		printf(" ________ \n");
		printf("/        \\\n");
		printf("|   ..   |\n");
		printf("|   ..   |\n");
		printf("|        |\n");
		printf("|   \x1b[33m00\x1b[0m   |\n");
		printf("|   \x1b[33m00\x1b[0m   |\n");
		printf("|        |\n");
		printf("|   ..   |\n");
		printf("|   ..   |\n");
		printf("\\________/\n");
		if(!zth_config(EnableDebugPrint))
			printf("\n\x1b[11A");
	} else if(fsm.state() == State::red_wait) {
		if(!zth_config(EnableDebugPrint))
			printf("\n\x1b[u");
		printf(" ________ \n");
		printf("/        \\\n");
		printf("|   \x1b[31;1m00\x1b[0m   |\n");
		printf("|   \x1b[31;1m00\x1b[0m   |\n");
		printf("|        |\n");
		printf("|   ..   |\n");
		printf("|   ..   |\n");
		printf("|        |\n");
		printf("|   ..   |\n");
		printf("|   ..   |\n");
		printf("\\________/\n");
		if(!zth_config(EnableDebugPrint))
			printf("\n\x1b[11A");
	} else if(fsm.state() == State::green) {
		if(!zth_config(EnableDebugPrint))
			printf("\n\x1b[u");
		printf(" ________ \n");
		printf("/        \\\n");
		printf("|   ..   |\n");
		printf("|   ..   |\n");
		printf("|        |\n");
		printf("|   ..   |\n");
		printf("|   ..   |\n");
		printf("|        |\n");
		printf("|   \x1b[32;1m00\x1b[0m   |\n");
		printf("|   \x1b[32;1m00\x1b[0m   |\n");
		printf("\\________/\n");
		if(!zth_config(EnableDebugPrint))
			printf("\n\x1b[11A");
	} else if(fsm.state() == State::blink_off) {
		if(!zth_config(EnableDebugPrint))
			printf("\n\x1b[u");
		printf(" ________ \n");
		printf("/        \\\n");
		printf("|   ..   |\n");
		printf("|   ..   |\n");
		printf("|        |\n");
		printf("|   ..   |\n");
		printf("|   ..   |\n");
		printf("|        |\n");
		printf("|   ..   |\n");
		printf("|   ..   |\n");
		printf("\\________/\n");
		if(!zth_config(EnableDebugPrint))
			printf("\n\x1b[11A");
	}
	fflush(NULL);
#endif
}

static zth::Timestamp green;
static Fsm_type fsm(desc, &cb, green);

static void trafficDetect() {
	char buf;
	while(read(0, &buf, 1) > 0)
		fsm.input(traffic);
}
zth_fiber(trafficDetect)

void main_fiber(int argc, char** argv) {
	async trafficDetect();
	fsm.run();
}

