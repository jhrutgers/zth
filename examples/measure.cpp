#include <zth>

/////////////////////////////////////////////////
// Tests

void testEmpty() {
}

void testNow() {
	zth::Timestamp::now();
}

static zth::Context* context1;
static zth::Context* context2;

void context2entry(void*) {
	while(true) {
		zth::context_switch(context2, context1);
	}
}

void testContextInit() {
	int res;
	context1 = zth::currentFiber().context();
	if((res = zth::context_create(context2, zth::ContextAttr(&context2entry))))
		zth::abort("Cannot create context2; %s", zth::err(res).c_str());
}

void testContext() {
	zth::context_switch(context1, context2);
}

void testContextCleanup() {
	zth::context_destroy(context2);
}

void testYield1() {
	zth::yield();
}

void testYield1Always() {
	zth::yield(NULL, true);
}

static zth::Fiber* testYieldFiberPtr = NULL;
void testYieldFiber() {
	testYieldFiberPtr = &zth::currentFiber();

	while(true)
		zth::yield(NULL, true);
}
zth_fiber(testYieldFiber)

void testYieldInit() {
	async testYieldFiber();
}

void testYield() {
	zth::yield(NULL, true);
}

void testYieldCleanup() {
	if(!testYieldFiberPtr)
		zth::abort("Test fiber did not run");

	testYieldFiberPtr->kill();
}

void testFiberCreateEntry()
{
}
zth_fiber(testFiberCreateEntry)

void testFiberCreate() {
	testFiberCreateEntry_future f = async testFiberCreateEntry();
	f->wait();
	zth::yield(NULL, true); // Make sure to clean up old fibers.
}

void testCurrentFiber() {
	zth::Fiber& f __attribute__((unused)) = zth::currentFiber();
}

void testNap0() {
	zth::nap(0);
}

void testNap100ms() {
	zth::nap(0.1);
}

/////////////////////////////////////////////////
// Tester

static std::string preciseTime(double s) {
	std::string res;
	if(s >= 0)
		res = " ";
	res += zth::format("%.12f", s);
	std::string::iterator it = res.end();
	size_t cnt = 0;
	while(true) {
		if(it == res.begin())
			break;
		if(*(it - 1) == '.')
			break;
		if(cnt > 0 && cnt % 3 == 0)
			it = res.insert(it, '\'');
		cnt++;
		--it;
	}
	res += " s";
	return res;
}

typedef enum { stateInit, stateCalibrate, stateMeasure, stateDone } State;
namespace zth { template <> inline cow_string str<State>(State value) { return str((int)value); } }

struct TestData {
	char const* description;
	void (*test)();
	double singleResult;
	size_t calibration;
	zth::Timestamp start;
	zth::TimeInterval duration;
};

typedef zth::FsmCallback<State,TestData*> Fsm_type;

static zth::TimeInterval maxIterations(Fsm_type& fsm) {
	return (size_t)1 << fsm.callbackArg()->calibration > std::numeric_limits<size_t>::max() / 2
		? zth::TimeInterval() : zth::TimeInterval(0.1);
}

static zth::TimeInterval calibrationTimeout(Fsm_type& fsm) {
	// Calibration should take at least 0.5 s; actual measurement is then twice as long.
	return zth::TimeInterval(0.5) - (zth::Timestamp::now() - fsm.callbackArg()->start);
}

Fsm_type::Description desc = {
	stateInit,
		zth::guards::always,		stateCalibrate,
		zth::guards::end,
	stateCalibrate,
		calibrationTimeout,			stateMeasure,
		maxIterations,				stateMeasure,
		zth::guards::always,		stateCalibrate,
		zth::guards::end,
	stateMeasure,
		zth::guards::always,		stateDone,
		zth::guards::end,
	stateDone,
		zth::guards::end,
};

static void cb(Fsm_type& fsm, TestData* testData) {
	if(!fsm.entry())
		return;

	zth_assert(testData);

	switch(fsm.state()) {
	case stateInit:
		testData->calibration = 0;
		testData->start = fsm.t();
		break;
	case stateCalibrate:
		for(size_t i = (size_t)1 << testData->calibration; i > 0; i--)
			testData->test();
		testData->calibration++;
		break;
	case stateMeasure:
		testData->calibration++;
		for(size_t i = (size_t)1 << testData->calibration; i > 0; i--)
			testData->test();
		testData->duration = fsm.dt();
		break;
	case stateDone:
		testData->singleResult += testData->duration.s() / ((size_t)1 << testData->calibration);
		printf("%-50s: %s    (2^%2zu iterations, total %.2f s)\n",
			testData->description,
			preciseTime(testData->singleResult).c_str(),
			testData->calibration,
			testData->duration.s());
		break;
	}
}

static Fsm_type::Compiler compiler(desc);

static double runTest(char const* set, char const* name, void(*test)(), bool calibrationRun = false) {
	zth::yield();

	static double calibration = 0;
	if(calibrationRun)
		calibration = 0;

	zth::cow_string testname = zth::format("[%-10s]  %s", set, name);
	TestData testData = { testname.c_str(), test, calibration };
	Fsm_type(compiler, &cb, &testData).run();

	if(calibrationRun)
		calibration = -testData.singleResult;

	return testData.singleResult - calibration;
}

static void run_testset(char const* testset) {
	bool all = strcmp("all", testset) == 0;
	char const* set;

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
void main_fiber(int argc, char** argv) {
	runTest("calib", "calibration", &testEmpty, true);
	runTest("calib", "empty test", &testEmpty);

	if(argc <= 1)
		run_testset("all");
	else
		for(int i = 1; i < argc; i++)
			run_testset(argv[i]);
}

