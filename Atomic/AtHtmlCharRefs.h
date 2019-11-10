#pragma once

#include "AtSeq.h"

namespace At
{
	namespace Html
	{
		enum { MinCharRefNameLen = 2, MaxCharRefNameLen = 31 };

		enum class CharRefs { Render, Escape };

		struct CharRefNameInfo
		{
			CharRefNameInfo(char const* name, uint codePoint1)                  : m_name(name), m_codePoint1(codePoint1)                           {}
			CharRefNameInfo(char const* name, uint codePoint1, uint codePoint2) : m_name(name), m_codePoint1(codePoint1), m_codePoint2(codePoint2) {}

			char const* m_name;
			uint m_codePoint1;
			uint m_codePoint2 {};
		};

		// Returns nullptr if not found
		CharRefNameInfo const* FindCharRefByName(Seq name);
	}
}
