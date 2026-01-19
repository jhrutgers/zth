/*
 * SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <zth>

#include <cmath>

/////////////////////////////////////////////////
// Tests

void testEmpty() {}

void testNow()
{
	zth::Timestamp::now();
}

static zth::Context* context1;
static zth::Context* context2;

void context2entry(void* /*unused*/)
{
	while(true)
		zth::context_switch(context2, context1);
}

void testContextInit()
{
	context1 = zth::currentFiber().context();

	int res = zth::context_create(context2, zth::ContextAttr(&context2entry));
	if(res)
		zth::abort("Cannot create context2; %s", zth::err(res).c_str());
}

void testContext()
{
	zth::context_switch(context1, context2);
}

void testContextCleanup()
{
	zth::context_destroy(context2);
}

void testYield1()
{
	zth::yield();
}

void testYield1Always()
{
	zth::yield(nullptr, true);
}

static zth::Fiber* testYieldFiberPtr = nullptr;

void testYieldFiber()
{
	testYieldFiberPtr = &zth::currentFiber();

	while(true)
		zth::yield(nullptr, true);
}
zth_fiber(testYieldFiber)

void testYieldInit()
{
	zth_async testYieldFiber();
}

void testYield()
{
	zth::yield(nullptr, true);
}

void testYieldCleanup()
{
	if(!testYieldFiberPtr)
		zth::abort("Test fiber did not run");

	testYieldFiberPtr->kill();
}

void testFiberCreateEntry() {}
zth_fiber(testFiberCreateEntry)

void testFiberCreate()
{
	testFiberCreateEntry_future f = zth_async testFiberCreateEntry();
	f.wait();
	zth::yield(nullptr, true); // Make sure to clean up old fibers.
}

void testCurrentFiber()
{
	zth::Fiber const& f __attribute__((unused)) = zth::currentFiber();
}

void testNap0()
{
	zth::nap(0);
}

void testNap100ms()
{
	zth::nap(0.1);
}

/////////////////////////////////////////////////
// Tester

static zth::string preciseTime(double s)
{
	// We would like to do std::format("%.12f", s), but newlib may not
	// support double formatting.  Therefore, do it the hard way...

	if(s != s)
		return "NaN";

	if(s - s != s - s) // inf - inf == NaN
		return s < 0 ? "-Inf" : "Inf";

	zth::string res;

	if(s < 0)
		res += "-";

	else
		res += " ";

	// We expect that all values are < 1 s, so simplify the formatting of
	// the integral part.

	double intpart = 0;
	s = modf(fabs(s), &intpart);
	res += zth::format("%u.", (unsigned)intpart);

	// Now print 12 decimals, grouped with '.

	for(int i = 0; i < 4; i++) {
		if(i)
			res += "'";
		s = modf(s * 1000.0, &intpart);
		// Ok, there may be an exotic case because of rounding that
		// intpart may become 1000. Ignore that for now.
		res += zth::format("%03u", (unsigned)intpart);
	}

	// Last decimal may not be rounded properly. Ignore that.
	res += " s";
	return res;
}

namespace fsm {

using namespace zth::fsm;

class TestFsm : public Fsm {
public:
	TestFsm(zth::cow_string description, void (*test)(), double calibrationOffset = 0)
		: m_description{std::move(description)}
		, m_test{test}
		, m_singleResult{calibrationOffset}
	{}

	size_t iterations() const
	{
		return (size_t)1U << m_calibration;
	}

	bool maxIterations() const
	{
		return iterations() > std::numeric_limits<size_t>::max() / 4U;
	}

	void calibrate()
	{
		// This function is called in the sequence with iterations
		// being 1<<0, 1<<1, 1<<2, 1<<3, 1<<4... So, the total number
		// of executed tests will be the number of calls to
		// calibration() * 2 - 1. Divide by 2U here, to let
		// m_calibrations match the number of total executed
		// iterations.
		for(size_t i = iterations() / 2U; i > 0; i--)
			m_test();

		m_calibration++;
	}

	void measure()
	{
		for(size_t i = iterations(); i > 0; i--)
			m_test();

		m_duration = dt();
	}

	void done()
	{
		m_singleResult += m_duration.s() / (double)iterations();

		printf("%-50s: %s    (2^%2u iterations, total %s)\n", m_description.c_str(),
		       preciseTime(m_singleResult).c_str(), (unsigned)m_calibration,
		       m_duration.str().c_str());

		stop();
	}

	double result() const
	{
		return m_singleResult;
	}

private:
	zth::cow_string m_description;
	void (*m_test)();
	double m_singleResult;

	size_t m_calibration{};
	zth::Timestamp m_start;
	zth::TimeInterval m_duration;
};

static constexpr auto maxIterations = guard(&TestFsm::maxIterations);
static constexpr auto calibrate = action(&TestFsm::calibrate);
static constexpr auto measure = action(&TestFsm::measure);
static constexpr auto done = action(&TestFsm::done);

// clang-format off
static constexpr auto transitions = compile(
	// Calibration should take at least 0.5 s; actual measurement is then twice as long.
	"calibrate"	+ timeout_ms<500>			>>= "measure",
			+ maxIterations				>>= "measure",
						  calibrate	,
	"measure"				/ measure	>>= "done",
	"done"					/ done
);
// clang-format on

} // namespace fsm

static void runTest(char const* set, char const* name, void (*test)(), bool calibrationRun = false)
{
	zth::yield();

	static double calibration = 0;
	if(calibrationRun)
		calibration = 0;

	fsm::TestFsm fsm{zth::format("[%-10s]  %s", set, name), test, calibration};
	fsm::transitions.init(fsm);
	fsm.run();

	if(calibrationRun)
		calibration = -fsm.result();
}

static void run_testset(char const* testset)
{
	bool all = strcmp("all", testset) == 0;
	char const* set = nullptr;

	set = "now";
	if(all || strcmp(set, testset) == 0) {
		runTest(set, "Timestamp::now()", &testNow);
	}

	set = "context";
	if(all || strcmp(set, testset) == 0) {
		testContextInit();
		runTest(set, "2 context_switch()s", &testContext);
		testContextCleanup();
	}

	set = "yield";
	if(all || strcmp(set, testset) == 0) {
		runTest(set, "single-fiber yield()", &testYield1);
		runTest(set, "single-fiber yield() always", &testYield1Always);
		testYieldInit();
		runTest(set, "yield() to fiber and back", &testYield);
		testYieldCleanup();
	}

	set = "fiber";
	if(all || strcmp(set, testset) == 0) {
		runTest(set, "currentFiber()", &testCurrentFiber);
		runTest(set, "create and cleanup fiber", &testFiberCreate);
	}

	set = "nap";
	if(all || strcmp(set, testset) == 0) {
		runTest(set, "nap(0)", &testNap0);
		runTest(set, "nap(100 ms)", &testNap100ms);
	}
}

// Specify on the command line the test set name(s) to be executed.  When none
// is specified, 'all' is assumed. The test set name is printed between
// brackets before the test description.
int main_fiber(int argc, char** argv)
{
	puts(zth::banner());

	runTest("calib", "calibration", &testEmpty, true);
	runTest("calib", "empty test", &testEmpty);

	if(argc <= 1)
		run_testset("all");
	else
		for(int i = 1; i < argc; i++)
			run_testset(argv[i]);

	return 0;
}

// Override log output, as it influences our measurements.
void zth_logv(char const* UNUSED_PAR(fmt), va_list UNUSED_PAR(arg)) {}
