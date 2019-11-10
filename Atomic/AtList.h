#pragma once

#include "AtConstIt.h"
#include "AtMem.h"

namespace At
{
	
	template <class T>
	struct ListNode
	{
		template <typename... Args>
		ListNode(Args&&... args) : m_val(std::forward<Args>(args)...) {}

		~ListNode()
		{
			if (std::is_trivially_destructible<T>::value)
				Mem::Zero<T>(&m_val, 1);
		}

		T            m_val;
		ListNode<T>* m_next {};
		ListNode<T>* m_prev {};
	};


	template <class T>
	class List
	{
	private:
		// Just... don't copy a list.
		List(List<T> const&) = delete;
		List<T>& operator=(List<T> const&) = delete;

	public:
		using Val = T;
		using Node = ListNode<T>;

		class It
		{
		public:
			using Val = T;

			It() noexcept = default;
			It(It const&) noexcept = default;

			It(Node* node)               : m_node(node), m_pastEnd(node == nullptr) {}
			It(Node* node, bool pastEnd) : m_node(node), m_pastEnd(pastEnd)         {}

			bool Valid() const noexcept { return m_node || m_pastEnd; }		// Invalid state is reached by decrementing past beginning
			bool Any() const noexcept { return m_node && !m_pastEnd; }

			Val* Ptr() const { if (!m_node || m_pastEnd) { return nullptr; } return &m_node->m_val; }
			Val& Ref() const { EnsureThrow(m_node && !m_pastEnd); return m_node->m_val; }

			Val& operator*  () const { return Ref(); }
			Val* operator-> () const { return &Ref(); }

			bool operator== (It const& x) const noexcept { return m_node == x.m_node && m_pastEnd == x.m_pastEnd; }
			bool operator!= (It const& x) const noexcept { return m_node != x.m_node || m_pastEnd != x.m_pastEnd; }

			It& operator++ ()    { return Increment(); }
			It& operator-- ()    { return Decrement(); }
			It  operator++ (int) { It prev{*this}; Increment(); return prev; }
			It  operator-- (int) { It prev{*this}; Decrement(); return prev; }

			It& Increment()
			{
				if (m_node && !m_pastEnd)
				{
					Node* nextNode = m_node->m_next;
					if (nextNode)
						m_node = nextNode;
					else
						m_pastEnd = true;
				}
				return *this;
			}

			It& Decrement()
			{
				if (m_pastEnd)
					m_pastEnd = false;
				else if (m_node)
					m_node = m_node->m_prev;
				return *this;
			}

		private:
			Node* m_node    {};
			bool  m_pastEnd {};

			friend class At::List<T>;
		};

		using ConstIt = At::ConstIt<It>;

	public:
		List() = default;
		~List() { Clear(); }

		void Clear() noexcept(std::is_nothrow_destructible<T>::value) { Erase_Inner(m_first, SIZE_MAX); EnsureAbort(m_len == 0); }

		It Erase(It& it, sizet n)
		{
			EnsureThrow(it.Valid());
			Node* node = it.m_node;
			if (!it.m_pastEnd)
				node = Erase_Inner(node, n);
			return It(node);
		}

		bool  Any() const noexcept { return m_len != 0; }
		sizet Len() const noexcept { return m_len; }

		It      begin()       noexcept { return It      (m_first); }
		ConstIt begin() const noexcept { return ConstIt (m_first); }

		It      end()         noexcept { return It      (m_last, true); }
		ConstIt end()   const noexcept { return ConstIt (m_last, true); }

		Val&       First  ()       { EnsureThrow(m_first); return m_first->m_val; }
		Val const& First  () const { EnsureThrow(m_first); return m_first->m_val; }

		Val&       Last   ()       { EnsureThrow(m_last);  return m_last->m_val; }
		Val const& Last   () const { EnsureThrow(m_last);  return m_last->m_val; }

		It         LastIt ()       { EnsureThrow(m_last);  return It      (m_last); }
		ConstIt    LastIt () const { EnsureThrow(m_last);  return ConstIt (m_last); }

		template <typename... Args>
		Val& Add(Args&&... args)
		{
			Node* node = new Node(std::forward<Args>(args)...);
			if (!m_first)
				m_first = m_last = node;
			else
			{
				m_last->m_next = node;
				node->m_prev = m_last;
				m_last = node;
			}
			++m_len;
			return node->m_val;
		}

		List<T>& PopFirst()
		{
			EnsureThrow(m_first);

			Node* node = m_first;
			m_first = node->m_next;
			delete node;
			--m_len;

			if (m_first)
				m_first->m_prev = nullptr;
			else
			{
				m_last = nullptr;
				EnsureThrow(m_len == 0);
			}

			return *this;
		}

		List<T>& PopLast()
		{
			EnsureThrow(m_last);

			Node* node = m_last;
			m_last = node->m_prev;
			delete node;
			--m_len;

			if (m_last)
				m_last->m_next = nullptr;
			else
			{
				m_first = nullptr;
				EnsureThrow(m_len == 0);
			}

			return *this;
		}
		
	private:
		Node* m_first {};
		Node* m_last  {};
		sizet m_len   {};

		Node* Erase_Inner(Node* nextNode, sizet n)
		{
			if (nextNode && n)
			{
				Node* prevNode = nextNode->m_prev;

				do
				{
					Node* node = nextNode;
					nextNode = node->m_next;
					delete node;
					--m_len;
					--n;
				}
				while (nextNode && n);

				if (prevNode)
					prevNode->m_next = nextNode;
				else
					m_first = nextNode;

				if (nextNode)
					nextNode->m_prev = prevNode;
				else
					m_last = prevNode;
			}

			return nextNode;
		}

	};

}
