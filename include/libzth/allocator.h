#ifndef ZTH_ALLOCATOR_H
#define ZTH_ALLOCATOR_H
/*
 * SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#include <libzth/macros.h>

#ifdef __cplusplus

#  include <libzth/config.h>
#  include <libzth/util.h>

#  include <exception>
#  include <list>
#  include <map>
#  include <string>
#  include <vector>

namespace zth {

/*!
 * \brief Wrapper for Config::Allocator::type::allocate().
 * \ingroup zth_api_cpp_util
 */
template <typename T>
__attribute__((warn_unused_result, returns_nonnull)) static inline T* allocate(size_t n = 1)
{
	typename Config::Allocator<T>::type allocator;
	return allocator.allocate(n);
}

/*!
 * \brief Wrapper for Config::Allocator::type::allocate().
 *
 * Does not throw \c std::bad_alloc upon allocation failure.
 * Instead, it returns \c nullptr.
 *
 * \ingroup zth_api_cpp_util
 */
template <typename T>
__attribute__((warn_unused_result)) static inline T* allocate_noexcept(size_t n = 1) noexcept
{
	try {
		return allocate<T>(n);
	} catch(std::bad_alloc const&) {
		return nullptr;
	} catch(...) {
		std::terminate();
	}

	// Should not get here.
	return nullptr;
}

template <typename T>
__attribute__((warn_unused_result)) static inline T* new_alloc()
{
	T* o = allocate<T>();
	try {
		new(o) T;
		return o;
	} catch(...) {
		deallocate(o);
		zth_throw();
	}

	// Should not get here.
	return nullptr;
}

template <typename T, typename Arg>
__attribute__((warn_unused_result)) static inline T* new_alloc(Arg const& arg)
{
	T* o = allocate<T>();
	try {
		new(o) T(arg);
		return o;
	} catch(...) {
		deallocate(o);
		zth_throw();
	}

	// Should not get here.
	return nullptr;
}

#  if __cplusplus >= 201103L
template <typename T, typename... Arg>
__attribute__((warn_unused_result)) static inline T* new_alloc(Arg&&... arg)
{
	T* o = allocate<T>();
	try {
		new(o) T{std::forward<Arg>(arg)...};
		return o;
	} catch(...) {
		deallocate(o);
		zth_throw();
	}

	// Should not get here.
	return nullptr;
}
#  endif

/*!
 * \brief Wrapper for Config::Allocator::type::deallocate().
 * \ingroup zth_api_cpp_util
 */
template <typename T>
static inline void deallocate(T* p, size_t n = 1) noexcept
{
	if(!p)
		return;

	typename Config::Allocator<T>::type allocator;
	allocator.deallocate(p, n);
}

template <typename T>
static inline void delete_alloc(T* p) noexcept
{
	if(unlikely(!p))
		return;

	p->~T();
	deallocate(p);
}

/*!
 * \brief Define new/delete operators for a class, which are allocator-aware.
 *
 * Put a call to this macro in the private section of your class
 * definition.  Additionally, make sure to have a virtual destructor to
 * deallocate subclasses properly.
 *
 * \param T the type of the class for which the operators are to be defined
 * \ingroup zth_api_cpp_util
 */
// cppcheck-suppress-macro duplInheritedMember
#  define ZTH_CLASS_NEW_DELETE(T)                                             \
  public:                                                                     \
	  static void* operator new(std::size_t UNUSED_PAR(n))                \
	  {                                                                   \
		  zth_assert(n == sizeof(T));                                 \
		  return ::zth::allocate<T>();                                \
	  }                                                                   \
	  static void operator delete(void* ptr)                              \
	  {                                                                   \
		  ::zth::deallocate<T>(static_cast<T*>(ptr));                 \
	  }                                                                   \
	  static void* operator new[](std::size_t sz)                         \
	  {                                                                   \
		  zth_assert(sz % sizeof(T) == 0);                            \
		  return ::zth::allocate<T>(sz / sizeof(T));                  \
	  }                                                                   \
	  static void operator delete[](void* ptr, size_t sz)                 \
	  {                                                                   \
		  zth_assert(sz % sizeof(T) == 0);                            \
		  ::zth::deallocate<T>(static_cast<T*>(ptr), sz / sizeof(T)); \
	  }                                                                   \
                                                                              \
  private:

/*!
 * \brief \c std::vector type using Config::Allocator::type.
 * \ingroup zth_api_cpp_util
 */
template <typename T>
struct vector_type {
	typedef std::vector<T, typename Config::Allocator<T>::type> type;
};

/*!
 * \brief \c std::map type using Config::Allocator::type.
 * \ingroup zth_api_cpp_util
 */
template <typename Key, typename T, typename Compare = std::less<Key> /**/>
struct map_type {
	typedef std::map<
		Key, T, Compare, typename Config::Allocator<std::pair<const Key, T> /**/>::type>
		type;
};

/*!
 * \brief \c std::list type using Config::Allocator::type.
 * \ingroup zth_api_cpp_util
 */
template <typename T>
struct list_type {
	typedef std::list<T, typename Config::Allocator<T>::type> type;
};

template <typename Traits, typename Allocator>
string to_zth_string(std::basic_string<char, Traits, Allocator> const& s)
{
	return string(s.data(), s.size());
}

inline std::string to_std_string(string const& s)
{
	return std::string(s.data(), s.size());
}

} // namespace zth
#endif // __cplusplus
#endif // ZTH_ALLOCATOR_H
