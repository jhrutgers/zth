#ifndef ZTH_UTIL_H
#define ZTH_UTIL_H
/*
 * SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
 *
 * SPDX-License-Identifier: MPL-2.0
 */

/*!
 * \defgroup zth_api_c_util util
 * \ingroup zth_api_c
 */
/*!
 * \defgroup zth_api_cpp_util util
 * \ingroup zth_api_cpp
 */

#include <libzth/macros.h>

#include <libzth/config.h>

/*!
 * \brief Helper for #ZTH_STRINGIFY()
 * \private
 */
#define ZTH_STRINGIFY_(x) #x
/*!
 * \brief Converts the argument to a string literal.
 */
#define ZTH_STRINGIFY(x) ZTH_STRINGIFY_(x)

/*!
 * \def likely(expr)
 * \brief Marks the given expression to likely be evaluated to true.
 * \details This may help compiler optimization.
 * \returns the evaluated \c expr
 */
#ifndef likely
#  ifdef __GNUC__
#    define likely(expr) \
	    __builtin_expect(!!(expr) /* NOLINT(readability-simplify-boolean-expr) */, 1)
#  else
#    define likely(expr) (expr)
#  endif
#endif

/*!
 * \def unlikely(expr)
 * \brief Marks the given expression to likely be evaluated to true.
 * \details This may help compiler optimization.
 * \returns the evaluated \c expr
 */
#ifndef unlikely
#  ifdef __GNUC__
#    define unlikely(expr) \
	    __builtin_expect(!!(expr) /* NOLINT(readability-simplify-boolean-expr) */, 0)
#  else
#    define unlikely(expr) (expr)
#  endif
#endif

#include <assert.h>
#if defined(__cplusplus) && __cplusplus < 201103L && !defined(static_assert)
#  pragma GCC diagnostic ignored "-Wunused-local-typedefs"
#  define static_assert(expr, msg) typedef int static_assert_[(expr) ? 1 : -1]
#endif

/*! \brief Helper for FOREACH and REVERSE. \private */
#define ZTH_GET_MACRO_ARGN(                                                                   \
	_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, NAME, ...) \
	NAME

#ifndef FOREACH
#  define FOREACH_0(WHAT)	     //!< \brief Helper for #FOREACH. \private
#  define FOREACH_1(WHAT, X) WHAT(X) //!< \brief Helper for #FOREACH. \private
#  define FOREACH_2(WHAT, X, ...) \
	  WHAT(X) FOREACH_1(WHAT, __VA_ARGS__) //!< \brief Helper for #FOREACH. \private
#  define FOREACH_3(WHAT, X, ...) \
	  WHAT(X) FOREACH_2(WHAT, __VA_ARGS__) //!< \brief Helper for #FOREACH. \private
#  define FOREACH_4(WHAT, X, ...) \
	  WHAT(X) FOREACH_3(WHAT, __VA_ARGS__) //!< \brief Helper for #FOREACH. \private
#  define FOREACH_5(WHAT, X, ...) \
	  WHAT(X) FOREACH_4(WHAT, __VA_ARGS__) //!< \brief Helper for #FOREACH. \private
#  define FOREACH_6(WHAT, X, ...) \
	  WHAT(X) FOREACH_5(WHAT, __VA_ARGS__) //!< \brief Helper for #FOREACH. \private
#  define FOREACH_7(WHAT, X, ...) \
	  WHAT(X) FOREACH_6(WHAT, __VA_ARGS__) //!< \brief Helper for #FOREACH. \private
#  define FOREACH_8(WHAT, X, ...) \
	  WHAT(X) FOREACH_7(WHAT, __VA_ARGS__) //!< \brief Helper for #FOREACH. \private
#  define FOREACH_9(WHAT, X, ...) \
	  WHAT(X) FOREACH_8(WHAT, __VA_ARGS__) //!< \brief Helper for #FOREACH. \private
#  define FOREACH_10(WHAT, X, ...) \
	  WHAT(X) FOREACH_9(WHAT, __VA_ARGS__) //!< \brief Helper for #FOREACH. \private
#  define FOREACH_11(WHAT, X, ...) \
	  WHAT(X) FOREACH_10(WHAT, __VA_ARGS__) //!< \brief Helper for #FOREACH. \private
#  define FOREACH_12(WHAT, X, ...) \
	  WHAT(X) FOREACH_11(WHAT, __VA_ARGS__) //!< \brief Helper for #FOREACH. \private
#  define FOREACH_13(WHAT, X, ...) \
	  WHAT(X) FOREACH_12(WHAT, __VA_ARGS__) //!< \brief Helper for #FOREACH. \private
#  define FOREACH_14(WHAT, X, ...) \
	  WHAT(X) FOREACH_13(WHAT, __VA_ARGS__) //!< \brief Helper for #FOREACH. \private
#  define FOREACH_15(WHAT, X, ...) \
	  WHAT(X) FOREACH_14(WHAT, __VA_ARGS__) //!< \brief Helper for #FOREACH. \private
#  define FOREACH_16(WHAT, X, ...) \
	  WHAT(X) FOREACH_15(WHAT, __VA_ARGS__) //!< \brief Helper for #FOREACH. \private
//... repeat as needed

/*!
 * \brief Evaluates to \c action(x) for every argument.
 */
#  define FOREACH(action, ...)                                                                   \
	  ZTH_GET_MACRO_ARGN(                                                                    \
		  0, ##__VA_ARGS__, FOREACH_16, FOREACH_15, FOREACH_14, FOREACH_13, FOREACH_12,  \
		  FOREACH_11, FOREACH_10, FOREACH_9, FOREACH_8, FOREACH_7, FOREACH_6, FOREACH_5, \
		  FOREACH_4, FOREACH_3, FOREACH_2, FOREACH_1, FOREACH_0)                         \
	  (action, ##__VA_ARGS__)
#endif

#ifndef REVERSE
#  define REVERSE_0()
#  define REVERSE_1(a)	     a
#  define REVERSE_2(a, ...)  REVERSE_1(__VA_ARGS__), a
#  define REVERSE_3(a, ...)  REVERSE_2(__VA_ARGS__), a
#  define REVERSE_4(a, ...)  REVERSE_3(__VA_ARGS__), a
#  define REVERSE_5(a, ...)  REVERSE_4(__VA_ARGS__), a
#  define REVERSE_6(a, ...)  REVERSE_5(__VA_ARGS__), a
#  define REVERSE_7(a, ...)  REVERSE_6(__VA_ARGS__), a
#  define REVERSE_8(a, ...)  REVERSE_7(__VA_ARGS__), a
#  define REVERSE_9(a, ...)  REVERSE_8(__VA_ARGS__), a
#  define REVERSE_10(a, ...) REVERSE_9(__VA_ARGS__), a
#  define REVERSE_11(a, ...) REVERSE_10(__VA_ARGS__), a
#  define REVERSE_12(a, ...) REVERSE_11(__VA_ARGS__), a
#  define REVERSE_13(a, ...) REVERSE_12(__VA_ARGS__), a
#  define REVERSE_14(a, ...) REVERSE_13(__VA_ARGS__), a
#  define REVERSE_15(a, ...) REVERSE_14(__VA_ARGS__), a
#  define REVERSE_16(a, ...) REVERSE_15(__VA_ARGS__), a
#  define REVERSE(...)                                                                           \
	  ZTH_GET_MACRO_ARGN(                                                                    \
		  0, ##__VA_ARGS__, REVERSE_16, REVERSE_15, REVERSE_14, REVERSE_13, REVERSE_12,  \
		  REVERSE_11, REVERSE_10, REVERSE_9, REVERSE_8, REVERSE_7, REVERSE_6, REVERSE_5, \
		  REVERSE_4, REVERSE_3, REVERSE_2, REVERSE_1, REVERSE_0)                         \
	  (__VA_ARGS__)
#endif

#include <stdarg.h>

#ifdef __cplusplus
#  include <cstdio>
#  include <cstdlib>
#  include <cstring>
#  include <limits>
#  include <memory>
#  include <string>
#  include <vector>

#  if __cplusplus >= 201103L
#    include <cinttypes>
#  else
#    include <inttypes.h>
#  endif

#  ifdef ZTH_HAVE_PTHREAD
#    include <pthread.h>
#  endif

#  ifdef ZTH_OS_WINDOWS
#    include <process.h>
#  else
#    include <sys/types.h>
#    include <unistd.h>
#  endif

EXTERN_C ZTH_EXPORT __attribute__((format(ZTH_ATTR_PRINTF, 1, 0))) void
zth_logv(char const* fmt, va_list arg);

#  ifdef __cplusplus
/*!
 * \brief Prefix for every #zth_dbg() call.
 * \private
 */
#    define ZTH_DBG_PREFIX " > "

/*!
 * \brief Debug printf()-like function.
 * \details Prints the given string for debugging purposes.
 *          The string is subject to the EnableDebugPrint configuration, and ANSI coloring.
 * \param group one of the %zth::Config::Print_x, where only \c x has to be specified
 * \param fmt \c printf()-like formatting string
 * \param a arguments for \p fmt
 * \ingroup zth_api_cpp_util
 * \hideinitializer
 */
#    define zth_dbg(group, fmt, a...)                                                            \
	    do { /* NOLINT(cppcoreguidelines-avoid-do-while) */                                  \
		    if(::zth::Config::SupportDebugPrint && ::zth::Config::Print_##group > 0      \
		       && zth_config(EnableDebugPrint)) {                                        \
			    if(::zth::Config::EnableColorLog)                                    \
				    ::zth::log_color(                                            \
					    ::zth::Config::Print_##group,                        \
					    ZTH_DBG_PREFIX "zth::" ZTH_STRINGIFY(group) ": " fmt \
											"\n",    \
					    ##a);                                                \
			    else                                                                 \
				    ::zth::log(                                                  \
					    ZTH_DBG_PREFIX "zth::" ZTH_STRINGIFY(group) ": " fmt \
											"\n",    \
					    ##a);                                                \
		    }                                                                            \
	    } while(0)

/*!
 * \def zth_assert(expr)
 * \brief \c assert(), but better integrated in Zth.
 */
#    ifndef NDEBUG
#      define zth_assert(expr)                                                              \
	      do { /* NOLINT(cppcoreguidelines-avoid-do-while) */                           \
		      if(unlikely(::zth::Config::EnableAssert && !(expr)))                  \
			      ::zth::assert_handler(                                        \
				      __FILE__, __LINE__,                                   \
				      ::zth::Config::EnableFullAssert ? ZTH_STRINGIFY(expr) \
								      : nullptr);           \
	      } while(false)
#    else
#      define zth_assert(...)                                     \
	      do { /* NOLINT(cppcoreguidelines-avoid-do-while) */ \
	      } while(0)
#    endif
#  endif

#  ifndef ZTH_CLASS_NOCOPY
#    if __cplusplus >= 201103L
#      define ZTH_CLASS_NOCOPY(Class)                                                      \
      public:                                                                              \
	      Class(Class const&) = delete;                                                \
	      Class(Class&&) =                                                             \
		      delete; /* NOLINT(misc-macro-parentheses,bugprone-macro-parentheses) \
			       */                                                          \
	      Class& operator=(Class const&) noexcept = delete;                            \
	      Class& operator=(Class&&) noexcept =                                         \
		      delete; /* NOLINT(misc-macro-parentheses,bugprone-macro-parentheses) \
			       */                                                          \
      private:
#    else
#      define ZTH_CLASS_NOCOPY(Class) \
      private:                        \
	      Class(Class const&);    \
	      Class& operator=(Class const&);
#    endif
#  endif

#  include <libzth/zmq.h>

namespace zth {

ZTH_EXPORT char const* banner() noexcept;

ZTH_EXPORT __attribute__((format(ZTH_ATTR_PRINTF, 1, 2), noreturn)) void
abort(char const* fmt, ...) noexcept;

ZTH_EXPORT __attribute__((format(ZTH_ATTR_PRINTF, 1, 0), noreturn)) void
abortv(char const* fmt, va_list args) noexcept;

ZTH_EXPORT __attribute__((noreturn)) void
assert_handler(char const* file, int line, char const* expr);

ZTH_EXPORT bool log_supports_ansi_colors() noexcept;

ZTH_EXPORT __attribute__((format(ZTH_ATTR_PRINTF, 2, 0))) void
log_colorv(int color, char const* fmt, va_list args);

/*!
 * \brief Logs a given printf()-like formatted string using an ANSI color code.
 * \details #zth_logv() is used for the actual logging.
 * \ingroup zth_api_cpp_util
 */
ZTH_EXPORT __attribute__((format(ZTH_ATTR_PRINTF, 2, 3))) inline void
log_color(int color, char const* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	log_colorv(color, fmt, args);
	va_end(args);
}

/*!
 * \brief Logs a given printf()-like formatted string.
 * \details #zth_logv() is used for the actual logging.
 * \see zth::log_colorv()
 * \ingroup zth_api_cpp_util
 */
ZTH_EXPORT __attribute__((format(ZTH_ATTR_PRINTF, 1, 0))) inline void
logv(char const* fmt, va_list arg)
{
	::zth_logv(fmt, arg);
}

/*!
 * \brief Logs a given printf()-like formatted string.
 * \details #zth_logv() is used for the actual logging.
 * \see zth::log_color()
 * \ingroup zth_api_cpp_util
 */
ZTH_EXPORT __attribute__((format(ZTH_ATTR_PRINTF, 1, 2))) inline void log(char const* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	logv(fmt, args);
	va_end(args);
}

/*!
 * \brief std::string type using Config::Allocator::type.
 * \ingroup zth_api_cpp_util
 */
typedef std::basic_string<char, std::char_traits<char>, Config::Allocator<char>::type> string;

/*!
 * \brief Copy-on-write string.
 *
 * It holds either a pointer to the a \c const \c char array, or a string
 * instance.  So, it is cheap for string literals, avoids duplicating these
 * into std::string, but still allows to do so when required.
 */
class cow_string {
public:
	cow_string()
		: m_cstr()
	{}

	// cppcheck-suppress noExplicitConstructor
	cow_string(char const* s)
		: m_cstr(s)
	{}

	// cppcheck-suppress noExplicitConstructor
	cow_string(string const& s)
		: m_cstr()
		, m_str(s)
	{}

	cow_string(cow_string const& s)
	{
		*this = s;
	}

	cow_string& operator=(cow_string const& s)
	{
		m_cstr = s.m_cstr;
		m_str = s.m_str;
		return *this;
	}

	cow_string& operator=(char const* s)
	{
		m_cstr = s;
		m_str.clear();
		return *this;
	}

	cow_string& operator=(string const& s)
	{
		m_cstr = nullptr;
		m_str = s;
		return *this;
	}

#  if __cplusplus >= 201103L
	cow_string(cow_string&& s) = default;
	cow_string& operator=(cow_string&& s) = default;

	// cppcheck-suppress noExplicitConstructor
	cow_string(string&& s)
		: m_cstr()
		, m_str(std::move(s))
	{}

	cow_string& operator=(string&& s)
	{
		m_cstr = nullptr;
		m_str = std::move(s);
		return *this;
	}

	string str() &&
	{
		// cppcheck-suppress returnStdMoveLocal
		return std::move(local());
	}
#  endif

	string const& str() const LREF_QUALIFIED
	{
		return local();
	}

	char const* c_str() const
	{
		return m_cstr ? m_cstr : m_str.c_str();
	}

	operator string const&() const
	{
		return str();
	}

	char const& at(size_t pos) const
	{
		return str().at(pos);
	}

	char operator[](size_t pos) const
	{
		return m_cstr ? m_cstr[pos] : str()[pos];
	}

	char& operator[](size_t pos)
	{
		return local()[pos];
	}

	char const* data() const
	{
		return c_str();
	}

	bool empty() const noexcept
	{
		return m_cstr ? *m_cstr == 0 : m_str.empty();
	}

	size_t size() const noexcept
	{
		return str().size();
	}

	size_t length() const noexcept
	{
		return str().length();
	}

	void clear() noexcept
	{
		m_cstr = nullptr;
		m_str.clear();
	}

	bool isConst() const noexcept
	{
		return m_cstr != nullptr;
	}

	bool isLocal() const noexcept
	{
		return m_cstr == nullptr;
	}

protected:
	string const& local() const
	{
		if(unlikely(m_cstr)) {
			m_str = m_cstr;
			m_cstr = nullptr;
		}
		return m_str;
	}

	string& local()
	{
		const_cast<cow_string const*>(this)->local();
		return m_str;
	}

private:
	mutable char const* m_cstr;
	mutable string m_str;
};

__attribute__((format(ZTH_ATTR_PRINTF, 1, 0))) string formatv(char const* fmt, va_list args);

/*!
 * \brief Format like \c sprintf(), but save the result in an \c zth::string.
 */
__attribute__((format(ZTH_ATTR_PRINTF, 1, 2))) inline string format(char const* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	string res = formatv(fmt, args);
	va_end(args);
	return res;
}

/*!
 * \brief Returns an \c zth::string representation of the given value.
 * \details Specialize for your own types.
 */
template <typename T>
inline cow_string str(T value)
{
	return cow_string(value);
}

template <>
inline cow_string str<char>(char value)
{
	return format("%c", value);
}

template <>
inline cow_string str<signed char>(signed char value)
{
	if(Config::UseLimitedFormatSpecifiers)
		return format("%d", (int)value);
	else
		return format("%hhd", value);
}

template <>
inline cow_string str<unsigned char>(unsigned char value)
{
	if(Config::UseLimitedFormatSpecifiers)
		return format("%u", (unsigned int)value);
	else
		return format("%hhu", value);
}

template <>
inline cow_string str<short>(short value)
{
	if(Config::UseLimitedFormatSpecifiers)
		return format("%d", (int)value);
	else
		return format("%hd", value);
}

template <>
inline cow_string str<unsigned short>(unsigned short value)
{
	if(Config::UseLimitedFormatSpecifiers)
		return format("%u", (unsigned int)value);
	else
		return format("%hu", value);
}

template <>
inline cow_string str<int>(int value)
{
	return format("%d", value);
}

template <>
inline cow_string str<unsigned int>(unsigned int value)
{
	return format("%u", value);
}

namespace impl {
ZTH_EXPORT ZTH_INLINE cow_string strull(unsigned long long value)
{
	if(Config::UseLimitedFormatSpecifiers) {
		if(value > std::numeric_limits<int>::max())
			return format(
				"0x%x%08x", (unsigned)(value >> 32U),
				(unsigned)(value & 0xFFFFFFFFU));
		else
			return format("%d", (int)value);
	} else {
		return format("%llu", value);
	}
}
} // namespace impl

template <>
inline cow_string str<long>(long value)
{
	if(Config::UseLimitedFormatSpecifiers) {
		if(sizeof(long) == sizeof(int)
		   || (value <= std::numeric_limits<int>::max()
		       && value >= std::numeric_limits<int>::min()))
			return format("%d", (int)value);
		else
			return impl::strull((unsigned long long)value);
	} else {
		return format("%ld", value);
	}
}

template <>
inline cow_string str<unsigned long>(unsigned long value)
{
	if(Config::UseLimitedFormatSpecifiers) {
		if(sizeof(unsigned long) == sizeof(unsigned int)
		   || value <= std::numeric_limits<unsigned int>::max())
			return format("%u", (unsigned int)value);
		else
			return impl::strull((unsigned long long)value);
	} else {
		return format("%lu", value);
	}
}

template <>
inline cow_string str<long long>(long long value)
{
	if(Config::UseLimitedFormatSpecifiers) {
		if(value > std::numeric_limits<int>::max()
		   || value < std::numeric_limits<int>::min())
			return impl::strull((unsigned long long)value);
		else
			return format("%d", (int)value);
	} else {
		return format("%lld", value);
	}
}

template <>
inline cow_string str<unsigned long long>(unsigned long long value)
{
	return impl::strull(value);
}

namespace impl {
ZTH_EXPORT ZTH_INLINE cow_string strf(float value)
{
	if(Config::UseLimitedFormatSpecifiers) {
		if(!(value < (float)std::numeric_limits<int>::max()
		     && value > (float)-std::numeric_limits<int>::max()))
			return "?range?";

		int i = (int)value;
		float fi = (float)i;
		if(std::abs(fi - value) < 0.0005F)
			return format("%d", i);

		int f = std::min(std::abs((int)((value - fi) * 1000.0F + 0.5F)), 999);
		return format("%d.%03d", i, f);
	} else {
		return format("%g", (double)value);
	}
}
} // namespace impl

template <>
inline cow_string str<float>(float value)
{
	if(Config::UseLimitedFormatSpecifiers)
		return impl::strf(value);
	else
		return format("%g", (double)value);
}

template <>
inline cow_string str<double>(double value)
{
	if(Config::UseLimitedFormatSpecifiers)
		return impl::strf((float)value);
	else
		return format("%g", value);
}

template <>
inline cow_string str<long double>(long double value)
{
	if(Config::UseLimitedFormatSpecifiers)
		return impl::strf((float)value);
	else
		return format("%Lg", value);
}

#  if __cplusplus >= 201103L
template <>
inline cow_string str<string&&>(string&& value)
{
	return cow_string(std::move(value));
}
#  endif

/*!
 * \brief Return a string like \c strerror() does, but as a \c zth::string.
 */
inline string err(int e)
{
#  ifdef ZTH_OS_BAREMETAL
	// You are probably low on memory. Don't include all strerror strings in the binary.
	return format("error %d", e);
#  elif defined(ZTH_HAVE_LIBZMQ)
	return format("%s (error %d)", zmq_strerror(e), e);
#  elif ZTH_THREADS && !defined(ZTH_OS_WINDOWS)
	char buf[128];
#    if !defined(ZTH_OS_LINUX) || (_POSIX_C_SOURCE >= 200112L) && !defined(_GNU_SOURCE)
	// XSI-compatible
	return format("%s (error %d)", strerror_r(e, buf, sizeof(buf)) ? "Unknown error" : buf, e);
#    else
	// GNU-specific
	return format("%s (error %d)", strerror_r(e, buf, sizeof(buf)), e);
#    endif
#  else
	// Not thread-safe
	return format("%s (error %d)", strerror(e), e);
#  endif
}

class UniqueIDBase {
protected:
	virtual char const* id_str() const = 0;
	virtual ~UniqueIDBase() is_default
	friend cow_string str<UniqueIDBase const&>(UniqueIDBase const&);
};

template <>
inline cow_string str<UniqueIDBase const&>(UniqueIDBase const& value)
{
	return cow_string(value.id_str());
}

/*!
 * \brief Keeps track of a process-wide unique ID within the type \p T.
 * \ingroup zth_api_cpp_util
 */
template <typename T, bool ThreadSafe = Config::EnableThreads>
class UniqueID : public UniqueIDBase {
#  if __cplusplus < 201103L
	ZTH_CLASS_NOCOPY(UniqueID)
#  else
public:
	UniqueID(UniqueID const&) = delete;
	UniqueID& operator=(UniqueID const&) = delete;

	UniqueID(UniqueID&& u) noexcept
		: m_id{}
	{
		*this = std::move(u);
	}

	UniqueID& operator=(UniqueID&& u) noexcept
	{
		m_id = u.m_id;
		m_name = std::move(u.m_name);
		m_id_str = std::move(u.m_id_str);

		u.m_id = 0;

		return *this;
	}
#  endif
public:
	static uint64_t getID() noexcept
	{
		return ThreadSafe ?
#  if GCC_VERSION < 40802L
				  __sync_add_and_fetch(&m_nextId, 1)
#  else
				  __atomic_add_fetch(&m_nextId, 1, __ATOMIC_RELAXED)
#  endif
				  : ++m_nextId;
	}

	explicit UniqueID(string const& name)
		: m_id(getID())
		, m_name(name)
	{}

#  if __cplusplus >= 201103L
	explicit UniqueID(string&& name)
		: m_id(getID())
		, m_name(std::move(name))
	{}
#  endif

	explicit UniqueID(char const* name = nullptr)
		: m_id(getID())
	{
		if(name)
			m_name = name;
	}

	virtual ~UniqueID() is_default

	void const* normptr() const noexcept
	{
		return this;
	}

	__attribute__((pure)) uint64_t id() const noexcept
	{
		return m_id;
	}

	string const& name() const noexcept
	{
		return m_name;
	}

	void setName(string const& name)
	{
		setName(name.c_str());
	}

	void setName(char const* name)
	{
		m_name = name;
		m_id_str.clear();
		changedName(this->name());
	}

#  if __cplusplus >= 201103L
	void setName(string&& name)
	{
		m_name = std::move(name);
		m_id_str.clear();
		changedName(this->name());
	}
#  endif

	virtual char const* id_str() const override
	{
		if(unlikely(m_id_str.empty())) {
			m_id_str =
#  ifdef ZTH_OS_BAREMETAL
				// No OS, no pid. And if newlib is used, don't try to format 64-bit
				// ints.
				format("%s #%u", name().empty() ? "Object" : name().c_str(),
				       (unsigned int)id());
#  else
				format("%s #%u:%" PRIu64,
				       name().empty() ? "Object" : name().c_str(),
#    ifdef ZTH_OS_WINDOWS
				       (unsigned int)_getpid(),
#    else
				       (unsigned int)getpid(),
#    endif
				       id());
#  endif
			if(unlikely(m_id_str.empty()))
				// Should not happen, but make sure the string is not empty.
				m_id_str = "?";
		}

		return m_id_str.c_str();
	}

private:
	virtual void changedName(string const& UNUSED_PAR(name)) {}

private:
	uint64_t m_id;
	string m_name;
	string mutable m_id_str;
	// If allocating once every ns, it takes more than 500 years until we run out of
	// identifiers.
	static uint64_t m_nextId;
};

template <typename T, bool ThreadSafe>
uint64_t UniqueID<T, ThreadSafe>::m_nextId = 0;

template <typename T, typename WhenTIsVoid>
struct choose_type {
	typedef T type;
};

template <typename WhenTIsVoid>
struct choose_type<void, WhenTIsVoid> {
	typedef WhenTIsVoid type;
};

#  if __cplusplus >= 201103L
template <size_t...>
struct Sequence {};
#    ifndef DOXYGEN
template <size_t N, size_t... S>
struct SequenceGenerator : SequenceGenerator<N - 1, N - 1, S...> {};

template <size_t... S>
struct SequenceGenerator<0, S...> {
	typedef Sequence<S...> type;
};
#    endif
#  endif
/*!
 * \brief Wrapper for a pointer, which checks validity of the pointer upon dereference.
 */
#  ifdef _DEBUG
template <typename T>
class safe_ptr {
public:
	typedef safe_ptr type;
	typedef T pointer_type;

	// cppcheck-suppress noExplicitConstructor
	constexpr safe_ptr(pointer_type* p) noexcept
		: m_p(p)
	{}

	constexpr operator pointer_type*() const noexcept
	{
		return ptr();
	}

	constexpr operator bool() const noexcept
	{
		return ptr();
	}

	constexpr14 pointer_type* operator->() const noexcept
	{
		zth_assert(ptr());
		return ptr();
	}

	constexpr14 pointer_type& operator*() const noexcept
	{
		zth_assert(ptr());
		// cppcheck-suppress nullPointerRedundantCheck
		return *ptr();
	}

protected:
	constexpr pointer_type* ptr() const noexcept
	{
		return m_p;
	}

private:
	pointer_type* m_p;
};
#  else	 // _DEBUG

// No checking, use use the pointer.
template <typename T>
class safe_ptr {
public:
	typedef T* type;
};
#  endif // !_DEBUG

/*!
 * \brief Singleton pattern.
 *
 * When a class inherits this class, the single instance of that class is available
 * via the static #instance() function.
 *
 * A class should use #Singleton as follows:
 *
 * \code
 * class Subclass : public Singleton<Subclass>
 * {
 * public:
 *   Subclass() {} // no need to invoke Singleton()
 *   // ...normal class implementation
 * };
 * \endcode
 *
 * Make sure that the template parameter \p T is the same as the subclass.
 *
 * \tparam T the type of the subclass that inherits this #Singleton.
 * \ingroup zth_api_cpp_util
 */
template <typename T>
class Singleton {
public:
	/*! \brief Alias of the \p T template parameter. */
	typedef T singleton_type;

protected:
	/*!
	 * \brief Constructor.
	 * \details (Only) the first instance of the \p T is recorded in \c m_instance.
	 */
	constexpr14 Singleton() noexcept
	{
		// Do not enforce construction of only one Singleton, only register the first one
		// as 'the' instance.

		if(!m_instance)
			m_instance = static_cast<singleton_type*>(this);
	}

	/*!
	 * \brief Destructor.
	 * \details After destruction, #instance() will return \c nullptr.
	 */
	~Singleton()
	{
		if(m_instance == static_cast<singleton_type*>(this))
			m_instance = nullptr;
	}

public:
	/*!
	 * \brief Return the only instance of \p T.
	 * \return the instance, or \c nullptr when not constructed yet/anymore.
	 */
	__attribute__((pure)) constexpr14 static typename safe_ptr<singleton_type>::type
	instance() noexcept
	{
		return m_instance;
	}

private:
	/*! \brief The only instance of \p T. */
	static singleton_type* m_instance;
};

template <typename T>
typename Singleton<T>::singleton_type* Singleton<T>::m_instance = nullptr;

/*!
 * \brief %Singleton pattern, but only per-thread.
 *
 * This is the same as the #zth::Singleton class, except that the instance is saved in thread-local
 * storage.
 *
 * \tparam T the type of the subclass that inherits this #ThreadLocalSingleton.
 * \ingroup zth_api_cpp_util
 */
template <typename T>
class ThreadLocalSingleton {
public:
	/*! \brief Alias of the \p T template parameter. */
	typedef T singleton_type;

protected:
	/*!
	 * \brief Constructor.
	 * \details (Only) the first instance of the \p T within the current thread is recorded in
	 * \c m_instance.
	 */
	__attribute__((no_sanitize_undefined)) ThreadLocalSingleton()
	{
		// Do not enforce construction of only one Singleton, only register the first one
		// as 'the' instance.

		if(!ZTH_TLS_GET(m_instance))
			ZTH_TLS_SET(m_instance, static_cast<singleton_type*>(this));
	}

	/*!
	 * \brief Destructor.
	 * \details After destruction, #instance() will return \c nullptr.
	 */
	__attribute__((no_sanitize_undefined)) ~ThreadLocalSingleton()
	{
		if(ZTH_TLS_GET(m_instance) == static_cast<singleton_type*>(this))
			ZTH_TLS_SET(m_instance, nullptr);
	}

public:
	/*!
	 * \brief Return the only instance of \p T within this thread.
	 * \return the instance, or \c nullptr when not constructed yet/anymore.
	 */
	__attribute__((pure)) static typename safe_ptr<singleton_type>::type instance() noexcept
	{
		return ZTH_TLS_GET(m_instance);
	}

private:
	/*! \brief The only instance of \p T. */
	ZTH_TLS_MEMBER(singleton_type*, m_instance)
};

template <typename T>
ZTH_TLS_DEFINE(
	typename ThreadLocalSingleton<T>::singleton_type*, ThreadLocalSingleton<T>::m_instance,
	nullptr)

/*!
 * \brief A simple std::vector, which can contain \p Prealloc without heap allocation.
 *
 * When the internal buffer is exhausted, the vector grows automatically
 * into an std::vector, using heap memory after all.
 *
 * In all cases, the elements are stored in contiguous memory, like
 * guaranteed by std::vector.  Upon a \c push_back(), underlying memory may
 * be reallocated to accommodate the new element, which renders previous
 * pointers invalid.
 *
 * \tparam T the type of elements to contain
 * \tparam Prealloc the minimum number of elements to contain without using heap
 * \tparam Allocator the allocator to use for std::vector
 * \ingroup zth_api_cpp_util
 */
template <typename T, int8_t Prealloc = 1, typename Allocator = typename Config::Allocator<T>::type>
class small_vector {
public:
	typedef T value_type;
	typedef Allocator allocator_type;
	typedef std::vector<value_type, allocator_type> vector_type;

	enum {
		prealloc_request = Prealloc > 0 ? Prealloc : 0,
		prealloc_request_size = prealloc_request * sizeof(value_type),
		vector_size = sizeof(vector_type),
		buffer_size =
			prealloc_request_size > vector_size ? prealloc_request_size : vector_size,
		prealloc = buffer_size / sizeof(value_type),
	};

	/*!
	 * \brief Default ctor.
	 */
	// cppcheck-suppress uninitMemberVar
	constexpr small_vector() noexcept
		: m_size()
	{
		static_assert(prealloc < std::numeric_limits<uint8_t>::max(), "");
	}

	/*!
	 * \brief Dtor.
	 */
	~small_vector()
	{
		if(is_small())
			resize(0);
		else
			vector().~vector_type();
	}

	/*!
	 * \brief Access an element.
	 */
	value_type& operator[](size_t index) noexcept
	{
		zth_assert(index < size());
		return data()[index];
	}

	/*!
	 * \brief Access an element.
	 */
	value_type const& operator[](size_t index) const noexcept
	{
		zth_assert(index < size());
		return data()[index];
	}

	/*!
	 * \brief Access the first element.
	 *
	 * Do not call when the vector is #empty().
	 */
	value_type& front() noexcept
	{
		return (*this)[0];
	}

	/*!
	 * \brief Access the first element.
	 *
	 * Do not call when the vector is #empty().
	 */
	value_type const& front() const noexcept
	{
		return (*this)[0];
	}

	/*!
	 * \brief Access the last element.
	 *
	 * Do not call when the vector is #empty().
	 */
	value_type& back() noexcept
	{
		zth_assert(!empty());
		return (*this)[size() - 1U];
	}

	/*!
	 * \brief Access the last element.
	 *
	 * Do not call when the vector is #empty().
	 */
	value_type const& back() const noexcept
	{
		zth_assert(!empty());
		return (*this)[size() - 1U];
	}

	/*!
	 * \brief Access the data array.
	 */
	value_type* data() noexcept
	{
		return is_small() ? array() : vector().data();
	}

	/*!
	 * \brief Access the data array.
	 */
	value_type const* data() const noexcept
	{
		return is_small() ? array() : vector().data();
	}

	/*!
	 * \brief Check if the vector is empty.
	 */
	bool empty() const noexcept
	{
		return is_small() ? m_size == 0 : vector().empty();
	}

	/*!
	 * \brief Return the number of elements stored in the vector.
	 */
	size_t size() const noexcept
	{
		return is_small() ? m_size : vector().size();
	}

	/*!
	 * \brief Reserve memory to accommodate at least the given number of elements.
	 * \exception std::bad_alloc when allocation fails
	 * \see #capacity()
	 */
	void reserve(size_t new_cap)
	{
		if(is_small() && new_cap <= prealloc)
			return;

		if(is_small())
			make_vector(new_cap);

		vector().reserve(new_cap);
	}

	/*!
	 * \brief Return the number of elements for which memory is currently reesrved.
	 * \see #reserve()
	 */
	size_t capacity() const noexcept
	{
		return is_small() ? (size_t)prealloc : vector().capacity();
	}

	/*!
	 * \brief Erase all elements from the vector.
	 *
	 * This does not release the memory.
	 */
	void clear() noexcept
	{
		if(is_small())
			resize(0);
		else
			vector().clear();
	}

	/*!
	 * \brief Erase all elements from the vector and release all heap memory.
	 */
	void clear_and_release() noexcept
	{
		if(is_small()) {
			resize(0);
		} else {
			vector_type v;
			v.swap(vector());
		}
	}

	/*!
	 * \brief Append an element to the vector using the copy constructor.
	 * \exception std::bad_alloc when allocation fails
	 */
	void push_back(value_type const& v)
	{
		reserve(size() + 1U);

		if(m_size < prealloc) {
			new(&array()[m_size]) value_type(v);
			m_size++;
		} else
			vector().push_back(v);
	}

#  if __cplusplus >= 201103L
	/*!
	 * \brief Append an element to the vector by construct in-place.
	 * \exception std::bad_alloc when allocation fails
	 */
	template <class... Args>
	void emplace_back(Args&&... args)
	{
		reserve(size() + 1U);

		if(m_size < prealloc) {
			new(&array()[m_size]) value_type(std::forward<Args>(args)...);
			m_size++;
		} else
			vector().emplace_back(std::forward<Args>(args)...);
	}
#  endif

	/*!
	 * \brief Remove the last element.
	 *
	 * Make sure the vector is not #empty() before calling.
	 */
	void pop_back() noexcept
	{
		zth_assert(!empty());
		if(is_small())
			array()[--m_size].~value_type();
		else
			vector().pop_back();
	}

	/*!
	 * \brief Resize the number of elements in the vector.
	 * \param count the requested number of elements
	 * \param x the value to save for newly constructed elements
	 */
	void resize(size_t count, value_type const& x = value_type())
	{
		if(is_small()) {
			while(count < m_size)
				array()[--m_size].~value_type();

			if(count <= prealloc) {
				while(count > m_size) {
					new(&array()[m_size]) value_type(x);
					m_size++;
				}
			} else {
				make_vector(count);
				vector().resize(count, x);
			}
		} else
			vector().resize(count, x);
	}

protected:
	/*!
	 * \brief Check if the current vector still fits in the internal buffer.
	 */
	bool is_small() const noexcept
	{
		return m_size <= prealloc;
	}

	/*!
	 * \brief Interpret buffer as the small vector.
	 */
	value_type* array() noexcept
	{
		zth_assert(is_small());
		return reinterpret_cast<value_type*>(m_buffer);
	}

	/*!
	 * \brief Interpret buffer as the small vector.
	 */
	value_type const* array() const noexcept
	{
		zth_assert(is_small());
		return reinterpret_cast<value_type const*>(m_buffer);
	}

	/*!
	 * \brief Interpret buffer as an std::vector.
	 */
	vector_type& vector() noexcept
	{
		zth_assert(!is_small());
		void* buffer = m_buffer;
		return *reinterpret_cast<vector_type*>(buffer);
	}

	/*!
	 * \brief Interpret buffer as an std::vector.
	 */
	vector_type const& vector() const noexcept
	{
		zth_assert(!is_small());
		void const* buffer = m_buffer;
		return *reinterpret_cast<vector_type const*>(buffer);
	}

	/*!
	 * \brief Make sure the std::vector is used as storage.
	 */
	void make_vector(size_t new_cap)
	{
		if(!is_small())
			return;

		vector_type v;
		v.reserve(std::max(new_cap, (size_t)m_size));

		for(size_t i = 0; i < m_size; i++) {
#  if __cplusplus >= 201103L
			v.emplace_back(std::move(array()[i]));
#  else
			v.push_back(array()[i]);
#  endif
			array()[i].~value_type();
		}

		m_size = prealloc + 1U;
		new(m_buffer) vector_type;
		vector().swap(v);
	}

private:
	/*!
	 * \brief The buffer that holds either an array of \c value_types, or an std::vector
	 * instance.
	 */
	alignas(vector_type) alignas(value_type) char m_buffer[buffer_size];

	/*!
	 * \brief The number of elements in the array.
	 *
	 * If the value is larger than \c prealloc, #m_buffer contains an
	 * std::vector instance, otherwise #m_buffer is an array of \c prealloc
	 * elements.
	 */
	uint8_t m_size;
};

template <size_t size>
struct smallest_uint_size {};

template <
	uint64_t x, typename size = smallest_uint_size<
			    x >= 0x100000000ULL ? 8U
			    : x >= 0x10000U	? 4U
			    : x >= 0x100U	? 2U
						: 1U> /**/>
struct smallest_uint {
	typedef uint64_t type;
};

template <uint64_t x>
struct smallest_uint<x, smallest_uint_size<1> /**/> {
	typedef uint8_t type;
};

template <uint64_t x>
struct smallest_uint<x, smallest_uint_size<2> /**/> {
	typedef uint16_t type;
};

template <uint64_t x>
struct smallest_uint<x, smallest_uint_size<4> /**/> {
	typedef uint32_t type;
};

} // namespace zth

#endif // __cplusplus

/*!
 * \copydoc zth::banner()
 * \details This is a C-wrapper for zth::banner().
 * \ingroup zth_api_c_util
 */
#ifdef __cplusplus
EXTERN_C ZTH_EXPORT ZTH_INLINE void zth_banner()
{
	zth::banner();
}
#else
ZTH_EXPORT void zth_banner();
#endif

/*!
 * \copydoc zth::abort()
 * \details This is a C-wrapper for zth::abort().
 * \ingroup zth_api_c_util
 */
ZTH_EXPORT __attribute__((format(ZTH_ATTR_PRINTF, 1, 2), noreturn)) void
zth_abort(char const* fmt, ...);

/*!
 * \copydoc zth::log_color()
 * \details This is a C-wrapper for zth::log_color().
 * \ingroup zth_api_c_util
 */
#ifdef __cplusplus
EXTERN_C ZTH_EXPORT ZTH_INLINE __attribute__((format(ZTH_ATTR_PRINTF, 2, 3))) void
zth_log_color(int color, char const* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	zth::log_colorv(color, fmt, args);
	va_end(args);
}
#else
ZTH_EXPORT __attribute__((format(ZTH_ATTR_PRINTF, 2, 3))) void
zth_log_color(int color, char const* fmt, ...);
#endif

/*!
 * \copydoc zth::log()
 * \details This is a C-wrapper for zth::log().
 * \ingroup zth_api_c_util
 */
#ifdef __cplusplus
EXTERN_C ZTH_EXPORT ZTH_INLINE __attribute__((format(ZTH_ATTR_PRINTF, 1, 2))) void
zth_log(char const* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	zth::logv(fmt, args);
	va_end(args);
}
#else
ZTH_EXPORT __attribute__((format(ZTH_ATTR_PRINTF, 1, 2))) void zth_log(char const* fmt, ...);
#endif

#ifdef ZTH_OS_BAREMETAL
// newlib probably doesn't have these. Provide some default implementation for
// them.
EXTERN_C __attribute__((format(gnu_printf, 2, 0))) int asprintf(char** strp, const char* fmt, ...);
EXTERN_C __attribute__((format(gnu_printf, 2, 0))) int
vasprintf(char** strp, const char* fmt, va_list ap);
#endif

#endif // ZTH_UTIL_H
