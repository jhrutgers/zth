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
 * \brief A fiber-aware FSM implementation, with concise eDSL and constexpr compatible.
 * \see \ref fsm14.cpp
 * \ingroup zth_api_cpp
 */

#if defined(__cplusplus) && __cplusplus >= 201402L

#	include <libzth/macros.h>
#	include <libzth/allocator.h>
#	include <libzth/sync.h>
#	include <libzth/util.h>

#	include <bitset>
#	include <functional>
#	include <limits>
#	include <stdexcept>
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

#		if defined(__cpp_noexcept_function_type) && __cpp_noexcept_function_type >= 201510
template <typename R>
struct function_traits<R (&)() noexcept> : function_traits_detail<R, void, void> {};
#		endif

// R(A) normal function
template <typename R, typename A>
struct function_traits<R (&)(A)> : function_traits_detail<R, void, A> {};

#		if defined(__cpp_noexcept_function_type) && __cpp_noexcept_function_type >= 201510
template <typename R, typename A>
struct function_traits<R (&)(A) noexcept> : function_traits_detail<R, void, A> {};
#		endif

// R() normal function
template <typename R>
struct function_traits<R (*)()> : function_traits_detail<R, void, void> {};

#		if defined(__cpp_noexcept_function_type) && __cpp_noexcept_function_type >= 201510
template <typename R>
struct function_traits<R (*)() noexcept> : function_traits_detail<R, void, void> {};
#		endif

// R(A) normal function
template <typename R, typename A>
struct function_traits<R (*)(A)> : function_traits_detail<R, void, A> {};

#		if defined(__cpp_noexcept_function_type) && __cpp_noexcept_function_type >= 201510
template <typename R, typename A>
struct function_traits<R (*)(A) noexcept> : function_traits_detail<R, void, A> {};
#		endif

// R(C::*)() class member
template <typename R, typename C>
struct function_traits<R (C::*)()> : function_traits_detail<R, C, void> {};

#		if defined(__cpp_noexcept_function_type) && __cpp_noexcept_function_type >= 201510
template <typename R, typename C>
struct function_traits<R (C::*)() noexcept> : function_traits_detail<R, C, void> {};
#		endif

// R(C::*)(A) class member
template <typename R, typename C, typename A>
struct function_traits<R (C::*)(A)> : function_traits_detail<R, C, A> {};

#		if defined(__cpp_noexcept_function_type) && __cpp_noexcept_function_type >= 201510
template <typename R, typename C, typename A>
struct function_traits<R (C::*)(A) noexcept> : function_traits_detail<R, C, A> {};
#		endif

// R(C::*)() const class member
template <typename R, typename C>
struct function_traits<R (C::*)() const> : function_traits_detail<R, C, void> {};

#		if defined(__cpp_noexcept_function_type) && __cpp_noexcept_function_type >= 201510
template <typename R, typename C>
struct function_traits<R (C::*)() const noexcept> : function_traits_detail<R, C, void> {};
#		endif

// R(C::*)(A) const class member
template <typename R, typename C, typename A>
struct function_traits<R (C::*)(A) const> : function_traits_detail<R, C, A> {};

#		if defined(__cpp_noexcept_function_type) && __cpp_noexcept_function_type >= 201510
template <typename R, typename C, typename A>
struct function_traits<R (C::*)(A) const noexcept> : function_traits_detail<R, C, A> {};
#		endif

#	endif // !DOXYGEN


///////////////////////////////////////////
// Symbol
//

class Fsm;

/*!
 * \brief Exception thrown when the FSM description is incorrect.
 * \ingroup zth_api_cpp_fsm14
 */
struct invalid_fsm : public std::logic_error {
	explicit invalid_fsm(char const* str)
		: std::logic_error{str}
	{}
};

/*!
 * \brief A input/state symbol.
 *
 * This is wrapper for a string literal.
 *
 * \ingroup zth_api_cpp_fsm14
 */
class Symbol {
	ZTH_CLASS_NEW_DELETE(Symbol)
public:
	/*!
	 * \brief Ctor.
	 * \param s the string, or \c nullptr to create a default (invalid) Symbol.
	 */
	constexpr Symbol(char const* s = nullptr) noexcept
		: m_symbol{s}
	{}

	/*!
	 * \brief Like == operator, but constexpr.
	 */
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

	/*!
	 * \brief Return a string representation of this symbol.
	 */
	__attribute__((returns_nonnull)) constexpr char const* str() const noexcept
	{
		return m_symbol ? m_symbol : "-";
	}

	/*!
	 * \brief Return the symbol string.
	 * \return \c nullptr in case of the default (invalid) Symbol
	 */
	constexpr char const* symbol() const noexcept
	{
		return m_symbol;
	}

	/*!
	 * \copydoc str()
	 */
	__attribute__((returns_nonnull)) operator char const *() const noexcept
	{
		return str();
	}

	/*!
	 * \brief Check if the symbol is valid.
	 */
	constexpr bool valid() const noexcept
	{
		return m_symbol;
	}

	/*!
	 * \copydoc valid()
	 */
	operator bool() const noexcept
	{
		return valid();
	}

private:
	/*! \brief The symbol string. */
	char const* m_symbol{};
};

/*!
 * \brief Literal suffix to convert a string literal to #zth::fsm::Symbol.
 * \ingroup zth_api_cpp_fsm14
 */
constexpr Symbol operator""_S(char const* s, size_t UNUSED_PAR(len)) noexcept
{
	return s;
}

/*!
 * \brief A state within the FSM.
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
		std::is_convertible<typename function_traits<T>::return_type, R>::value
		&& (!haveArg
		    || std::is_convertible<Fsm&, typename function_traits<T>::arg_type>::value
		    || std::is_base_of<
			    Fsm,
			    std::remove_reference_t<typename function_traits<T>::arg_type>>::value)>
class Callback {};

template <typename T, typename R>
class Callback<T, R, false, false, true> : public Named<> {
	ZTH_CLASS_NEW_DELETE(Callback)
public:
	template <typename T_>
	constexpr Callback(T_&& c, char const* name = nullptr)
		: Named{name}
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
	ZTH_CLASS_NEW_DELETE(Callback)
public:
	template <typename T_>
	constexpr Callback(T_&& c, char const* name = nullptr)
		: Named{name}
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
	static void check(Fsm& UNUSED_PAR(fsm))
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
	ZTH_CLASS_NEW_DELETE(Callback)
public:
	template <typename T_>
	constexpr Callback(T_&& c, char const* name = nullptr)
		: Named{name}
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

class GuardPollInterval : public TimeInterval {
	ZTH_CLASS_NEW_DELETE(GuardPollInterval)
public:
	constexpr GuardPollInterval(bool enabled = false)
		: TimeInterval(enabled ? TimeInterval::null() : TimeInterval::infinity())
	{}

	constexpr GuardPollInterval(GuardPollInterval&&) noexcept = default;
	GuardPollInterval& operator=(GuardPollInterval&&) noexcept = default;
	constexpr GuardPollInterval(GuardPollInterval const&) noexcept = default;
	GuardPollInterval& operator=(GuardPollInterval const&) noexcept = default;

	template <typename... A>
	constexpr GuardPollInterval(A&&... a) noexcept
		: TimeInterval(std::forward<A>(a)...)
	{}

	constexpr operator bool() const noexcept
	{
		return hasPassed();
	}
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

	virtual GuardPollInterval enabled(Fsm& fsm) const = 0;
	virtual cow_string name() const = 0;

	auto operator()(Fsm& fsm) const
	{
		return enabled(fsm);
	}
};

template <typename T>
class TypedGuard final
	: public Guard
	, protected Callback<T, GuardPollInterval> {
	ZTH_CLASS_NEW_DELETE(TypedGuard)
public:
	using Callback_type = Callback<T, GuardPollInterval>;

	template <typename T_>
	constexpr explicit TypedGuard(T_&& g, char const* name = nullptr)
		: Callback_type{std::forward<T_>(g), name}
	{}

	virtual GuardPollInterval enabled(Fsm& fsm) const final
	{
		return this->call(fsm);
	}

	virtual cow_string name() const final
	{
		return Callback_type::name();
	}
};

class InputGuard final : public Guard {
	ZTH_CLASS_NEW_DELETE(InputGuard)
public:
	constexpr explicit InputGuard(Symbol&& input)
		: m_input{std::move(input)}
	{}

	virtual GuardPollInterval enabled(Fsm& fsm) const final;

	virtual cow_string name() const final
	{
		return m_input.str();
	}

private:
	Symbol m_input;
};

/*!
 * \brief Create a guard from a function.
 *
 * The supported types are:
 *
 * - function pointer: <tt>R (*)(A)</tt>
 * - member function pointer: <tt>R (C::*)()</tt>
 * - const member function pointer <tt>R (C::*)() const</tt>
 * - lambda (C++17 when used in a \c constexpr): <tt>[...](A) -> R {...}</tt>
 *
 * The return type R must be \c bool or zth::TimeInterval.  The TimeInterval
 * indicates the time until the guard may become enabled and should be checked
 * again.  When the interval is 0 or negative, the guard is supposed to be
 * enabled.
 *
 * In case the return type is \c bool, \c false is equivalent to a time
 * interval of 0.  \c true is equivalent to an infinite time interval.
 *
 * The argument A may be omitted. If provided, it must be zth::fsm::Fsm&, or a
 * reference to the actual subclass type of zth::fsm::Fsm.
 *
 * The class type C must be zth::fsm::Fsm, or the type of the subclass of
 * zth::fsm::Fsm. The member function does not have to be static.
 *
 * For C++17 and higher, these function can also be \c noexcept.
 *
 * \return a Guard object, to be used in the FSM transitions description.
 * \ingroup zth_api_cpp_fsm14
 */
template <typename T>
constexpr auto guard(T&& g, char const* name = nullptr)
{
	return TypedGuard<std::decay_t<T>>{std::forward<T>(g), name};
}

/*!
 * \brief Create a guard from a input symbol.
 *
 * Using \c input("symbol") is usually better than \c guard("symbol").
 * However, you can create a \c constexpr guard using this function, while
 * input() only returns a temporary object.
 *
 * \return a Guard object, to be used in the FSM transitions description.
 * \see input(Symbol&&)
 * \ingroup zth_api_cpp_fsm14
 */
constexpr inline auto guard(Symbol&& input) noexcept
{
	return InputGuard{std::move(input)};
}

inline bool always_guard() noexcept
{
	return true;
}

/*!
 * \brief Trivial guard that is always enabled.
 * \ingroup zth_api_cpp_fsm14
 * \hideinitializer
 */
// This could be solved simply by using [](){ return true; } as function, but
// using lambda in constexpr is only allowed since C++17. So, use the separate
// function instead.
inline17 constexpr auto always = guard(always_guard, "always");

inline bool never_guard() noexcept
{
	return false;
}

/*!
 * \brief Trivial guard that is never enabled.
 * \ingroup zth_api_cpp_fsm14
 * \hideinitializer
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
	ZTH_CLASS_NEW_DELETE(TypedAction)
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
 * \brief Create an action from a function.
 *
 * The supported types are:
 *
 * - function pointer: <tt>void (*)(A)</tt>
 * - member function pointer: <tt>void (C::*)()</tt>
 * - const member function pointer <tt>void (C::*)() const</tt>
 * - lambda (C++17 when used in a \c constexpr): <tt>[...](A) {...}</tt>
 *
 * The argument A may be omitted. If provided, it must be zth::fsm::Fsm&, or a
 * reference to the actual subclass type of zth::fsm::Fsm.
 *
 * The class type C must be zth::fsm::Fsm, or the type of the subclass of
 * zth::fsm::Fsm. The member function does not have to be static.
 *
 * For C++17 and higher, these function can also be \c noexcept.
 *
 * \return an Action object, to be used in the FSM transitions description.
 * \ingroup zth_api_cpp_fsm14
 */
template <typename T>
constexpr auto action(T&& a, char const* name = nullptr)
{
	return TypedAction<std::decay_t<T>>{std::forward<T>(a), name};
}

inline void nothing_action() {}

/*!
 * \brief Trivial action that does nothing.
 * \ingroup zth_api_cpp_fsm14
 * \hideinitializer
 */
inline17 constexpr auto nothing = action(nothing_action, "nothing");

class GuardedActionBase
	: public Guard
	, protected Action {
protected:
	constexpr GuardedActionBase() = default;

public:
	virtual GuardPollInterval tryRun(Fsm& fsm) const
	{
		if(auto e = !enabled(fsm))
			return e;

		run(fsm);
		return true;
	}

	GuardPollInterval operator()(Fsm& fsm) const
	{
		return tryRun(fsm);
	}
};

class GuardedAction final : public GuardedActionBase {
	ZTH_CLASS_NEW_DELETE(GuardedAction)
public:
	constexpr GuardedAction(Guard const& guard, Action const& action) noexcept
		: m_guard{guard}
		, m_action{action}
	{}

	constexpr GuardedAction(Symbol&& input, Action const& action) noexcept
		: m_guard{never}
		, m_input{std::move(input)}
		, m_action{action}
	{}

	constexpr GuardedAction(Guard const& guard) noexcept
		: GuardedAction{guard, nothing}
	{}

	constexpr GuardedAction(Symbol&& input) noexcept
		: GuardedAction{std::move(input), nothing}
	{}

	constexpr GuardedAction(Action const& action) noexcept
		: GuardedAction{always, action}
	{}

	constexpr GuardedAction() noexcept
		: GuardedAction{always, nothing}
	{}

	constexpr bool isInput() const noexcept
	{
		return m_input.valid();
	}

	constexpr bool hasGuard() const noexcept
	{
		return !isInput();
	}

	constexpr Symbol const& input() const noexcept
	{
		return m_input;
	}

	virtual GuardPollInterval enabled(Fsm& fsm) const final;

	constexpr auto const& guard() const noexcept
	{
		zth_assert(hasGuard());
		return m_guard;
	}

	constexpr auto const& action() const noexcept
	{
		return m_action;
	}

	virtual void run(Fsm& fsm) const final
	{
		m_action(fsm);
	}

	virtual cow_string name() const final
	{
		return isInput() ? format("input %s / %s", input().str(), action().name().c_str())
				 : format(
					 "guard %s / %s", guard().name().c_str(),
					 action().name().c_str());
	}

private:
	Guard const& m_guard;
	Symbol m_input;
	Action const& m_action;
};

constexpr inline auto operator/(Guard const& g, Action const& a) noexcept
{
	return GuardedAction{g, a};
}

constexpr inline auto operator+(Symbol&& input) noexcept
{
	return GuardedAction{std::move(input)};
}

/*!
 * \brief Create a guard from an input symbol.
 *
 * This function returns a temporary, while \c guard("symbol") returns an
 * object that can be \c constexpr.
 *
 * \see guard(Symbol&&)
 * \ingroup zth_api_cpp_fsm14
 */
constexpr inline auto input(Symbol&& symbol) noexcept
{
	return GuardedAction{std::move(symbol)};
}

constexpr inline auto operator+(GuardedAction&& g) noexcept
{
	return std::move(g);
}

// + "a" / b is first compiled into GuardedAction{ "a" } / b.
// This operator/ should fix that by using "a" as input guard for b.
constexpr inline auto operator/(GuardedAction&& ga, Action const& a)
{
	if(!(ga.isInput() && &ga.action() == &nothing))
		throw invalid_fsm("Ambiguous /");

	return GuardedAction{Symbol{ga.input()}, a};
}

class Transition;

class TransitionStart final : public GuardedActionBase {
	ZTH_CLASS_NEW_DELETE(TransitionStart)
public:
	constexpr TransitionStart(State&& state) noexcept
		: m_state{std::move(state)}
		, m_guardedAction{never / nothing}
	{}

	constexpr TransitionStart(Guard const& guard) noexcept
		: m_state{}
		, m_guardedAction{guard / nothing}
	{}

	constexpr TransitionStart(Action const& action) noexcept
		: m_state{}
		, m_guardedAction{always / action}
	{}

	constexpr TransitionStart(GuardedAction&& ga) noexcept
		: m_state{}
		, m_guardedAction{std::move(ga)}
	{}

	constexpr TransitionStart(State&& state, GuardedAction&& guardedAction) noexcept
		: m_state{std::move(state)}
		, m_guardedAction{std::move(guardedAction)}
	{}

	constexpr auto const& state() const noexcept
	{
		return m_state;
	}

	constexpr bool isInput() const noexcept
	{
		return m_guardedAction.isInput();
	}

	constexpr bool hasGuard() const noexcept
	{
		return m_guardedAction.hasGuard();
	}

	constexpr Symbol const& input() const noexcept
	{
		return m_guardedAction.input();
	}

	constexpr auto const& guard() const noexcept
	{
		return m_guardedAction.guard();
	}

	constexpr auto const& action() const noexcept
	{
		return m_guardedAction.action();
	}

	virtual GuardPollInterval enabled(Fsm& fsm) const final
	{
		return m_guardedAction.enabled(fsm);
	}

	virtual GuardPollInterval tryRun(Fsm& fsm) const final
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

constexpr inline auto operator+(State&& state, GuardedAction&& action) noexcept
{
	return TransitionStart{std::move(state), std::move(action)};
}

constexpr inline auto operator+(State&& state, Guard const& guard) noexcept
{
	return TransitionStart{std::move(state), GuardedAction{guard, nothing}};
}

constexpr inline auto operator+(State&& state, Symbol&& input) noexcept
{
	return TransitionStart{std::move(state), GuardedAction{std::move(input)}};
}

constexpr inline auto operator+(char const* state, Symbol&& input) noexcept
{
	return TransitionStart{state, GuardedAction{std::move(input)}};
}

constexpr inline auto operator+(State&& state, char const* input) noexcept
{
	return TransitionStart{std::move(state), GuardedAction{input}};
}

constexpr inline auto const& operator+(Guard const& guard) noexcept
{
	return guard;
}

constexpr inline auto operator/(State&& state, Action const& action) noexcept
{
	return TransitionStart{std::move(state), GuardedAction{always, action}};
}

// "a" + "b" / c is first compiled into "a" + TransitionStart{ "b" + always / c }.
// This operator+ should fix that by converting "b" from State to Input guard.
constexpr inline auto operator+(State&& state, TransitionStart&& t)
{
	if(!(t.hasGuard() && &t.guard() == &always))
		throw invalid_fsm{"Ambiguous +"};

	return TransitionStart{std::move(state), GuardedAction{Symbol{t.state()}, t.action()}};
}

class Transition final : public GuardedActionBase {
	ZTH_CLASS_NEW_DELETE(Transition)
public:
	template <typename F>
	constexpr Transition(F&& from) noexcept
		: m_from{std::forward<F>(from)}
		, m_to{}
	{}

	template <typename F, typename T>
	constexpr Transition(F&& from, T&& to) noexcept
		: m_from{std::forward<F>(from)}
		, m_to{std::forward<T>(to)}
	{}

	virtual GuardPollInterval enabled(Fsm& fsm) const final
	{
		return m_from.enabled(fsm);
	}

	virtual GuardPollInterval tryRun(Fsm& fsm) const final
	{
		return m_from.tryRun(fsm);
	}

	constexpr auto const& from() const noexcept
	{
		return m_from.state();
	}

	constexpr bool isInput() const noexcept
	{
		return m_from.isInput();
	}

	constexpr bool hasGuard() const noexcept
	{
		return m_from.hasGuard();
	}

	constexpr Symbol const& input() const noexcept
	{
		return m_from.input();
	}

	constexpr auto const& guard() const noexcept
	{
		return m_from.guard();
	}

	constexpr auto const& action() const noexcept
	{
		return m_from.action();
	}

	constexpr auto const& to() const noexcept
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

constexpr inline Transition operator>>=(TransitionStart&& from, State&& to) noexcept
{
	return Transition{std::move(from), std::move(to)};
}

constexpr inline Transition operator>>=(State&& from, State&& to) noexcept
{
	return Transition{std::move(from) + always, std::move(to)};
}

constexpr inline Transition operator>>=(char const* from, State&& to) noexcept
{
	return Transition{State{from} + always, std::move(to)};
}

constexpr inline Transition operator>>=(Guard const& from, State&& to) noexcept
{
	return Transition{State{} + from, std::move(to)};
}

constexpr inline Transition operator>>=(Action const& from, State&& to) noexcept
{
	return Transition{State{} / from, std::move(to)};
}

constexpr inline Transition operator>>=(GuardedAction&& from, State&& to) noexcept
{
	return Transition{State{} + std::move(from), std::move(to)};
}

class TransitionsBase {
public:
	using index_type = size_t;

	virtual size_t size() const noexcept = 0;

	virtual bool hasGuard(index_type i) const noexcept = 0;

	virtual bool hasInput(index_type i) const noexcept
	{
		return !hasGuard(i);
	}

	virtual Guard const& guard(index_type i) const noexcept = 0;
	virtual GuardPollInterval enabled(index_type i, Fsm& fsm) const = 0;
	virtual Symbol input(index_type i) const noexcept = 0;
	virtual Action const& action(index_type i) const noexcept = 0;
	virtual index_type to(index_type i) const noexcept = 0;
	virtual State const& state(index_type i) const noexcept = 0;

	Fsm spawn() const;

	template <typename F, std::enable_if_t<std::is_base_of<Fsm, F>::value, int> = 0>
	F& init(F& fsm) const noexcept
	{
		fsm.init(*this);
		return fsm;
	}

	void dump(FILE* f = stdout) const
	{
		size_t size_ = size();
		for(index_type i = 0; i < size_; i++) {
			fprintf(f, "%3zu: %-16s + %s %-18s / %-18s >>= %3zu\n", i, state(i).str(),
				hasGuard(i) ? "guard" : "input",
				hasGuard(i) ? guard(i).name().c_str() : input(i).str(),
				action(i).name().c_str(), to(i));
		}
	}

	void uml(FILE* f = stdout) const
	{
		fprintf(f, "@startuml\n");

		size_t size_ = size();
		index_type statei = 0;
		size_t t = 0;
		for(index_type i = 0; i < size_; i++) {
			if(state(i).valid()) {
				statei = i;
				fprintf(f, "state \"%s\" as s%zu\n", state(statei).str(), statei);
				t = 0;
			}

			string edge;

			if(statei == 0)
				edge += "[*]";
			else
				edge += format("s%zu", statei);

			if(to(i))
				edge += format(" --> s%zu", to(i));

			edge += format(" : (%zu)", t);

			if(hasGuard(i)) {
				if(&guard(i) == &never)
					continue;

				if(&guard(i) != &always)
					edge += format(" [%s]", guard(i).name().c_str());
			} else {
				edge += format(" %s", input(i).str());
			}

			if(&action(i) != &nothing) {
				edge += format(" / %s", action(i).name().c_str());
			}

			fprintf(f, "%s\n", edge.c_str());
			t++;
		}

		fprintf(f, "@enduml\n");
	}
};

template <size_t Size>
class Transitions final : public TransitionsBase {
	ZTH_CLASS_NEW_DELETE(Transitions)
protected:
	using Index = typename smallest_uint<Size>::type;

	enum class Flag : uint8_t {
		input = 0x01,
	};

	struct CompiledTransition {
		constexpr CompiledTransition() noexcept
			: guard{}
			, action{}
			, from{}
			, to{}
			, flags{}
		{}

		// This is either a Guard const* or char const*, depending on
		// m_flags & Flag::input.  A union would be better, but you
		// cannot change the active union field in a contexpr before
		// C++20.
		void const* guard;
		Action const* action;
		State from;
		Index to;
		uint8_t flags;
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

			if(!l.begin()->from().valid())
				throw invalid_fsm{"First state must be valid"};
		}

		index_type i = 1;
		State prev;

		for(auto const& t : l) {
			if(!prev.constexpr_eq(t.from()) && t.from().valid()) {
				prev = f.m_transitions[i].from = t.from();
				if(find(t.from(), l) != (size_t)i)
					throw invalid_fsm{"State transitions are not contiguous"};
			}
			if(t.hasGuard()) {
				f.m_transitions[i].guard = &t.guard();
			} else {
				f.m_transitions[i].guard = t.input().symbol();
				f.m_transitions[i].flags = static_cast<uint8_t>(Flag::input);
			}
			f.m_transitions[i].action = &t.action();
			f.m_transitions[i].to = static_cast<Index>(find(t.to(), l));
			i++;
		}

		return f;
	}

	virtual size_t size() const noexcept final
	{
		return Size + 1u;
	}

	virtual bool isInput(index_type i) const noexcept final
	{
		return (m_transitions[i].flags & static_cast<uint8_t>(Flag::input)) != 0;
	}

	virtual bool hasGuard(index_type i) const noexcept final
	{
		return !isInput(i);
	}

	virtual Guard const& guard(index_type i) const noexcept final
	{
		zth_assert(i < size());
		zth_assert(hasGuard(i));
		return *static_cast<Guard const*>(m_transitions[i].guard);
	}

	virtual Symbol input(index_type i) const noexcept final
	{
		zth_assert(i < size());
		zth_assert(!hasGuard(i));
		return static_cast<char const*>(m_transitions[i].guard);
	}

	virtual GuardPollInterval enabled(index_type i, Fsm& fsm) const final;

	virtual Action const& action(index_type i) const noexcept final
	{
		zth_assert(i < size());
		return *m_transitions[i].action;
	}

	virtual index_type to(index_type i) const noexcept final
	{
		zth_assert(i < size());
		return static_cast<index_type>(m_transitions[i].to);
	}

	virtual State const& state(index_type i) const noexcept final
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
			throw invalid_fsm{"Target state not found"};

		// Unreachable.
		return Size + 1u;
	}

private:
	static_assert(std::numeric_limits<Index>::max() > Size, "");
	CompiledTransition m_transitions[Size + 1u];
};

/*!
 * \brief Compile a transition description.
 *
 * \ingroup zth_api_cpp_fsm14
 */
template <typename... T>
constexpr auto compile(T&&... t)
{
	return Transitions<sizeof...(T)>::compile({Transition{std::forward<T>(t)}...});
}

/*!
 * \brief FSM base class.
 *
 * If you want to hold some state in the FSM, inherit this class.  A reference
 * to your inherited class can be passed to the guard and action functions.
 *
 * \ingroup zth_api_cpp_fsm14
 */
class Fsm : public UniqueID<Fsm> {
	ZTH_CLASS_NEW_DELETE(Fsm)
public:
	using index_type = TransitionsBase::index_type;

	/*!
	 * \brief Flags that indicate properties of the current state.
	 * \see #flag()
	 */
	enum class Flag : size_t {
		entry,	    //!< \brief Just entered the current state.
		selfloop,   //!< \brief Took transition to the same state.
		blocked,    //!< \brief The FSM is blocked, as no guards are enabled.
		transition, //!< \brief A transition is taken.
		pushed,	    //!< \brief The current state was entered using push().
		popped,	    //!< \brief The current state was entered using pop().
		stop,	    //!< \brief stop() was requested. run() will terminate.
		input,	    //!< \brief The current state was entered via an input symbol.
		flags,	    //!< \brief Not a flag, just a count of the other flags.
	};

	/*!
	 * \brief Ctor.
	 */
	Fsm(cow_string const& name = "FSM")
		: UniqueID{name}
	{}

	/*!
	 * \brief Move ctor.
	 */
	Fsm(Fsm&& f)
		: Fsm{}
	{
		*this = std::move(f);
	}

	/*!
	 * \brief Move assignment.
	 */
	Fsm& operator=(Fsm&& f) noexcept
	{
		m_fsm = f.m_fsm;
		m_flags = f.m_flags;
		m_prev = f.m_prev;
		m_transition = f.m_transition;
		m_state = f.m_state;
		m_stack = std::move(f.m_stack);
		m_inputs = std::move(f.m_inputs);

		f.m_fsm = nullptr;
		f.m_state = 0;

		UniqueID::operator=(std::move(f));
		return *this;
	}

	/*!
	 * \brief Dtor.
	 */
	virtual ~Fsm() = default;

	/*!
	 * \brief Checks if the FSM was initialized.
	 *
	 * When valid, #run() can be called.
	 */
	bool valid() const noexcept
	{
		return m_fsm;
	}

	/*!
	 * \brief Reserve memory to #push() a amount of states.
	 * \exception std::bad_alloc when allocation fails
	 */
	void reserveStack(size_t capacity)
	{
		if(m_stack.capacity() > capacity * 2U)
			m_stack.shrink_to_fit();

		m_stack.reserve(capacity);
	}

	/*!
	 * \brief Reset the state machine.
	 */
	virtual void reset() noexcept
	{
		zth_dbg(fsm, "[%s] Reset", id_str());
		m_prev = m_transition = m_state = 0;
		m_flags.reset();
		m_stack.clear();
		m_t = Timestamp::now();
	}

	/*!
	 * \brief Return the current state.
	 */
	State const& state() const noexcept
	{
		zth_assert(valid());
		return m_fsm->state(m_state);
	}

	/*!
	 * \brief Return the previous state.
	 */
	State const& prev() const noexcept
	{
		zth_assert(valid());
		return m_fsm->state(m_prev);
	}

	/*!
	 * \brief Try to take one step in the state machine.
	 *
	 * This basically does:
	 *
	 * - Find the first enabled guard of the current state.
	 * - If there is none, return the shortest returned time interval until
	 *   the guard must be checked again.
	 * - Otherwise:
	 *   - If a target state is specified, call leave(), set the new state,
	 *     and call enter().
	 *   - If no target state is specified, call enter() for the same state.
	 */
	GuardPollInterval step()
	{
		zth_assert(valid());

		auto i = m_state;
		auto size = m_fsm->size();

		// Find next enabled guard.
		GuardPollInterval again{false};
		GuardPollInterval p;
		while(!(p = m_fsm->enabled(i, *this))) {
			// Save the shortest poll interval.
			if(p < again)
				again = p;

			if(++i == size || m_fsm->state(i).valid()) {
				// No enabled guards.
				setFlag(Flag::blocked);
				zth_dbg(fsm, "[%s] Blocked for %s", id_str(), again.str().c_str());
				return again;
			}
		}

		// Got it. Take transition.
		m_transition = i;
		auto to = m_fsm->to(i);

		clearFlag(Flag::blocked);

		if(!to) {
			// No transition, only do the action.
			if(m_fsm->hasInput(i))
				zth_dbg(fsm, "[%s] Have input %s, no transition", id_str(),
					m_fsm->input(i).str());
			else
				zth_dbg(fsm, "[%s] Guard %s enabled, no transition", id_str(),
					m_fsm->guard(i).name().c_str());
			clearFlag(Flag::transition);
			setFlag(Flag::input, m_fsm->hasInput(i));
			enter();
		} else {
			if(m_fsm->hasInput(i))
				zth_dbg(fsm, "[%s] Have input %s, transition %s -> %s", id_str(),
					m_fsm->input(i).str(), m_fsm->state(m_state).str(),
					m_fsm->state(to).str());
			else
				zth_dbg(fsm, "[%s] Guard %s enabled, transition %s -> %s", id_str(),
					m_fsm->guard(i).name().c_str(), m_fsm->state(m_state).str(),
					m_fsm->state(to).str());

			setFlag(Flag::selfloop, to == m_state);
			setFlag(Flag::transition);
			if(m_state)
				leave();

			m_prev = m_state;
			m_state = to;

			clearFlag(Flag::popped);
			clearFlag(Flag::pushed);
			setFlag(Flag::input, m_fsm->hasInput(i));
			enter();

			setFlag(Flag::entry);
		}

		return true;
	}

	/*!
	 * \brief Run the state machine until the given time stamp.
	 *
	 * When the time stamp is hit, or when stop() is called, the FSM stops
	 * execution.  The amount of time waiting for the next enabled guard is
	 * returned.
	 */
	GuardPollInterval run(Timestamp const& until)
	{
		zth_dbg(fsm, "[%s] Run for %s", id_str(), (until - Timestamp::now()).str().c_str());
		clearFlag(Flag::stop);

		while(true) {
			GuardPollInterval p = step();

			if(flag(Flag::stop))
				// Return now, but we could continue anyway.
				return p;

			auto now = Timestamp::now();
			if(now > until)
				return p;

			if(p)
				continue;

			auto p_end = now + p;
			m_trigger.wait(std::min(p_end, until), now);
		}
	}

	/*!
	 * \brief Run the state machine.
	 *
	 * When \p returnWhenBlocked is \c true, and there are no enabled
	 * guards, the function returns with the time until the next enabled
	 * guard.  Otherwise, the FSM keeps running until #stop() is called.
	 */
	GuardPollInterval run(bool returnWhenBlocked = false)
	{
		zth_dbg(fsm, "[%s] Run%s", id_str(), returnWhenBlocked ? " until blocked" : "");
		clearFlag(Flag::stop);

		while(true) {
			GuardPollInterval p = step();

			if(flag(Flag::stop))
				// Return now, but we could continue anyway.
				return p;

			if(p)
				continue;

			if(returnWhenBlocked)
				return p;

			m_trigger.wait(p);
		}
	}

	/*!
	 * \brief Trigger a currently running FSM to reevaluate the guards
	 *	immediately.
	 *
	 * This may be used when any of the guard function may have become
	 * enabled, by some event outside of the state machine.
	 */
	void trigger() noexcept
	{
		m_trigger.signal(false);
	}

	/*!
	 * \brief Interrupt a running FSM.
	 *
	 * This is the callback function for the #zth::fsm::stop action.  When
	 * the FSM is not running (by #run()), calling #stop() does nothing.
	 */
	void stop() noexcept
	{
		zth_dbg(fsm, "[%s] Stop requested", id_str());
		setFlag(Flag::stop);
	}

	/*!
	 * \brief Push the current state onto the state stack.
	 *
	 * This is the callback function for the #zth::fsm::push() action.
	 *
	 * The FSM must be #valid().
	 *
	 * \see #pop()
	 */
	void push()
	{
		zth_assert(valid());

		m_stack.push_back(m_prev);
		setFlag(Flag::pushed);
		zth_dbg(fsm, "[%s] Push %s", id_str(), m_fsm->state(m_state).str());
	}

	/*!
	 * \brief Pop the previous state from the state stack.
	 *
	 * #leave() and #enter() are called when appropriate.
	 *
	 * This is the callback function for the #zth::fsm::pop() action.
	 * #push() and pop() must come in pairs.
	 *
	 * The FSM must be #valid().
	 *
	 * \see #push()
	 */
	virtual void pop()
	{
		zth_assert(valid());
		zth_assert(!m_stack.empty());

		index_type to = m_stack.back();
		zth_assert(to != 0);

		zth_dbg(fsm, "[%s] Pop %s -> %s", id_str(), m_fsm->state(m_state).str(),
			m_fsm->state(to).str());

		clearFlag(Flag::blocked);
		setFlag(Flag::selfloop, m_state == to);

		if(!flag(Flag::transition)) {
			// leave() was not called by step().
			setFlag(Flag::transition);
			leave();
		}

		m_prev = m_state;
		m_state = to;
		m_stack.pop_back();
		m_transition = 0;

		clearFlag(Flag::pushed);
		setFlag(Flag::popped);
		clearFlag(Flag::input);
		enter();

		setFlag(Flag::entry);
	}

	/*!
	 * \brief Check if the current state was reached via #pop().
	 *
	 * This is the callback function for the #zth::fsm::popped() guard.
	 */
	bool popped() const noexcept
	{
		return flag(Flag::popped);
	}

	/*!
	 * \brief Check if the given flag is set.
	 */
	bool flag(Flag f) const noexcept
	{
		return m_flags.test(flagIndex(f));
	}

	/*!
	 * \brief Check if the state was just entered.
	 *
	 * This is the callback function for the #zth::fsm::entry() guard.
	 */
	bool entry() noexcept
	{
		if(!flag(Flag::entry))
			return false;

		clearFlag(Flag::entry);
		return true;
	}

	/*!
	 * \brief Return the input symbol that triggered the transition to the
	 *	current state.
	 *
	 * Returns the invalid Symbol when that was not the case.
	 */
	Symbol input() const noexcept
	{
		if(!flag(Flag::input))
			return Symbol{};

		zth_assert(valid());
		zth_assert(m_fsm->hasInput(m_transition));

		return m_fsm->input(m_transition);
	}

	/*!
	 * \brief %Register the given input symbol.
	 *
	 * #trigger() is called to trigger immediate reevaluation of the
	 * guards.
	 */
	void input(Symbol i)
	{
		if(!i.valid())
			return;

		m_inputs.emplace_back(std::move(i));
		trigger();
	}

	/*!
	 * \brief Remove the given input symbol.
	 * \return \c false if the given symbol was not registered
	 * \see #input(Symbol)
	 */
	bool clearInput(Symbol i) noexcept
	{
		if(!i.valid())
			return false;

		for(size_t j = 0; j < m_inputs.size(); j++)
			if(m_inputs[j] == i) {
				m_inputs[j] = std::move(m_inputs.back());
				m_inputs.pop_back();
				return true;
			}

		return false;
	}

	/*!
	 * \brief Clear the input symbol that triggered the current state.
	 *
	 * This is the callback function for the #zth::fsm::consume action.
	 *
	 * \see #input()
	 */
	void clearInput() noexcept
	{
		clearInput(input());
	}

	/*!
	 * \brief Clear all inputs.
	 */
	void clearInputs() noexcept
	{
		m_inputs.clear();
	}

	/*!
	 * \brief Reserve memory for the given amount of input symbols.
	 * \see #input(Symbol)
	 * \exception std::bad_alloc when allocation fails
	 */
	void reserveInputs(size_t capacity)
	{
		if(m_inputs.capacity() > capacity * 2U)
			m_inputs.shrink_to_fit();

		m_inputs.reserve(capacity);
	}

	/*!
	 * \brief Checks if the given input symbol was registered before.
	 * \see #input(Symbol)
	 */
	bool hasInput(Symbol i) const noexcept
	{
		if(!i.valid())
			return false;

		for(size_t j = 0; j < m_inputs.size(); j++)
			if(m_inputs[j] == i)
				return true;

		return false;
	}

	/*!
	 * \brief Returns the time stamp when the current state was entered.
	 *
	 * This is the time of the last transition. This includes self-loops,
	 * but not transitions without a target state.
	 */
	Timestamp const& t() const noexcept
	{
		return m_t;
	}

	/*!
	 * \brief Returns the time since the current state was entered.
	 * \see #t()
	 */
	TimeInterval dt() const
	{
		return Timestamp::now() - t();
	}

	/*!
	 * \brief Callback function for the #zth::fsm::timeout_s guard.
	 */
	template <time_t s>
	static GuardPollInterval timeoutGuard_s(Fsm& fsm)
	{
		return TimeInterval{s} - fsm.dt();
	}

	/*!
	 * \brief Callback function for the #zth::fsm::timeout_ms guard.
	 */
	template <uint64_t ms>
	static GuardPollInterval timeoutGuard_ms(Fsm& fsm)
	{
		return TimeInterval{
			       (time_t)(ms / 1'000ULL), (long)(ms * 1'000'000ULL) % 1'000'000'000L}
		       - fsm.dt();
	}

	/*!
	 * \brief Callback function for the #zth::fsm::timeout_us guard.
	 */
	template <uint64_t us>
	static GuardPollInterval timeoutGuard_us(Fsm& fsm)
	{
		return TimeInterval{
			       (time_t)(us / 1'000'000ULL), (long)(us * 1'000ULL) % 1'000'000'000L}
		       - fsm.dt();
	}

protected:
	/*!
	 * \brief Set or clear the given flag.
	 */
	bool setFlag(Flag f, bool value = true) noexcept
	{
		m_flags.set(flagIndex(f), value);
		return value;
	}

	/*!
	 * \brief Set the given flag.
	 *
	 * Equivalent to \c setFlag(f, false).
	 */
	void clearFlag(Flag f) noexcept
	{
		setFlag(f, false);
	}

	/*!
	 * \brief Called when the current state is about to be left.
	 */
	virtual void leave()
	{
		zth_dbg(fsm, "[%s] Leave %s", id_str(), state().str());
	}

	/*!
	 * \brief Called when the current state was just entered.
	 *
	 * This function calls the action.  When overriding this function in a
	 * subclass, usually call this function first before adding custom
	 * logic.
	 */
	virtual void enter()
	{
		if(flag(Flag::transition))
			m_t = Timestamp::now();

		if(m_transition) {
			zth_dbg(fsm, "[%s] Enter %s%s; run action %s", id_str(), state().str(),
				flag(Flag::selfloop) ? " (loop)" : "",
				m_fsm->action(m_transition).name().c_str());

			m_fsm->action(m_transition).run(*this);
		} else {
			zth_dbg(fsm, "[%s] Enter %s (dummy transition)", id_str(), state().str());
		}
	}

private:
	/*!
	 * \brief Return the bit index of the given flag in the set of flags.
	 */
	static constexpr size_t flagIndex(Flag f) noexcept
	{
		return static_cast<size_t>(f);
	}

	/*!
	 * \brief Initialize the FSM with the given transitions.
	 */
	void init(TransitionsBase const& c) noexcept
	{
		if(c.size() == 0)
			// Invalid.
			return;

		m_fsm = &c;
		reset();
	}

	friend TransitionsBase;

private:
	/*! \brief The transitions. */
	TransitionsBase const* m_fsm{};
	/*! \brief The flags. */
	std::bitset<static_cast<size_t>(Flag::flags)> m_flags;
	/*! \brief The previous state. */
	index_type m_prev{};
	/*! \brief The last taken transition. */
	index_type m_transition{};
	/*! \brief The current state. */
	index_type m_state{};
	/*! \brief The state push/pop stack. */
	vector_type<index_type>::type m_stack;
	/*! \brief The input symbol list. */
	vector_type<Symbol>::type m_inputs;
	/*! \brief The trigger for #run() and #trigger(). */
	Signal m_trigger;
	/*! \brief The time when entered the current state. */
	Timestamp m_t;
};

/*!
 * \brief Guard that is only enabled upon entry of a state.
 * \ingroup zth_api_cpp_fsm14
 * \hideinitializer
 */
inline17 constexpr auto entry = guard(&Fsm::entry, "entry");

/*!
 * \brief Action to push the new state onto the stack.
 * \see pop
 * \ingroup zth_api_cpp_fsm14
 * \hideinitializer
 */
inline17 constexpr auto push = action(&Fsm::push, "push");

/*!
 * \brief Action to pop the current state from the stack.
 * \ingroup zth_api_cpp_fsm14
 * \hideinitializer
 */
inline17 constexpr auto pop = action(&Fsm::pop, "pop");

/*!
 * \brief Guard to indicate that the current state was reached via \c pop.
 * \ingroup zth_api_cpp_fsm14
 * \hideinitializer
 */
inline17 constexpr auto popped = guard(&Fsm::popped, "popped");

/*!
 * \brief Action to return from Fsm::run().
 * \ingroup zth_api_cpp_fsm14
 * \hideinitializer
 */
inline17 constexpr auto stop = action(&Fsm::stop, "stop");

/*!
 * \brief Action consume the current input symbol.
 *
 * Usually, combined with the \c input guard. When a transition is taken
 * because of an input symbol, remove that symbol from the list of inputs
 * symbol.  If \c consume is not used, the symbol guard will be enabled again
 * the next evaluation of the guards.
 *
 * \ingroup zth_api_cpp_fsm14
 * \hideinitializer
 */
inline17 constexpr auto consume = action<void (Fsm::*)() noexcept>(&Fsm::clearInput, "consume");

/*!
 * \brief A guard that is enabled after a \p s seconds after entering the
 *	current state.
 * \ingroup zth_api_cpp_fsm14
 * \hideinitializer
 */
template <time_t s>
inline17 constexpr auto timeout_s = guard(&Fsm::timeoutGuard_s<s>, "timeout");

/*!
 * \brief A guard that is enabled after a \p ms milliseconds after entering the
 *	current state.
 * \ingroup zth_api_cpp_fsm14
 * \hideinitializer
 */
template <uint64_t ms>
inline17 constexpr auto timeout_ms = guard(&Fsm::timeoutGuard_ms<ms>, "timeout");

/*!
 * \brief A guard that is enabled after a \p us microseconds after entering the
 *	current state.
 * \ingroup zth_api_cpp_fsm14
 * \hideinitializer
 */
template <uint64_t us>
inline17 constexpr auto timeout_us = guard(&Fsm::timeoutGuard_us<us>, "timeout");

inline GuardPollInterval InputGuard::enabled(Fsm& fsm) const
{
	return fsm.hasInput(m_input);
}

inline GuardPollInterval GuardedAction::enabled(Fsm& fsm) const
{
	if(isInput())
		return fsm.hasInput(input());
	else
		return m_guard.enabled(fsm);
}

template <size_t Size>
GuardPollInterval Transitions<Size>::enabled(Transitions::index_type i, Fsm& fsm) const
{
	if(isInput(i))
		return fsm.hasInput(input(i));
	else
		return guard(i).enabled(fsm);
}

inline Fsm TransitionsBase::spawn() const
{
	Fsm fsm;
	fsm.init(*this);
	return fsm;
}

} // namespace fsm
} // namespace zth
#endif // C++14
#endif // ZTH_FSM14_H
