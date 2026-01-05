/*
 * SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
 *
 * SPDX-License-Identifier: CC0-1.0
 */

// This example uses the old and deprecated FSM implementation.  See fsm14.cpp
// for the improved one.

// This example does not work on Windows, as Windows does not support poll()ing
// stdin. And the ANSI console might be troubling.

#include <zth>

// The easiest way to define a State type would be:
//
// enum State { init, peek, blink_on, blink_off, red_wait, red, amber, green, end };
// typedef zth::FsmCallback<State, zth::Timestamp&, Input> Fsm_type;
//
// Now, fsm.state() would return one of the State enum values, which could be
// used like this in the callback function:
//
// switch(fsm.state()) {
// case init:
//     ...
// case peek:
//     ...
// }
//
// For the sake of this example, we choose some other State types.
//

#ifdef NDEBUG
// State description with enums, which is as efficient as strings, but does not
// emit the string names in the final binary.
struct State {
	typedef enum { init, peek, blink_on, blink_off, red_wait, red, amber, green, end } type;
};

// Do provide a str() specialization for this type.
namespace zth {

template <>
cow_string str<State::type>(State::type value)
{
	return str((int)value);
}

} // namespace zth
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
} // namespace State

// There exists an str() specialization for char const*, so we don't have to define one.
#endif

// Inputs may be passed to an Fsm, which is recorded and can be processed later
// on.  This way, async input symbols are collected, like the traffic
// generation in the trafficDetect() function below.
enum Input { traffic };

namespace zth {

template <>
cow_string str<Input>(Input value)
{
	return str((int)value);
}

} // namespace zth

// As the type of the Fsm is quite complex, and you need it multiple times,
// please to a typedef.
typedef zth::FsmCallback<State::type, zth::Timestamp&, Input> Fsm_type;

// This is a custom guard function. The Fsm template is deducted, but is
// effectively Fsm_type.
template <typename Fsm>
static zth::TimeInterval tooLongGreen(Fsm& fsm)
{
	return zth::TimeInterval(10) - (zth::Timestamp::now() - fsm.callbackArg());
}

// This is the description of the state machine.  States, guards, and
// transitions are constant; it is impossible to define them dynamically.  The
// desc object is to be processed during initialization. Don't make it const.
//
// clang-format off
static Fsm_type::Description desc = {
	// The structure is as follows:
	//
	// State where we are in,
	//    guard to check,                                           state to go to when the guard is valid
	//    another guard to check if the first one is not valid,     state to go to if this guard is valid
	//    ...many other guard/state pairs...
	//    zth::guards::end to indicate the end of the guard list
	// Another state
	//    ...
	//
	// Guards are processed in order (top-down).  If no guard is valid, the Fsm
	// will remain in the current state.  The Fsm will reevaluate the guards as
	// many times as appropriate.
	//
	// States mentioned after the guards must exist within this description
	// list.  However, it does not matter where in the list the state is.
	// States can only be in the list once, but there can be many transitions
	// pointing to the same state.

	State::init,
		zth::guards::always,			State::blink_on,
		zth::guards::end,
	State::peek,
		zth::guards::peek<Fsm_type,traffic>,	State::amber,
		zth::guards::always,			State::blink_on,
		zth::guards::end,
	State::blink_on,
		zth::guards::timeout_s<1>,		State::blink_off,
		zth::guards::end,
	State::blink_off,
		zth::guards::timeout_s<1>,		State::peek,
		zth::guards::end,
	State::red_wait,
		zth::guards::timeout_s<2>,		State::red,
		zth::guards::end,
	State::red,
		zth::guards::input<Fsm_type,traffic>,	State::green,
		zth::guards::timeout_s<10>,		State::blink_off,
		zth::guards::end,
	State::green,
		// Stay at green as long as there is traffic, but limited to some maximum time.
		tooLongGreen,				State::amber,
		zth::guards::input<Fsm_type,traffic>,	State::green,
		zth::guards::timeout_s<3>,		State::amber,
		zth::guards::end,
	State::amber,
		zth::guards::timeout_s<2>,		State::red_wait,
		zth::guards::end,

	// The description must end with a state without guards:
	State::end,
		zth::guards::end,
	// (Note that this state cannot be reached in this example, but that does
	// not matter.) This is the marker that there are no more states after
	// this.  Anything below here, will be ignored.
};
// clang-format on

// This is the callback function that is invoked when the Fsm changes something
// to the state.
static void cb(Fsm_type& fsm, zth::Timestamp& green)
{
	if(fsm.state() == State::red && fsm.exit() && fsm.next() == State::green)
		green = zth::Timestamp::now();

	if(!fsm.entry())
		return;

	// I know, switch(fsm.state()) is nicer, but then the State must be
	// some integral type...
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
		printf("\nPress enter to generate traffic.\n");
		if(!zth_config(EnableDebugPrint))
			printf("\x1b[17A\x1b[s");
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
	fflush(nullptr);
}

// Something to be passed to the callback function.
static zth::Timestamp green;

// Here we actually create the Fsm.  The description has to be compiled in
// order to be used by a Fsm.  You could create multiple Fsms using the same
// description, but it has to be compiled only once.  Therefore, create one
// compiler for one description.
static typename Fsm_type::Compiler compiler(desc);
static Fsm_type fsm(compiler, &cb, green);
// static Fsm_type fsm2(compiler, &cb, green2); // This is OK.
// static Fsm_type fsm3(compiler, &cb, green3); // This is OK.

// If you would only create one Fsm for the description, you can pass it
// directly to the Fsm instance.
//
// static Fsm_type fsm(desc, &cb, green);
// static Fsm_type fsm2(desc, &cb, green);	// This is not OK, as desc will now
//						// be compiled multiple times.

static void trafficDetect()
{
	char buf = 0;
	while(zth::io::read(0, &buf, 1) > 0)
		fsm.input(traffic);
}
zth_fiber(trafficDetect)

int main_fiber(int /*argc*/, char** /*argv*/)
{
	async trafficDetect();
	// Run the Fsm, until it hits a final state (but there is none in this example).
	fsm.run();
	return 0;
}
