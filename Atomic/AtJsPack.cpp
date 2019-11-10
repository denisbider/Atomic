#include "AtIncludes.h"
#include "AtJsPack.h"

#include "AtJsPackGrammar.h"


namespace At
{

	void JsWsPack_ProcessNode(Enc& enc, ParseNode const& p)
	{
		if (p.IsType(JsPack::id_WsNlComments))
			enc.Ch(' ');
		else if (p.HasValue())
			enc.Add(p.Value());
		else
		{
			for (ParseNode const& c : p)
				JsWsPack_ProcessNode(enc, c);
		}
	}


	void JsWsPack(Enc& enc, Seq content)
	{
		ParseTree pt { content };
		if (!pt.Parse(JsPack::C_Script))
		{
			ParseTree pt2 { content };
			pt2.RecordBestToStack();
			EnsureThrow(!pt2.Parse(JsPack::C_Script));
			throw Str().Obj(pt2, ParseTree::BestAttempt);
		}

		JsWsPack_ProcessNode(enc, pt.Root());
	}

}
