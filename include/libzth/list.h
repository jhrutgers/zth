#ifndef ZTH_LIST_H
#define ZTH_LIST_H
/*
 * SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#include <libzth/macros.h>

#ifdef __cplusplus

#  include <libzth/allocator.h>
#  include <libzth/config.h>
#  include <libzth/util.h>

#  include <functional>

namespace zth {

class Listable;

template <typename T>
class List;

template <typename T = Listable, typename Compare = std::less<T> /**/>
class SortedList;

class Listable {
public:
	typedef void* user_type;

	constexpr Listable() noexcept
		: prev()
		, next()
		, user()
	{}

	constexpr Listable(Listable const& UNUSED_PAR(e)) noexcept
		: prev()
		, next()
		, user()
	{}

	Listable& operator=(Listable const& UNUSED_PAR(rhs)) noexcept
	{
		if(Config::EnableAssert)
			prev = next = nullptr;
		return *this;
	}

#  if __cplusplus >= 201103L
	Listable(Listable&& l) noexcept
	{
		*this = std::move(l);
	}

	Listable& operator=(Listable&& l) noexcept
	{
		// Cannot move while in a list.
		zth_assert(!prev);
		zth_assert(!l.prev);
		zth_assert(!next);
		zth_assert(!l.next);
		return *this;
	}
#  endif // C++11

private:
	union {
		Listable* prev; // for List
		Listable* left; // for SortedList
	};
	union {
		Listable* next;	 // for List
		Listable* right; // for SortedList
	};
	union {
		user_type user;	    // For List
		uint_fast8_t level; // for SortedList
	};

	template <typename T>
	friend class List;
	template <typename T, typename Compare>
	friend class SortedList;
};

template <typename T = Listable>
class List {
	ZTH_CLASS_NOCOPY(List)
	ZTH_CLASS_NEW_DELETE(List)
public:
	typedef T type;
	typedef Listable elem_type;
	typedef elem_type::user_type user_type;

	template <bool cyclic>
	class Iterator {
	public:
		constexpr explicit Iterator(elem_type* start, elem_type* current = nullptr) noexcept
			: m_start(start)
			, m_current(current)
		{}

		bool atBegin() const noexcept
		{
			return !m_current && m_start;
		}

		bool atEnd() const noexcept
		{
			return m_current == m_start;
		}

		elem_type* get() const noexcept
		{
			zth_assert(cyclic || !atEnd());
			return unlikely(atBegin()) ? m_start : m_current;
		}

		template <bool C>
		bool operator==(Iterator<C> const& rhs) const noexcept
		{
			if(atEnd())
				return rhs.atEnd();
			if(rhs.atEnd())
				return false;
			return get() == rhs.get();
		}

		template <bool C>
		bool operator!=(Iterator<C> const& rhs) const noexcept
		{
			return !(*this == rhs);
		}

		void next() noexcept
		{
			zth_assert((cyclic || !atEnd()) && m_start);
			m_current = unlikely(atBegin()) ? m_start->next : m_current->next;
		}

		void prev() noexcept
		{
			zth_assert((cyclic || !atEnd()) && m_start);
			m_current =
				unlikely(m_current->prev == m_start) ? nullptr : m_current->prev;
		}

		Iterator& operator++() noexcept
		{
			next();
			return *this;
		}

		Iterator operator++(int) noexcept
		{
			Iterator i = *this;
			next();
			return i;
		}

		Iterator& operator--() noexcept
		{
			prev();
			return *this;
		}

		Iterator operator--(int) noexcept
		{
			Iterator i = *this;
			prev();
			return i;
		}

		type& operator*() const noexcept
		{
			elem_type* elem = get();
			zth_assert(elem);
			// cppcheck-suppress nullPointerRedundantCheck
			return *static_cast<type*>(elem);
		}

		type* operator->() const noexcept
		{
			return &this->operator*();
		}

		user_type const& user() const noexcept
		{
			elem_type const* elem = get();
			zth_assert(elem);
			// cppcheck-suppress nullPointerRedundantCheck
			return elem->user;
		}

		user_type& user() noexcept
		{
			elem_type* elem = get();
			zth_assert(elem);
			// cppcheck-suppress nullPointerRedundantCheck
			return elem->user;
		}

	private:
		elem_type* m_start;
		elem_type* m_current;
	};

	typedef Iterator<false> iterator;
	typedef Iterator<true> cyclic_iterator;

	constexpr List() noexcept
		: m_head()
		, m_tail()
	{}

	~List() noexcept
	{
		if(Config::EnableAssert)
			clear();
	}

	type& back() const noexcept
	{
		zth_assert(!empty());
		return *static_cast<type*>(m_tail);
	}

	user_type const& user_back() const noexcept
	{
		zth_assert(!empty());
		return m_tail->user;
	}

	user_type& user_back() noexcept
	{
		zth_assert(!empty());
		return m_tail->user;
	}

	iterator push_back(elem_type& elem) noexcept
	{
		zth_assert(elem.prev == nullptr);
		zth_assert(elem.next == nullptr);

		elem.user = user_type();

		if(m_tail == nullptr) {
			zth_assert(m_head == nullptr);
			m_tail = m_head = elem.prev = elem.next = &elem;
			return begin();
		} else {
			elem.next = m_tail->next;
			elem.prev = m_tail;
			m_tail = elem.next->prev = elem.prev->next = &elem;
			return iterator(m_head, &elem);
		}
	}

	user_type pop_back() noexcept
	{
		zth_assert(!empty());

		elem_type* elem = m_tail;

		if(m_tail == m_head) {
			m_tail = m_head = nullptr;
			zth_assert(elem->prev == elem->next);
			zth_assert(elem->prev == elem);
		} else {
			m_tail = m_tail->prev;
			elem->prev->next = elem->next;
			elem->next->prev = elem->prev;
		}

		if(Config::EnableAssert)
			elem->prev = elem->next = nullptr;

		return elem->user;
	}

	type& front() const noexcept
	{
		zth_assert(!empty());
		return *static_cast<type*>(m_head);
	}

	user_type const& user_front() const noexcept
	{
		zth_assert(!empty());
		return m_head->user;
	}

	user_type& user_front() noexcept
	{
		zth_assert(!empty());
		return m_head->user;
	}

	iterator push_front(elem_type& elem) noexcept
	{
		zth_assert(elem.prev == nullptr);
		zth_assert(elem.next == nullptr);

		if(m_head == nullptr) {
			zth_assert(m_tail == nullptr);
			m_tail = m_head = elem.prev = elem.next = &elem;
		} else {
			elem.next = m_head;
			elem.prev = m_head->prev;
			elem.prev->next = elem.next->prev = m_head = &elem;
		}
		elem.user = user_type();
		return iterator(m_head, &elem);
	}

	user_type pop_front() noexcept
	{
		zth_assert(!empty());

		elem_type* elem = m_head;

		if(m_tail == m_head) {
			m_tail = m_head = nullptr;
			zth_assert(elem->prev == elem->next);
			zth_assert(elem->prev == elem);
		} else {
			m_head = m_head->next;
			elem->prev->next = elem->next;
			elem->next->prev = elem->prev;
		}

		if(Config::EnableAssert)
			elem->prev = elem->next = nullptr;

		return elem->user;
	}

	bool empty() const noexcept
	{
		zth_assert(m_head != nullptr || m_tail == nullptr);
		return m_head == nullptr;
	}

	void clear() noexcept
	{
		if(Config::EnableAssert)
			while(!empty())
				pop_front();
		else
			m_head = m_tail = nullptr;
	}

	iterator begin() const noexcept
	{
		return iterator(m_head);
	}

	cyclic_iterator cyclic(elem_type& start) const noexcept
	{
		zth_assert(contains(start));
		return cyclic_iterator(&start);
	}

	iterator end() const noexcept
	{
		return iterator(m_head, m_head);
	}

	bool contains(elem_type const& elem) const noexcept
	{
		for(iterator it = begin(); it != end(); ++it)
			if(it.get() == &elem)
				return true;
		return false;
	}

	size_t size() const noexcept
	{
		size_t res = 0;
		for(iterator it = begin(); it != end(); ++it)
			res++;
		return res;
	}

	iterator insert(iterator const& pos, elem_type& elem) noexcept
	{
		if(pos.atEnd()) {
			push_back(elem);
			return iterator(m_head, &elem);
		} else if(pos.atBegin()) {
			push_front(elem);
			return begin();
		} else {
			elem_type* before = pos.get();
			elem.next = before;
			elem.prev = before->prev;
			before->prev = elem.prev->next = &elem;
			elem.user = user_type();
			return iterator(m_head, &elem);
		}
	}

	iterator erase(iterator const& pos) noexcept
	{
		zth_assert(!pos.atEnd());
		if(pos.atBegin()) {
			erase(*pos.get());
			return begin();
		} else if(pos.get() == m_tail) {
			erase(*pos.get());
			return end();
		} else {
			elem_type* next = pos.get()->next;
			erase(*pos.get());
			return iterator(m_head, next);
		}
	}

	user_type erase(elem_type& elem) noexcept
	{
		zth_assert(contains(elem));
		if(m_head == &elem) {
			return pop_front();
		} else if(m_tail == &elem) {
			return pop_back();
		} else {
			elem.prev->next = elem.next;
			elem.next->prev = elem.prev;
			if(Config::EnableAssert)
				elem.next = elem.prev = nullptr;
			return elem.user;
		}
	}

	void rotate(elem_type& elem) noexcept
	{
		zth_assert(contains(elem));
		m_head = &elem;
		m_tail = elem.prev;
	}

private:
	elem_type* m_head;
	elem_type* m_tail;
};

template <typename T, typename Compare>
class SortedList {
	ZTH_CLASS_NOCOPY(SortedList)
	ZTH_CLASS_NEW_DELETE(SortedList)
public:
	typedef T type;
	typedef Listable elem_type;

	constexpr SortedList() noexcept
		: m_comp()
		, m_t()
		, m_head()
		, m_size()
	{}

	~SortedList() noexcept
	{
		clear();
	}

	void insert(elem_type& x) noexcept
	{
		zth_assert(!x.left);
		zth_assert(!x.right);
		zth_assert(x.level == 0);
		zth_assert(!contains(x));
		if(!m_t)
			m_head = &x;
		m_t = insert_(x, m_t);
		m_size++;
		check();
	}

	void erase(elem_type& x) noexcept
	{
		zth_assert(contains(x));
		m_t = delete_(x, m_t);
		if(m_head == &x)
			m_head = lowest(m_t);
		if(Config::EnableAssert) {
			x.level = 0;
			x.left = x.right = nullptr;
		}
		m_size--;
		check();
	}

	void clear() noexcept
	{
		if(Config::EnableAssert) {
			while(!empty())
				erase(*m_t);
		} else {
			m_t = m_head = nullptr;
			m_size = 0;
		}
	}

	size_t size() const noexcept
	{
		zth_assert(m_size > 0 || !m_t);
		return m_size;
	}

	bool empty() const noexcept
	{
		zth_assert(m_size > 0 || !m_t);
		return !m_t;
	}

	bool contains(elem_type& x) const noexcept
	{
		elem_type* t = m_t;
		while(t) {
			if(t == &x)
				return true;
			else if(m_comp(static_cast<type&>(x), static_cast<type&>(*t)))
				t = t->left;
			else if(m_comp(static_cast<type&>(*t), static_cast<type&>(x)))
				t = t->right;
			else if((uintptr_t)&x < (uintptr_t)t)
				t = t->left;
			else
				t = t->right;
		}
		return false;
	}

	type& front() const noexcept
	{
		zth_assert(m_head && m_t);
		return static_cast<type&>(*m_head);
	}

protected:
	static elem_type* skew(elem_type* t) noexcept
	{
		if(!t)
			return nullptr;
		else if(!t->left)
			return t;
		else if(t->left->level == t->level) {
			elem_type* l = t->left;
			t->left = l->right;
			l->right = t;
			return l;
		} else
			return t;
	}

	static elem_type* split(elem_type* t) noexcept
	{
		if(!t)
			return nullptr;
		else if(!t->right || !t->right->right)
			return t;
		else if(t->level == t->right->right->level) {
			elem_type* r = t->right;
			t->right = r->left;
			r->left = t;
			zth_assert(r->level < std::numeric_limits<decltype(r->level)>::max());
			r->level++;
			return r;
		} else
			return t;
	}

	elem_type* insert_(elem_type& x, elem_type* t) noexcept
	{
		if(!t) {
			x.level = 1;
			x.left = x.right = nullptr;
			return &x;
		} else if(m_comp(static_cast<type&>(x), static_cast<type&>(*t)))
			t->left = insert_(x, t->left);
		else if(m_comp(static_cast<type&>(*t), static_cast<type&>(x)))
			t->right = insert_(x, t->right);
		else if((uintptr_t)&x < (uintptr_t)t)
			t->left = insert_(x, t->left);
		else
			t->right = insert_(x, t->right);

		if(t == m_head && t->left)
			m_head = t->left;

		return split(skew(t));
	}

	elem_type* delete_(elem_type& x, elem_type* t) noexcept
	{
		if(!t)
			return nullptr;
		else if(&x == t) {
			if(!t->left && !t->right) {
				return nullptr;
			} else if(!t->left) {
				elem_type* l = successor(t);
				l->right = delete_(*l, t->right);
				l->left = t->left;
				l->level = t->level;
				t = l;
			} else {
				elem_type* l = predecessor(t);
				l->left = delete_(*l, t->left);
				l->right = t->right;
				l->level = t->level;
				t = l;
			}
		} else if(m_comp(static_cast<type&>(x), static_cast<type&>(*t)))
			t->left = delete_(x, t->left);
		else if(m_comp(static_cast<type&>(*t), static_cast<type&>(x)))
			t->right = delete_(x, t->right);
		else if((uintptr_t)&x < (uintptr_t)t)
			t->left = delete_(x, t->left);
		else
			t->right = delete_(x, t->right);

		decrease_level(t);
		t = skew(t);
		t->right = skew(t->right);
		if(t->right)
			t->right->right = skew(t->right->right);
		t = split(t);
		t->right = split(t->right);
		return t;
	}

	static elem_type* lowest(elem_type* t) noexcept
	{
		if(!t)
			return nullptr;
		else if(!t->left)
			return t;
		else
			return lowest(t->left);
	}

	static elem_type* highest(elem_type* t) noexcept
	{
		if(!t)
			return nullptr;
		else if(!t->right)
			return t;
		else
			return highest(t->right);
	}

	static elem_type* successor(elem_type* t) noexcept
	{
		return t ? lowest(t->right) : nullptr;
	}

	static elem_type* predecessor(elem_type* t) noexcept
	{
		return t ? highest(t->left) : nullptr;
	}

	// cppcheck-suppress constParameterPointer
	static void decrease_level(elem_type* t) noexcept
	{
#  ifndef CPPCHECK // Internal error, somehow.
		decltype(t->level) ll = t->left ? t->left->level : 0;
		decltype(t->level) lr = t->right ? t->right->level : 0;
		decltype(t->level) should_be = (decltype(t->level))((ll < lr ? ll : lr) + 1U);
		if(should_be < t->level) {
			t->level = should_be;
			if(t->right && should_be < t->right->level)
				t->right->level = should_be;
		}
#  endif
	}

	void check() const noexcept
	{
		zth_dbg(list, "SortedList %p (head %p):", this, m_head);
		print(m_t, 2);
		zth_assert(m_size == check(m_t));
		zth_assert((!m_t && !m_head) || (m_t && m_head && lowest(m_t) == m_head));
	}

	size_t check(elem_type* t) const noexcept
	{
		if(!Config::EnableAssert)
			return 0;
		if(!t)
			return 0;

		zth_assert(
			!t->left || m_comp(static_cast<type&>(*t->left), static_cast<type&>(*t))
			|| (!m_comp(static_cast<type&>(*t), static_cast<type&>(*t->left))
			    && ((uintptr_t)t > (uintptr_t)t->left)));
		zth_assert(
			!t->right || m_comp(static_cast<type&>(*t), static_cast<type&>(*t->right))
			|| (!m_comp(static_cast<type&>(*t->right), static_cast<type&>(*t))
			    || ((uintptr_t)t < (uintptr_t)t->right)));

		zth_assert(t->left || t->right || t->level == 1);
		zth_assert(!t->left || t->left->level == t->level - 1);
		zth_assert(
			!t->right || t->right->level == t->level - 1
			|| t->right->level == t->level);
		zth_assert(!t->right || !t->right->right || t->right->right->level < t->level);
		zth_assert(t->level < 2 || (t->left && t->right));
		return 1 + check(t->left) + check(t->right);
	}

	static void print(elem_type* t, int indent = 0) noexcept
	{
		if(Config::Print_list == 0 || !Config::SupportDebugPrint)
			return;

		if(!t)
			zth_dbg(list, "%*s (null)", indent, "");
		else {
			zth_dbg(list, "%*s node %p level=%d: %s", indent, "", static_cast<type*>(t),
				t->level, static_cast<type*>(t)->str().c_str());
			if(t->left || t->right) {
				print(t->left, indent + 2);
				print(t->right, indent + 2);
			}
		}
	}

private:
	Compare m_comp;
	elem_type* m_t;
	elem_type* m_head;
	size_t m_size;
};

} // namespace zth
#endif // __cplusplus
#endif // ZTH_LIST_H
