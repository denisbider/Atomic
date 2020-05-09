#include "AtIncludes.h"
#include "AtParse.h"

namespace At
{
	// ParseTree::Bucket

	ParseNode* ParseTree::Bucket::AddUnconstructedNode()
	{
		EnsureThrow(m_nodesUsed != NrNodesMax);
		return NodePtrAt(m_nodesUsed++);
	}


	void ParseTree::Bucket::DiscardNodesFrom(ParseNode* p)
	{
		sizet* ps = (sizet*) p;
		EnsureAbort(ps >= m_nodeStorage && ps <= m_nodeStorage + (m_nodesUsed * ElemsPerNode));
		m_nodesUsed = ((sizet) (ps - m_nodeStorage)) / ElemsPerNode;
	}



	// ParseTree

	ParseTree::ParseTree(Seq srcText)
	{
		m_firstBucket = m_lastBucket = new Bucket;
		new (m_firstBucket->AddUnconstructedNode()) ParseNode(*this, srcText);

		m_bestRemaining = srcText;
		m_bestToRow = 1;
		m_bestToCol = 1;

		m_recordBestToStack = false;
	}

	ParseTree::~ParseTree() noexcept
	{
		for (Bucket* b=m_lastBucket; b!=0; )
		{
			Bucket* d = b;
			b = b->m_prevBucket;
			NoExcept(delete d);
		}

		m_firstBucket = 0;
		m_lastBucket = 0;
	}


	bool ParseTree::Parse(ParseFunc parseFunc, ParseBehavior behavior)
	{
		EnsureThrow(HaveRoot() && "No root node");
		EnsureThrow(!Root().m_committed && "Already committed");

		if (!parseFunc(Root()))
			return false;
		if (!HaveRoot())
			return false;
		if (ParseAll == behavior && Root().m_remaining.n != 0)
			return false;

		Root().m_committed = true;
		return true;
	}


	void ParseTree::EncObj(Enc& enc, EBestAttempt) const
	{
		enc.Add("Best parse attempt ended at row ").UInt(m_bestToRow).Add(", column ").UInt(m_bestToCol);

		if (m_bestToStack.Any())
		{
			enc.Add("\r\nStack:");

			for (sizet i=0; i!=m_bestToStack.Len(); ++i)
			{
				BestToStackEntry const& entry = m_bestToStack[i];
				enc.Add("\r\n").UInt(i).Add(". ").Add(entry.m_type->Tag()).Add(" at row ").UInt(entry.m_startRow).Add(", column ").UInt(entry.m_startCol);
			}
		}
	}


	ParseNode* ParseTree::NewNode(ParseNode& parent, Ruid const& type)
	{
		if (m_lastBucket->m_nodesUsed == Bucket::NrNodesMax)
		{
			Bucket* newBucket = new Bucket;
			newBucket->m_prevBucket = m_lastBucket;
			m_lastBucket = newBucket;
		}

		return new (m_lastBucket->AddUnconstructedNode()) ParseNode(parent, type);
	}


	void ParseTree::FailNode(ParseNode* p)
	{
		if (m_bestRemaining.p < p->m_remaining.p)
		{
			m_bestRemaining = p->m_remaining;
			m_bestToRow = p->m_toRow;
			m_bestToCol = p->m_toCol;

			if (m_recordBestToStack)
			{
				m_bestToStack.Clear();

				ParseNode* x = p;
				do
				{
					BestToStackEntry& entry = m_bestToStack.Add();
					entry.m_type      = x->m_type;
					entry.m_startRow  = x->m_startRow;
					entry.m_startCol  = x->m_startCol;

					x = x->m_parent;
				}
				while (x != nullptr);
			}
		}

		DiscardNode(p);
	}

	void ParseTree::DiscardNode(ParseNode* p)
	{
		for (Bucket* b=m_lastBucket; ; b=b->m_prevBucket)
		{
			EnsureAbort(b != nullptr);
			if (b->ContainsNode(p))
			{
				b->DiscardNodesFrom(p);
				break;
			}

			b->m_nodesUsed = 0;
		}
	}
}
