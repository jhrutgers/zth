#ifndef ZTH_FSM_H
#define ZTH_FSM_H
/*
 * Zth (libzth), a cooperative userspace multitasking library.
 * Copyright (C) 2019-2021  Jochem Rutgers
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

/*!
 * \defgroup zth_api_cpp_fsm fsm
 * \ingroup zth_api_cpp
 */

#ifdef __cplusplus
#include <libzth/time.h>
#include <libzth/waiter.h>
#include <libzth/util.h>

#include <vector>
#include <map>

// Suppress missing braces, as this is what you do when you specify the FSM.
#pragma GCC diagnostic ignored "-Wmissing-braces"

namespace zth {

	namespace guards {
		enum End {
			/*!
			 * \brief End-of-guard-list marker.
			 * \ingroup zth_api_cpp_fsm
			 */
			end = 0 };

		/*!
		 * \brief A guard that is always true.
		 * \ingroup zth_api_cpp_fsm
		 */
		template <typename Fsm> ZTH_EXPORT TimeInterval always(Fsm& fsm) {
			zth_dbg(fsm, "[%s] Guard always", fsm.id_str());
			return TimeInterval();
		}

		/*!
		 * \brief A guard that is true when the given input has been set.
		 * \details If set, the input is cleared from the Fsm.
		 * \see zth::guards::peek()
		 * \ingroup zth_api_cpp_fsm
		 */
		template <typename FsmInput, typename FsmInput::Input i, typename Fsm> ZTH_EXPORT TimeInterval input(Fsm& fsm) {
			if(fsm.clearInput(i)) {
				zth_dbg(fsm, "[%s] Guard input %s", fsm.id_str(), str(i).c_str());
				return TimeInterval();
			} else
				return TimeInterval::infinity();
		}

		/*!
		 * \brief A guard that is true when the given input has been set.
		 * \details The input is not cleared from the Fsm.
		 * \see zth::guards::input()
		 * \ingroup zth_api_cpp_fsm
		 */
		template <typename FsmInput, typename FsmInput::Input i, typename Fsm> ZTH_EXPORT TimeInterval peek(Fsm& fsm) {
			if(fsm.hasInput(i)) {
				zth_dbg(fsm, "[%s] Guard peek input %s", fsm.id_str(), str(i).c_str());
				return TimeInterval();
			} else
				return TimeInterval::infinity();
		}

		template <typename Fsm> TimeInterval timeout_(Fsm& fsm, TimeInterval const& timeout) {
			TimeInterval ti = timeout - fsm.dt();
			if(ti.hasPassed())
				zth_dbg(fsm, "[%s] Guard timeout %s", fsm.id_str(), timeout.str().c_str());
			return ti;
		}

		/*!
		 * \brief A guard that is true when the Fsm stayed for the given time in this state.
		 * \ingroup zth_api_cpp_fsm
		 */
		template <time_t s, typename Fsm> ZTH_EXPORT TimeInterval timeout_s(Fsm& fsm) {
			static constexpr TimeInterval const timeout(s);
			return timeout_<Fsm>(fsm, timeout);
		}

		/*!
		 * \brief A guard that is true when the Fsm stayed for the given time in this state.
		 * \ingroup zth_api_cpp_fsm
		 */
		template <unsigned int ms, typename Fsm> ZTH_EXPORT TimeInterval timeout_ms(Fsm& fsm) {
			static TimeInterval const timeout((double)ms * 1e-3);
			return timeout_<Fsm>(fsm, timeout);
		}

		/*!
		 * \brief A guard that is true when the Fsm stayed for the given time in this state.
		 * \ingroup zth_api_cpp_fsm
		 */
		template <uint64_t us, typename Fsm> ZTH_EXPORT TimeInterval timeout_us(Fsm& fsm) {
			static TimeInterval const timeout((double)us * 1e-6);
			return timeout_<Fsm>(fsm, timeout);
		}

		/*!
		 * \brief A wrapper for #zth::Fsm::guardLock().
		 * \ingroup zth_api_cpp_fsm
		 */
		template <typename Fsm> ZTH_EXPORT TimeInterval lock(Fsm& fsm) {
			if(fsm.guardLock()) {
				zth_dbg(fsm, "[%s] Guard lock", fsm.id_str());
				return TimeInterval();
			} else
				return TimeInterval::infinity();
		}

		/*!
		 * \brief A wrapper for #zth::Fsm::guardStep().
		 * \ingroup zth_api_cpp_fsm
		 */
		template <typename Fsm> ZTH_EXPORT TimeInterval step(Fsm& fsm) {
			if(fsm.guardStep()) {
				zth_dbg(fsm, "[%s] Guard step", fsm.id_str());
				return TimeInterval();
			} else
				return TimeInterval::infinity();
		}
	}

	/*!
	 * \brief A guard is evaluated, and when true, the corresponding transition is taken.
	 * \details Use one of the guards in \c zth::guards, like \c zth::guards::always(), or define your own.
	 *          A guard is a function that gets the current Fsm reference as argument and should return a #zth::TimeInterval.
	 *          When the interval has passed (zero or negative duration), the guard is assumed to be taken.
	 *          A positive duration indicates how much time it will probably take for the guard to become valid, which can be
	 *          used to suspend the fiber.
	 * \see \ref fsm.cpp for an example
	 * \ingroup zth_api_cpp_fsm
	 */
	template <typename Fsm>
	class FsmGuard {
	public:
		typedef TimeInterval (*Function)(Fsm& fsm);
		// cppcheck-suppress noExplicitConstructor
		constexpr FsmGuard(Function function)
			: m_function(function)
		{}

		// cppcheck-suppress noExplicitConstructor
		FsmGuard(guards::End) : m_function() {}

		TimeInterval operator()(Fsm& fsm) const { zth_assert(valid()); return m_function(fsm); }
		constexpr bool valid() const { return m_function; }
	private:
		Function m_function;
	};

	/*!
	 * \brief The description of a Fsm.
	 * \details Don't try to understand this definition, just follow the example.
	 * \see zth::Fsm
	 * \see \ref fsm.cpp for an example
	 * \ingroup zth_api_cpp_fsm
	 */
	template <typename Fsm>
	struct FsmDescription {
		union {
			typename Fsm::State state;
			FsmDescription const* stateAddr;
		};
		FsmGuard<Fsm> guard;
	};

	/*!
	 * \brief A compiler to turn an #zth::FsmDescription into something that can be used by #zth::Fsm.
	 * \see zth::Fsm
	 * \see \ref fsm.cpp for an example
	 * \ingroup zth_api_cpp_fsm
	 */
	template <typename Fsm>
	class FsmCompiler {
	public:
		constexpr explicit FsmCompiler(FsmDescription<Fsm>* description)
			: m_description(description)
			, m_compiled()
		{}

#if GCC_VERSION >= 49000L
		__attribute__((returns_nonnull))
#endif
		__attribute__((warn_unused_result)) Fsm* create() const { return new Fsm(*this); }

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

	/*!
	 * \brief A Finite-state machine.
	 * \tparam State_ the type of a state, which can be anything, as long as it is default-constructable and assignable, and comparable with \c std:less.
	 * \tparam Input_ the type of inputs, as used by #Fsm::input(), which has the same type requirements as \c State_.
	 * \tparam FsmImpl_ the actual subclass type of \c Fsm, which is used for the #zth::FsmDescription.
	 *                  So, if you derive from Fsm, pass your class to \p FsmImpl_, such that every guard gets your actual \c Fsm type reference.
	 *                  When \c void, assume that there is no subclass, and just use the \c Fsm reference instead.
	 * \see \ref fsm.cpp for an example
	 * \ingroup zth_api_cpp_fsm
	 */
	template <typename State_, typename Input_ = int, typename FsmImpl_ = void>
	class Fsm : public UniqueID<Fsm<void,void,void> > {
	public:
		typedef typename choose_type<FsmImpl_,Fsm>::type FsmImpl;
		typedef State_ State;
		typedef Input_ Input;
		typedef FsmCompiler<FsmImpl> Compiler;
		typedef FsmDescription<FsmImpl> Description[];

	protected:
		enum EvalState { evalCompile, evalInit, evalReset, evalIdle, evalState, evalRecurse };
		typedef FsmDescription<FsmImpl> const* StateAddr;

		enum Lockstep { lockstepNormal, lockstepLock, lockstepStep, lockstepSteppingNext, lockstepStepping };

	public:
		explicit Fsm(Compiler const& compiler, char const* name = "FSM")
			: UniqueID(name)
			, m_compiler(&compiler)
			, m_evalState(evalInit)
			, m_lockstep(lockstepNormal)
		{
#if __cplusplus >= 201103L && !defined(DOXYGEN)
			static_assert(std::is_base_of<Fsm,FsmImpl>::value, "");
#endif
		}

		explicit Fsm(Description description, char const* name = "FSM")
			: UniqueID(name)
			, m_description(description)
			, m_evalState(evalCompile)
			, m_lockstep(lockstepNormal)
		{}

		virtual ~Fsm() {}

		State const& state() const {
			switch(m_evalState) {
			default:
				return m_state->state;
			case evalCompile:
				return m_description->state;
			case evalInit:
				return m_compiler->description()->state;
			case evalReset:
				return m_compiledDescription->state;
			}
		}

		Timestamp const& t() const { return m_t; }
		TimeInterval dt() const { return Timestamp::now() - t(); }

		State const& next() const {
			switch(m_evalState) {
			default:
				zth_assert(exit() || m_stateNext == m_state);
				return m_stateNext->state;
			case evalCompile:
				return m_description->state;
			case evalInit:
				return m_compiler->description()->state;
			case evalReset:
				return m_compiledDescription->state;
			}
		}

		void reset() {
			zth_dbg(fsm, "[%s] Reset", id_str());
			switch(m_evalState) {
			default:
				// If you get here, you probably called reset from within the callback function, which you shouldn't do.
				zth_assert(false);
				ZTH_FALLTHROUGH
			case evalIdle:
				m_evalState = evalReset;
				break;
			case evalCompile:
			case evalInit:
			case evalReset:
				break;
			}
			clearInputs();
		}

		bool entry() const { return m_entry; }
		bool exit() const { return m_exit; }

		/*!
		 * \brief Check if lockstep mode is enabled.
		 */
		bool lockstep() const { return m_lockstep != lockstepNormal; }

		/*!
		 * \brief Enable/disable lockstep mode.
		 *
		 * Assume the following FSM:
		 *
		 * \verbatim
		 * +--> A ---+
		 * |         |
		 * +--- B <--+
		 * \endverbatim
		 *
		 * Normally, the transitions have guards that determine if the Fsm is allowed to continue.
		 * In lockstep mode, one add special states where the Fsm can wait for an explicit input.
		 * For example, if such explicit input is required before continuing back to A:
		 *
		 * \verbatim
		 * +--> A ---+
		 * |         |
		 * +--- B <--+
		 *     ^ \
		 *    /   \
		 *   +- C <+
		 * \endverbatim
		 *
		 * In this Fsm, once B is reached, the Fsm may go to the stalled state C,
		 * wait for an explicit input, go back to B and proceed to A.
		 * To implement this, add a guard that checks #guardLock() at entry of B.
		 * If it returns \c true, proceed to C. In C, add a guard that checks #guardStep().
		 * If it returns \c true, proceed back to B. Any #guardLock() guards are ignored
		 * until state B is left. If the expected explicit input was received, call #step().
		 */
		void setLockstep(bool enable) {
			if(lockstep() == enable)
				return;

			switch(m_lockstep) {
			case lockstepNormal:
				m_lockstep = lockstepLock;
				break;
			default:
				m_lockstep = lockstepNormal;
				break;
			}
			trigger();
		}

		/*!
		 * \brief Check if a lock was requested while running in lockstep mode.
		 * \return \c true if the Fsm is to be paused
		 * \see #setLockstep()
		 */
		bool guardLock() {
			switch(m_lockstep) {
			case lockstepLock:
			case lockstepStep:
				return true;
			default:
				return false;
			}
		}

		/*!
		 * \brief Check if a step was requested while paused in lockstep mode.
		 * \return \c true if the Fsm may continue
		 * \see #setLockstep()
		 */
		bool guardStep() {
			switch(m_lockstep) {
			case lockstepLock:
				return false;
			case lockstepStep:
				m_lockstep = lockstepSteppingNext;
				return true;
			default:
				return true;
			}
		}

		/*!
		 * \brief Allow the Fsm to proceed one step, if in lockstep mode.
		 * \see #setLockstep()
		 */
		void step() {
			switch(m_lockstep) {
			case lockstepLock:
				m_lockstep = lockstepStep;
				trigger();
				break;
			default:;
			}
		}

		void input(Input i) {
			m_inputs.push_back(i);
			trigger();
		}

		bool clearInput(Input i) {
			for(size_t j = 0; j < m_inputs.size(); j++)
				if(m_inputs[j] == i) {
					m_inputs[j] = m_inputs.back();
					m_inputs.pop_back();
					return true;
				}
			return false;
		}

		void clearInputs() {
			m_inputs.clear();
		}

		void setInputsCapacity(size_t capacity) {
#if __cplusplus >= 201103L
			if(m_inputs.capacity() > capacity * 2U) {
				// Shrink when the current capacity is significantly more.
				m_inputs.shrink_to_fit();
			}
#endif

			m_inputs.reserve(capacity);
		}

		bool hasInput(Input i) const {
			for(size_t j = 0; j < m_inputs.size(); j++)
				if(m_inputs[j] == i)
					return true;
			return false;
		}

		TimeInterval eval(bool alwaysDoCallback = false) {
			bool didCallback = false;
			TimeInterval wait = TimeInterval::null();

			switch(m_evalState) {
			case evalCompile:
				m_compiledDescription = Compiler(m_description).description();
				if(false) {
					ZTH_FALLTHROUGH
			case evalInit:
					m_compiledDescription = (StateAddr)m_compiler->description();
				}
				ZTH_FALLTHROUGH
			case evalReset:
				m_stateNext = m_state = m_compiledDescription;
				m_evalState = evalState;
				zth_dbg(fsm, "[%s] Initial state %s", id_str(), str(state()).c_str());
				m_t = Timestamp::now();
				m_exit = false;
				m_entry = true;
				callback();
				m_entry = false;
				didCallback = true;
				ZTH_FALLTHROUGH
			case evalIdle: {
				do {
					m_evalState = evalState;
					while((wait = evalOnce()).hasPassed())
						didCallback = true;
					if(alwaysDoCallback && !didCallback)
						callback();
				} while(m_evalState == evalRecurse);
				m_evalState = evalIdle;
				break;
			}
			case evalState:
				m_evalState = evalRecurse;
				ZTH_FALLTHROUGH
			case evalRecurse:
				break;
			}
			return wait;
		}

		void run() {
			TimeInterval wait = eval();
			if(!m_state->guard.valid()) {
		done:
				zth_dbg(fsm, "[%s] Final state %s", id_str(), str(state()).c_str());
				return;
			}

			PolledMemberWaiting<Fsm> wakeup(*this, &Fsm::trigger_, wait);
			do {
				if(wakeup.interval().isInfinite()) {
					m_trigger.wait();
				} else {
					scheduleTask(wakeup);
					m_trigger.wait();
					unscheduleTask(wakeup);
				}
				wait = eval();
				wakeup.setInterval(wait, currentFiber().runningSince());
			} while(m_state->guard.valid());
			goto done;
		}

		void trigger() {
			m_trigger.signal(false);
		}

	private:
		bool trigger_() {
			m_trigger.signal(false);
			return true;
		}

		TimeInterval evalOnce() {
			TimeInterval wait = TimeInterval::infinity();
			StateAddr p = m_state;
			while(p->guard.valid()) {
				TimeInterval ti((p++)->guard(static_cast<FsmImpl&>(*this)));
				if(ti.hasPassed()) {
					setState(p->stateAddr);
					wait = ti; // NVRO
					return wait; // Return any passed TimeInterval, which indicates that a transition was taken.
				} else if(ti < wait) {
					// Save shortest wait interval.
					wait = ti;
				}
			}
			zth_dbg(fsm, "[%s] State %s blocked for %s", id_str(), str(state()).c_str(), wait.str().c_str());
			return wait;
		}

		void setState(StateAddr nextState) {
			if(nextState == m_state)
				zth_dbg(fsm, "[%s] Selfloop of state %s", id_str(), str(state()).c_str());
			else
				zth_dbg(fsm, "[%s] Transition %s -> %s", id_str(), str(state()).c_str(), str(nextState->state).c_str());

			m_stateNext = nextState;

			m_exit = true;
			callback();
			m_exit = false;

			m_state = nextState;
			m_t = Timestamp::now();

			switch(m_lockstep) {
			case lockstepSteppingNext:
				m_lockstep = lockstepStepping;
				break;
			case lockstepStepping:
				m_lockstep = lockstepLock;
				break;
			default:;
			}

			m_entry = true;
			callback();
			m_entry = false;
		}

	protected:
		/*!
		 * \brief This function is called when the FSM has to execute the code belonging by the current state.
		 * \details It is called upon entry and exit of a state, and arbitrary times during a state.
		 */
		virtual void callback() = 0;

	private:
		union {
			FsmDescription<FsmImpl>* m_description; // only valid during stateCompile
			Compiler const* m_compiler; // only valid during stateInit
			FsmDescription<FsmImpl> const* m_compiledDescription; // otherwise
		};
		StateAddr m_state;
		StateAddr m_stateNext;
		EvalState m_evalState;
		Timestamp m_t;
		bool m_entry;
		bool m_exit;
		Lockstep m_lockstep;
		Signal m_trigger;
		std::vector<Input> m_inputs;
	};

	template <typename State_, typename CallbackArg_ = void, typename Input_ = int, typename FsmImpl_ = void>
	class FsmCallback : public Fsm<State_,Input_,typename choose_type<FsmImpl_, FsmCallback<State_, CallbackArg_, Input_, void> >::type> {
	public:
		typedef typename choose_type<FsmImpl_, FsmCallback<State_, CallbackArg_, Input_, void> >::type FsmImpl;
		typedef Fsm<State_,Input_,FsmImpl> base;
		typedef CallbackArg_ CallbackArg;
		typedef void (*Callback)(FsmCallback&, CallbackArg);

		explicit FsmCallback(typename base::Compiler const& compiler, Callback callback, CallbackArg callbackArg, char const* name = "FSM")
			: base(compiler, name)
			, m_callback(callback)
			, m_callbackArg(callbackArg)
		{}

		// cppcheck-suppress passedByValue
		explicit FsmCallback(typename base::Description description, Callback callback, CallbackArg callbackArg, char const* name = "FSM")
			: base(description, name)
			, m_callback(callback)
			, m_callbackArg(callbackArg)
		{}

		virtual ~FsmCallback() {}

		CallbackArg callbackArg() const { return m_callbackArg; }

	protected:
		virtual void callback() {
			if(m_callback) {
				zth_dbg(fsm, "[%s] Callback for %s%s", this->id_str(), str(this->state()).c_str(), this->entry() ? " (entry)" : this->exit() ? " (exit)" : "");
				m_callback(*this, m_callbackArg);
			}
		}

	private:
		Callback const m_callback;
		CallbackArg const m_callbackArg;
	};

	template <typename State_, typename Input_, typename FsmImpl_>
	class FsmCallback<State_,void,Input_,FsmImpl_> : public Fsm<State_,Input_,typename choose_type<FsmImpl_,FsmCallback<State_,void,Input_,void> >::type> {
	public:
		typedef typename choose_type<FsmImpl_, FsmCallback<State_, void, Input_, void> >::type FsmImpl;
		typedef Fsm<State_,Input_,FsmImpl> base;
		typedef void (*Callback)(FsmCallback&);

		explicit FsmCallback(typename base::Compiler const& compiler, Callback callback = NULL, char const* name = "FSM")
			: base(compiler, name)
			, m_callback(callback)
		{}

		// cppcheck-suppress passedByValue
		explicit FsmCallback(typename base::Description description, Callback callback = NULL, char const* name = "FSM")
			: base(description, name)
			, m_callback(callback)
		{}

		virtual ~FsmCallback() {}

	protected:
		virtual void callback() {
			if(m_callback) {
				zth_dbg(fsm, "[%s] Callback for %s%s", this->id_str(), str(this->state()).c_str(), this->entry() ? " (entry)" : this->exit() ? " (exit)" : "");
				m_callback(*this);
			}
		}

	private:
		Callback const m_callback;
	};

} // namespace
#endif // __cplusplus
#endif // ZTH_FSM_H
