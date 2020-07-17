#include "AtIncludes.h"
#include "AtParse.h"

namespace At
{
	DEF_RUID_B(Root)
	DEF_RUID_B(Append)
	DEF_RUID_B(Discard)


	// Construction

	ParseNode::ParseNode(ParseTree& tree, Seq srcText) : m_tree(tree), m_type(&id_Root)
	{
		m_start = srcText;
		m_remaining = srcText;

		m_startRow = 1;
		m_startCol = 1;
		m_toRow = 1;
		m_toCol = 1;
	}


	ParseNode::ParseNode(ParseNode& parent, Ruid const& type) : m_tree(parent.m_tree), m_type(&type)
	{
		m_depth = parent.m_depth + 1;
		m_parent = &parent;

		m_start = parent.m_remaining;
		m_remaining = parent.m_remaining;
		
		m_startRow = parent.m_toRow;
		m_startCol = parent.m_toCol;
		m_toRow = parent.m_toRow;
		m_toCol = parent.m_toCol;
	}


	// Interface for end users

	void ParseNode::Dump(Enc& enc, sizet indent) const
	{
		enc.Add(m_type->Tag()).Add(" at ").UInt(m_startRow).Add(":").UInt(m_startCol);
		if (HasValue())
			enc.Add(" - \"").JsStrEncode(Value()).Add("\"");
		else
		{
			for (ParseNode const* c=m_firstChild; c!=0; c=c->m_nextSibling)
			{
				enc.Add("\r\n").Chars(indent, ' ').Add("  ");
				c->Dump(enc, indent+2);
			}
		}
	}


	ParseNode const* ParseNode::FindAncestor(Ruid const* const* ids) const
	{
		for (ParseNode const* a=m_parent; a!=0; a=a->m_parent)
			for (Ruid const* const* id = ids; *id != nullptr; ++id) 
				if (a->m_type->Is(**id))
					return a;
		return 0;
	}


	ParseNode const* ParseNode::FlatFindFirstOf(Ruid const* const* ids) const
	{
		for (ParseNode const* c=m_firstChild; c!=0; c=c->m_nextSibling)
			for (Ruid const* const* id = ids; *id != nullptr; ++id) 
				if (c->m_type->Is(**id))
					return c;
		return 0;
	}


	ParseNode const* ParseNode::DeepFindFirstOf(Ruid const* const* ids) const
	{
		for (Ruid const* const* id = ids; *id != nullptr; ++id) 
			if (m_type->Is(**id))
				return this;
	
		for (ParseNode const* c=m_firstChild; c!=0; c=c->m_nextSibling)
		{
			ParseNode const* match = c->DeepFindFirstOf(ids);
			if (match)
				return match;
		}
	
		return 0;
	}


	ParseNode const* ParseNode::FrontFindFirstOf(Ruid const* const* ids) const
	{
		for (Ruid const* const* id = ids; *id != nullptr; ++id) 
			if (m_type->Is(**id))
				return this;
	
		if (m_firstChild != nullptr)
			return m_firstChild->FrontFindFirstOf(ids);
	
		return 0;
	}



	// ParseNode - interface for parser functions

	void ParseNode::ConsumeUtf8Seq(sizet nrBytesExact)
	{
		EnsureThrow(m_remaining.n >= nrBytesExact);
		
		Seq reader { m_remaining.p, nrBytesExact };
		while (true)
		{
			uint c { reader.ReadUtf8Char() };
			if (c == UINT_MAX)
			{
				EnsureThrow(!reader.n && "The number of bytes to consume must exactly correspond to a UTF-8 sequence");
				break;
			}

			UpdateRowCol(c);
		}

		AddToValue(nrBytesExact);
	}


	void ParseNode::UpdateRowCol(uint c)
	{
		if (c == 10)
		{
			++m_toRow;
			m_toCol = 1;
		}
		else if (c == 9)
			m_toCol = m_tree.ApplyTab(m_toCol);
		else
			m_toCol += Unicode::CharInfo::Get(c).m_width;
	}


	void ParseNode::AddToValue(sizet nrBytes)
	{
		EnsureThrow(!m_committed && "Already committed");
		EnsureThrow(!m_firstChild && "Cannot add value data to constructed parser");

		if (!m_value.n)
			m_value = m_remaining.ReadBytes(nrBytes);
		else
		{
			EnsureThrow((m_value.p + m_value.n == m_remaining.p) && "Can only add data sequentially");
			m_value.n     += nrBytes;
			m_remaining.p += nrBytes;
			m_remaining.n -= nrBytes;
		}
	}


	void ParseNode::RefineType(Ruid const& newType)
	{
		EnsureThrow(newType.Is(*m_type));
		m_type = &newType;
	}


	void ParseNode::RefineParentType(Ruid const& parentType, Ruid const& newType)
	{
		ParseNode* pn { this };
		while (true)
		{
			pn = pn->m_parent;
			EnsureThrow(pn != nullptr);
			if (pn->IsType(parentType))
			{
				pn->RefineType(newType);
				break;
			}
		}
	}


	ParseNode* ParseNode::NewChild(Ruid const& type)
	{
		return m_tree.NewNode(*this, type);
	}


	bool ParseNode::CommitChild(ParseNode* child)
	{
		EnsureThrow(!m_committed && "Already committed");
		EnsureThrow(child->m_start.p == m_remaining.p && "Position mismatch");
		EnsureThrow(!child->m_type->Is(id_Discard) && "Cannot commit a child with Discard type");

		if (!child->m_type->Is(id_Append))
		{
			// Add as a distinct entity
			EnsureThrow(!HasValue() && "Cannot add a child to a value node");

			if (!m_firstChild)
				m_firstChild = m_lastChild = child;
			else
			{
				EnsureAbort(m_lastChild->m_nextSibling == nullptr);

				m_lastChild->m_nextSibling = child;
				m_lastChild = child;
			}
		}
		else
		{
			// Append with anything else we might have
			if (child->HasValue())
			{
				EnsureThrow(!m_firstChild && "Cannot add a value parser to a constructed parser");
				if (!m_value.n)
					m_value = child->m_value;
				else
				{
					EnsureThrow((m_value.p + m_value.n == child->m_value.p) && "Values being appended must be adjacent");
					m_value.n += child->m_value.n;
				}
			}
			else if (child->m_firstChild != nullptr)
			{
				EnsureThrow(!HasValue() && "Cannot add a constructed parser to a value parser");
				for (ParseNode* p=child->m_firstChild; p!=0; p=p->m_nextSibling)
				{
					if (!m_firstChild)
						m_firstChild = m_lastChild = p;
					else
					{
						m_lastChild->m_nextSibling = p;
						m_lastChild = p;
					}
				}
			}
		}

		m_remaining = child->m_remaining;
		m_toRow = child->m_toRow;
		m_toCol = child->m_toCol;

		child->m_committed = true;
		return true;
	}
}
