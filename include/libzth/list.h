#ifndef __ZTH_LIST_H
#define __ZTH_LIST_H
#ifdef __cplusplus

namespace zth {
	
	template <typename T> class List;

	template <typename ChildClass>
	class ListElement {
	public:
		typedef ChildClass type;

		ListElement()
			: prev()
			, next()
		{}

		ListElement(ListElement const& e)
			: prev()
			, next()
		{}

		ListElement& operator=(ListElement const& rhs) {
			if(Config::EnableAssert)
				prev = next = NULL;
			return *this;
		}

		type* listNext() const { return static_cast<type*>(next); }
		type* listPrev() const { return static_cast<type*>(prev); }

	private:
		ListElement* prev;
		ListElement* next;

		friend class List<type>;
	};

	template <typename T>
	class List {
	public:
		typedef T type;
		typedef ListElement<type> elem_type;

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
				m_tail = m_head = elem.prev = elem.next = elem;
			} else {
				elem->next = m_head;
				elem->prev = m_head->prev;
				elem->prev->next = elem->next->prev = m_head = elem;
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

} // namespace
#endif // __cplusplus
#endif // __ZTH_LIST_H
