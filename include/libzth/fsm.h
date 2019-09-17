#include <set>
#include <map>

namespace zth {
#if 0
	class FsmBase : public UniqueID<FsmBase> {
	public:
		~FsmBase() { 
			while(!m_inputs.empty())
				unlink(m_inputs.begin());
		}

		void trigger() {
			// wakeup m_fiber from sleep or suspend
		}

		void link(FsmInputBase& input) {
			if(m_inputs.insert(&input).second)
				input.link(*this);
		}

		void unlink(FsmInputBase& input) {
			if(m_inputs.erase(&input)) {
				input.unlink(*this);
				trigger();
			}
		}

	protected:
		Fiber* fiber() const { return m_fiber; }

	private:
		Fiber* m_fiber;
		std::set<FsmInputBase*> m_inputs;
	};

	class FsmInputBase : public UniqueID<FsmInputBase> {
	public:
		~FsmInputBase() { 
			while(!m_fsms.empty())
				unlink(m_inputs.begin());
		}

		void link(FsmBase& fsm) {
			if(m_fsms.insert(&fsm).second)
				fsm.link(*this);
		}

		void unlink(FsmBase& fsm) {
			if(m_fsms.erase(&fsm))
				fsm.unlink(*this);
		}

	protected:
		void triggerFsms() {
			for(decltype(m_fsms.begin()) it = m_fsms.begin(); it != m_fsms.end(); ++it)
				(*it)->trigger();
		}

	private:
		std::set<FsmBase*> m_fsms;
	};

	template <typename Value_>
	class FsmInput : public FsmInputBase {
	public:
		typedef Value_ Value;
		FsmInput(Value const& value) : m_value(value) {}

		Value const& get() const { return m_value; }
		operator Value const&() const { return get(); }

		void set(Value const& value) {
			m_value = value;
			triggerFsms();
		}

		FsmInput& operator=(Value const& value) {
			set(value);
			return *this;
		}

		FsmInput& operator+=(Value const& value) { *this = get() + value; return *this; }
		FsmInput& operator-=(Value const& value) { *this = get() - value; return *this; }
		FsmInput& operator/=(Value const& value) { *this = get() / value; return *this; }
		FsmInput& operator*=(Value const& value) { *this = get() * value; return *this; }

	private:
		Value m_value;
	};

	template <typename T = int>
	class Fsm : public FsmBase {
	public:
		typedef T State;

		Fsm(State initial = State())
			: m_state(initial), m_stateNext(initial)
			, m_entry(true), m_exit(false)
			, m_t(Timestamp::now())
		{}

		void transition(State state) {
			m_stateNext = state;
			m_exit = true;
			setTimeout();
		}
		
		Fsm& operator=(State state) {
			transition(state);
			return *this;
		}

		State eval() {
			yield();
			m_entry = m_state != m_stateNext;
			m_state = m_stateNext;
			m_exit = false;

			if(entry())
				m_t = Timestamp::now();
		}

		State state() const {
			return m_state;
		}

		operator State() const { return state(); }

		bool entry() {
			return m_entry;
		}
		
		bool exit() {
			return m_exit;
		}

		Timestamp const& t() const { return m_t; }
		TimeInterval dt() const { return Timestamp::now() - m_t; }

		void setTimeout(TimeInterval const& timeout = TimeInterval()) { m_timeout = timeout; }
		TimeInterval const& getTimeout() const { return m_timeout; }
		bool timedout() const { return getTimeout() != TimeInterval() && dt() >= getTimeout(); }

	private:
		State m_state;
		State m_stateNext;
		bool m_entry;
		bool m_exit;
		Timestamp m_t;
		TimeInterval m_timeout;
	};


	class Fsm;

	class FsmState {
	protected:
		FsmState(char const* name = NULL) : m_name(name) {}
	private:
		FsmState(FsmState const&)
#ifdef __cplusplus >= 201103L
			= delete;
		FsmState(FsmState&&)
			= delete
#endif
			;
		FsmState& operator=(FsmState const&)
#ifdef __cplusplus >= 201103L
			= delete;
		FsmState& operator=(FsmState const&)
			= delete
#endif
			;
	public:
		typedef uintptr_t Id;
		char const* name() const { return m_name; }
		Id id() const { return (Id)name(); }
		bool operator==(FsmStateId const& state) const { return state.id() == id(); }
		bool operator!=(FsmStateId const& state) const { return state.id() != id(); }
	protected:
		virtual void onEntry(FsmState const& from) {}
		virtual void onExit(FsmState const& to) {}
	private:
		char const* const m_name;

		friend class Fsm;
	};

	class FsmStateId {
	public:
		typedef FsmState::Id Id;
		FsmStateId(FsmState const& state) : m_id(state.id()) {}
		FsmStateId(FsmStateId const& stateName) { *this = stateName; }
		bool operator==(FsmStateId const& stateName) const { return id() == stateName.id(); }
		bool operator!=(FsmStateId const& stateName) const { return !(*this == stateName); }
		FsmStateId& operator=(FsmStateId const& stateName) { m_id = stateName.id(); return *this; }
		Id id() const { return m_id; }
	private:
		Id m_id;
	};

	class FsmInit : public FsmState {
	public:
		FsmInit() : FsmState(__func__) {}
	};
	
	class FsmSelf : public FsmState {
	public:
		FsmSelf() : FsmState(__func__) {}
	};

	class FsmFinal : public FsmState {
	public:
		FsmFinal() : FsmState(__func__) {}
	};

	class FsmGuard {
	protected:
		FsmGuard(char const* name = NULL) : m_name(name) {}
		static TimeInterval const pass;
		static TimeInterval const poll;
		static TimeInterval const wait;
	public:
		char const* name() const { return m_name; }
		virtual TimeInterval operator()(Fsm const& fsm) const = 0;
		virtual void relate(Fsm const& fsm) {}
		virtual void unrelate(Fsm const& fsm) {}
	private:
		char const* const m_name;
	};

	class FsmGuardEnd {
	public:
		FsmGuardEnd() : FsmGuard() {}
		virtual TimeInterval operator()(Fsm const& fsm) const { return pass; }
	};

	class FsmTrue : public FsmGuard {
	public:
		FsmTrue() : FsmGuard("true") {}
		virtual TimeInterval operator()(Fsm const& fsm) const { return pass; }
	};

	FsmTrue const fsmTrue;

	template <typename T>
	class FsmBoolWrapper : public FsmGuard {
	public:
		FsmBoolWrapper(T& wrappee, char const* name = NULL) : FsmGuard(name), m_wrappee(wrappee) {}
		virtual TimeInterval operator()(Fsm const& fsm) const { return (bool)m_wrappee ? pass : poll; }
	private:
		T& m_wrappee;
	};

	template <typename F>
	class FsmFGuard : public FsmGuard {
	public:
		FsmFGuard(F f, char const* name = NULL) : FsmGuard(name), m_f(f) {}
		virtual TimeInterval operator()(Fsm const& fsm) const { return f() ? pass : poll; }
	private:
		F m_f;
	};
	
	FsmGuardEnd const fsmGuardEnd;

	class FsmTimeout : public FsmGuard {
	public:
		FsmTimeout(TimeInterval const& timeout) : FsmGuard("timeout"), m_timeout(timeout) {}
		virtual TimeInterval operator()(Fsm const& fsm) const {
			TimeInterval dt = fsm.t();
			return dt >= m_timeout ? pass : m_timeout - dt;
		}
	private:
		TimeInterval m_timeout;
	};

	class FsmSignalGuard : public FsmGuard {
	public:
		FsmSignalGuard() : FsmGuard("signal") {}
		virtual TimeInterval operator()(Fsm const& fsm) const {
			if(m_signalled.erase(&fsm))
				return pass;

			m_fsms.insert(&fsm);
			return wait;
		}

		void signal() {
			if(m_signalled.empty()) {
				m_signalled.swap(m_fsms);
			} else {
				for(decltype(m_fsms.begin()) it = m_fsms.begin(); it != m_fsms.end(); ++it) {
					m_signalled.insert(*it);
					it->wakeup();
				}
				m_fsms.clear();
			}
		}

		virtual void unrelate(Fsm const& fsm) { 
			m_fsms.erase(&fsm);
			m_signalled.erase(&fsm);
		}
	private:
		std::set<Fsm const*> m_fsms;
		std::set<Fsm const*> m_signalled;
	};

	struct FsmTransition {
		FsmGuard const& guard;
		FsmStateId to;
#ifdef __cplusplus >= 201103L
		std::function<void(FsmState&,FsmState&,FsmGuard const&)> onTransition;
#else
		void (*onTransition)(FsmState& from, FsmState& to, FsmGuard const& guard);
#endif
	};
	
	FsmTransition const noTransition = { fsmGuardEnd, FsmSelf() };
	
	struct FsmStateTransitionPair {
		FsmState state;
		FsmTransition transitions[];
	};
	
	typedef FsmStateTransitionPair const FsmDescription[];

	class Fsm {
	public:
		Fsm(FsmDescription st);
		FsmState const& eval();

		void reset() { transition(fsmTrue, FsmInit()); }
		FsmState const& state() const { return *m_state; }
		FsmState const& nextState() const { return *m_nextState; }
		FsmGuard const& guard() const { return *m_guard; }
		TimeInterval const& t() const { return Timestamp::now() - m_t; }
		void wakeup();
	protected:
		void transition(FsmGuard const& guard, FsmStateId const& state);
	private:
		FsmState const* m_state;
		FsmState const* m_nextState;
		FsmGuard const* m_guard;
		Timestamp m_t;
		std::map<FsmState::Id, FsmStateTransitionPair*> m_states;
	};

	
	class StateA : public FsmState {
	public:
		StateA() : FsmState(__func__) {}
		static void t(Fsm& fsm) {}
	};

	FsmTrue inputA;
	FsmTrue inputB;

	static FsmDescription sts = {
		{ FsmInit(), {
			{ fsmTrue,         StateA() },
			  noTransition } },
		{ StateA(), {
			{ inputA,          StateB() },
			{ inputB,          StateB(), &StateA::t },
			{ FsmTimeout(0.5), FsmFinal() },
			  noTransition } },
		{ StateB(),
			  noTransition } },
		{ FsmFinal() }
	};

	static Fsm fsm(sts);

#endif

	namespace guards {
		enum End { end = 0 };
		template <typename Fsm> bool always(Fsm& fsm) { return true; }
	}

	template <typename Fsm>
	class FsmGuard {
	public:
		typedef bool (*Function)(Fsm& fsm);
		FsmGuard(Function function)
			: m_function(function)
		{}

		FsmGuard(guards::End) : m_function() {}

		bool operator()(Fsm& fsm) const { return valid() ? m_function(fsm) : false; }
		bool valid() const { return m_function; }
	private:
		Function m_function;
	};

	template <typename Fsm>
	struct FsmDescription {
		union {
			typename Fsm::State state;
			FsmDescription<Fsm> const* stateAddr;
		};
		FsmGuard<Fsm> guard;
	};

	template <typename Fsm>
	class FsmFactory {
	public:
		FsmFactory(FsmDescription<Fsm>* description)
			: m_description(description)
			, m_compiled()
		{} 

		__attribute__((malloc,returns_nonnull,warn_unused_result)) Fsm* create() const { return new Fsm(*this); }

		FsmDescription<Fsm> const* description() const { compile(); return m_description; }

	protected:
		void compile() const {
			if(likely(m_compiled))
				return;

			std::map<typename Fsm::State,FsmDescription<Fsm> const*> stateAddr;
			FsmDescription<Fsm>* p = m_description;
			while(true) {
				// Register this state
				zth_assert(stateAddr.count(p->state) == 0);
				stateAddr[p->state] = const_cast<FsmDescription<Fsm> const*>(p);

				if(!p->guard.valid())
					// End of description
					break;

				// Go to end of guard list
				while((p++)->guard.valid());
			}

			// Patch State references
			p = m_description;
			while(true) {
				if(!p->guard.valid())
					// End of description
					break;

				// Patch this state's guard list
				while((p++)->guard.valid()) {
					zth_assert(stateAddr.count(p->state) == 1);
					p->stateAddr = stateAddr[p->state];
				}
			}

			m_compiled = true;
		}

	private:
		FsmDescription<Fsm>* const m_description;
		mutable bool m_compiled;
	};

	template <typename State_>
	class Fsm : UniqueID<Fsm<void> > {
	protected:
		enum EvalState { evalCompile, evalInit, evalIdle, evalState, evalRecurse };
		typedef FsmDescription<Fsm> const* StateAddr;

	public:
		typedef State_ State;
		typedef FsmFactory<Fsm> Factory;
		typedef FsmDescription<Fsm> Description[];

		explicit Fsm(Factory const& factory)
			: m_factory(&factory)
			, m_evalState(evalInit)
		{}
		
		explicit Fsm(Description description)
			: m_description(description)
			, m_evalState(evalCompile)
		{}
		
		virtual ~Fsm() {}

		State const& state() const {
			switch(m_evalState) {
			default:
				return m_state->state;
			case evalCompile:
				return m_description->state;
			case evalInit:
				return m_factory->description()->state;
			}
		}

		State const& next() const {
			switch(m_evalState) {
			default:
				zth_assert(exit() || m_stateNext == m_state);
				return m_stateNext->state;
			case evalCompile:
				return m_description->state;
			case evalInit:
				return m_factory->description()->state;
			}
		}

		bool entry() const { return m_entry; }
		bool exit() const { return m_exit; }

		void eval(bool alwaysDoCallback = false) {
			bool didCallback = false;

			switch(m_evalState) {
			case evalCompile:
				m_state = Factory(m_description).description();
				if(false) {
					ZTH_FALLTHROUGH
			case evalInit:
					m_state = (StateAddr)m_factory->description();
				}
				m_stateNext = m_state;
				m_evalState = evalState;
				m_exit = false;
				m_entry = true;
				callback();
				m_entry = false;
				didCallback = true;
				ZTH_FALLTHROUGH
			case evalIdle: {
				do {
					m_evalState = evalState;
					while(evalOnce())
						didCallback = true;
					if(alwaysDoCallback && !didCallback)
						callback();
				} while(m_evalState == evalRecurse);
				m_evalState = evalIdle;
				return;
			}
			case evalState:
				m_evalState = evalRecurse;
				ZTH_FALLTHROUGH
			case evalRecurse:
				return;
			}
		}
	
	private:
		bool evalOnce() {
			StateAddr p = m_state;
			while(p->guard.valid())
				if((p++)->guard(*this)) {
					if(p->stateAddr == m_state)
						// Take this self-loop.
						return false;

					setState(p->stateAddr);
					return true;
				}
			return false;
		}

		void setState(StateAddr nextState) {
			m_stateNext = nextState;

			m_exit = true;
			callback();
			m_exit = false;

			m_state = nextState;

			m_entry = true;
			callback();
			m_entry = false;
		}

	protected:
		virtual void callback() = 0;

	private:
		union {
			FsmDescription<Fsm>* m_description; // only valid during stateCompile
			Factory const* m_factory; // only valid during stateInit
			StateAddr m_state;
		};
		StateAddr m_stateNext;
		EvalState m_evalState;
		bool m_entry;
		bool m_exit;
	};
	
	template <typename State_, typename CallbackArg_ = void>
	class FsmCallback : public Fsm<State_> {
	public:
		typedef Fsm<State_> base;
		typedef CallbackArg_ CallbackArg;
		typedef void (*Callback)(FsmCallback&, CallbackArg);

		explicit FsmCallback(typename base::Factory const& factory, Callback callback = NULL, CallbackArg const& callbackArg = CallbackArg())
			: base(factory)
			, m_callback(callback)
			, m_callbackArg(callbackArg)
		{}
		
		explicit FsmCallback(typename base::Description description, Callback callback = NULL, CallbackArg const& callbackArg = CallbackArg())
			: base(description)
			, m_callback(callback)
			, m_callbackArg(callbackArg)
		{}

		virtual ~FsmCallback() {}

	protected:
		virtual void callback() {
			if(m_callback)
				m_callback(*this, m_callbackArg);
		}

	private:
		Callback const m_callback;
		CallbackArg const m_callbackArg;
	};
	
	template <typename State_>
	class FsmCallback<State_, void> : public Fsm<State_> {
	public:
		typedef Fsm<State_> base;
		typedef void (*Callback)(FsmCallback&);

		explicit FsmCallback(typename base::Factory const& factory, Callback callback = NULL)
			: base(factory)
			, m_callback(callback)
		{}

		explicit FsmCallback(typename base::Description description, Callback callback = NULL)
			: base(description)
			, m_callback(callback)
		{}

		virtual ~FsmCallback() {}

	protected:
		virtual void callback() {
			if(m_callback)
				m_callback(*this);
		}

	private:
		Callback const m_callback;
	};

} // namespace
