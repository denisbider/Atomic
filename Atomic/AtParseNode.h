#pragma once

#include "AtAscii.h"
#include "AtException.h"
#include "AtPinStore.h"
#include "AtRpVec.h"
#include "AtRuid.h"
#include "AtUnicode.h"
#include "AtUtf8.h"


namespace At
{

	class ParseNode;
	class ParseTree;

	typedef bool (*ParseFunc)(ParseNode&);

	DECL_RUID(Root)
	DECL_RUID(Append)
	DECL_RUID(Discard)

	class ParseNode
	{
	public:
		struct Err          : StrErr { Err          (Seq s) : StrErr(s) {} };
		struct NodeNotFound : Err    { NodeNotFound ()      : Err("Expected parser node not found") {} };

	public:
		// Interface for end users
		sizet StartRow () const noexcept { return m_startRow; }
		sizet StartCol () const noexcept { return m_startCol; }
		sizet ToRow    () const noexcept { return m_toRow; }
		sizet ToCol    () const noexcept { return m_toCol; }

		enum ETagRowCol { TagRowCol };
		void EncObj(Enc& enc, ETagRowCol) const { enc.Add(m_type->Tag()).Add(" at row ").UInt(m_startRow).Add(", column ").UInt(m_startCol); }

		void Dump(Enc& enc, sizet indent = 0) const;

		Ruid const& Type   ()              const noexcept { return *m_type; }
		bool        IsType (Ruid const& x) const noexcept { return m_type->Is(x); }
	
		Seq BeforeSrc           () const;		// defined inline in AtParseTree.h
		Seq BeforeRemaining     () const;		// defined inline in AtParseTree.h
		Seq SrcText             () const          { return Seq(m_start.p, NumCast<sizet>(m_remaining.p - m_start.p)); }
		Seq SrcTextAndRemaining () const noexcept { return m_start; }
	
		bool HasValue () const noexcept { return !!m_value.n; }
		Seq  Value    () const noexcept { return m_value; }

		ParseNode const* Parent () const noexcept { return m_parent; } 

		bool HaveChildren    () const noexcept { return m_firstChild  != nullptr; }
		bool HaveNextSibling () const noexcept { return m_nextSibling != nullptr; }

		ParseNode const& FirstChild  () const { EnsureThrow(m_firstChild  != nullptr); return *m_firstChild;  }
		ParseNode const& LastChild   () const { EnsureThrow(m_lastChild   != nullptr); return *m_lastChild;   }
		ParseNode const& NextSibling () const { EnsureThrow(m_nextSibling != nullptr); return *m_nextSibling; }

		struct ChildIt
		{
			ChildIt(ParseNode const* node) noexcept : m_node(node) {}
			ChildIt& operator++ () { EnsureThrow(m_node != nullptr); m_node = m_node->m_nextSibling; return *this; }
			bool operator== (ChildIt const& it) const noexcept { return m_node == it.m_node; }
			bool operator!= (ChildIt const& it) const noexcept { return m_node != it.m_node; }
			ParseNode const& operator* () const { EnsureThrow(m_node != nullptr); return *m_node; }
		private:
			ParseNode const* m_node;
		};

		ChildIt begin() const noexcept { return ChildIt(m_firstChild); }
		ChildIt end()   const noexcept { return ChildIt(nullptr); }
	
		ParseNode const* FindAncestor     (Ruid const* const* types) const;
		ParseNode const* FlatFindFirstOf  (Ruid const* const* types) const;
		ParseNode const* DeepFindFirstOf  (Ruid const* const* types) const;
		ParseNode const* FrontFindFirstOf (Ruid const* const* types) const;

		ParseNode const* FindAncestor     (Ruid const& id) const { Ruid const* ids[] = { &id, 0 }; return FindAncestor    (ids); }
		ParseNode const* FlatFind         (Ruid const& id) const { Ruid const* ids[] = { &id, 0 }; return FlatFindFirstOf (ids); }
		ParseNode const* DeepFind         (Ruid const& id) const { Ruid const* ids[] = { &id, 0 }; return DeepFindFirstOf (ids); }
		ParseNode const* FrontFind        (Ruid const& id) const { Ruid const* ids[] = { &id, 0 }; return FrontFindFirstOf(ids); }

		ParseNode const& FindAncestorRef  (Ruid const& id) const { ParseNode const* node = FindAncestor(id); if (!node) throw NodeNotFound(); return *node; }
		ParseNode const& FlatFindRef      (Ruid const& id) const { ParseNode const* node = FlatFind    (id); if (!node) throw NodeNotFound(); return *node; }
		ParseNode const& DeepFindRef      (Ruid const& id) const { ParseNode const* node = DeepFind    (id); if (!node) throw NodeNotFound(); return *node; }
		ParseNode const& FrontFindRef     (Ruid const& id) const { ParseNode const* node = FrontFind   (id); if (!node) throw NodeNotFound(); return *node; }

		ParseNode const* FindAncestor     (Ruid const& id1, Ruid const& id2) const { Ruid const* ids[] = { &id1, &id2, 0 }; return FindAncestor    (ids); }
		ParseNode const* FlatFindFirstOf  (Ruid const& id1, Ruid const& id2) const { Ruid const* ids[] = { &id1, &id2, 0 }; return FlatFindFirstOf (ids); }
		ParseNode const* DeepFindFirstOf  (Ruid const& id1, Ruid const& id2) const { Ruid const* ids[] = { &id1, &id2, 0 }; return DeepFindFirstOf (ids); }
		ParseNode const* FrontFindFirstOf (Ruid const& id1, Ruid const& id2) const { Ruid const* ids[] = { &id1, &id2, 0 }; return FrontFindFirstOf(ids); }

	public:
		// Interface for parser functions
		ParseTree const& Tree() const { return m_tree; }

		bool HaveByte()          const { return m_remaining.n > 0; }
		bool HaveFollowingByte() const { return m_remaining.n > 1; }
		byte CurByte()           const { EnsureThrow(m_remaining.n > 0); return m_remaining.p[0]; }
		byte FollowingByte()     const { EnsureThrow(m_remaining.n > 1); return m_remaining.p[1]; }
		Seq  Remaining()         const { return m_remaining; }

		void ConsumeByte     ()                         { EnsureThrow(m_remaining.n); UpdateRowCol(m_remaining.p[0]); AddToValue(1); }
		void ConsumeUtf8Char (uint c, sizet encodedLen) { EnsureThrow(m_remaining.n >= encodedLen); UpdateRowCol(c); AddToValue(encodedLen); }
		void ConsumeUtf8Seq  (sizet nrBytesExact);

		void RefineType(Ruid const& newType);
		void RefineParentType(Ruid const& parentType, Ruid const& newType);

		ParseNode* NewChild(Ruid const& type);	// Returns nullptr if MaxDepth exceeded
		bool CommitChild  (ParseNode* child);	// Always returns true
		bool FailChild    (ParseNode* child);	// Always returns false. Defined inline after ParseTree
		void DiscardChild (ParseNode* child);						  // Defined inline after ParseTree

	private:
		// IMPORTANT:
		// OBJECT MUST NOT HOLD RESOURCES - DESTRUCTOR IS NOT CALLED

		ParseTree&			m_tree;
		Ruid const*			m_type;
		sizet				m_depth       {};
		ParseNode*			m_parent      {};
		ParseNode*			m_nextSibling {};

		// Input
		Seq					m_start;
		Seq					m_remaining;
		sizet				m_startRow, m_toRow;
		sizet				m_startCol, m_toCol;
		bool				m_committed {};
	
		// Value (V-type parsers)
		Seq					m_value;

		// Children (C-type parsers)
		ParseNode*			m_firstChild {};
		ParseNode*			m_lastChild  {};

		ParseNode(ParseTree& tree, Seq srcText);
		ParseNode(ParseNode& parent, Ruid const& type);

		void UpdateRowCol(uint c);
		void AddToValue(sizet nrBytes);

		friend class ParseTree;
	};

}
