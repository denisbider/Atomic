#include "AtIncludes.h"
#include "AtParse.h"

namespace At
{
	namespace Parse
	{

		// Neutral
	
		bool N_End(ParseNode& p) { return !p.HaveByte(); }



		// Generic

		bool G_Req(ParseNode& p, Ruid const& type, ParseFunc pf)
		{
			ParseNode* pn = p.NewChild(type);
			if (!pf(*pn))
				return p.FailChild(pn);
			return p.CommitChild(pn);
		}


		template <int R1, int R2>
		bool G_Req(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2)
		{
			bool success;
			ParseNode* pn = p.NewChild(type);
			success = pf1(*pn); if (R1 && !success) return p.FailChild(pn);
			success = pf2(*pn); if (R2 && !success) return p.FailChild(pn);
			return p.CommitChild(pn);
		}
	
		template bool G_Req<0,1>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2);
		template bool G_Req<1,0>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2);
		template bool G_Req<1,1>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2);


		template <int R1, int R2, int R3>
		bool G_Req(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3)
		{
			bool success;
			ParseNode* pn = p.NewChild(type);
			success = pf1(*pn); if (R1 && !success) return p.FailChild(pn);
			success = pf2(*pn); if (R2 && !success) return p.FailChild(pn);
			success = pf3(*pn); if (R3 && !success) return p.FailChild(pn);
			return p.CommitChild(pn);
		}

		template bool G_Req<0,0,1>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3);
		template bool G_Req<0,1,0>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3);
		template bool G_Req<0,1,1>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3);
		template bool G_Req<1,0,0>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3);
		template bool G_Req<1,0,1>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3);
		template bool G_Req<1,1,0>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3);
		template bool G_Req<1,1,1>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3);

	
		template <int R1, int R2, int R3, int R4>
		bool G_Req(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4)
		{
			bool success;
			ParseNode* pn = p.NewChild(type);
			success = pf1(*pn); if (R1 && !success) return p.FailChild(pn);
			success = pf2(*pn); if (R2 && !success) return p.FailChild(pn);
			success = pf3(*pn); if (R3 && !success) return p.FailChild(pn);
			success = pf4(*pn); if (R4 && !success) return p.FailChild(pn);
			return p.CommitChild(pn);
		}

		template bool G_Req<0,0,0,1>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4);
		template bool G_Req<0,0,1,0>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4);
		template bool G_Req<0,0,1,1>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4);
		template bool G_Req<0,1,0,0>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4);
		template bool G_Req<0,1,0,1>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4);
		template bool G_Req<0,1,1,0>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4);
		template bool G_Req<0,1,1,1>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4);
		template bool G_Req<1,0,0,0>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4);
		template bool G_Req<1,0,0,1>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4);
		template bool G_Req<1,0,1,0>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4);
		template bool G_Req<1,0,1,1>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4);
		template bool G_Req<1,1,0,0>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4);
		template bool G_Req<1,1,0,1>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4);
		template bool G_Req<1,1,1,0>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4);
		template bool G_Req<1,1,1,1>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4);
	
	
		template <int R1, int R2, int R3, int R4, int R5>
		bool G_Req(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5)
		{
			bool success;
			ParseNode* pn = p.NewChild(type);
			success = pf1(*pn); if (R1 && !success) return p.FailChild(pn);
			success = pf2(*pn); if (R2 && !success) return p.FailChild(pn);
			success = pf3(*pn); if (R3 && !success) return p.FailChild(pn);
			success = pf4(*pn); if (R4 && !success) return p.FailChild(pn);
			success = pf5(*pn); if (R5 && !success) return p.FailChild(pn);
			return p.CommitChild(pn);
		}

		template bool G_Req<0,0,0,0,1>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5);
		template bool G_Req<0,0,0,1,0>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5);
		template bool G_Req<0,0,0,1,1>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5);
		template bool G_Req<0,0,1,0,0>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5);
		template bool G_Req<0,0,1,0,1>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5);
		template bool G_Req<0,0,1,1,0>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5);
		template bool G_Req<0,0,1,1,1>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5);
		template bool G_Req<0,1,0,0,0>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5);
		template bool G_Req<0,1,0,0,1>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5);
		template bool G_Req<0,1,0,1,0>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5);
		template bool G_Req<0,1,0,1,1>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5);
		template bool G_Req<0,1,1,0,0>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5);
		template bool G_Req<0,1,1,0,1>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5);
		template bool G_Req<0,1,1,1,0>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5);
		template bool G_Req<0,1,1,1,1>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5);
		template bool G_Req<1,0,0,0,0>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5);
		template bool G_Req<1,0,0,0,1>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5);
		template bool G_Req<1,0,0,1,0>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5);
		template bool G_Req<1,0,0,1,1>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5);
		template bool G_Req<1,0,1,0,0>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5);
		template bool G_Req<1,0,1,0,1>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5);
		template bool G_Req<1,0,1,1,0>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5);
		template bool G_Req<1,0,1,1,1>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5);
		template bool G_Req<1,1,0,0,0>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5);
		template bool G_Req<1,1,0,0,1>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5);
		template bool G_Req<1,1,0,1,0>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5);
		template bool G_Req<1,1,0,1,1>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5);
		template bool G_Req<1,1,1,0,0>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5);
		template bool G_Req<1,1,1,0,1>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5);
		template bool G_Req<1,1,1,1,0>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5);
		template bool G_Req<1,1,1,1,1>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5);
	
	
		template <int R1, int R2, int R3, int R4, int R5, int R6>
		bool G_Req(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5, ParseFunc pf6)
		{
			bool success;
			ParseNode* pn = p.NewChild(type);
			success = pf1(*pn); if (R1 && !success) return p.FailChild(pn);
			success = pf2(*pn); if (R2 && !success) return p.FailChild(pn);
			success = pf3(*pn); if (R3 && !success) return p.FailChild(pn);
			success = pf4(*pn); if (R4 && !success) return p.FailChild(pn);
			success = pf5(*pn); if (R5 && !success) return p.FailChild(pn);
			success = pf6(*pn); if (R6 && !success) return p.FailChild(pn);
			return p.CommitChild(pn);
		}

		template bool G_Req<0,0,0,0,0,1>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5, ParseFunc pf6);
		template bool G_Req<0,0,0,0,1,0>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5, ParseFunc pf6);
		template bool G_Req<0,0,0,0,1,1>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5, ParseFunc pf6);
		template bool G_Req<0,0,0,1,0,0>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5, ParseFunc pf6);
		template bool G_Req<0,0,0,1,0,1>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5, ParseFunc pf6);
		template bool G_Req<0,0,0,1,1,0>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5, ParseFunc pf6);
		template bool G_Req<0,0,0,1,1,1>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5, ParseFunc pf6);
		template bool G_Req<0,0,1,0,0,0>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5, ParseFunc pf6);
		template bool G_Req<0,0,1,0,0,1>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5, ParseFunc pf6);
		template bool G_Req<0,0,1,0,1,0>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5, ParseFunc pf6);
		template bool G_Req<0,0,1,0,1,1>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5, ParseFunc pf6);
		template bool G_Req<0,0,1,1,0,0>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5, ParseFunc pf6);
		template bool G_Req<0,0,1,1,0,1>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5, ParseFunc pf6);
		template bool G_Req<0,0,1,1,1,0>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5, ParseFunc pf6);
		template bool G_Req<0,0,1,1,1,1>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5, ParseFunc pf6);
		template bool G_Req<0,1,0,0,0,0>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5, ParseFunc pf6);
		template bool G_Req<0,1,0,0,0,1>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5, ParseFunc pf6);
		template bool G_Req<0,1,0,0,1,0>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5, ParseFunc pf6);
		template bool G_Req<0,1,0,0,1,1>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5, ParseFunc pf6);
		template bool G_Req<0,1,0,1,0,0>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5, ParseFunc pf6);
		template bool G_Req<0,1,0,1,0,1>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5, ParseFunc pf6);
		template bool G_Req<0,1,0,1,1,0>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5, ParseFunc pf6);
		template bool G_Req<0,1,0,1,1,1>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5, ParseFunc pf6);
		template bool G_Req<0,1,1,0,0,0>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5, ParseFunc pf6);
		template bool G_Req<0,1,1,0,0,1>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5, ParseFunc pf6);
		template bool G_Req<0,1,1,0,1,0>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5, ParseFunc pf6);
		template bool G_Req<0,1,1,0,1,1>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5, ParseFunc pf6);
		template bool G_Req<0,1,1,1,0,0>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5, ParseFunc pf6);
		template bool G_Req<0,1,1,1,0,1>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5, ParseFunc pf6);
		template bool G_Req<0,1,1,1,1,0>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5, ParseFunc pf6);
		template bool G_Req<0,1,1,1,1,1>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5, ParseFunc pf6);

		template bool G_Req<1,0,0,0,0,0>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5, ParseFunc pf6);
		template bool G_Req<1,0,0,0,0,1>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5, ParseFunc pf6);
		template bool G_Req<1,0,0,0,1,0>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5, ParseFunc pf6);
		template bool G_Req<1,0,0,0,1,1>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5, ParseFunc pf6);
		template bool G_Req<1,0,0,1,0,0>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5, ParseFunc pf6);
		template bool G_Req<1,0,0,1,0,1>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5, ParseFunc pf6);
		template bool G_Req<1,0,0,1,1,0>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5, ParseFunc pf6);
		template bool G_Req<1,0,0,1,1,1>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5, ParseFunc pf6);
		template bool G_Req<1,0,1,0,0,0>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5, ParseFunc pf6);
		template bool G_Req<1,0,1,0,0,1>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5, ParseFunc pf6);
		template bool G_Req<1,0,1,0,1,0>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5, ParseFunc pf6);
		template bool G_Req<1,0,1,0,1,1>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5, ParseFunc pf6);
		template bool G_Req<1,0,1,1,0,0>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5, ParseFunc pf6);
		template bool G_Req<1,0,1,1,0,1>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5, ParseFunc pf6);
		template bool G_Req<1,0,1,1,1,0>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5, ParseFunc pf6);
		template bool G_Req<1,0,1,1,1,1>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5, ParseFunc pf6);
		template bool G_Req<1,1,0,0,0,0>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5, ParseFunc pf6);
		template bool G_Req<1,1,0,0,0,1>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5, ParseFunc pf6);
		template bool G_Req<1,1,0,0,1,0>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5, ParseFunc pf6);
		template bool G_Req<1,1,0,0,1,1>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5, ParseFunc pf6);
		template bool G_Req<1,1,0,1,0,0>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5, ParseFunc pf6);
		template bool G_Req<1,1,0,1,0,1>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5, ParseFunc pf6);
		template bool G_Req<1,1,0,1,1,0>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5, ParseFunc pf6);
		template bool G_Req<1,1,0,1,1,1>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5, ParseFunc pf6);
		template bool G_Req<1,1,1,0,0,0>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5, ParseFunc pf6);
		template bool G_Req<1,1,1,0,0,1>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5, ParseFunc pf6);
		template bool G_Req<1,1,1,0,1,0>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5, ParseFunc pf6);
		template bool G_Req<1,1,1,0,1,1>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5, ParseFunc pf6);
		template bool G_Req<1,1,1,1,0,0>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5, ParseFunc pf6);
		template bool G_Req<1,1,1,1,0,1>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5, ParseFunc pf6);
		template bool G_Req<1,1,1,1,1,0>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5, ParseFunc pf6);
		template bool G_Req<1,1,1,1,1,1>(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5, ParseFunc pf6);


		bool G_Repeat(ParseNode& p, Ruid const& type, ParseFunc pf, sizet minCount, sizet maxCount)
		{
			EnsureThrow(minCount <= maxCount);

			ParseNode* pn { p.NewChild(type) };
			sizet i;
			for (i=0; i!=maxCount; ++i)
			{
				sizet prevRemaining { pn->Remaining().n };
				if (pf(*pn))
					EnsureThrow(pn->Remaining().n < prevRemaining);
				else
					break;
			}

			if (i < minCount)
				return p.FailChild(pn);
			else
				return p.CommitChild(pn);
		}


		bool G_UntilExcl(ParseNode& p, Ruid const& type, ParseFunc pfRepeat, ParseFunc pfUntil, sizet minCount, sizet maxCount)
		{
			EnsureThrow(minCount <= maxCount);

			ParseNode* pn { p.NewChild(type) };
			sizet i;
			for (i=0; i!=maxCount; ++i)
			{
				ParseNode* pn2 { pn->NewChild(id_Discard) };
				bool untilResult { pfUntil(*pn2) };
				pn->DiscardChild(pn2);
				if (untilResult)
					break;

				sizet prevRemaining { pn->Remaining().n };
				if (pfRepeat(*pn))
					EnsureThrow(pn->Remaining().n < prevRemaining);
				else
					return p.FailChild(pn);
			}

			if (i < minCount)
				return p.FailChild(pn);
			else
				return p.CommitChild(pn);
		}


		bool G_Choice(ParseNode& p, Ruid const& type, ParseFunc* pfArray)
		{
			while (*pfArray)
			{
				ParseNode* pn { p.NewChild(type) };
				if ((*pfArray)(*pn))
					return p.CommitChild(pn);

				p.FailChild(pn);
				++pfArray;
			}
		
			return false;
		}


		bool G_Choice(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2)
		{
			ParseFunc pfArray[] { pf1, pf2, 0 };
			return G_Choice(p, type, pfArray);
		}

		bool G_Choice(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3)
		{
			ParseFunc pfArray[] { pf1, pf2, pf3, 0 };
			return G_Choice(p, type, pfArray);
		}

		bool G_Choice(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4)
		{
			ParseFunc pfArray[] { pf1, pf2, pf3, pf4, 0 };
			return G_Choice(p, type, pfArray);
		}

		bool G_Choice(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5)
		{
			ParseFunc pfArray[] { pf1, pf2, pf3, pf4, pf5, 0 };
			return G_Choice(p, type, pfArray);
		}

		bool G_Choice(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5, ParseFunc pf6)
		{
			ParseFunc pfArray[] { pf1, pf2, pf3, pf4, pf5, pf6, 0 };
			return G_Choice(p, type, pfArray);
		}

		bool G_Choice(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5, ParseFunc pf6, ParseFunc pf7)
		{
			ParseFunc pfArray[] { pf1, pf2, pf3, pf4, pf5, pf6, pf7, 0 };
			return G_Choice(p, type, pfArray);
		}

		bool G_Choice(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5, ParseFunc pf6, ParseFunc pf7, ParseFunc pf8)
		{
			ParseFunc pfArray[] { pf1, pf2, pf3, pf4, pf5, pf6, pf7, pf8, 0 };
			return G_Choice(p, type, pfArray);
		}

		bool G_Choice(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5, ParseFunc pf6, ParseFunc pf7, ParseFunc pf8, ParseFunc pf9)
		{
			ParseFunc pfArray[] { pf1, pf2, pf3, pf4, pf5, pf6, pf7, pf8, pf9, 0 };
			return G_Choice(p, type, pfArray);
		}

		bool G_Choice(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5, ParseFunc pf6, ParseFunc pf7, ParseFunc pf8, ParseFunc pf9, ParseFunc pf10)
		{
			ParseFunc pfArray[] { pf1, pf2, pf3, pf4, pf5, pf6, pf7, pf8, pf9, pf10, 0 };
			return G_Choice(p, type, pfArray);
		}

		bool G_Choice(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5, ParseFunc pf6, ParseFunc pf7, ParseFunc pf8, ParseFunc pf9, ParseFunc pf10, ParseFunc pf11)
		{
			ParseFunc pfArray[] { pf1, pf2, pf3, pf4, pf5, pf6, pf7, pf8, pf9, pf10, pf11, 0 };
			return G_Choice(p, type, pfArray);
		}

		bool G_Choice(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5, ParseFunc pf6, ParseFunc pf7, ParseFunc pf8, ParseFunc pf9, ParseFunc pf10, ParseFunc pf11, ParseFunc pf12)
		{
			ParseFunc pfArray[] { pf1, pf2, pf3, pf4, pf5, pf6, pf7, pf8, pf9, pf10, pf11, pf12, 0 };
			return G_Choice(p, type, pfArray);
		}


		bool G_OneOrMoreOf(ParseNode& p, Ruid const& type, ParseFunc* pfArray)
		{
			ParseNode* pn = p.NewChild(type);

			bool success = false;
			while (*pfArray)
			{
				if ((*pfArray)(*pn))
					success = true;
				++pfArray;
			}
	
			if (success)
				p.CommitChild(pn);
			else
				p.FailChild(pn);

			return success;
		}


		bool G_OneOrMoreOf(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2)
		{
			ParseFunc pfArray[] { pf1, pf2, 0 };
			return G_OneOrMoreOf(p, type, pfArray);
		}

		bool G_OneOrMoreOf(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3)
		{
			ParseFunc pfArray[] { pf1, pf2, pf3, 0 };
			return G_OneOrMoreOf(p, type, pfArray);
		}

		bool G_OneOrMoreOf(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4)
		{
			ParseFunc pfArray[] { pf1, pf2, pf3, pf4, 0 };
			return G_OneOrMoreOf(p, type, pfArray);
		}

		bool G_OneOrMoreOf(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5)
		{
			ParseFunc pfArray[] { pf1, pf2, pf3, pf4, pf5, 0 };
			return G_OneOrMoreOf(p, type, pfArray);
		}


		bool G_Not(ParseNode& p, ParseFunc pfNot, ParseFunc pfYes)
		{
			ParseNode* pn { p.NewChild(id_Discard) };
			bool pfNotResult { pfNot(*pn) };
			p.DiscardChild(pn);
			if (pfNotResult)
				return false;

			return pfYes(p);
		}


		bool G_NotFollowedBy(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2)
		{
			ParseNode* pn { p.NewChild(type) };
			if (!pf1(*pn))
				return p.FailChild(pn);

			ParseNode* pn2 { pn->NewChild(id_Discard) };
			bool pf2Result { pf2(*pn2) };
			pn->DiscardChild(pn2);
			if (pf2Result)
				return p.FailChild(pn);

			return p.CommitChild(pn);
		}
	

		bool G_SeqMatch(ParseNode& p, Ruid const& type, CaseMatch cm, Seq s)
		{
			ParseNode* pn = p.NewChild(type);
			return V_SeqMatch(*pn, cm, s) ? p.CommitChild(pn) : p.FailChild(pn);
		}



		// Value

		bool V_ByteIf(ParseNode& p, CharPredicate pred)
		{
			if (!p.HaveByte() || !pred(p.CurByte()))
				return false;

			p.ConsumeByte();
			return true;
		}


		bool V_ByteIs(ParseNode& p, byte c)
		{
			if (!p.HaveByte() || p.CurByte() != c)
				return false;
		
			p.ConsumeByte();
			return true;
		}

		
		bool V_ByteIsOf(ParseNode& p, char const* z)
		{
			if (!p.HaveByte() || !ZChr(z, p.CurByte()))
				return false;
		
			p.ConsumeByte();
			return true;
		}


		bool V_ByteNfb(ParseNode& p, byte c, byte other)
		{
			if (!p.HaveByte() || p.CurByte() != c)
				return false;

			if (p.HaveFollowingByte() && p.FollowingByte() == other)
				return false;

			p.ConsumeByte();
			return true;
		}


		bool V_ByteNfbOneOf(ParseNode& p, byte c, char const* zOther)
		{
			if (!p.HaveByte() || p.CurByte() != c)
				return false;

			if (p.HaveFollowingByte() && ZChr(zOther, p.FollowingByte()) != nullptr)
				return false;

			p.ConsumeByte();
			return true;
		}

		
		bool V_ByteNfbSeq(ParseNode& p, byte c, Seq s, CaseMatch cm)
		{
			if (!p.HaveByte() || p.CurByte() != c)
				return false;

			if (p.Remaining().DropByte().StartsWith(s, cm))
				return false;

			p.ConsumeByte();
			return true;
		}


		bool V_Utf8CharIf(ParseNode& p, CharPredicate pred)
		{
			Seq   reader { p.Remaining() };
			sizet origN  { reader.n };
			uint  c      { reader.ReadUtf8Char() };

			if (c == UINT_MAX)
				return false;
			if (!pred(c))
				return false;

			p.ConsumeUtf8Char(c, origN - reader.n);
			return true;
		}

	
		bool V_Remaining(ParseNode& p)
		{
			while (p.Remaining().n)
				p.ConsumeByte();
			return true;
		}


		bool V_SeqMatch(ParseNode& p, CaseMatch cm, Seq s)
		{
			Seq reader { p.Remaining() };
			if (!reader.StartsWith(s, cm))
				return false;

			p.ConsumeUtf8Seq(s.n);
			return true;
		}


		bool V_HWs_Indent(ParseNode& p, sizet minNrCols, sizet maxNrCols)
		{
			sizet origCol { p.ToCol() };
			sizet curCol  { origCol };
			sizet minCol  { origCol + minNrCols };
			sizet maxCol  { SatAdd(origCol, maxNrCols) };

			Seq   reader  { p.Remaining() };
			sizet nrBytes {};

			while (reader.n)
			{
				sizet newCol;
				byte  c { reader.p[0] };

				if (c == 32)
					newCol = curCol + 1;
				else if (c == 9)
					newCol = p.Tree().ApplyTab(curCol);
				else
					break;

				if (newCol > maxCol)
					break;

				++nrBytes;
				reader.DropByte();
				curCol = newCol;
			}

			if (curCol < minCol)
				return false;

			while (nrBytes--)
				p.ConsumeByte();
			return true;
		}



		// Constructed

		DEF_RUID_B(Ws)
		DEF_RUID_B(At)
		DEF_RUID_B(Eq)
		DEF_RUID_B(And)
		DEF_RUID_B(Dot)
		DEF_RUID_B(HWs)
		DEF_RUID_B(Apos)
		DEF_RUID_B(Bang)
		DEF_RUID_B(CRLF)
		DEF_RUID_B(Dash)
		DEF_RUID_B(Less)
		DEF_RUID_B(Grtr)
		DEF_RUID_B(Hash)
		DEF_RUID_B(Plus)
		DEF_RUID_B(Comma)
		DEF_RUID_B(Colon)
		DEF_RUID_B(Slash)
		DEF_RUID_B(Percent)
		DEF_RUID_B(Backtick)
		DEF_RUID_B(DblQuote)
		DEF_RUID_B(SglQuote)
		DEF_RUID_B(Backslash)
		DEF_RUID_B(Semicolon)
		DEF_RUID_B(CurlyOpen)
		DEF_RUID_B(CurlyClose)
		DEF_RUID_B(OpenBr)
		DEF_RUID_B(CloseBr)
		DEF_RUID_B(QuestnMark)
		DEF_RUID_B(SqOpenBr)
		DEF_RUID_B(SqCloseBr)
		DEF_RUID_B(Remaining);


		bool C_UntilIncl(ParseNode& p, Ruid const& type, ParseFunc pfRepeat, Ruid const& untilType, sizet minCount, sizet maxCount)
		{
			EnsureThrow(minCount <= maxCount);

			ParseNode* pn { p.NewChild(type) };
			sizet i;
			for (i=0; i!=maxCount; ++i)
			{
				sizet prevRemaining { pn->Remaining().n };
				if (pfRepeat(*pn))
					EnsureThrow(pn->Remaining().n < prevRemaining);
				else
					return p.FailChild(pn);

				if (pn->LastChild().IsType(untilType))
					break;
			}

			if (i < minCount)
				return p.FailChild(pn);
			else
				return p.CommitChild(pn);
		}


		bool C_HWs_Indent(ParseNode& p, Ruid const& type, sizet minNrCols, sizet maxNrCols)
		{
			ParseNode* pn { p.NewChild(type) };
			if (!V_HWs_Indent(*pn, minNrCols, maxNrCols))
				return p.FailChild(pn);
			return p.CommitChild(pn);
		}

	}
}
