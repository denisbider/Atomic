#pragma once

#include "AtStr.h"

namespace At
{
	class TagStack
	{
	public:
		bool Any() const { return m_tags.Any(); }

		Seq Last() const;			// Returns empty Seq if no tag is open

		void Push     (Seq tag) { EnsureThrow(tag.n); m_tags.ReserveExact(150).Ch('/').Add(tag); }
		void PopExact (Seq tag) { EnsureThrow(m_tags.Any()); Seq last { Last() }; EnsureThrow(last == tag); m_tags.Resize(SatSub(m_tags.Len(), last.n + 1)); }
		void Pop      ()        { EnsureThrow(m_tags.Any()); Seq last { Last() }; m_tags.Resize(SatSub(m_tags.Len(), last.n + 1)); }

		// Returns true if the stack contains the tag, false otherwise
		bool Contains(Seq tag) const;

	private:
		Str m_tags;		// Stores tags in the form: /first/second/third
	};
}
