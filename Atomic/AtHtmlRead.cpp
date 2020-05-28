#include "AtIncludes.h"
#include "AtHtmlRead.h"

namespace At
{
	namespace Html
	{

	/*
		Seq ReadQuotedString(ParseNode const& qsNode, PinStore& store)
		{
			// ...
		}


		bool ReadAttrValue(ParseNode const& attrNode, PinStore& store, Seq& value)
		{
			EnsureThrow(attrNode.IsType(id_Attr));
			ParseNode const* qsNode { attrNode.FlatFind(id_QStr) };
			if (qsNode)
				{ value = ReadQuotedString(*qsNode, store); return true; }

			ParseNode const* unqNode { attrNode.FlatFind(id_AttrValUnq) };
			if (unqNode)
			{
				// ...
			}

			value = Seq();
			return false;
		}
	*/

	}
}
