#include "AtIncludes.h"
#include "AtHtmlRead.h"

namespace At
{
	namespace Html
	{

		Seq ReadQuotedString(ParseNode const& qsNode)
		{
			EnsureThrow(qsNode.IsType(id_QStr));
			Seq reader = qsNode.SrcText();
			uint delim = reader.ReadByte();
			EnsureThrow(delim == '\'' || delim == '"');
			EnsureThrow(reader.RevReadByte() == delim);
			return reader;
		}


		bool ReadAttrValue(ParseNode const& attrNode, Seq& value)
		{
			EnsureThrow(attrNode.IsType(id_Attr));
			ParseNode const* qsNode { attrNode.FlatFind(id_QStr) };
			if (qsNode)
				{ value = ReadQuotedString(*qsNode); return true; }

			ParseNode const* unqNode { attrNode.FlatFind(id_AttrValUnq) };
			if (unqNode)
				{ value = unqNode->SrcText(); return true; }

			value = Seq();
			return false;
		}

	}
}
