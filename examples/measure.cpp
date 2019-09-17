#include <zth>

#define MEASURE_TARGET_TIME_s 1.0

class FsmGuard {
public:
	template <typename Fsm>
	FsmGuard(bool (*f)(Fsm&)) : m_f(reinterpret_cast<void*>(f)) { printf("guard %p = %p\n", this, m_f); }

	FsmGuard(void*) : m_f() {}

	template <typename Fsm>
	bool operator()(Fsm& fsm) { return reinterpret_cast<bool(Fsm&)>(m_f)(fsm); }

	void* f() const { return m_f; }

private:
	void* m_f;
};

bool always(int& fsm) { return true; }

struct FsmDescriptionPair {
	int state;
	FsmGuard guard;
};

typedef FsmDescriptionPair FsmDescription[];

template <typename Context>
class Fsm {
public:
	Fsm(Context& context)
		: context(context)
	{}

	void build(FsmDescription const description, size_t size) {
		FsmDescriptionPair const* p = description;
		while((uintptr_t)p < (uintptr_t)description + size) {
			printf("%p State %d:\n", p, p->state);
			for(; p->guard.f(); ++p)
				printf("Transition: %p -> %d\n", p->guard.f(), p[1].state);
			++p;
		}
	}

	int state() const { return m_state; }

	int eval() {
	/*
		Transition* t = transitions[state()];
		while(++t)
			if(t->guard())
				setState(t->state);
	*/
		return 0;
	}

	Context& context;
private:
	int m_state;
};

class Measurement {
public:
	Measurement()
		: m_fsm(*this)
		, m_iterations(1)
	{
		fsmInit();
	}

	bool test() {
		while(true)
			switch(m_fsm.eval()) {
			case sTestStart:
				m_iteration = m_iterations;
				m_start = zth::Timestamp::now();
				return true;
			case sTest:
				m_iteration--;
				return true;
			case sTestEnd:
				m_duration = zth::Timestamp::now() - m_start;
				break;
			default:
				return false;
			}
	}

	uint64_t iterations() const { return m_iterations; }
	uint64_t iteration() const { return m_iteration; }
	zth::TimeInterval const& duration() const { return m_duration; }
	double t() const { return m_fsm.state() == sTestDone ? duration().s() / (double)iterations() : -1; }

protected:
	typedef Fsm<Measurement&> Fsm_t;
	static bool iterationsDone(Fsm_t& fsm) { return fsm.context.iteration() == 0; }
	static bool reachedDurationThreshold(Fsm_t& fsm) { return fsm.context.duration().s() > MEASURE_TARGET_TIME_s; }
	enum States { sTestStart = 100, sTest, sTestEnd, sTestDone };

	void fsmInit() {
		FsmDescription transitions = {
			sTestStart,
				always, sTest,
				NULL,
			sTest,
				iterationsDone, sTestEnd,
				NULL,
			sTestEnd,
				reachedDurationThreshold, sTestDone,
				always, sTestStart,
				NULL,
			sTestDone,
				NULL,
		};

		m_fsm.build(transitions, sizeof(transitions));
	}

private:
	Fsm_t m_fsm;
	uint64_t m_iterations;
	uint64_t m_iteration;
	zth::Timestamp m_start;
	zth::TimeInterval m_duration;
};

void measure_timestamp() {

	Measurement m;
	while(m.test())
		zth::Timestamp::now();

	printf("Timestamp(): %g us over %" PRIu64 " iterations\n", m.t() * 1e6, m.iterations());
}

void main_fiber(int argc, char** argv) {
	measure_timestamp();
}

