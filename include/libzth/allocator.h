#ifndef ZTH_ALLOCATOR_H
#define ZTH_ALLOCATOR_H
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

#ifdef __cplusplus

#include <libzth/macros.h>
#include <libzth/config.h>
#include <libzth/util.h>

#include <map>
#include <list>
#include <string>
#include <vector>

namespace zth {

	/*!
	 * \brief Wrapper for Config::Allocator::type::allocate().
	 * \ingroup zth_api_cpp_util
	 */
	template <typename T>
	__attribute__((warn_unused_result)) static inline T* allocate(size_t n = 1)
	{
		typename Config::Allocator<T>::type allocator;
		return allocator.allocate(n);
	}

	template <typename T>
	__attribute__((warn_unused_result)) static inline T* new_alloc()
	{
		T* o = allocate<T>(1);
		new(o) T;
		return o;
	}

	template <typename T, typename Arg>
	__attribute__((warn_unused_result)) static inline T* new_alloc(Arg const& arg)
	{
		T* o = allocate<T>(1);
		new(o) T(arg);
		return o;
	}

#if __cplusplus >= 201103L
	template <typename T, typename... Arg>
	__attribute__((warn_unused_result)) static inline T* new_alloc(Arg&&... arg)
	{
		T* o = allocate<T>(1);
		new(o) T(std::forward<Arg>(arg)...);
		return o;
	}
#endif

	/*!
	 * \brief Wrapper for Config::Allocator::type::deallocate().
	 * \ingroup zth_api_cpp_util
	 */
	template <typename T>
	static inline void deallocate(T* p, size_t n = 1) noexcept
	{
		typename Config::Allocator<T>::type allocator;
		allocator.deallocate(p, n);
	}

	template <typename T>
	static inline void delete_alloc(T* p) noexcept
	{
		if(unlikely(!p))
			return;

		p->~T();
		deallocate(p, 1);
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
#define ZTH_CLASS_NEW_DELETE(T)						\
	public:								\
		void* operator new(std::size_t UNUSED_PAR(n))		\
		{							\
			zth_assert(n == sizeof(T));			\
			return ::zth::allocate<T>();			\
		}							\
		void operator delete(void* ptr)				\
		{							\
			::zth::deallocate<T>(static_cast<T*>(ptr));	\
		}							\
	private:

	template <typename T>
	struct vector_type {
		typedef std::vector<T, typename Config::Allocator<T>::type> type;
	};

	template <typename Key, typename T, typename Compare = std::less<Key> >
	struct map_type {
		typedef std::map<Key, T, Compare, typename Config::Allocator<std::pair<const Key, T> >::type> type;
	};

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

} // namespace
#endif // __cplusplus
#endif // ZTH_ALLOCATOR_H
