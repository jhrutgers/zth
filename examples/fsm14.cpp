// This example does not work on Windows, as Windows does not support poll()ing
// stdin. And the ANSI console might be troubling.

#include <zth>

using zth::operator""_s;

namespace fsm {

using namespace zth::fsm;

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

	fflush(nullptr);
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

	fflush(nullptr);
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

	fflush(nullptr);
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

	fflush(nullptr);
}

constexpr static auto init = action(init_, "init");
constexpr static auto off = action(off_, "off");
constexpr static auto red = action(red_, "red");
constexpr static auto amber = action(amber_, "amber");
constexpr static auto green = action(green_, "green");

class TrafficLight : public Fsm {
public:
	void justGotGreen()
	{
		m_green = t();
	}

	auto tooLongGreen() const
	{
		return 10_s - (zth::Timestamp::now() - m_green);
	}

private:
	zth::Timestamp m_green;
};

constexpr static auto justGotGreen = action(&TrafficLight::justGotGreen, "justGotGreen");
constexpr static auto tooLongGreen = guard(&TrafficLight::tooLongGreen, "tooLongGreen");

// clang-format off
constexpr static auto transitions = compile(
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

static void trafficDetect(fsm::TrafficLight& fsm)
{
	char buf = 0;
	while(zth::io::read(0, &buf, 1) > 0)
		fsm.input("traffic");
}
zth_fiber(trafficDetect)

int main_fiber(int /*argc*/, char** /*argv*/)
{
	fsm::transitions.dump();

	fsm::TrafficLight fsm;
	fsm::transitions.init(fsm);

	async trafficDetect(fsm);

	// Run the Fsm, until it hits a final state (but there is none in this example).
	fsm.run();

	return 0;
}
