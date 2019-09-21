#ifndef __ZTH_UTIL_H
#define __ZTH_UTIL_H
/*
 * Zth (libzth), a cooperative userspace multitasking library.
 * Copyright (C) 2019  Jochem Rutgers
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
#    define likely(expr) __builtin_expect(!!(expr), 1)
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
#    define unlikely(expr) __builtin_expect(!!(expr), 0)
#  else
#    define unlikely(expr) (expr)
#  endif
#endif

#include <assert.h>
#if __cplusplus && __cplusplus < 201103L && !defined(static_assert)
#  define static_assert(expr, msg)	typedef int static_assert_[(expr) ? 1 : -1]
#endif

#ifndef INIT_CALL
/*!
 * \brief Mark the given function \c f to be invoked during static initialization.
 */
#  define INIT_CALL(f)		struct f##__init { f##__init() { f(); } }; static f##__init f##__init_;
#endif
#ifndef DEINIT_CALL
/*!
 * \brief Mark the given function \c f to be invoked at exit.
 */
#  define DEINIT_CALL(f)	struct f##__deinit { f##__deinit() { atexit(f); } }; static f##__deinit f##__deinit_;
#endif

#ifndef FOREACH
#  define FOREACH_0(WHAT)														//!< \brief Helper for #FOREACH. \private
#  define FOREACH_1(WHAT, X)			WHAT(X)									//!< \brief Helper for #FOREACH. \private
#  define FOREACH_2(WHAT, X, ...)		WHAT(X)FOREACH_1(WHAT, __VA_ARGS__)		//!< \brief Helper for #FOREACH. \private
#  define FOREACH_3(WHAT, X, ...)		WHAT(X)FOREACH_2(WHAT, __VA_ARGS__)		//!< \brief Helper for #FOREACH. \private
#  define FOREACH_4(WHAT, X, ...)		WHAT(X)FOREACH_3(WHAT, __VA_ARGS__)		//!< \brief Helper for #FOREACH. \private
#  define FOREACH_5(WHAT, X, ...)		WHAT(X)FOREACH_4(WHAT, __VA_ARGS__)		//!< \brief Helper for #FOREACH. \private
#  define FOREACH_6(WHAT, X, ...)		WHAT(X)FOREACH_5(WHAT, __VA_ARGS__)		//!< \brief Helper for #FOREACH. \private
#  define FOREACH_7(WHAT, X, ...)		WHAT(X)FOREACH_6(WHAT, __VA_ARGS__)		//!< \brief Helper for #FOREACH. \private
#  define FOREACH_8(WHAT, X, ...)		WHAT(X)FOREACH_7(WHAT, __VA_ARGS__)		//!< \brief Helper for #FOREACH. \private
#  define FOREACH_9(WHAT, X, ...)		WHAT(X)FOREACH_8(WHAT, __VA_ARGS__)		//!< \brief Helper for #FOREACH. \private
#  define FOREACH_10(WHAT, X, ...)		WHAT(X)FOREACH_9(WHAT, __VA_ARGS__)		//!< \brief Helper for #FOREACH. \private
//... repeat as needed

/*! \brief Helper for #FOREACH. \private */
#  define _GET_MACRO(_0,_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,NAME,...) NAME
/*!
 * \brief Evaluates to \c action(x) for every argument.
 */
#  define FOREACH(action,...) \
		_GET_MACRO(0,##__VA_ARGS__,FOREACH_10,FOREACH_9,FOREACH_8,FOREACH_7,FOREACH_6,FOREACH_5,FOREACH_4,FOREACH_3,FOREACH_2,FOREACH_1,FOREACH_0)(action,##__VA_ARGS__)
#endif

#include <stdarg.h>

#ifdef __cplusplus
#include <string>
#include <string.h>
#include <pthread.h>
#include <memory>
#include <inttypes.h>
#include <stdlib.h>

#ifdef ZTH_HAVE_PTHREAD
#  include <pthread.h>
#else
#  include <sys/types.h>
#  include <unistd.h>
#endif

EXTERN_C ZTH_EXPORT __attribute__((format(ZTH_ATTR_PRINTF, 1, 0), weak)) void zth_logv(char const* fmt, va_list arg);

#ifdef __cplusplus
/*!
 * \brief Prefix for every #zth_dbg() call.
 * \private
 */
#define ZTH_DBG_PREFIX " > "

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
#  define zth_dbg(group, fmt, a...) \
	do { \
		if(::zth::Config::EnableDebugPrint && ::zth::Config::Print_##group > 0 && zth_config(EnableDebugPrint)) { \
			if(::zth::Config::EnableColorLog) \
				::zth::log_color(::zth::Config::Print_##group, ZTH_DBG_PREFIX "zth::" ZTH_STRINGIFY(group) ": " fmt "\n", ##a); \
			else \
				::zth::log(ZTH_DBG_PREFIX "zth::" ZTH_STRINGIFY(group) ": " fmt "\n", ##a); \
		} \
	} while(0)

/*!
 * \def zth_assert(expr)
 * \brief \c assert(), but better integrated in Zth.
 */
#  ifndef NDEBUG
#    define zth_assert(expr) do { if(unlikely(::zth::Config::EnableAssert && !(expr))) \
		::zth::abort("assertion failed at " __FILE__ ":" ZTH_STRINGIFY(__LINE__) ": " ZTH_STRINGIFY(expr)); } while(false)
#  else
#    define zth_assert(...)
#  endif
#endif

#include <libzth/zmq.h>

namespace zth {
	ZTH_EXPORT char const* banner();
    ZTH_EXPORT __attribute__((format(ZTH_ATTR_PRINTF, 1, 2), noreturn)) void abort(char const* fmt, ...);
	ZTH_EXPORT __attribute__((format(ZTH_ATTR_PRINTF, 1, 0), noreturn)) void abortv(char const* fmt, va_list args);

	ZTH_EXPORT __attribute__((format(ZTH_ATTR_PRINTF, 2, 0))) void log_colorv(int color, char const* fmt, va_list args);

	/*!
	 * \brief Logs a given printf()-like formatted string using an ANSI color code.
	 * \details #zth_logv() is used for the actual logging.
	 * \ingroup zth_api_cpp_util
	 */
	ZTH_EXPORT __attribute__((format(ZTH_ATTR_PRINTF, 2, 3))) inline void log_color(int color, char const* fmt, ...) {
		va_list args; va_start(args, fmt); log_colorv(color, fmt, args); va_end(args); }

	/*!
	 * \brief Logs a given printf()-like formatted string.
	 * \details #zth_logv() is used for the actual logging.
	 * \see zth::log_colorv()
	 * \ingroup zth_api_cpp_util
	 */
	ZTH_EXPORT __attribute__((format(ZTH_ATTR_PRINTF, 1, 0))) inline void logv(char const* fmt, va_list arg) {
		::zth_logv(fmt, arg); }
	
	/*!
	 * \brief Logs a given printf()-like formatted string.
	 * \details #zth_logv() is used for the actual logging.
	 * \see zth::log_color()
	 * \ingroup zth_api_cpp_util
	 */
	ZTH_EXPORT __attribute__((format(ZTH_ATTR_PRINTF, 1, 2))) inline void log(char const* fmt, ...) {
		va_list args; va_start(args, fmt); logv(fmt, args); va_end(args); }

	/*!
	 * \brief Format like \c vsprintf(), but save the result in an \c std::string.
	 */
	__attribute__((format(ZTH_ATTR_PRINTF, 1, 0))) inline std::string formatv(char const* fmt, va_list args) {
		int const maxstack = (int)sizeof(void*) * 8;

		char sbuf[maxstack];
		char* hbuf = NULL;
		char* buf = sbuf;

		va_list args2;
		va_copy(args2, args);
		int c = vsnprintf(sbuf, maxstack, fmt, args);

		if(c >= maxstack) {
			hbuf = (char*)malloc(c + 1);
			if(unlikely(!hbuf))
				c = 0;
			else {
				int c2 = vsprintf(hbuf, fmt, args2);
				assert(c2 <= c);
				c = c2;
			}
		}

		va_end(args2);

		std::string res(buf, (size_t)(c < 0 ? 0 : c));
		if(hbuf)
			free(hbuf);
		return res;
	}

	/*!
	 * \brief Format like \c sprintf(), but save the result in an \c std::string.
	 */
	__attribute__((format(ZTH_ATTR_PRINTF, 1, 2))) inline std::string format(char const* fmt, ...) {
		va_list args;
		va_start(args, fmt);
		std::string res = formatv(fmt, args);
		va_end(args);
		return res;
	}

	/*!
	 * \brief Returns an \c std::string() representation of the given value.
	 * \details Specialize for your own types.
	 */
	template <typename T> inline std::string str(T value) { return value; }
	template <> inline std::string str<char>(char value) { return format("%c", value); }
	template <> inline std::string str<signed char>(signed char value) { return format("%hhd", value); }
	template <> inline std::string str<unsigned char>(unsigned char value) { return format("%hhu", value); }
	template <> inline std::string str<short>(short value) { return format("%hd", value); }
	template <> inline std::string str<unsigned short>(unsigned short value) { return format("%hu", value); }
	template <> inline std::string str<int>(int value) { return format("%d", value); }
	template <> inline std::string str<unsigned int>(unsigned int value) { return format("%u", value); }
	template <> inline std::string str<long>(long value) { return format("%ld", value); }
	template <> inline std::string str<unsigned long>(unsigned long value) { return format("%lu", value); }
	template <> inline std::string str<long long>(long long value) { return format("%lld", value); }
	template <> inline std::string str<unsigned long long>(unsigned long long value) { return format("%llu", value); }
	template <> inline std::string str<float>(float value) { return format("%g", value); }
	template <> inline std::string str<double>(double value) { return format("%g", value); }
	template <> inline std::string str<long double>(long double value) { return format("%Lg", value); }
#if __cplusplus >= 201103L
	template <> inline std::string str<std::string&&>(std::string&& value) { return std::string(std::move(value)); }
#endif
	
	/*!
	 * \brief Return a string like \c strerror() does, but as a \c std::string.
	 */
	inline std::string err(int e) {
#ifdef ZTH_HAVE_LIBZMQ
		return format("%s (error %d)", zmq_strerror(e), e);
#elif ZTH_THREADS && !defined(ZTH_OS_WINDOWS)
		char buf[128];
#  if !defined(ZTH_OS_LINUX) || (_POSIX_C_SOURCE >= 200112L) && !defined(_GNU_SOURCE)
		// XSI-compatible
		return format("%s (error %d)", strerror_r(e, buf, sizeof(buf)) ? "Unknown error" : buf, e);
#  else
		// GNU-specific
		return format("%s (error %d)", strerror_r(e, buf, sizeof(buf)), e);
#  endif
#else
		// Not thread-safe
		return format("%s (error %d)", strerror(e), e);
#endif
	}

	class UniqueIDBase {
	protected:
		virtual std::string const& id_sstr() const = 0;
		friend std::string str<UniqueIDBase const&>(UniqueIDBase const&);
	};
	
	template <> inline std::string str<UniqueIDBase const&>(UniqueIDBase const& value) { return value.id_sstr(); }

	/*!
	 * \brief Keeps track of a process-wide unique ID within the type \p T.
	 * \ingroup zth_api_cpp_util
	 */
	template <typename T, bool ThreadSafe = Config::EnableThreads>
	class UniqueID : public UniqueIDBase {
	public:
		static uint64_t getID() {
			return ThreadSafe ?
#if GCC_VERSION < 40802L
				__sync_add_and_fetch(&m_nextId, 1)
#else
				__atomic_add_fetch(&m_nextId, 1, __ATOMIC_RELAXED)
#endif
				: ++m_nextId; }
	
		UniqueID(std::string const& name) : m_id(getID()), m_name(name) {}
#if __cplusplus >= 201103L
		UniqueID(std::string&& name) : m_id(getID()), m_name(std::move(name)) {}
#endif
		UniqueID(char const* name = NULL) : m_id(getID()) { if(name) m_name = name; }
		virtual ~UniqueID() {}

		void const* normptr() const { return this; }

		__attribute__((pure)) uint64_t id() const { return m_id; }

		std::string const& name() const { return m_name; }

		void setName(std::string const& name) {
			setName(name.c_str());
		}
		
		void setName(char const* name) {
			m_name = name;
			m_id_sstr.clear();
			changedName(this->name());
		}

#if __cplusplus >= 201103L
		void setName(std::string&& name) {
			m_name = std::move(name);
			m_id_sstr.clear();
			changedName(this->name());
		}
#endif
	
		std::string const& id_sstr() const {
			if(unlikely(m_id_sstr.empty()))
				m_id_sstr = format("%s #%u:%" PRIu64, name().empty() ? "Object" : name().c_str(), 
#ifdef ZTH_OS_WINDOWS
					(unsigned int)_getpid(),
#else
					(unsigned int)getpid(),
#endif
					id());

			return m_id_sstr;
		}

		char const* id_str() const {
			return id_sstr().c_str();
		}

	protected:
		virtual void changedName(std::string const& name) {}

	private:
		UniqueID(UniqueID const&)
#if __cplusplus >= 201103L
			= delete
#endif
			;
		UniqueID& operator=(UniqueID const&)
#if __cplusplus >= 201103L
			= delete
#endif
			;

		uint64_t const m_id;
		std::string m_name;
		std::string mutable m_id_sstr;
		// If allocating once every ns, it takes more than 500 millenia until we run out of identifiers.
		static uint64_t m_nextId;
	};

	template <typename T, bool ThreadSafe> uint64_t UniqueID<T,ThreadSafe>::m_nextId = 0;
}

#endif // __cplusplus

/*!
 * \copydoc zth::banner()
 * \details This is a C-wrapper for zth::banner().
 * \ingroup zth_api_c_util
 */
#ifdef __cplusplus
EXTERN_C ZTH_EXPORT ZTH_INLINE void zth_banner() { zth::banner(); }
#else
ZTH_EXPORT void zth_banner();
#endif

/*!
 * \copydoc zth::abort()
 * \details This is a C-wrapper for zth::abort().
 * \ingroup zth_api_c_util
 */
ZTH_EXPORT __attribute__((format(ZTH_ATTR_PRINTF, 1, 2), noreturn)) void zth_abort(char const* fmt, ...);

/*!
 * \copydoc zth::log_color()
 * \details This is a C-wrapper for zth::log_color().
 * \ingroup zth_api_c_util
 */
#ifdef __cplusplus
EXTERN_C ZTH_EXPORT ZTH_INLINE __attribute__((format(ZTH_ATTR_PRINTF, 2, 3))) void zth_log_color(int color, char const* fmt, ...) {
	va_list args; va_start(args, fmt); zth::log_colorv(color, fmt, args); va_end(args); }
#else
ZTH_EXPORT __attribute__((format(ZTH_ATTR_PRINTF, 2, 3))) void zth_log_color(int color, char const* fmt, ...);
#endif

/*!
 * \copydoc zth::log()
 * \details This is a C-wrapper for zth::log().
 * \ingroup zth_api_c_util
 */
#ifdef __cplusplus
EXTERN_C ZTH_EXPORT ZTH_INLINE __attribute__((format(ZTH_ATTR_PRINTF, 1, 2))) void zth_log(char const* fmt, ...) {
	va_list args; va_start(args, fmt); zth::logv(fmt, args); va_end(args); }
#else
ZTH_EXPORT __attribute__((format(ZTH_ATTR_PRINTF, 1, 2))) void zth_log(char const* fmt, ...);
#endif

#endif // __ZTH_UTIL_H
