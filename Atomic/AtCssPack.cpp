#include "AtIncludes.h"
#include "AtCssPack.h"

#include "AtCssGrammar.h"


namespace At
{

	void CssPack_ProcessNode(Enc& enc, ParseNode const& p)
	{
		if (p.IsType(Css::id_WsComments) || p.IsType(Css::id_WhitespaceTkn))	// WhitespaceTkn can be bare inside a UrlTkn
			enc.Ch(' ');
		else if (p.HasValue())
			enc.Add(p.Value());
		else
		{
			for (ParseNode const& c : p)
				CssPack_ProcessNode(enc, c);
		}
	}


	void CssPack(Enc& enc, Seq content)
	{
		ParseTree pt { content };
		if (!pt.Parse(Css::C_Tokens))
		{
			ParseTree pt2 { content };
			pt2.RecordBestToStack();
			EnsureThrow(!pt2.Parse(Css::C_Tokens));
			throw Str().Obj(pt2, ParseTree::BestAttempt);
		}

		CssPack_ProcessNode(enc, pt.Root());
	}

}
