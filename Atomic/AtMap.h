#pragma once

#include "AtConstIt.h"
#include "AtRange.h"
#include "AtReconstruct.h"
#include "AtRp.h"
#include "AtStr.h"


namespace At
{

	// KeyInVal: for storing objects in Map that contain a key, but the full object is larger than the key. An object may either:
	// - implement a KeyPtr() method that returns the key either by value, or by const reference, or
	// - provide a specialization of KeyInVal with static GetKey method that retrieves the key for the object.
	
	template <class V>
	struct KeyInVal
	{
		using Val = V;
		static decltype(((V const*) nullptr)->Key()) GetKey(V const& v) noexcept { return v.Key();  }
	};
	
	template <class V> struct KeyInVal<Rp<V>>
	{
		using Val = Rp<V>;
		static decltype(((V const*) nullptr)->Key()) GetKey(Rp<V> const& v) noexcept { return v->Key(); }
	};


	// KeyIsVal: for storing objects in Map where the object value is the key.
	template <class V, typename = V> struct KeyIsVal;

	template <class V>
	struct KeyIsVal<V, std::enable_if_t<!std::is_scalar<V>::value, V>>
	{
		using Val = V;
		static V const& GetKey(V const& v) noexcept { return v; }
	};

	template <class V>
	struct KeyIsVal<V, std::enable_if_t< std::is_scalar<V>::value, V>>
	{
		using Val = V;
		static V GetKey(V v) noexcept { return v; }
	};


	template <class KT> struct MapKey : KT
	{
		using KeyOrRef = decltype(GetKey(*((KT::Val const*) nullptr)));
		using Key      = std::remove_const_t<std::remove_reference_t<KeyOrRef>>;
		using KeyOrPtr = std::conditional_t<std::is_reference<KeyOrRef>::value, Key const*, Key>;
	};

	template <class KeyOrPtr, class KeyOrRef, std::enable_if_t<std::is_reference<KeyOrRef>::value, int> = 0>
	inline KeyOrPtr GetKeyOrPtr_FromRef(KeyOrRef keyOrRef) noexcept { return &keyOrRef; }

	template <class KeyOrPtr, class KeyOrRef, std::enable_if_t<!std::is_reference<KeyOrRef>::value, int> = 0>
	inline KeyOrPtr GetKeyOrPtr_FromRef(KeyOrRef keyOrRef) noexcept { return keyOrRef; }

	template <class KeyOrRef, class KeyOrPtr, std::enable_if_t<std::is_reference<KeyOrRef>::value, int> = 0>
	inline KeyOrRef GetKeyOrRef_FromPtr(KeyOrPtr keyOrPtr) noexcept { return *keyOrPtr; }

	template <class KeyOrRef, class KeyOrPtr, std::enable_if_t<!std::is_reference<KeyOrRef>::value, int> = 0>
	inline KeyOrRef GetKeyOrRef_FromPtr(KeyOrPtr keyOrPtr) noexcept { return keyOrPtr; }


	template <class KT> struct MapCritLess : MapKey<KT>
	{
		static bool KeyEarlier(KeyOrRef a, KeyOrRef b) { return a <  b; }
		static bool KeyEqual  (KeyOrRef a, KeyOrRef b) { return a == b; }
	};

	template <class KT> struct MapCritGrtr : MapKey<KT>
	{
		static bool KeyEarlier(KeyOrRef a, KeyOrRef b) { return a > b; }
		static bool KeyEqual  (KeyOrRef a, KeyOrRef b) { return a == b; }
	};

	template <class KT> struct MapCritLessInsensitive : MapKey<KT>
	{
		static bool KeyEarlier(KeyOrRef a, KeyOrRef b) { return Seq(a).Compare(b, CaseMatch::Insensitive) <  0; }
		static bool KeyEqual  (KeyOrRef a, KeyOrRef b) { return Seq(a).Compare(b, CaseMatch::Insensitive) == 0; }
	};


	template <class B>
	class MapCore : public B
	{
	private:
		// Just... don't copy a map.
		MapCore(MapCore<B> const&) = delete;
		MapCore<B>& operator=(MapCore<B> const&) = delete;

		struct FindKeyResult;
		struct UpdateChildren;
		template <class E> struct NodeBase;
		struct LeafNode;
		struct NonLeafNode;
		union  Node;
		struct KeyAndNode;

	public:
		using Val      = typename B::Val;
		using KeyOrRef = typename B::KeyOrRef;
		using KeyOrPtr = typename B::KeyOrPtr;

		class It
		{
		public:
			using Val = typename B::Val;

			It() noexcept = default;
			It(It const&) noexcept = default;

			It(MapCore<B> const& map, Node const* node, sizet index) noexcept : m_ver(map.m_version), m_map((MapCore<B>*) &map), m_node((Node*) node), m_index(index) {}

			bool Valid () const noexcept { return m_map && m_map->m_version == m_ver; }
			bool Any   () const noexcept { return !!m_node; }

			Val* Ptr() const { EnsureThrow(Valid()); if (!Any()) return nullptr; return &m_node->u_leaf.m_entries[m_index]; }
			Val& Ref() const { Val* p { Ptr() }; EnsureThrow(p); return *p; }

			Val& operator*  () const { EnsureThrow(Valid() && Any()); return  m_node->u_leaf.m_entries[m_index]; }
			Val* operator-> () const { EnsureThrow(Valid() && Any()); return &m_node->u_leaf.m_entries[m_index]; }

			bool operator== (It const& x) const noexcept { return m_map && m_node == x.m_node && (!m_node || m_index == x.m_index); }
			bool operator!= (It const& x) const noexcept { return !operator==(x); }

			It& operator++ ()    { return Increment(); }
			It& operator-- ()    { return Decrement(); }
			It  operator++ (int) { It prev{*this}; Increment(); return prev; }
			It  operator-- (int) { It prev{*this}; Decrement(); return prev; }

			It& Increment()
			{
				EnsureThrow(Valid() && Any());
				sizet len { m_node->u_leaf.m_entries.Len() };
				EnsureAbort(m_index < len);
				if (m_index != len - 1)
					++m_index;
				else
				{
					m_node = m_node->NextSibling();		// If NextSibling() is nullptr, iterator becomes past-end
					m_index = 0;
				}
				return *this;
			}

			It& Decrement()
			{
				EnsureThrow(Valid());
				if (m_node)
				{
					if (m_index != 0)
						--m_index;
					else
					{
						m_node = m_node->PrevSibling();
						if (m_node)
							m_index = m_node->u_leaf.m_entries.Len() - 1;
						else
							m_map = nullptr;	// Attempt to decrement past beginning. Iterator becomes invalid
					}
				}
				else
				{
					// Map is not null, but node is null: past-end. Decrement to last value in map
					if (m_map->m_len)
					{
						Node* node = m_map->m_root;
						while (!node->IsLeaf())
							node = node->DescendLast();
						
						m_node = node;
						m_index = node->u_leaf.m_entries.Len() - 1;
					}
					else
						m_map = nullptr;		// Attempt to decrement past beginning. Iterator becomes invalid
				}
				return *this;
			}

		private:
			sizet       m_ver    {};
			MapCore<B>* m_map    {};			// If null, the iterator has an invalid (null) value which is not equal to anything
			Node*       m_node   {};			// If null, but m_map is not null, the iterator has a past-end value
			sizet       m_index  {};

			friend class At::MapCore<B>;
		};

		using ConstIt = At::ConstIt<It>;
	
		MapCore() = default;
		MapCore(MapCore&& x) noexcept : m_len(x.m_len), m_root(x.m_root) { x.m_version = SIZE_MAX; x.m_len = 0; x.m_root = nullptr; }
		~MapCore() noexcept { m_version = SIZE_MAX; NoExcept(delete m_root); m_root = nullptr; }

		void Clear() noexcept
		{
			++m_version;
			if (m_root)
			{
				m_root->Clear();
				m_len = 0;
			}
		}

		bool  Any() const noexcept { return m_len != 0; }
		sizet Len() const noexcept { return m_len; }

		It      begin()       noexcept { return TBegin<It>(); }
		ConstIt begin() const noexcept { return TBegin<ConstIt>(); }

		It      end()         noexcept { return It     (*this, nullptr, 0U); }
		ConstIt end()   const noexcept { return ConstIt(*this, nullptr, 0U); }

		Val&       First()       { return begin().Ref(); }
		Val const& First() const { return begin().Ref(); }

		Val&       Last()        { return end().Decrement().Ref(); }
		Val const& Last() const  { return end().Decrement().Ref(); }

		template <typename... Args>
		It Add(Args&&... args)
		{
			Val temp(std::forward<Args>(args)...);
			return Add(std::move(temp));
		}

		It Add(Val&& value)
		{
			KeyOrRef key { GetKey(value) };
			++m_version;

			if (!m_root)
				m_root = new Node(0, nullptr, 0);

			if (m_root->IsLeaf() && !m_root->u_leaf.m_entries.Any())
			{
				m_root->u_leaf.m_entries.Add(std::forward<Val>(value));
				++m_len;
				return It(*this, m_root, 0U);
			}

			// Find leaf node into which this value should be added
			Node* node { m_root };
			while (!node->IsLeaf())
			{
				NonLeafNode& nonLeaf { node->u_nonLeaf };
				sizet index;
				FindKeyResult::E fkr { nonLeaf.FindLastKeyLessThan(key, index) };
				if (fkr == FindKeyResult::Found)
					node = node->Descend(index);
				else
				{
					EnsureAbort(fkr != FindKeyResult::Empty);
					node = node->DescendFirst();
				}
			}

			return AddToLeafNode(node, key, std::forward<Val>(value));
		}

		template <typename... Args>
		It FindOrAdd(bool& added, Args&&... args)
		{
			Val temp(std::forward<Args>(args)...);
			return FindOrAdd(added, std::move(temp));
		}

		It FindOrAdd(bool& added, Val&& value)
		{
			if (!m_root || (m_root->IsLeaf() && !m_root->u_leaf.m_entries.Any()))
			{
				added = true;
				return Add(std::move(value));
			}

			KeyOrRef key { GetKey(value) };
			It lowerBound = LowerBound(key);

			Node* node;
			if (!lowerBound.Any())
			{
				node = m_root;
				while (!node->IsLeaf())
					node = node->DescendLast();
			}
			else if (KeyEqual(key, GetKey(*lowerBound)))
			{
				added = false;
				return lowerBound;
			}
			else
				node = lowerBound.m_node;

			added = true;
			return AddToLeafNode(node, key, std::forward<Val>(value));
		}

		bool Contains(KeyOrRef key) const noexcept { return Find(key).Any(); }

		// If no match is found, returns a past-end iterator where Valid() == true and Any() == false.
		It      Find(KeyOrRef key)       noexcept { return TFind<It>     (key); }
		ConstIt Find(KeyOrRef key) const noexcept { return TFind<ConstIt>(key); }

		// Returns an iterator pointing to the first entry not less than (= greater or equal to) the specified key.
		// If there is no such entry, returns a past-end iterator where Valid() == true and Any() == false.
		It      LowerBound(KeyOrRef key)       noexcept { return TLowerBound<It>     (key); }
		ConstIt LowerBound(KeyOrRef key) const noexcept { return TLowerBound<ConstIt>(key); }

		Range<It>      EqualRange(KeyOrRef key)       noexcept { return TEqualRange<It>     (key); }
		Range<ConstIt> EqualRange(KeyOrRef key) const noexcept { return TEqualRange<ConstIt>(key); }

		It Erase(It const& it)
		{
			EnsureThrow(it.Valid() && it.Any() && it.m_map == this);
			EnsureAbort(m_len > 0);

			++m_version;

			LeafNode& leaf { it.m_node->Leaf() };
			leaf.m_entries.Erase(it.m_index, 1);
			--m_len;

			Node* retNode {};
			sizet retIndex {};

			if (!leaf.m_parent)
			{
				// This is the root node - no maintenance to do
				retNode = it.m_node;
				retIndex = it.m_index;
			}
			else if (!leaf.m_entries.Any())
			{
				// Non-root leaf node that's now empty. Delete the node and remove it from its parent
				retNode = it.m_node->NextSibling();		// If this is null, we removed the last entry and return a past-end iterator
				RemoveNode(it.m_node, retNode);
			}
			else
			{
				// Non-root leaf node which remains non-empty
				if (it.m_index == 0)
				{
					// We removed this node's first entry. Update key in parent nodes
					PropagateFirstKeyAfterChange(*it.m_node, GetKey(leaf.m_entries.First()));
				}

				if (leaf.m_entries.Len() < LeafNodeJoinThreshold)
				{
					Node *joinNode1, *joinNode2;
					if (FindBestSiblingForJoin(it.m_node, MaxLeafEntries, joinNode1, joinNode2))
					{
						retNode = joinNode1;
						retIndex = it.m_index;

						if (it.m_node == joinNode2)
							retIndex += joinNode1->Leaf().m_entries.Len();	// The node from which we removed the entry is being appended to its previous sibling

						joinNode1->Leaf().MergeFrom(joinNode2->Leaf());
						RemoveNode(joinNode2, retNode);
					}
				}

				if (!retNode)
				{
					// We did not join nodes. The node from which we removed the entry remains
					retNode = it.m_node;
					retIndex = it.m_index;
				}
			}

			// Fix return iterator if it would point past the end of a node
			if (retNode)
			{
				sizet const retNodeLen = retNode->Leaf().m_entries.Len();
				if (retIndex >= retNodeLen)
				{
					EnsureThrowWithNr2(retIndex == retNodeLen, retIndex, retNodeLen);
					retNode = retNode->NextSibling();	// If this is null, we removed the last entry and return a past-end iterator
					retIndex = 0;
				}
			}

			return It(*this, retNode, retIndex);
		}
	
	private:
		enum
		{
			TargetNodeBytes          = 555,				// Testing indicates that 450 - 1000 are good node sizes, with optimum at around 550
			MaxLeafEntries           = PickMax<int>(TargetNodeBytes / sizeof(Val),        8),
			MaxNonLeafEntries        = PickMax<int>(TargetNodeBytes / sizeof(KeyAndNode), 8),
			LeafNodeJoinThreshold    = MaxLeafEntries    / 4,
			NonLeafNodeJoinThreshold = MaxNonLeafEntries / 4,
		};

		static KeyOrRef GetEntryKey(Val const&        v  ) { return GetKey(v); }
		static KeyOrRef GetEntryKey(KeyAndNode const& kan) { return GetKeyOrRef_FromPtr<KeyOrRef, KeyOrPtr>(kan.m_keyOrPtr); }

		struct FindKeyResult { enum E { Found, Empty, FirstKeyEqual, FirstKeyGreater }; };
		struct UpdateChildren { enum E { No, Yes }; };

		template <class E>
		struct NodeBase
		{
			uint const m_height;						// Must be in same location for both node types
			uint m_indexInParent;						// Must be in same location for both node types
			Node* m_parent;								// Must be in same location for both node types
			Vec<E> m_entries;

			NodeBase(uint height, Node* parent, sizet indexInParent) noexcept
				: m_height(height)
				, m_indexInParent((uint) indexInParent)
				, m_parent(parent) {}

			NodeBase(NodeBase&& x) noexcept
				: m_height(x.m_height)
				, m_indexInParent(x.m_indexInParent)
				, m_parent(x.m_parent)
				, m_entries(std::move(x.m_entries)) {}

			~NodeBase() noexcept { m_indexInParent = UINT_MAX; m_parent = nullptr; }

			typename FindKeyResult::E FindLastKeyLessThan(KeyOrRef key, sizet& index)
			{
				index = SIZE_MAX;
				if (!m_entries.Any())
					return FindKeyResult::Empty;

			#if 1
				for (sizet i=0; i!=m_entries.Len(); ++i)
				{
					KeyOrRef entryKey { GetEntryKey(m_entries[i]) };
					if (!(KeyEarlier(entryKey, key)))
					{
						if (i != 0) { index = i-1; return FindKeyResult::Found; }
						if (KeyEqual(key, entryKey)) return FindKeyResult::FirstKeyEqual;
						return FindKeyResult::FirstKeyGreater;
					}
				}

				index = m_entries.Len() - 1;
				return FindKeyResult::Found;
			#else
				// Binary search within the node is a non-optimization.
				// Below code works, but performs no better than a linear search, or does worse with small node sizes that are most efficient to use, anyway.
				sizet lo {};
				sizet hi { m_entries.Len() };
				while (true)
				{
					sizet mid { (hi + lo) / 2 };
					KeyOrRef entryKey { GetEntryKey(m_entries[mid]) };
					if (KeyEarlier(entryKey, key))
					{
						lo = mid;
						if (hi == lo + 1)
						{
							index = lo;
							return FindKeyResult::Found;
						}
					}
					else
					{
						hi = mid;
						if (hi == lo)
						{
							EnsureAbort(lo == 0);
							if (KeyEqual(key, entryKey))
								return FindKeyResult::FirstKeyEqual;
							else
								return FindKeyResult::FirstKeyGreater;				
						}
					}
				}
			#endif
			}
		};

		struct LeafNode : NodeBase<Val>
		{
			LeafNode(Node* p, sizet i) noexcept : NodeBase<Val>(0, p, i) { m_entries.FixCap(MaxLeafEntries); }

			void SplitInto(sizet i, LeafNode& leafTo)   { m_entries.SplitInto(i, leafTo  .m_entries); }
			void MergeFrom(         LeafNode& leafFrom) { m_entries.MergeFrom(   leafFrom.m_entries); }
		};

		struct NonLeafNode : NodeBase<KeyAndNode>
		{
			NonLeafNode(uint h, Node* p, sizet i) noexcept : NodeBase<KeyAndNode>(h, p, i) { m_entries.FixCap(MaxNonLeafEntries); }

			void UpdateChildParentInfo(sizet fromIndex) noexcept
			{
				for (sizet i=fromIndex; i!=m_entries.Len(); ++i)
					m_entries[i].m_node.SetParent((Node*) this, i);
			}

			void SplitInto(sizet i, NonLeafNode& other) { sizet origLen{other.m_entries.Len()}; m_entries.SplitInto(i, other.m_entries); other.UpdateChildParentInfo(origLen); }
			void MergeFrom(         NonLeafNode& other) { sizet origLen{      m_entries.Len()}; m_entries.MergeFrom(   other.m_entries);       UpdateChildParentInfo(origLen); }
		};

		union Node
		{
			LeafNode u_leaf;
			NonLeafNode u_nonLeaf;

			Node(uint height, Node* parent, sizet indexInParent) noexcept : u_nonLeaf(height, parent, indexInParent)
			{
				if (height == 0)
					new (&u_leaf) LeafNode(parent, indexInParent);
				else
					new (&u_nonLeaf) NonLeafNode(height, parent, indexInParent); 
			}
			
			Node(Node&& x) noexcept { MoveFrom_Uninitialized(std::forward<Node>(x), UpdateChildren::Yes); }
			Node(Node&& x, typename UpdateChildren::E updateChildren) noexcept { MoveFrom_Uninitialized(std::forward<Node>(x), updateChildren); }
			~Node() noexcept { Destroy(); }

			void Clear() noexcept
			{
				EnsureAbort(!Parent());
				Destroy();
				new (&u_leaf) LeafNode(nullptr, 0);
			}

			void MoveFrom(Node&& x, typename UpdateChildren::E updateChildren) noexcept { Destroy(); MoveFrom_Uninitialized(std::forward<Node>(x), updateChildren); }

			uint Height() const noexcept { return u_leaf.m_height; }
			Node* Parent() const noexcept { return u_leaf.m_parent; }
			uint IndexInParent() const noexcept { return u_leaf.m_indexInParent; }
			void SetParent(Node* parent, sizet indexInParent) noexcept { u_leaf.m_indexInParent = (uint) indexInParent; u_leaf.m_parent = parent; }

			bool IsLeaf() const noexcept { return u_leaf.m_height == 0; }
			LeafNode& Leaf() noexcept { EnsureAbort(IsLeaf()); return u_leaf; }
			NonLeafNode& NonLeaf() noexcept { EnsureAbort(!IsLeaf()); return u_nonLeaf; }
			uint NrEntries() const noexcept { if (IsLeaf()) return (uint) u_leaf.m_entries.Len(); return (uint) u_nonLeaf.m_entries.Len(); }

			Node* DescendFirst() noexcept   { Node& n { NonLeaf().m_entries.First().m_node }; EnsureAbort(n.Parent() == this); return &n; }
			Node* DescendLast() noexcept    { Node& n { NonLeaf().m_entries.Last() .m_node }; EnsureAbort(n.Parent() == this); return &n; }
			Node* Descend(sizet i) noexcept { Node& n { NonLeaf().m_entries[i]     .m_node }; EnsureAbort(n.Parent() == this); return &n; }
			Node* Ascend() noexcept { return u_leaf.m_parent; }

			Node* PrevSibling() noexcept
			{
				// Go up as far as necessary
				sizet indexInParent { IndexInParent() };
				Node* node { Parent() };
				while (node && !indexInParent)
				{
					indexInParent = node->IndexInParent();
					node = node->Parent();
				}

				if (node)	// If null, there is no previous sibling
				{
					// Go down to preceding node of same height
					node = node->Descend(indexInParent - 1);
					while (node->Height() != Height())
						node = node->DescendLast();
				}

				return node;
			}
			
			Node* NextSibling() noexcept
			{
				sizet indexInParent { IndexInParent() };
				Node* node { Parent() };
				while (node && indexInParent == node->NonLeaf().m_entries.Len() - 1)
				{
					indexInParent = node->IndexInParent();
					node = node->Parent();
				}

				if (node)	// If null, there is no next sibling
				{
					// Go down to preceding node of same height
					node = node->Descend(indexInParent + 1);
					while (node->Height() != Height())
						node = node->DescendFirst();
				}

				return node;
			}

			void SplitInto(sizet i, Node& other)
			{
				if (IsLeaf())
					u_leaf.SplitInto(i, other.Leaf());
				else
					u_nonLeaf.SplitInto(i, other.NonLeaf());
			}

		private:
			void Destroy() noexcept
			{
				if (IsLeaf())
					u_leaf.~LeafNode();
				else
					u_nonLeaf.~NonLeafNode();
			}
			
			void MoveFrom_Uninitialized(Node&& x, typename UpdateChildren::E updateChildren) noexcept
			{
				if (x.IsLeaf())
					new (&u_leaf) LeafNode(std::move(x.u_leaf));
				else
				{
					new (&u_nonLeaf) NonLeafNode(std::move(x.u_nonLeaf));
					if (updateChildren != UpdateChildren::No)
						u_nonLeaf.UpdateChildParentInfo(0);
				}
			}
		};

		static_assert(offsetof(Node, u_leaf   ) == 0, "Offset of u_leaf in Node must be zero");
		static_assert(offsetof(Node, u_nonLeaf) == 0, "Offset of u_nonLeaf in Node must be zero");

		struct KeyAndNode
		{
			static_assert(std::is_nothrow_default_constructible<KeyOrPtr>::value &&
			              std::is_nothrow_copy_assignable<KeyOrPtr>::value &&
			              std::is_nothrow_destructible<KeyOrPtr>::value, "If GetKey() does not return a reference, the key must meet exception safety requirements");

			KeyAndNode(uint height, Node* parent, sizet indexInParent) noexcept : m_node(height, parent, indexInParent) {}

			KeyOrPtr m_keyOrPtr;
			Node m_node;

			void SetKey(KeyOrRef newKey) noexcept { m_keyOrPtr = GetKeyOrPtr_FromRef<KeyOrPtr, KeyOrRef>(newKey); }

			void SetKeyFromNode() noexcept
			{
				if (m_node.IsLeaf())
					SetKey(GetKey(m_node.u_leaf.m_entries.First()));
				else
					m_keyOrPtr = m_node.u_nonLeaf.m_entries.First().m_keyOrPtr;
			}
		};

		sizet m_version {};			// Incremented to invalidate iterators when tree content changes
		sizet m_len     {};
		Node* m_root    {};

		template <class I>
		I TBegin() const noexcept
		{
			if (!m_len)
				return I(*this, nullptr, 0U);
			
			Node* node { m_root };
			while (!node->IsLeaf())
				node = node->DescendFirst();

			return I(*this, node, 0U);
		}

		void PropagateFirstKeyAfterChange(Node& nodeChanged, KeyOrRef newFirstKey) noexcept
		{
			Node* node { &nodeChanged };
			while (node->Parent())
			{
				Node* parent { node->Parent() };
				sizet indexInParent { node->IndexInParent() };
				KeyAndNode& kan { parent->NonLeaf().m_entries[indexInParent] };

				EnsureAbort(&kan.m_node == node);
				kan.SetKey(newFirstKey);

				if (indexInParent != 0)
					break;

				node = parent;
			}
		}

		bool CheckSplitLeafNode(Node*& node, Node*& newNode)
		{
			if (node->NrEntries() < MaxLeafEntries)
				return false;

			if (!node->Parent())
				SplitRootNode(node, newNode);
			else
				SplitNonRootNode(node, newNode);

			return true;
		}

		bool CheckSplitNonLeafNode(Node*& node, Node*& newNode)
		{
			if (node->NrEntries() < MaxNonLeafEntries)
				return false;

			if (!node->Parent())
				SplitRootNode(node, newNode);
			else
				SplitNonRootNode(node, newNode);

			return true;
		}

		bool CheckSplitParentOfNode(Node*& node, Node*& parent)
		{
			EnsureAbort(parent == node->Parent());

			uint indexInParent { node->IndexInParent() };
			Node* newParent;

			if (!CheckSplitNonLeafNode(parent, newParent))
				return false;

			uint nrParentEntries { parent->NrEntries() };
			if (indexInParent >= nrParentEntries)
			{
				indexInParent -= nrParentEntries;
				parent = newParent;
			}

			node = &(parent->NonLeaf().m_entries[indexInParent].m_node);
			return true;
		}

		void SplitRootNode(Node*& node, Node*& newNode)
		{
			Node tempRoot { node->Height() + 1, nullptr, 0 };
			Vec<KeyAndNode>& tempRootEntries { tempRoot.NonLeaf().m_entries };
			tempRootEntries.Add(node->Height(), nullptr, 0U);
			tempRootEntries.Add(node->Height(), nullptr, 0U);
			node->SplitInto(node->NrEntries()/2, tempRootEntries[1].m_node);

			// If there was an exception up to this point, there is no change. From this point on, no exceptions until we complete the split
			tempRootEntries[0].m_node.MoveFrom(std::move(*m_root), UpdateChildren::Yes);
			m_root->MoveFrom(std::move(tempRoot), UpdateChildren::Yes);

			NonLeafNode& rootNonLeaf { m_root->NonLeaf() };
			rootNonLeaf.m_entries[0].SetKeyFromNode();
			rootNonLeaf.m_entries[1].SetKeyFromNode();
			rootNonLeaf.UpdateChildParentInfo(0);
					
			node    = &rootNonLeaf.m_entries[0].m_node;
			newNode = &rootNonLeaf.m_entries[1].m_node;
		}

		void SplitNonRootNode(Node*& node, Node*& newNode)
		{
			Node* parent { node->Parent() };
			CheckSplitParentOfNode(node, parent);

			KeyAndNode tempKan { node->Height(), nullptr, 0 };
			if (tempKan.m_node.IsLeaf())
				tempKan.m_node.u_leaf.m_entries.ReserveExact(MaxLeafEntries);
			else
				tempKan.m_node.u_nonLeaf.m_entries.ReserveExact(MaxNonLeafEntries);
				
			// If there was an exception up to this point, there is no change. From this point on, no exceptions until we complete the split
			uint newIndexInParent { node->IndexInParent() + 1 };
			NonLeafNode& parentNonLeaf { parent->NonLeaf() };
			parentNonLeaf.m_entries.Insert(newIndexInParent, std::move(tempKan));
			parentNonLeaf.UpdateChildParentInfo(newIndexInParent);

			KeyAndNode& kanInParent { parentNonLeaf.m_entries[newIndexInParent] };
			node->SplitInto(node->NrEntries()/2, kanInParent.m_node);
			kanInParent.SetKeyFromNode();

			newNode = &kanInParent.m_node;
		}

		It AddToLeafNode(Node* node, KeyOrRef key, Val&& value)
		{
			// Should the leaf node be split?
			{
				Node* newNode;
				if (CheckSplitLeafNode(node, newNode))
					if (!(KeyEarlier(key, GetKey(newNode->Leaf().m_entries.First()))))
						node = newNode;
			}

			// Insert value into leaf node
			LeafNode& leaf = node->Leaf();
			sizet index;
			if (node->Leaf().FindLastKeyLessThan(key, index) == FindKeyResult::Found)
				++index;
			else
				index = 0;

			leaf.m_entries.Insert(index, std::forward<Val>(value));

			// If there was an exception up to this point, there is no change. From this point on, no exceptions
			if (index == 0)
			{
				// See if first key changed
				EnsureAbort(leaf.m_entries.Len() > 1);
				KeyOrRef keyFirst { GetKey(leaf.m_entries.First()) };
				KeyOrRef keySecond { GetKey(leaf.m_entries[1]) };
				if (!KeyEqual(keyFirst, keySecond))
				{
					EnsureAbort(KeyEarlier(keyFirst, keySecond));
					PropagateFirstKeyAfterChange(*node, GetKey(leaf.m_entries.First()));
				}
			}

			++m_len;
			return It(*this, node, index);
		}

		template <class I>
		I TFind(KeyOrRef key) const noexcept
		{
			// Not implemented in terms of TLowerBound because TLowerBound will always descend to leaf node, but TFind can return failure earlier.
			if (m_len)
			{
				Node* node { m_root };
				while (!node->IsLeaf())
				{
					sizet index;
					FindKeyResult::E fkr { node->u_nonLeaf.FindLastKeyLessThan(key, index) };
					if (fkr == FindKeyResult::Found)
						node = node->Descend(index);
					else if (fkr == FindKeyResult::FirstKeyEqual)
						node = node->DescendFirst();
					else
					{
						EnsureAbort(fkr == FindKeyResult::FirstKeyGreater);
						return I(*this, nullptr, 0U);
					}
				}

				sizet index;
				FindKeyResult::E fkr { node->u_leaf.FindLastKeyLessThan(key, index) };
				if (fkr == FindKeyResult::FirstKeyEqual)
					return I(*this, node, 0U);
				if (fkr == FindKeyResult::Found)
				{
					I it(*this, node, index);
					++it;
					if (it.Any() && KeyEqual(GetKey(*it), key))
						return it;
				}
				EnsureAbort(fkr != FindKeyResult::Empty);
			}

			return I(*this, nullptr, 0U);
		}

		template <class I>
		I TLowerBound(KeyOrRef key) const noexcept
		{
			if (!m_len)
				return I(*this, nullptr, 0U);

			Node* node { m_root };
			while (!node->IsLeaf())
			{
				sizet index;
				FindKeyResult::E fkr { node->u_nonLeaf.FindLastKeyLessThan(key, index) };
				if (fkr == FindKeyResult::Found)
					node = node->Descend(index);
				else
				{
					EnsureAbort(fkr == FindKeyResult::FirstKeyEqual || fkr == FindKeyResult::FirstKeyGreater);
					node = node->DescendFirst();
				}
			}

			sizet index;
			FindKeyResult::E fkr { node->u_leaf.FindLastKeyLessThan(key, index) };
			if (fkr == FindKeyResult::Found)
			{
				I it(*this, node, index);
				++it;
				return it;
			}
			else
			{
				EnsureAbort(fkr == FindKeyResult::FirstKeyEqual || fkr == FindKeyResult::FirstKeyGreater);
				return I(*this, node, 0U);
			}
		}

		template <class I>
		Range<I> TEqualRange(KeyOrRef key) const noexcept
		{
			I first { TFind<I>(key) };
			if (!first.Any())
				return Range<I>(first, first);

			I last { first };
			do ++last;
			while (last.Any() && KeyEqual(key, GetKey(*last)));

			return Range<I>(first, last);
		}

		bool FindBestSiblingForJoin(Node* node, sizet maxEntries, Node*& joinNode1, Node*& joinNode2) noexcept
		{
			Node* prevSibling { node->PrevSibling() };
			Node* nextSibling { node->NextSibling() };
			EnsureAbort(prevSibling || nextSibling);

			sizet prevSiblingEntries { If(!prevSibling, sizet, SIZE_MAX, prevSibling->NrEntries()) };
			sizet nextSiblingEntries { If(!nextSibling, sizet, SIZE_MAX, nextSibling->NrEntries()) };

			if (prevSiblingEntries < nextSiblingEntries)	// If true, prevSiblingEntries can't be SIZE_MAX
			{
				if (prevSiblingEntries + node->NrEntries() <= maxEntries)
				{
					joinNode1 = prevSibling;				// Join node with its previous sibling
					joinNode2 = node;
					return true;
				}
			}
			else											// nextSiblingEntries can't be SIZE_MAX
			{
				if (node->NrEntries() + nextSiblingEntries <= maxEntries)
				{
					joinNode1 = node;						// Join node with its next sibling
					joinNode2 = nextSibling;
					return true;
				}
			}

			return false;
		}

		void RemoveNode(Node* removeNode, Node*& preserveNodePtr) noexcept
		{
			do
			{
				sizet indexInParent { removeNode->IndexInParent() };
				Node* parent { removeNode->Parent() };
				NonLeafNode& parentNonLeaf { parent->NonLeaf() };
				EnsureAbort(&parentNonLeaf.m_entries[indexInParent].m_node == removeNode);

				sizet preserveNodeIndex = SIZE_MAX;
				if (preserveNodePtr && preserveNodePtr->Parent() == parent)
				{
					preserveNodeIndex = preserveNodePtr->IndexInParent();
					EnsureAbort(preserveNodeIndex != indexInParent);
				}

				parentNonLeaf.m_entries.Erase(indexInParent, 1);
				parentNonLeaf.UpdateChildParentInfo(indexInParent);
				removeNode = nullptr;

				if ((SIZE_MAX != preserveNodeIndex) && (preserveNodeIndex > indexInParent))
				{
					--preserveNodeIndex;
					preserveNodePtr = &(parentNonLeaf.m_entries[preserveNodeIndex].m_node);
				}

				if (!parentNonLeaf.m_entries.Any())					// Node now empty?
				{
					if (parentNonLeaf.m_parent)						// Node not root?
						removeNode = parent;						// Delete the node and remove it from its parent
				}
				else if (!parentNonLeaf.m_parent)					// Node is root?
				{
					if (parentNonLeaf.m_entries.Len() == 1)			// Root node with only one child node remaining?
					{
						// Move child node into root
						Node tempRoot { std::move(parentNonLeaf.m_entries.First().m_node), UpdateChildren::No };
						m_root->MoveFrom(std::move(tempRoot), UpdateChildren::Yes);
						m_root->SetParent(nullptr, 0);

						if (SIZE_MAX != preserveNodeIndex)
						{
							EnsureAbort(!preserveNodeIndex);
							preserveNodePtr = m_root;
						}
					}
				}
				else
				{
					if (indexInParent == 0)																			// Removed this node's first entry?
						PropagateFirstKeyAfterChange(*parent, GetEntryKey(parentNonLeaf.m_entries.First()));		// Update key in parent nodes

					if (parentNonLeaf.m_entries.Len() < NonLeafNodeJoinThreshold)
					{
						Node *joinNode1, *joinNode2;
						if (FindBestSiblingForJoin(parent, MaxNonLeafEntries, joinNode1, joinNode2))
						{
							preserveNodeIndex = SIZE_MAX;
							if (preserveNodePtr)
								if ((preserveNodePtr->Parent() == joinNode1) || (preserveNodePtr->Parent() == joinNode2))
								{
									preserveNodeIndex = preserveNodePtr->IndexInParent();
									if (preserveNodePtr->Parent() == joinNode2)
										preserveNodeIndex += joinNode1->NonLeaf().m_entries.Len();
								}

							joinNode1->NonLeaf().MergeFrom(joinNode2->NonLeaf());
							removeNode = joinNode2;

							if (SIZE_MAX != preserveNodeIndex)
								preserveNodePtr = &(joinNode1->NonLeaf().m_entries[preserveNodeIndex].m_node);
						}
					}
				}
			}
			while (removeNode);
		}
	};

	template <class V> class Map         : public MapCore<MapCritLess<KeyInVal<V>>> {};
	template <class V> class MapR        : public MapCore<MapCritGrtr<KeyInVal<V>>> {};
	template <class V> class OrderedSet  : public MapCore<MapCritLess<KeyIsVal<V>>> {};
	template <class V> class OrderedSetR : public MapCore<MapCritGrtr<KeyIsVal<V>>> {};

}
