/*
 * SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
 *
 * SPDX-License-Identifier: CC0-1.0
 */

// In an embedded system, you often see that the application basically runs a
// state machine, with timed tasks around it. Zth provides a FSM
// implementation, which allows you to define the transitions with guards and
// actions. This example explores this functionality.
//
// The FSM requires C++14 or later.
//
// This example does not work on Windows, as Windows does not support poll()ing
// stdin. And the ANSI console might be troubling. This is unrelated to the FSM
// itself.

#include <zth>

// Always handy if you want to specify zth::TimeInterval in seconds.
// NOLINTNEXTLINE(misc-unused-using-decls)
using zth::operator""_s;

// Create a namespace, with all functions related to it.  It is not required,
// but it prevents polluting the global namespace.
namespace fsm {

// We are heavily depending on the functions within zth::fsm. Importing it
// makes the syntax cleaner. And, as we are in our own fsm namespace, that
// won't conflict with the rest of the application.
using namespace zth::fsm;

// Let's define several functions.
static void init_()
{
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
}

static void off_()
{
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

	(void)fflush(nullptr);
}

static void red_()
{
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

	(void)fflush(nullptr);
}

static void amber_()
{
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

	(void)fflush(nullptr);
}

static void green_()
{
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

	(void)fflush(nullptr);
}

// The functions above are actions to be executed when a transition is taken.
// We need an Action object to be used later on in the FSM description.  For
// this, use the zth::fsm::action() function to create it. It is constexpr
// compatible.
//
// The provided names are optional, but make debugging easier. Additionally,
// these names are used when generating the UML description.
//
// Action takes function pointers, member function pointers, and lambda's
// (C++17 and later). The function pointers and lambda's may have one argument,
// which is zth::fsm::Fsm&, or a reference to the Fsm subclass (such as
// TrafficLight below). If this argument is present, the Fsm instance is passed
// to the action. The return value of an action is ignored.
constexpr auto init = action(init_, "init");
constexpr auto off = action(off_, "off");
constexpr auto red = action(red_, "red");
constexpr auto amber = action(amber_, "amber");
#if __cplusplus < 201703L
constexpr auto green = action(green_, "green");
#else
// Just an example that lambda's do work.
constexpr auto green = action([]() { green_(); }, "green");
#endif

// When you want to add more state to the FSM, you may want to group it in your
// own FSM class.  Inherit zth::fsm::Fsm for that. There are some virtual
// functions, like enter() and leave(), which may be handy to override.  In
// this example, we just want to save m_green across states.
//
// You could also add the actions above to this class. That would probably be
// cleaner, but we leave it as is for demonstration purposes.
class TrafficLight : public Fsm {
public:
	// This will be an action, called when the traffic light becomes green.
	void justGotGreen()
	{
		m_green = t();
	}

	// This will be a guard, which is enabled when we are longer green than
	// 10 s.  Guards are polled again after the returned amount of time has
	// passed.  If you return 0 or a negative value, the guard becomes
	// enabled.
	auto tooLongGreen() const
	{
		return 10_s - (zth::Timestamp::now() - m_green);
	}

private:
	zth::Timestamp m_green;
};

// Again, we need guard objects. Wrap the member functions using
// zth::fsm::action() and zth::fsm::guard().
//
// Like for action(), guard() accepts function pointers, member function
// pointers, and lambdas (C++17).  The optional Fsm& (subclass) argument is
// also supported. The return type of the guard must be either a bool, or a
// zth::TimeInterval. In case a bool is returned, true indicates that the guard
// will be enabled.  If a TimeInterval is specified, it is the time till the
// guard function must be invoked again to check if it is enabled.
constexpr auto justGotGreen = action(&TrafficLight::justGotGreen, "justGotGreen");
constexpr auto tooLongGreen = guard(&TrafficLight::tooLongGreen, "tooLongGreen");

// Here is the state machine description. zth::fsm::compile() produces some
// binary format, which is suitable to start the FSM later on. compile() takes
// an arbitrary number of arguments, which all define a state transition.
//
// Every line (=argument) has the following format:
//
//	state + guard / action >>= target_state,
//
// Every element is optional:
//
// - If the state is omitted, the state is taken from the previous line.
// - If the guard is omitted, the 'always' guard is used.
// - If the action is omitted, the 'nothing' action is used.
// - If the target_state is omitted, no transition is taken when the guard is
//   enabled.  This is especially useful for the 'entry' guard, which is only
//   enabled upon entry of the state.
//
// The state is a zth::fsm::State, but it has an implicit conversion from a
// string literal.  In some circumstances, it is required to force the type.
// For example, the transition `"from" >>= "to"` does not compile, as the >>=
// operator cannot be applied to two char const*. In this case, force one of
// the states to a State using the _S suffix.  So: `"from"_S >>= "to"` works
// fine.
//
// The FSM always starts in the first state mentioned. From there, it iterates
// over all guards from this state, and takes the first transition that has an
// enabled guard.  While taking this transition, the action is executed.
//
// There are several standard guards:
//
// - zth::fsm::always: this guard is always enabled.
// - zth::fsm::never: this guard is never enabled. Not sure if that is useful.
// - zth::fsm::entry: this guard is enabled upon entering a state. If you use
//   this guard, you typically don't specify a target_state, although it is
//   possible. The entry guard is usually (but not required to be) the first
//   transition mentioned in the list of transitions for a state.
// - zth::fsm::timeout_s<X>: this guard is enabled after the specified amount
//   of time. There is also a timeout_ms and timeout_us guard.
// - zth::fsm::input(X): this guard is enabled when the FSM has received the
//   mentioned input symbol by calling Fsm::input(X). When the transition is
//   taken, the input symbol is not cleared automatically. Use the 'consume'
//   action for that.
// - zth::fsm::popped: this guard is enabled after returning to this state via
//   the 'pop' action (see below).
//
// There are several standard actions:
//
// - zth::fsm::nothing: this action does nothing.
// - zth::fsm::push: push the target_state onto a stack. The FSM continues with
//   the target_state, but returns to the previous state when the 'pop' action
//   is executed. The stack is of infinite depth, in principle.
// - zth::fsm::pop: pop the state from the stack, and return to the state where
//   the last 'push' action was executed.
// - zth::fsm::consume: remove the input symbol mentioned in the 'input' guard
//   of the same transition from the Fsm.  If you do a 'input' guard without
//   'consume', you can peek if some input symbol is available, without
//   removing it.
// - zth::fsm::stop: this action requests the run() function to return
//   immediately.  You can use this to interrupt the Fsm execution. If
//   Fsm::run() is called afterwards, execution is resumed at the point where
//   'stop' was invoked.
//
// Note that compile() is a constexpr function. The returned transitions is a
// compiled table with transitions, which can be used later on to initialize a
// zth::fsm::Fsm instance with it.

// clang-format off
constexpr auto transitions = compile(
	"init"					/ init		>>= "blink on",
	"blink on"	+ entry			/ amber		,
			+ timeout_s<1>				>>= "blink off",
	"blink off"	+ entry			/ off		,
			+ timeout_s<1>				>>= "peek",
	"peek"		+ input("traffic")			>>= "amber",
			+ always				>>= "blink on",
	"red (wait)"	+ entry			/ red		,
			+ timeout_s<2>				>>= "red",
	"red"		+ input("traffic")	/ consume	>>= "green (first)",
			+ timeout_s<10>				>>= "blink off",
	"green (first)"				/ justGotGreen	>>= "green",
	"green"		+ entry			/ green		,
			+ tooLongGreen				>>= "amber",
			+ input("traffic")	/ consume	>>= "green",
			+ timeout_s<3>				>>= "amber",
	"amber"		+ entry			/ amber		,
			+ timeout_s<2>				>>= "red (wait)"
);
// clang-format on

} // namespace fsm

// This fiber blocks on stdin. When something is read, the "traffic" input
// symbol is passed to the fsm. As a result, the input("traffic") guard will
// become enabled.
static void trafficDetect(fsm::TrafficLight& fsm)
{
	char buf = 0;
	while(zth::io::read(0, &buf, 1) > 0)
		fsm.input("traffic");
}

int main_fiber(int /*argc*/, char** /*argv*/)
{
	// Print the compiled transitions to stdout.
	fsm::transitions.dump();
	(void)fflush(stdout);

	// Print the FSM in plantuml format to stderr. You may pass it through
	// `plantuml -p fsm.png` to render it.
	fsm::transitions.uml(stderr);
	(void)fflush(stderr);

	// Construct the Fsm. It is not a valid FSM, until...
	fsm::TrafficLight fsm;
	// ...init() is called. This will register the transitions to the FSM
	// and reset it.
	fsm::transitions.init(fsm);

	// If you don't have your own Fsm subclass, you may also do:
	//
	// auto fsm  = fsm::transitions.spawn();
	//
	// This will create an zth::fsm::Fsm instance, initialize it, and
	// return it.

	zth::fiber(trafficDetect, fsm);

	// Run the Fsm, until it hits the 'stop' action (but there is none in
	// this example). You could pass a zth::Timestamp or zth::TimeInterval
	// to run() to return after a while.
	fsm.run();

	return 0;
}
