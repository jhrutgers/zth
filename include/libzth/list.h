#ifndef __ZTH_LIST_H
#define __ZTH_LIST_H
#ifdef __cplusplus

#include <libzth/config.h>
#include <libzth/util.h>

#include <functional>

namespace zth {
	
	template <typename T> class List;
	template <typename T, typename Compare = std::less<T> > class SortedList;

	template <typename ChildClass>
	class Listable {
	public:
		typedef ChildClass type;

		Listable()
			: prev()
			, next()
			, level()
		{}

		Listable(Listable const& e)
			: prev()
			, next()
			, level()
		{}

		Listable& operator=(Listable const& rhs) {
			if(Config::EnableAssert)
				prev = next = NULL;
			return *this;
		}

		type* listNext() const { zth_assert(level == 0 && next); return static_cast<type*>(next); }
		type* listPrev() const { zth_assert(level == 0 && prev); return static_cast<type*>(prev); }

	private:
		union {
			Listable* prev; // for List
			Listable* left; // for SortedList
		};
		union {
			Listable* next; // for List
			Listable* right;// for SortedList
		};
		uint_fast8_t level; // for SortedList

		friend class List<type>;
		template <typename T_, typename Compare> friend class SortedList;
	};

	template <typename T>
	class List {
	public:
		typedef T type;
		typedef Listable<type> elem_type;

		List()
			: m_head()
			, m_tail()
		{}

		~List() {
			if(Config::EnableAssert)
				clear();
		}

		type& back() {
			zth_assert(!empty());
			return *static_cast<type*>(m_tail);
		}

		void push_back(elem_type& elem) {
			zth_assert(elem.prev == NULL);
			zth_assert(elem.next == NULL);

			if(m_tail == NULL) {
				zth_assert(m_head == NULL);
				m_tail = m_head = elem.prev = elem.next = &elem;
			} else {
				elem.next = m_tail->next;
				elem.prev = m_tail;
				m_tail = elem.next->prev = elem.prev->next = &elem;
			}
		}
		
		void pop_back() {
			zth_assert(!empty());

			elem_type* elem = m_tail;

			if(m_tail == m_head) {
				m_tail = m_head = NULL;
				zth_assert(elem->prev == elem->next);
				zth_assert(elem->prev == elem);
			} else {
				m_tail = m_tail->prev;
				elem->prev->next = elem->next;
				elem->next->prev = elem->prev;
			}

			if(Config::EnableAssert)
				elem->prev = elem->next = NULL;
		}

		type& front() {
			zth_assert(!empty());
			return *static_cast<type*>(m_head);
		}
		
		void push_front(elem_type& elem) {
			zth_assert(elem.prev == NULL);
			zth_assert(elem.next == NULL);

			if(m_head == NULL) {
				zth_assert(m_tail == NULL);
				m_tail = m_head = elem.prev = elem.next = &elem;
			} else {
				elem.next = m_head;
				elem.prev = m_head->prev;
				elem.prev->next = elem.next->prev = m_head = &elem;
			}
		}
		
		void pop_front() {
			zth_assert(!empty());

			elem_type* elem = m_head;

			if(m_tail == m_head) {
				m_tail = m_head = NULL;
				zth_assert(elem->prev == elem->next);
				zth_assert(elem->prev == elem);
			} else {
				m_head = m_head->next;
				elem->prev->next = elem->next;
				elem->next->prev = elem->prev;
			}

			if(Config::EnableAssert)
				elem->prev = elem->next = NULL;
		}

		bool empty() const {
			zth_assert(m_head != NULL || m_tail == NULL);
			return m_head == NULL;
		}

		void clear() {
			if(Config::EnableAssert)
				while(!empty())
					pop_front();
			else
				m_head = m_tail = NULL;
		}
		
		class iterator {
		public:
			iterator(elem_type* start, elem_type* current = NULL) : m_start(start), m_current(current) {}
			bool atBegin() const { return !m_current && m_start; }
			bool atEnd() const { return m_current == m_start; }
			elem_type* get() const { return atBegin() ? m_start : m_current; }
			bool operator==(iterator const& rhs) const { return m_current == rhs.m_current; }
			bool operator!=(iterator const& rhs) const { return !(*this == rhs); }

			void next() {
				zth_assert(!atEnd());
				m_current = atBegin() ? m_start->next : m_current->next;
			}

			void prev() {
				zth_assert(!atBegin());
				zth_assert(m_start);
				m_current = m_current->prev == m_start ? NULL : m_current->prev;
			}

			iterator& operator++() { next(); return *this; }
			iterator operator++(int) { iterator i = *this; next(); return i; }
			iterator& operator--() { prev(); return *this; }
			iterator operator--(int) { iterator i = *this; prev(); return i; }

			type& operator*() const {
				elem_type* elem = get();
				zth_assert(elem);
				return *static_cast<type*>(elem);
			}
			type* operator->() const { return &this->operator*(); }

		private:
			elem_type* m_start;
			elem_type* m_current;
		};

		iterator begin() const {
			return iterator(m_head);
		}

		iterator end() const {
			return iterator(m_head, m_head);
		}

		bool contains(elem_type& elem) const {
			for(iterator it = begin(); it != end(); ++it)
				if(it.get() == &elem)
					return true;
			return false;
		}

		size_t size() const {
			size_t res = 0;
			for(iterator it = begin(); it != end(); ++it)
				res++;
			return res;
		}

		iterator insert(iterator const& pos, elem_type& elem) {
			if(pos.atEnd()) {
				push_back(elem);
				return iterator(m_head, &elem);
			} else if(pos.atBegin()) {
				push_front(elem);
				return begin();
			} else {
				elem_type* before = pos.get();
				elem->next = before;
				elem->prev = before->prev;
				before->prev = elem->prev->next = elem;
				return iterator(m_head, &elem);
			}
		}

		iterator erase(iterator const& pos) {
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

		void erase(elem_type& elem) {
			zth_assert(contains(elem));
			if(m_head == &elem) {
				pop_front();
			} else if(m_tail == &elem) {
				pop_back();
			} else {
				elem.prev->next = elem.next;
				elem.next->prev = elem.prev;
				if(Config::EnableAssert)
					elem.next = elem.prev = NULL;
			}
		}

		bool contains(type& elem) const {
			for(iterator it = begin(); it != end(); ++it)
				if(it.get() == &elem)
					return true;
			return false;
		}

		void rotate(elem_type& elem) {
			zth_assert(contains(elem));
			m_head = &elem;
			m_tail = elem.prev;
		}

	private:
		List(List const&);
		List& operator=(List const&);

		elem_type* m_head;
		elem_type* m_tail;
	};

	template <typename T, typename Compare>
	class SortedList {
	public:
		typedef T type;
		typedef Listable<type> elem_type;

		SortedList()
			: m_comp()
			, m_t()
			, m_head()
			, m_size()
		{}

		~SortedList() {
			clear();
		}

		void insert(elem_type& x) {
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

		void erase(elem_type& x) {
			zth_assert(contains(x));
			m_t = delete_(x, m_t);
			if(m_head == &x)
				m_head = lowest(m_t);
			if(Config::EnableAssert)
				x.left = x.right = NULL;
			m_size--;
			check();
		}
		
		void clear() {
			if(Config::EnableAssert) {
				while(!empty())
					erase(*m_t);
			} else {
				m_t = m_head = NULL;
				m_size = 0;
			}
		}

		size_t size() const {
			zth_assert(m_size > 0 || !m_t);
			return m_size;
		}

		bool empty() const {
			zth_assert(m_size > 0 || !m_t);
			return !m_t;
		}

		bool contains(elem_type& x) const {
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

		type& front() const {
			zth_assert(m_head && m_t);
			return static_cast<type&>(*m_head);
		}

	protected:
		static elem_type* skew(elem_type* t) {
			if(!t)
				return NULL;
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

		static elem_type* split(elem_type* t) {
			if(!t)
				return NULL;
			else if(!t->right || !t->right->right)
				return t;
			else if(t->level == t->right->right->level) {
				elem_type* r = t->right;
				t->right = r->left;
				r->left = t;
				zth_assert(r->level < std::numeric_limits<typeof(r->level)>::max());
				r->level++;
				return r;
			} else
				return t;
		}
		
		elem_type* insert_(elem_type& x, elem_type* t) {
			if(!t) {
				x.level = 1;
				x.left = x.right = NULL;
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

		elem_type* delete_(elem_type& x, elem_type* t) {
			if(!t)
				return NULL;
			else if(&x == t) {
				if(!t->left && !t->right) {
					if(Config::EnableAssert)
						t->level = 0;
					return NULL;
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
			}else if(m_comp(static_cast<type&>(x), static_cast<type&>(*t)))
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

		static elem_type* lowest(elem_type* t) {
			if(!t)
				return NULL;
			else if(!t->left)
				return t;
			else
				return lowest(t->left);
		}
		
		static elem_type* highest(elem_type* t) {
			if(!t)
				return NULL;
			else if(!t->right)
				return t;
			else
				return highest(t->right);
		}

		static elem_type* successor(elem_type* t) {
			return t ? lowest(t->right) : NULL;
		}
		
		static elem_type* predecessor(elem_type* t) {
			return t ? highest(t->left) : NULL;
		}

		static void decrease_level(elem_type* t) {
			typeof(t->level) ll = t->left ? t->left->level : 0;
			typeof(t->level) lr = t->right ? t->right->level : 0;
			typeof(t->level) should_be = (ll < lr ? ll : lr) + 1;
			if(should_be < t->level) {
				t->level = should_be;
				if(t->right && should_be < t->right->level)
					t->right->level = should_be;
			}
		}

		void check() {
			zth_dbg(list, "SortedList %p (head %p):", this, m_head);
			print(m_t, 2);
			zth_assert(m_size == check(m_t));
			zth_assert((!m_t && !m_head) || (m_t && m_head && lowest(m_t) == m_head));
		}

		size_t check(elem_type* t) {
			if(!Config::EnableAssert)
				return 0;
			if(!t)
				return 0;

			zth_assert(!t->left
				|| m_comp(static_cast<type&>(*t->left), static_cast<type&>(*t))
				|| (!m_comp(static_cast<type&>(*t), static_cast<type&>(*t->left)) && ((uintptr_t)t > (uintptr_t)t->left)));
			zth_assert(!t->right
				|| m_comp(static_cast<type&>(*t), static_cast<type&>(*t->right))
				|| (!m_comp(static_cast<type&>(*t->right), static_cast<type&>(*t)) || ((uintptr_t)t < (uintptr_t)t->right)));

			zth_assert(t->left || t->right || t->level == 1);
			zth_assert(!t->left || t->left->level == t->level - 1);
			zth_assert(!t->right || t->right->level == t->level - 1 || t->right->level == t->level);
			zth_assert(!t->right || !t->right->right || t->right->right->level < t->level);
			zth_assert(t->level < 2 || (t->left && t->right));
			return 1 + check(t->left) + check(t->right);
		}

		static void print(elem_type* t, int indent = 0) {
			if(Config::Print_list == 0 || !Config::EnableDebugPrint)
				return;

			if(!t)
				zth_dbg(list, "%*s (null)", indent, "");
			else {
				zth_dbg(list, "%*s node %p level=%d: %s", indent, "", static_cast<type*>(t), t->level, static_cast<type*>(t)->str().c_str());
				if(t->left || t->right) {
					print(t->left, indent + 2);
					print(t->right, indent + 2);
				}
			}
		}

	private:
		SortedList(SortedList const&);
		SortedList& operator=(SortedList const&);

		Compare m_comp;
		elem_type* m_t;
		elem_type* m_head;
		size_t m_size;
	};

} // namespace
#endif // __cplusplus
#endif // __ZTH_LIST_H
