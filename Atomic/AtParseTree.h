#pragma once

#include "AtParseNode.h"

namespace At
{

	class ParseTree : NoCopy
	{
	public:
		ParseTree(Seq srcText);
		~ParseTree();

		ParseTree& RecordBestToStack ()                 { m_recordBestToStack = true; return *this; }
		ParseTree& SetTabStop        (uint tabStop)     { EnsureThrow(tabStop >= 1); m_tabStop = tabStop; return *this; }
		ParseTree& SetFlag           (Ruid const& flag) { if (!m_flags.Contains(&flag)) m_flags.Add(&flag); return *this; }

		sizet ApplyTab (sizet col)         const { EnsureThrow(col >= 1); return ((((col - 1) / m_tabStop) + 1) * m_tabStop) + 1; }
		bool  HaveFlag (Ruid const& flag) const { return m_flags.Contains(&flag); }

		enum ParseBehavior { ParseAll, AllowPartial };
		bool Parse(ParseFunc parseFunc, ParseBehavior behavior = ParseAll);

		enum EBestAttempt { BestAttempt };
		void EncObj(Enc& enc, EBestAttempt) const;

		Seq   BestRemaining () const { return m_bestRemaining; }
		sizet BestToRow     () const { return m_bestToRow; }
		sizet BestToCol     () const { return m_bestToCol; }

		bool HaveRoot() const { return m_firstBucket != nullptr && m_firstBucket->m_nodesUsed > 0; }

		ParseNode&       Root()       { EnsureThrow(HaveRoot()); return *(m_firstBucket->NodePtrAt(0)); }
		ParseNode const& Root() const { EnsureThrow(HaveRoot()); return *(m_firstBucket->NodePtrAt(0)); }

	private:
		struct Bucket
		{
			enum { ElemsPerNode = (sizeof(ParseNode) + sizeof(sizet) - 1) / sizeof(sizet),
				   NodeStorageBytesMax = 4000,
				   NrNodesMax = NodeStorageBytesMax / (ElemsPerNode * sizeof(sizet)),
				   NodeStorageElems = NrNodesMax * ElemsPerNode };

			bool ContainsNode(ParseNode const* p) { sizet* ps = (sizet*) p; return (ps >= m_nodeStorage) && (ps < (m_nodeStorage + NodeStorageElems)); }

			ParseNode*       NodePtrAt(sizet i)       { return (ParseNode*) (m_nodeStorage + (i * ElemsPerNode)); }
			ParseNode const* NodePtrAt(sizet i) const { return (ParseNode*) (m_nodeStorage + (i * ElemsPerNode)); }

			ParseNode* AddUnconstructedNode();
			void DiscardNodesFrom(ParseNode* p);

			sizet m_nodesUsed {};
			sizet m_nodeStorage[NodeStorageElems];
			Bucket* m_prevBucket {};
		};

		uint             m_tabStop { 4 };
		Vec<Ruid const*> m_flags;

		Bucket*		m_firstBucket;
		Bucket*     m_lastBucket;

		Seq			m_bestRemaining;
		sizet		m_bestToRow;
		sizet		m_bestToCol;

		struct BestToStackEntry
		{
			Ruid const* m_type;
			sizet m_startRow;
			sizet m_startCol;
		};

		bool m_recordBestToStack;
		Vec<BestToStackEntry> m_bestToStack;

		ParseNode* NewNode(ParseNode& parent, Ruid const& type);
		void FailNode(ParseNode* p);
		void DiscardNode(ParseNode* p);

		friend class ParseNode;
	};


	// Cannot be defined at declaration because ParseTree not yet defined
	inline Seq  ParseNode::BeforeSrc       () const { byte const* p { m_tree.Root().m_start.p }; return Seq(p, NumCast<sizet>(m_start.p     - p)); }
	inline Seq  ParseNode::BeforeRemaining () const { byte const* p { m_tree.Root().m_start.p }; return Seq(p, NumCast<sizet>(m_remaining.p - p)); }

	inline bool ParseNode::FailChild       (ParseNode* child) { m_tree.FailNode(child); return false; }
	inline void ParseNode::DiscardChild    (ParseNode* child) { m_tree.DiscardNode(child); }

}
