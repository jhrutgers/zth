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
}

/////////////////////////////////////////////////
// Tester

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
	return fsm.callbackArg()->calibration > std::numeric_limits<size_t>::max() / 2
		? zth::TimeInterval() : zth::TimeInterval(0.1);
}

static zth::TimeInterval calibrationTimeout(Fsm_type& fsm) {
	return zth::TimeInterval(1) - (fsm.t() - fsm.callbackArg()->start);
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

static void cb(Fsm_type& fsm, TestData* testData) {
	if(!fsm.entry())
		return;

	zth_assert(testData);

	switch(fsm.state()) {
	case stateInit:
		testData->calibration = 9; // Do at least 10 runs.
		testData->start = fsm.t();
		break;
	case stateCalibrate:
		testData->test();
		testData->calibration++;
		break;
	case stateMeasure:
		for(size_t i = testData->calibration; i > 0; i--)
			testData->test();
		testData->duration = fsm.dt();
		break;
	case stateDone:
		testData->singleResult += testData->duration.s() / testData->calibration;
		printf("%-30s: %s (%8zu iterations)\n",
			testData->description,
			preciseTime(testData->singleResult).c_str(),
			testData->calibration);
		break;
	}
}

static Fsm_type::Compiler compiler(desc);

double runTest(char const* name, void(*test)(), bool calibrationRun = false) {
	zth::yield();

	static double calibration = 0;
	if(calibrationRun)
		calibration = 0;

	TestData testData = { name, test, calibration };
	Fsm_type(compiler, &cb, &testData).run();

	if(calibrationRun)
		calibration = -testData.singleResult;

	return testData.singleResult - calibration;
}

void main_fiber(int argc, char** argv) {
	runTest("calibration", &testEmpty, true);
	runTest("empty test", &testEmpty);
	runTest("Timestamp::now()", &testNow);

	testContextInit();
	runTest("2 context_switch()s", &testContext);
	testContextCleanup();
	
	runTest("single-fiber yield()", &testYield1);
	runTest("single-fiber yield() always", &testYield1Always);
	testYieldInit();
	runTest("yield() to fiber and back", &testYield);
	testYieldCleanup();

	runTest("create and cleanup fiber", &testFiberCreate);
}

