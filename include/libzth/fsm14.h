#ifndef ZTH_FSM14_H
#define ZTH_FSM14_H
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
 * \defgroup zth_api_cpp_fsm14 fsm (C++14)
 * \ingroup zth_api_cpp
 */

#if defined(__cplusplus) && __cplusplus >= 201402L

#	include <libzth/macros.h>
#	include <libzth/allocator.h>
#	include <libzth/util.h>

#	include <bitset>
#	include <functional>
#	include <limits>
#	include <type_traits>
#	include <utility>

namespace zth {
namespace fsm {



///////////////////////////////////////////
// Misc utilities
//

#	ifdef DOXYGEN

template <typename T>
struct function_traits;

#	else // !DOXYGEN

template <typename R, typename C, typename A>
struct function_traits_detail {
	using return_type = R;
	using class_type = C;
	using arg_type = A;

	static constexpr bool is_member = !std::is_same<C, void>::value;
	static constexpr bool is_functor = false;
};

// Assume it's a lambda, with captures (so it is a functor).
template <typename T>
struct function_traits : function_traits<decltype(&T::operator())> {
	static constexpr bool is_functor = true;
};

// R() normal function
template <typename R>
struct function_traits<R (&)()> : function_traits_detail<R, void, void> {};

// R(A) normal function
template <typename R, typename A>
struct function_traits<R (&)(A)> : function_traits_detail<R, void, A> {};

// R() normal function
template <typename R>
struct function_traits<R (*)()> : function_traits_detail<R, void, void> {};

// R(A) normal function
template <typename R, typename A>
struct function_traits<R (*)(A)> : function_traits_detail<R, void, A> {};

// R(C::*)() class member
template <typename R, typename C>
struct function_traits<R (C::*)()> : function_traits_detail<R, C, void> {};

// R(C::*)(A) class member
template <typename R, typename C, typename A>
struct function_traits<R (C::*)(A)> : function_traits_detail<R, C, A> {};

// R(C::*)() const class member
template <typename R, typename C>
struct function_traits<R (C::*)() const> : function_traits_detail<R, C, void> {};

// R(C::*)(A) const class member
template <typename R, typename C, typename A>
struct function_traits<R (C::*)(A) const> : function_traits_detail<R, C, A> {};

#	endif // !DOXYGEN


///////////////////////////////////////////
// Symbol
//

class Fsm;

/*!
 * \ingroup zth_api_cpp_fsm14
 */
class Symbol {
public:
	constexpr Symbol(char const* s = nullptr) noexcept
		: m_symbol{s}
	{}

	constexpr bool constexpr_eq(Symbol const& s) const noexcept
	{
		char const* a = m_symbol;
		char const* b = s.m_symbol;

		// We cannot compare the pointers a == b directly in a
		// constexpr context.  This implementation does a string
		// compare.

		if(!a ^ !b)
			return false;
		if(!a && !b)
			return true;

		while(*a && *a == *b) {
			a++;
			b++;
		}

		return *a == *b;
	}

	bool operator==(Symbol const& s) const noexcept
	{
		return *this == s.m_symbol;
	}

	bool operator==(char const* s) const noexcept
	{
		return m_symbol == s || constexpr_eq(s);
	}

	template <typename S>
	bool operator!=(S&& s) const noexcept
	{
		return !(*this == std::forward<S>(s));
	}

	char const* str() const noexcept
	{
		return m_symbol;
	}

	operator char const *() const noexcept
	{
		return str();
	}

	constexpr bool valid() const noexcept
	{
		return m_symbol;
	}

	operator bool() const noexcept
	{
		return valid();
	}

private:
	char const* m_symbol{};
};

/*!
 * \ingroup zth_api_cpp_fsm14
 */
constexpr Symbol operator""_S(char const* s, size_t UNUSED_PAR(len)) noexcept
{
	return s;
}

/*!
 * \ingroup zth_api_cpp_fsm14
 */
using State = Symbol;

template <bool enable = Config::NamedFsm>
class Named {};

template <>
class Named<false> {
protected:
	constexpr explicit Named(char const* UNUSED_PAR(name) = nullptr) noexcept {}

	~Named() = default;

public:
	cow_string name() const
	{
		return format("%p", this);
	}
};

template <>
class Named<true> : public Named<false> {
protected:
	constexpr explicit Named(char const* name) noexcept
		: m_name{name}
	{}

	~Named() = default;

public:
	cow_string name() const
	{
		if(m_name)
			return m_name;

		return Named<false>::name();
	}

private:
	char const* m_name;
};

template <
	typename T, typename R,
	bool haveArg = !std::is_same<typename function_traits<T>::arg_type, void>::value,
	bool isMember = !haveArg && function_traits<T>::is_member && !function_traits<T>::is_functor
			&& std::is_base_of<Fsm, typename function_traits<T>::class_type>::value,
	bool isOk =
		std::is_same<typename function_traits<T>::return_type, R>::value
		&& (!haveArg
		    || std::is_convertible<Fsm&, typename function_traits<T>::arg_type>::value
		    || std::is_base_of<
			    Fsm,
			    std::remove_reference_t<typename function_traits<T>::arg_type>>::value)>
class Callback {};

template <typename T, typename R>
class Callback<T, R, false, false, true> : public Named<> {
public:
	template <typename T_>
	constexpr Callback(T_&& c, char const* name = nullptr)
		: Named<>{name}
		, m_callback{std::forward<T_>(c)}
	{}

	R call(Fsm& UNUSED_PAR(fsm)) const
	{
		return m_callback();
	}

private:
	T m_callback;
};

template <typename T, typename R>
class Callback<T, R, true, false, true> : public Named<> {
public:
	template <typename T_>
	constexpr Callback(T_&& c, char const* name = nullptr)
		: Named<>{name}
		, m_callback{std::forward<T_>(c)}
	{}

	R call(Fsm& fsm) const
	{
		using A = typename function_traits<T>::arg_type;
		check<A>(fsm);
		return m_callback(static_cast<A>(fsm));
	}

private:
	template <
		typename A,
		std::enable_if_t<std::is_base_of<Fsm, std::remove_reference_t<A>>::value, int> = 0>
	static void check(Fsm& fsm)
	{
#	ifdef __GXX_RTTI
		zth_assert(dynamic_cast<std::remove_reference_t<A>*>(&fsm) != nullptr);
#	endif
	}

	template <
		typename A,
		std::enable_if_t<!std::is_base_of<Fsm, std::remove_reference_t<A>>::value, int> = 0>
	static void check(Fsm& fsm)
	{}

private:
	T m_callback;
};

template <typename T, typename R>
class Callback<T, R, false, true, true> : public Named<> {
public:
	template <typename T_>
	constexpr Callback(T_&& c, char const* name = nullptr)
		: Named<>{name}
		, m_callback{std::forward<T_>(c)}
	{}

	R call(Fsm& fsm) const
	{
		using C = typename function_traits<T>::class_type;
#	ifdef __GXX_RTTI
		zth_assert(dynamic_cast<C*>(&fsm) != nullptr);
#	endif
		return ((static_cast<C&>(fsm)).*m_callback)();
	}

private:
	T m_callback;
};

class Guard {
protected:
	constexpr Guard() = default;
	~Guard() = default;

	Guard(Guard&&) noexcept = default;
	Guard& operator=(Guard&&) noexcept = default;

public:
	Guard(Guard const&) = delete;
	void operator=(Guard const&) = delete;

	virtual bool enabled(Fsm& fsm) const = 0;
	virtual cow_string name() const = 0;

	bool operator()(Fsm& fsm) const
	{
		return enabled(fsm);
	}
};

template <typename T>
class TypedGuard final
	: public Guard
	, protected Callback<T, bool> {
public:
	using Callback_type = Callback<T, bool>;

	template <typename T_>
	constexpr explicit TypedGuard(T_&& g, char const* name = nullptr)
		: Callback_type{std::forward<T_>(g), name}
	{}

	virtual bool enabled(Fsm& fsm) const final
	{
		return this->call(fsm);
	}

	virtual cow_string name() const final
	{
		return Callback_type::name();
	}
};

/*!
 * \ingroup zth_api_cpp_fsm14
 */
template <typename T>
constexpr auto guard(T&& g, char const* name = nullptr)
{
	return TypedGuard<std::decay_t<T>>{std::forward<T>(g), name};
}

inline bool always_guard()
{
	return true;
}

/*!
 * \ingroup zth_api_cpp_fsm14
 */
// This could be solved simply by using [](){ return true; } as function, but
// clang-tidy trips on it.  For now, just use the seperate function.  Might be
// related to: https://bugs.llvm.org/show_bug.cgi?id=20209
inline17 constexpr auto always = guard(always_guard, "always");

inline bool never_guard()
{
	return false;
}

/*!
 * \ingroup zth_api_cpp_fsm14
 */
inline17 constexpr auto never = guard(never_guard, "never");

class Action {
protected:
	constexpr Action() = default;
	~Action() = default;

	Action(Action&&) noexcept = default;
	Action& operator=(Action&&) noexcept = default;

public:
	Action(Action const&) = delete;
	void operator=(Action const&) = delete;

	virtual void run(Fsm& UNUSED_PAR(fsm)) const {}

	virtual cow_string name() const
	{
		return format("%p\n", this);
	}

	void operator()(Fsm& fsm) const
	{
		run(fsm);
	}
};

template <typename T>
class TypedAction final
	: public Action
	, protected Callback<T, void> {
public:
	using Callback_type = Callback<T, void>;

	template <typename T_>
	constexpr explicit TypedAction(T_&& a, char const* name = nullptr)
		: Callback_type{std::forward<T_>(a), name}
	{}

	virtual void run(Fsm& fsm) const final
	{
		this->call(fsm);
	}

	virtual cow_string name() const final
	{
		return Callback_type::name();
	}
};

/*!
 * \ingroup zth_api_cpp_fsm14
 */
template <typename T>
constexpr auto action(T&& a, char const* name = nullptr)
{
	return TypedAction<std::decay_t<T>>{std::forward<T>(a), name};
}

inline void nothing_action() {}

/*!
 * \ingroup zth_api_cpp_fsm14
 */
inline17 constexpr auto nothing = action(nothing_action, "nothing");

class GuardedActionBase
	: public Guard
	, protected Action {
protected:
	constexpr GuardedActionBase() = default;

public:
	virtual bool tryRun(Fsm& fsm) const
	{
		if(!enabled(fsm))
			return false;

		run(fsm);
		return true;
	}

	bool operator()(Fsm& fsm) const
	{
		return tryRun(fsm);
	}
};

class GuardedAction final : public GuardedActionBase {
public:
	constexpr GuardedAction(Guard const& guard, Action const& action)
		: m_guard{guard}
		, m_action{action}
	{}

	constexpr GuardedAction(Guard const& guard)
		: GuardedAction{guard, nothing}
	{}

	constexpr GuardedAction(Action const& action)
		: GuardedAction{always, action}
	{}

	constexpr GuardedAction()
		: GuardedAction{always, nothing}
	{}

	virtual bool enabled(Fsm& fsm) const final
	{
		return m_guard.enabled(fsm);
	}

	constexpr auto const& guard() const
	{
		return m_guard;
	}

	constexpr auto const& action() const
	{
		return m_action;
	}

	virtual void run(Fsm& fsm) const final
	{
		m_action(fsm);
	}

	virtual cow_string name() const final
	{
		return format("%s / %s", guard().name().c_str(), action().name().c_str());
	}

private:
	Guard const& m_guard;
	Action const& m_action;
};

constexpr inline auto operator/(Guard const& g, Action const& a)
{
	return GuardedAction{g, a};
}

class Transition;

class TransitionStart final : public GuardedActionBase {
public:
	template <typename S>
	constexpr TransitionStart(S&& state, GuardedAction&& guardedAction)
		: m_state{std::forward<S>(state)}
		, m_guardedAction{std::move(guardedAction)}
	{}

	constexpr auto const& state() const
	{
		return m_state;
	}

	constexpr auto const& guard() const
	{
		return m_guardedAction.guard();
	}

	constexpr auto const& action() const
	{
		return m_guardedAction.action();
	}

	virtual bool enabled(Fsm& fsm) const final
	{
		return guard().enabled(fsm);
	}

	virtual bool tryRun(Fsm& fsm) const final
	{
		return m_guardedAction.tryRun(fsm);
	}

	virtual cow_string name() const override
	{
		return format("%s + %s", state().str(), m_guardedAction.name().c_str());
	}

private:
	State m_state;
	GuardedAction m_guardedAction;
};

constexpr inline auto operator+(State&& state, GuardedAction&& action)
{
	return TransitionStart{std::move(state), std::move(action)};
}

constexpr inline auto operator+(State&& state, Guard const& guard)
{
	return TransitionStart{std::move(state), GuardedAction{guard, nothing}};
}

constexpr inline auto operator+(Guard const& guard)
{
	return TransitionStart{State{}, GuardedAction{guard, nothing}};
}

constexpr inline auto operator/(State&& state, Action const& action)
{
	return TransitionStart{std::move(state), GuardedAction{always, action}};
}

class Transition final : public GuardedActionBase {
public:
	template <typename F>
	constexpr Transition(F&& from)
		: m_from{std::forward<F>(from)}
		, m_to{}
	{}

	template <typename F, typename T>
	constexpr Transition(F&& from, T&& to)
		: m_from{std::forward<F>(from)}
		, m_to{std::forward<T>(to)}
	{}

	virtual bool enabled(Fsm& fsm) const final
	{
		return m_from.enabled(fsm);
	}

	virtual bool tryRun(Fsm& fsm) const final
	{
		return m_from.tryRun(fsm);
	}

	constexpr auto const& from() const
	{
		return m_from.state();
	}

	constexpr auto const& guard() const
	{
		return m_from.guard();
	}

	constexpr auto const& action() const
	{
		return m_from.action();
	}

	constexpr auto const& to() const
	{
		return m_to;
	}

	virtual cow_string name() const final
	{
		return format("%s >>= %s", m_from.name().c_str(), m_to.str());
	}

private:
	TransitionStart m_from;
	State m_to;
};

constexpr inline Transition operator>>=(TransitionStart&& from, State&& to)
{
	return Transition{std::move(from), std::move(to)};
}

constexpr inline Transition operator>>=(State&& from, State&& to)
{
	return Transition{std::move(from) + always, std::move(to)};
}

constexpr inline Transition operator>>=(Guard const& from, State&& to)
{
	return Transition{State{} + from, std::move(to)};
}

constexpr inline Transition operator>>=(Action const& from, State&& to)
{
	return Transition{State{} / from, std::move(to)};
}

constexpr inline Transition operator>>=(GuardedAction&& from, State&& to)
{
	return Transition{State{} + std::move(from), std::move(to)};
}

class TransitionsBase {
public:
	using index_type = size_t;

	virtual size_t size() const = 0;
	virtual Guard const& guard(index_type i) const = 0;
	virtual Action const& action(index_type i) const = 0;
	virtual index_type to(index_type i) const = 0;
	virtual State const& state(index_type i) const = 0;

	Fsm init() const;

	template <typename F, std::enable_if_t<std::is_base_of<Fsm, F>::value, int> = 0>
	F& init(F& fsm) const
	{
		fsm.init(*this);
		return fsm;
	}

	void dump(FILE* f = stdout) const
	{
		size_t size_ = size();
		for(index_type i = 0; i < size_; i++) {
			fprintf(f, "%3zu: %-8s + %-18s / %-18s >>= %3zu\n", i, state(i).str(),
				guard(i).name().c_str(), action(i).name().c_str(), to(i));
		}
	}
};

template <size_t Size>
class Transitions final : public TransitionsBase {
protected:
	using Index = size_t;

	struct CompiledTransition {
		constexpr CompiledTransition()
			: guard{}
			, action{}
			, from{}
			, to{}
		{}

		Guard const* guard;
		Action const* action;
		State from;
		Index to;
	};

	constexpr Transitions() = default;

public:
	static constexpr Transitions compile(std::initializer_list<Transition> l)
	{
		zth_assert(l.size() == Size);

		Transitions f;

		if(Size == 0) {
			f.m_transitions[0].guard = &never;
			f.m_transitions[0].action = &nothing;
		} else {
			f.m_transitions[0].guard = &always;
			f.m_transitions[0].action = &nothing;
			f.m_transitions[0].to = 1;
		}

		index_type i = 1;
		State prev;

		for(auto const& t : l) {
			if(!prev.constexpr_eq(t.from()) && t.from().valid()) {
				prev = f.m_transitions[i].from = t.from();
				if(find(t.from(), l) != (size_t)i)
					throw std::invalid_argument(
						"State transitions are not contiguous");
			}
			f.m_transitions[i].guard = &t.guard();
			f.m_transitions[i].action = &t.action();
			f.m_transitions[i].to = static_cast<Index>(find(t.to(), l));
			i++;
		}

		return std::move(f);
	}

	virtual size_t size() const final
	{
		return Size + 1u;
	}

	virtual Guard const& guard(index_type i) const final
	{
		zth_assert(i < size());
		return *m_transitions[i].guard;
	}

	virtual Action const& action(index_type i) const final
	{
		zth_assert(i < size());
		return *m_transitions[i].action;
	}

	virtual index_type to(index_type i) const final
	{
		zth_assert(i < size());
		return static_cast<index_type>(m_transitions[i].to);
	}

	virtual State const& state(index_type i) const final
	{
		zth_assert(i < size());
		return m_transitions[i].from;
	}

private:
	static constexpr size_t find(State const& state, std::initializer_list<Transition> l)
	{
		if(!state.valid())
			return 0;

		size_t i = 0;
		for(auto const& t : l) {
			if(t.from().constexpr_eq(state))
				return i + 1;
			i++;
		}

#	if GCC_VERSION < 90000
		// This following if-statement is not required,
		// but triggers https://gcc.gnu.org/bugzilla/show_bug.cgi?id=67371 otherwise.
		if(i >= l.size())
#	endif
			throw std::invalid_argument{"Target state not found"};

		// Unreachable.
		return 0;
	}

private:
	static_assert(std::numeric_limits<Index>::max() > Size, "");
	CompiledTransition m_transitions[Size + 1];
};

/*!
 * \ingroup zth_api_cpp_fsm14
 */
template <typename... T>
constexpr auto compile(T&&... t)
{
	return Transitions<sizeof...(T)>::compile({Transition{std::forward<T>(t)}...});
}

/*!
 * \ingroup zth_api_cpp_fsm14
 */
class Fsm : public UniqueID<Fsm> {
	ZTH_CLASS_NEW_DELETE(Fsm)
public:
	using index_type = TransitionsBase::index_type;

	enum class Flag : size_t {
		entry,
		selfloop,
		blocked,
		transition,
		pushed,
		popped,
		stop,
		flags, // Not a flag, just a count of the other flags.
	};

	Fsm() = default;

	Fsm(Fsm&& f) noexcept
	{
		*this = std::move(f);
	}

	Fsm& operator=(Fsm&&) noexcept
	{
		return *this;
	}

	virtual ~Fsm() = default;

	bool valid() const
	{
		return m_fsm;
	}

	virtual void reset()
	{
		zth_dbg(fsm, "[%s] Reset", id_str());
		m_prev = m_transition = m_state = 0;
		m_flags.reset();
	}

	State const& state() const
	{
		zth_assert(valid());
		return m_fsm->state(m_state);
	}

	bool step()
	{
		zth_assert(valid());

		auto i = m_state;
		auto size = m_fsm->size();

		// Find next enabled guard.
		while(!m_fsm->guard(i).enabled(*this)) {
			if(++i == size || m_fsm->state(i).valid()) {
				// No enabled guards.
				setFlag(Flag::blocked);
				zth_dbg(fsm, "[%s] Blocked", id_str());
				return false;
			}
		}

		// Got it. Take transition.
		m_transition = i;
		auto to = m_fsm->to(i);

		clearFlag(Flag::blocked);
		clearFlag(Flag::popped);
		clearFlag(Flag::pushed);

		if(!to) {
			// No transition, only do the action.
			zth_dbg(fsm, "[%s] Guard %s enabled, no transition\n", id_str(),
				m_fsm->guard(i).name().c_str());
			clearFlag(Flag::transition);
			enter();
		} else {
			zth_dbg(fsm, "[%s] Guard %s enabled, transition %s -> %s\n", id_str(),
				m_fsm->guard(i).name().c_str(), m_fsm->state(m_state).str(),
				m_fsm->state(to).str());

			setFlag(Flag::selfloop, to == m_state);
			setFlag(Flag::transition);
			if(m_state)
				leave();

			m_prev = m_state;
			m_state = to;

			enter();
			setFlag(Flag::entry);
		}

		return true;
	}

	void run()
	{
		zth_dbg(fsm, "[%s] Run", id_str());
		clearFlag(Flag::stop);

		while(step() && !flag(Flag::stop))
			;
	}

	void stop()
	{
		zth_dbg(fsm, "[%s] Stop requested", id_str());
		setFlag(Flag::stop);
	}

	void push()
	{
		zth_assert(valid());

		m_stack.push_back(m_prev);
		setFlag(Flag::pushed);
		zth_dbg(fsm, "[%s] Push", id_str());
	}

	virtual void pop()
	{
		zth_assert(valid());
		zth_assert(!m_stack.empty());

		index_type to = m_stack.back();
		zth_assert(to != 0);

		zth_dbg(fsm, "[%s] Pop %s -> %s", id_str(), m_fsm->state(m_state).str(),
			m_fsm->state(to).str());

		clearFlag(Flag::blocked);
		clearFlag(Flag::pushed);
		setFlag(Flag::selfloop, m_state == to);
		setFlag(Flag::transition);
		leave();

		m_prev = m_state;
		m_state = to;
		m_stack.pop_back();
		setFlag(Flag::popped);

		enter();
		setFlag(Flag::entry);
	}

	bool flag(Flag f) const
	{
		return m_flags.test(flagIndex(f));
	}

	bool entryGuard()
	{
		if(!flag(Flag::entry))
			return false;

		clearFlag(Flag::entry);
		return true;
	}

protected:
	bool setFlag(Flag f, bool value = true)
	{
		m_flags.set(flagIndex(f), value);
		return value;
	}

	void clearFlag(Flag f)
	{
		setFlag(f, false);
	}

	virtual void leave()
	{
		zth_dbg(fsm, "[%s] Leave %s", id_str(), state().str());
	}

	virtual void enter()
	{
		if(flag(Flag::popped)) {
			zth_dbg(fsm, "[%s] Enter %s (popped)", id_str(), state().str());
		} else {
			zth_dbg(fsm, "[%s] Enter %s%s; run action %s", id_str(), state().str(),
				flag(Flag::selfloop) ? " (loop)" : "",
				m_fsm->action(m_transition).name().c_str());

			m_fsm->action(m_transition).run(*this);
		}
	}

private:
	static constexpr size_t flagIndex(Flag f)
	{
		return static_cast<size_t>(f);
	}

	void init(TransitionsBase const& c)
	{
		if(c.size() == 0)
			// Invalid.
			return;

		m_fsm = &c;
		reset();
	}

	friend TransitionsBase;

private:
	TransitionsBase const* m_fsm{};
	std::bitset<static_cast<size_t>(Flag::flags)> m_flags;
	index_type m_prev{};
	index_type m_transition{};
	index_type m_state{};
	vector_type<index_type>::type m_stack;
};

/*!
 * \ingroup zth_api_cpp_fsm14
 */
inline17 static constexpr auto entry = guard(&Fsm::entryGuard, "entry");

/*!
 * \ingroup zth_api_cpp_fsm14
 */
inline17 static constexpr auto push = action(&Fsm::push, "push");

/*!
 * \ingroup zth_api_cpp_fsm14
 */
inline17 static constexpr auto pop = action(&Fsm::pop, "pop");

/*!
 * \ingroup zth_api_cpp_fsm14
 */
inline17 static constexpr auto stop = action(&Fsm::stop, "stop");

Fsm TransitionsBase::init() const
{
	Fsm fsm;
	fsm.init(*this);
	return std::move(fsm);
}

} // namespace fsm
} // namespace zth
#endif // C++14
#endif // ZTH_FSM14_H
