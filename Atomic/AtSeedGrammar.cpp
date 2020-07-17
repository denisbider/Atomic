#include "AtIncludes.h"
#include "AtSeedGrammar.h"

namespace At
{
	namespace Seed
	{
		using namespace Parse;

		DEF_RUID_B(NewLine)
		DEF_RUID_B(Comment)
		DEF_RUID_B(Token)
		DEF_RUID_B(Elem)
		DEF_RUID_X(CharLit,      id_Elem)
		DEF_RUID_X(StrLit,       id_Elem)
		DEF_RUID_X(NumLit,       id_Elem)
		DEF_RUID_X(NumLitDec,    id_NumLit)
		DEF_RUID_X(NumLitHex,    id_NumLit)
		DEF_RUID_X(TokenChain,   id_Elem)
		DEF_RUID_X(List,         id_Elem)
		DEF_RUID_X(BraceList,    id_List)
		DEF_RUID_X(RndBraceList, id_BraceList)
		DEF_RUID_X(SqBraceList,  id_BraceList)
		DEF_RUID_X(AngBraceList, id_BraceList)
		DEF_RUID_X(ClyBraceList, id_BraceList)
		DEF_RUID_X(OuterList,    id_List)
		DEF_RUID_X(HierarList,   id_OuterList)
		DEF_RUID_X(LinearList,   id_OuterList)
		DEF_RUID_B(Char)
		DEF_RUID_B(Chars)
		DEF_RUID_B(EscChar)
		DEF_RUID_B(EscCharUni)

		// Whitespace and comments
		bool C_NewLine      (ParseNode& p) { return G_Req            (p, id_NewLine,      G_Req01<V_Cr, V_Lf>                                                          ); }

		bool V_CommentChar  (ParseNode& p) { return V_Utf8CharIf     (p,                  [] (uint c) -> bool { return c>=32 && c!=127; }                              ); }
		bool C_Comment      (ParseNode& p) { return G_Req            (p, id_Comment,      G_Req10<V_Semicolon, G_Repeat<V_CommentChar>>                                ); }

		bool C_HWsC         (ParseNode& p) { return G_OneOrMoreOf    (p, id_Append,       C_HWs, C_Comment                                                             ); }
		bool C_HWsC_Nl      (ParseNode& p) { return G_Choice         (p, id_Append,       G_Req<C_HWsC, N_End>, G_Req01<C_HWsC, C_NewLine>                             ); }

		// Character literal
		bool V_CharNoSQ     (ParseNode& p) { return V_Utf8CharIf     (p,                  [] (uint c) -> bool { return c>=32 && c!=127 && c!='\''; }                   ); }
		bool C_CharNoSQ     (ParseNode& p) { return G_Req            (p, id_Char,         V_CharNoSQ                                                                   ); }

		bool V_EscChar2nd   (ParseNode& p) { return V_ByteIf         (p,                  [] (uint c) -> bool { return !!ZChr("0tnr`'\"", c); }                        ); }
		bool C_EscChar      (ParseNode& p) { return G_Req            (p, id_EscChar,      G_Req<V_Backtick, V_EscChar2nd>                                              ); }
		
		bool V_EscCharUni   (ParseNode& p) { return G_Req<1,1,1,1>   (p, id_Append,       V_Backtick, V_Hash, G_Repeat<V_AsciiHexDigit, 1, 6>, V_Semicolon             ); }
		bool C_EscCharUni   (ParseNode& p) { return G_Req            (p, id_EscCharUni,   V_EscCharUni                                                                 ); }

		bool C_CharOrEsc    (ParseNode& p) { return G_Choice         (p, id_Append,       C_CharNoSQ, C_EscChar, C_EscCharUni                                          ); }
		bool C_CharLit      (ParseNode& p) { return G_Req<1,1,1>     (p, id_CharLit,      C_SglQuote, C_CharOrEsc, C_SglQuote                                          ); }

		// String literal
		bool V_CharNoDQ     (ParseNode& p) { return V_Utf8CharIf     (p,                  [] (uint c) -> bool { return c>=32 && c!=127 && c!='"'; }                    ); }
		bool C_Chars        (ParseNode& p) { return G_Req            (p, id_Chars,        G_Repeat<V_CharNoDQ>                                                         ); }
		bool C_CharsOrEsc   (ParseNode& p) { return G_Choice         (p, id_Append,       C_Chars, C_EscChar, C_EscCharUni                                             ); }
		bool C_StrLit       (ParseNode& p) { return G_Req<1,1,1>     (p, id_StrLit,       C_DblQuote, G_Repeat<C_CharsOrEsc>, C_DblQuote                               ); }

		// Decimal number literal:
		//     0
		//    -1000000
		//     100_000
		//     1.000_001e+23
		bool V_NrZero       (ParseNode& p) { return V_ByteIs         (p,                  '0'                                                                          ); }
		bool V_NrX          (ParseNode& p) { return V_ByteIf         (p,                  [] (uint c) -> bool { return c == 'x' || c == 'X'; }                         ); }
		bool V_NrZeroNfbX   (ParseNode& p) { return G_NotFollowedBy  (p, id_Append,       V_NrZero, V_NrX                                                              ); }
		bool V_NrDigitNZ    (ParseNode& p) { return V_ByteIf         (p,                  [] (uint c) -> bool { return c >= '1' && c <= '9'; }                         ); }
		bool V_NrDecNfbUnd  (ParseNode& p) { return G_NotFollowedBy  (p, id_Append,       V_AsciiDecDigit, V_Underscore                                                ); }
		bool V_NrDecUnders  (ParseNode& p) { return G_Req<1,1,1>     (p, id_Append,       V_AsciiDecDigit, V_Underscore, V_AsciiDecDigit                               ); }
		bool V_NrDecDigits  (ParseNode& p) { return G_Repeat         (p, id_Append,       G_Choice<V_NrDecNfbUnd, V_NrDecUnders>                                       ); }
		bool V_NrNonZero    (ParseNode& p) { return G_Req<1,0>       (p, id_Append,       V_NrDigitNZ, V_NrDecDigits                                                   ); }
		bool V_NrIntPart    (ParseNode& p) { return G_Choice         (p, id_Append,       V_NrZeroNfbX, V_NrNonZero                                                    ); }
		bool V_NrFracPart   (ParseNode& p) { return G_Req<1,1>       (p, id_Append,       V_Dot, V_NrDecDigits                                                         ); }
		bool V_NrExpEe      (ParseNode& p) { return V_ByteIsOf       (p,                  "Ee"                                                                         ); }
		bool V_NrExpPlMin   (ParseNode& p) { return V_ByteIsOf       (p,                  "+-"                                                                         ); }
		bool V_NrExpPart    (ParseNode& p) { return G_Req<1,1,1>     (p, id_Append,       V_NrExpEe, V_NrExpPlMin, G_Repeat<V_AsciiDecDigit>                           ); }
		bool C_NumLitDec    (ParseNode& p) { return G_Req<0,1,0,0>   (p, id_NumLitDec,    V_Dash, V_NrIntPart, V_NrFracPart, V_NrExpPart                               ); }

		// Hexadecimal number literal:
		//     0x00
		//     0x1234FFFF
		//     0x1234_FFFF
		bool V_NrHexPrefix  (ParseNode& p) { return V_SeqMatchInsens (p,                  "0x"                                                                         ); }
		bool V_NrHexNfbUnd  (ParseNode& p) { return G_NotFollowedBy  (p, id_Append,       V_AsciiHexDigit, V_Underscore                                                ); }
		bool V_NrHexUnders  (ParseNode& p) { return G_Req<1,1,1>     (p, id_Append,       V_AsciiHexDigit, V_Underscore, V_AsciiHexDigit                               ); }
		bool C_NumLitHex    (ParseNode& p) { return G_Req<1,1>       (p, id_NumLitHex,    V_NrHexPrefix, G_Repeat<G_Choice<V_NrHexNfbUnd, V_NrHexUnders>>              ); }

		// Forward declarations
		bool C_NextIndent   (ParseNode& p);
		bool C_BraceList    (ParseNode& p);
		bool C_OuterList    (ParseNode& p);

		// Token
		bool IsTokenNonFirst(uint c)       { return Ascii::IsAlpha(c) || Ascii::IsDecDigit(c) || ZChr("_-", c); }
		bool IsTokenFirst   (uint c)       { return Ascii::IsAlpha(c); }

		bool C_Token        (ParseNode& p) { return G_Req<1,0>       (p, id_Token,        V_ByteIf<IsTokenFirst>, G_Repeat<V_ByteIf<IsTokenNonFirst>>                  ); }
		bool C_Token_OptAtt (ParseNode& p) { return G_Req<1,0>       (p, id_Append,       C_Token, C_BraceList                                                         ); }
		bool C_TokenChain   (ParseNode& p) { return G_Req<1,0>       (p, id_TokenChain,   C_Token_OptAtt, G_Repeat<G_Req<C_Dot, C_Token_OptAtt>>                       ); }

		// Linear list - optional backslash indicates that elements continue on next line:
		//     a b \
		//         (c) d \
		//         (e f (g h ())) ()
		bool C_LinearElem   (ParseNode& p) { return G_Choice         (p, id_Append,       C_CharLit, C_StrLit, C_NumLitHex, C_NumLitDec, C_TokenChain, C_BraceList     ); }
		bool C_LinearElems  (ParseNode& p) { return G_Req<1,0>       (p, id_Append,       C_LinearElem, G_Repeat<G_Req<C_HWs, C_LinearElem>>                           ); }
		bool C_LLContSep    (ParseNode& p) { return G_Req<0,1,1,1>   (p, id_Append,       C_HWs, C_Backslash, C_HWsC_Nl, C_NextIndent                                  ); }
		bool C_LLElemSep    (ParseNode& p) { return G_Choice         (p, id_Append,       C_LLContSep, C_HWs                                                           ); }
		bool C_LinearList   (ParseNode& p) { return G_Req<1,0,1>     (p, id_LinearList,   C_LinearElems, G_Repeat<G_Req<C_LLElemSep, C_LinearElem>>, C_HWsC_Nl         ); }

		// Brace list - same list as in the linear list example, but in brackets. Can use any bracket type: (), [], <>, {}
		//     (a b (c) d (e f \
		//         (g h ())) ())
		bool C_RndBraceList (ParseNode& p) { return G_Req<1,0,1>     (p, id_RndBraceList, C_OpenBr, C_LinearElems, C_CloseBr                                           ); }
		bool C_SqBraceList  (ParseNode& p) { return G_Req<1,0,1>     (p, id_SqBraceList,  C_SqOpenBr, C_LinearElems, C_SqCloseBr                                       ); }
		bool C_AngBraceList (ParseNode& p) { return G_Req<1,0,1>     (p, id_AngBraceList, C_Less, C_LinearElems, C_Grtr                                                ); }
		bool C_ClyBraceList (ParseNode& p) { return G_Req<1,0,1>     (p, id_ClyBraceList, C_CurlyOpen, C_LinearElems, C_CurlyClose                                     ); }
		bool C_BraceList    (ParseNode& p) { return G_Choice         (p, id_Append,       C_RndBraceList, C_SqBraceList, C_AngBraceList, C_ClyBraceList                ); }

		// Hierarchical list - similar as in the linear list example, but each following line is a sub-list. Therefore, "d" and the final "()" cannot be expressed:
		//     a b:
		//         c
		//         e f:
		//             g h ()
		bool C_HLCont1      (ParseNode& p) { return G_Req<1,1,1>     (p, id_Append,       C_HWsC_Nl, C_NextIndent, C_OuterList                                         ); }
		bool C_HLContN      (ParseNode& p) { return G_Repeat         (p, id_Append,       G_Req<C_NextIndent, C_OuterList>                                             ); }
		bool C_HierarList   (ParseNode& p) { return G_Req<1,1,1,1>   (p, id_HierarList,   C_LinearElems, G_Req01<C_HWs, C_Colon>, C_HLCont1, C_HLContN                 ); }

		// Top-most entities
		bool C_OuterList    (ParseNode& p) { return G_Choice         (p, id_Append,       C_HierarList, C_LinearList                                                   ); }
		bool C_OuterLists   (ParseNode& p) { return G_Repeat         (p, id_Append,       G_Req01<G_Repeat<C_HWsC_Nl>, C_OuterList>                                    ); }
		bool C_SourceFile   (ParseNode& p) { return G_OneOrMoreOf    (p, id_Append,       G_Repeat<C_OuterLists>, G_Repeat<C_HWsC_Nl>                                  ); }


		bool C_NextIndent(ParseNode& p)
		{
			if (!Ascii::IsHorizWhitespace(p.Remaining().FirstByte()))
				return false;

			// Find parent OuterList
			ParseNode const& parentList { p.FindAncestorRef(id_OuterList) };
			sizet parentCol             { parentList.StartCol()           };
			sizet expectCol             { p.Tree().ApplyTab(parentCol)    };

			// Consume horizontal whitespace
			ParseNode* pn = p.NewChild(id_Append);
			if (!pn)
				return false;

			EnsureThrow(C_HWs(*pn));

			if (pn->ToCol() != expectCol)
				return p.FailChild(pn);

			return p.CommitChild(pn);
		}

	}
}
