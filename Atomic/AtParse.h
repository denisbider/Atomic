#pragma once

#include "AtParseTree.h"

namespace At
{

	// Parse functions
	//
	// There are three types of parsing functions:
	// - Value parsers are prefixed "V_". These parser functions add to the character value of the parameter parser.
	//   This means that the parameter parser must itself be a value parser - or empty, and thus becomes a value parser.
	// - Constructed parsers are prefixed "C_". These parser functions add to the child parser list of the parameter parser.
	//   This means that the parameter parser must itself be a constructed parser - or empty, and thus becomes a constructed parser.
	// - Neutral parsers are prefixed "N_". Such a parser does not generate a result, and can thus be combined with either constructed
	//   or value parsers. Since a parser that does not generate a result must not consume input, the only possible neutral parser is
	//   one that succeeds if something does NOT appear in the input stream, or that succeeds if the input stream ends.
	// - Generic parsers are prefixed "G_". These parser functions take constructed or value parser functions as parameters,
	//   and will combine them in various ways. Generic parsers generally take a Ruid parameter which can be used with
	//   id_Append to indicate that the generic parser function's results should be appended to the parameter parser's existing
	//   children. Any other type indicates that the generic parser's function results should be added as a single node to the list of
	//   parameter parser's children. In that case, the type parameter indicates the type of this node.
	//
	// Contract:
	// - A parser function that succeeds (returns true) must advance its parameter parser, and must add to the parameter parser,
	//   either directly using ConsumeXxxx(), or indirectly using CommitChild(), all bytes that it has advanced over in sequence,
	//   and each byte exactly once.
	// - A parser function may also succeed without consuming any bytes. However, a parser of this type may not be included in a loop.
	// - A parser function that fails (returns false) must NOT advance its parameter parser, nor add anything to the parameter parser.
	//   However, a constructed parser function that fails SHOULD call FailChild(). If parsing of the full entity fails, the best
	//   incomplete result will offer useful diagnostic information about the reason for failure.
	// - A parser function that uses other parser functions should rely on those functions adhering to this contract.

	namespace Parse
	{
		// Neutral
	
		bool N_End(ParseNode& p);

	
		// Generic
	
		bool G_Req(ParseNode& p, Ruid const& type, ParseFunc pf);
	
		template <int R1, int R2>                                 bool G_Req(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2);
		template <int R1, int R2, int R3>                         bool G_Req(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3);
		template <int R1, int R2, int R3, int R4>                 bool G_Req(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4);
		template <int R1, int R2, int R3, int R4, int R5>         bool G_Req(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5);
		template <int R1, int R2, int R3, int R4, int R5, int R6> bool G_Req(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5, ParseFunc pf6);

		template <bool (*F1)(ParseNode&), bool (*F2)(ParseNode&)> inline bool G_Req01(ParseNode& p) { return G_Req<0,1>(p, id_Append, F1, F2); }
		template <bool (*F1)(ParseNode&), bool (*F2)(ParseNode&)> inline bool G_Req10(ParseNode& p) { return G_Req<1,0>(p, id_Append, F1, F2); }
		template <bool (*F1)(ParseNode&), bool (*F2)(ParseNode&)> inline bool G_Req  (ParseNode& p) { return G_Req<1,1>(p, id_Append, F1, F2); }
	
		bool G_Repeat(ParseNode& p, Ruid const& type, ParseFunc pf, sizet minCount = 1, sizet maxCount = SIZE_MAX);
		template <bool (*F)(ParseNode&), sizet MinCount=1, sizet MaxCount=SIZE_MAX> inline bool G_Repeat(ParseNode& p) { return G_Repeat(p, id_Append, F, MinCount, MaxCount); }
	
		bool G_UntilExcl(ParseNode& p, Ruid const& type, ParseFunc pfRepeat, ParseFunc pfUntil, sizet minCount = 1, sizet maxCount = SIZE_MAX);
		template <bool (*FR)(ParseNode&), bool (*FU)(ParseNode&)> inline bool G_UntilExcl(ParseNode& p) { return G_UntilExcl(p, id_Append, FR, FU); }
	
		bool G_Choice(ParseNode& p, Ruid const& type, ParseFunc* pfArray);
		bool G_Choice(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2);
		bool G_Choice(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3);
		bool G_Choice(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4);
		bool G_Choice(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5);
		bool G_Choice(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5, ParseFunc pf6);
		bool G_Choice(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5, ParseFunc pf6, ParseFunc pf7);
		bool G_Choice(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5, ParseFunc pf6, ParseFunc pf7, ParseFunc pf8);
		bool G_Choice(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5, ParseFunc pf6, ParseFunc pf7, ParseFunc pf8, ParseFunc pf9);
		bool G_Choice(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5, ParseFunc pf6, ParseFunc pf7, ParseFunc pf8, ParseFunc pf9, ParseFunc pf10);
		bool G_Choice(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5, ParseFunc pf6, ParseFunc pf7, ParseFunc pf8, ParseFunc pf9, ParseFunc pf10, ParseFunc pf11);
		bool G_Choice(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5, ParseFunc pf6, ParseFunc pf7, ParseFunc pf8, ParseFunc pf9, ParseFunc pf10, ParseFunc pf11, ParseFunc pf12);

		template <bool (*F1)(ParseNode&), bool (*F2)(ParseNode&)>                                                                         inline bool G_Choice(ParseNode& p) { return G_Choice(p, id_Append, F1, F2); }
		template <bool (*F1)(ParseNode&), bool (*F2)(ParseNode&), bool (*F3)(ParseNode&)>                                                 inline bool G_Choice(ParseNode& p) { return G_Choice(p, id_Append, F1, F2, F3); }
		template <bool (*F1)(ParseNode&), bool (*F2)(ParseNode&), bool (*F3)(ParseNode&), bool (*F4)(ParseNode&)>                         inline bool G_Choice(ParseNode& p) { return G_Choice(p, id_Append, F1, F2, F3, F4); }
		template <bool (*F1)(ParseNode&), bool (*F2)(ParseNode&), bool (*F3)(ParseNode&), bool (*F4)(ParseNode&), bool (*F5)(ParseNode&)> inline bool G_Choice(ParseNode& p) { return G_Choice(p, id_Append, F1, F2, F3, F4, F5); }

		template <bool (*F)(ParseNode&)> inline bool G_OptIfEnd(ParseNode& p) { return G_Choice(p, id_Append, F, N_End); }

		bool G_OneOrMoreOf(ParseNode& p, Ruid const& type, ParseFunc* pfArray);
		bool G_OneOrMoreOf(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2);
		bool G_OneOrMoreOf(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3);
		bool G_OneOrMoreOf(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4);
		bool G_OneOrMoreOf(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2, ParseFunc pf3, ParseFunc pf4, ParseFunc pf5);

		bool G_Not(ParseNode& p, ParseFunc pfNot, ParseFunc pfYes);
		template <bool (*FN)(ParseNode&), bool (*FY)(ParseNode&)> inline bool G_Not(ParseNode& p) { return G_Not(p, FN, FY); }

		bool G_NotFollowedBy(ParseNode& p, Ruid const& type, ParseFunc pf1, ParseFunc pf2);
		template <bool (*F1)(ParseNode&), bool (*F2)(ParseNode&)> inline bool G_NotFollowedBy(ParseNode& p) { return G_NotFollowedBy(p, id_Append, F1, F2); }

		bool G_SeqMatch(ParseNode& p, Ruid const& type, CaseMatch cm, Seq s);		
		inline bool G_SeqMatchExact (ParseNode& p, Ruid const& type, Seq s) { return G_SeqMatch(p, type, CaseMatch::Exact, s); }
		inline bool G_SeqMatchInsens(ParseNode& p, Ruid const& type, Seq s) { return G_SeqMatch(p, type, CaseMatch::Insensitive, s); }

	
		// Value
	
		typedef bool (*CharPredicate)(uint);
		bool V_ByteIf(ParseNode& p, CharPredicate pred);
		template <bool (*P)(uint)> inline bool V_ByteIf(ParseNode& p) { return V_ByteIf(p, P); }

		inline bool V_ByteAny(ParseNode& p) { if (!p.HaveByte()) return false; p.ConsumeByte(); return true; }
	
		inline bool V_AsciiWhitespace      (ParseNode& p) { return V_ByteIf(p, Ascii::IsWhitespace); }
		inline bool V_AsciiHorizWhitespace (ParseNode& p) { return V_ByteIf(p, Ascii::IsHorizWhitespace); }
		inline bool V_AsciiDecDigit        (ParseNode& p) { return V_ByteIf(p, Ascii::IsDecDigit); }
		inline bool V_AsciiHexDigit        (ParseNode& p) { return V_ByteIf(p, Ascii::IsHexDigit); }
		inline bool V_AsciiAlphaUpper      (ParseNode& p) { return V_ByteIf(p, Ascii::IsAlphaUpper); }
		inline bool V_AsciiAlphaLower      (ParseNode& p) { return V_ByteIf(p, Ascii::IsAlphaLower); }
		inline bool V_AsciiAlpha           (ParseNode& p) { return V_ByteIf(p, Ascii::IsAlpha); }
		inline bool V_AsciiAlphaNum        (ParseNode& p) { return V_ByteIf(p, Ascii::IsAlphaNum); }
		inline bool V_Ws                   (ParseNode& p) { return G_Repeat(p, id_Append, V_AsciiWhitespace); }
		inline bool V_HWs                  (ParseNode& p) { return G_Repeat(p, id_Append, V_AsciiHorizWhitespace); }

		bool V_ByteIs(ParseNode& p, byte c);
		template <byte B> inline bool V_ByteIs(ParseNode& p) { return V_ByteIs(p, B); }

		bool V_ByteIsOf     (ParseNode& p, char const* z);
		bool V_ByteNfb      (ParseNode& p, byte c, byte other);					// Not followed by
		bool V_ByteNfbOneOf (ParseNode& p, byte c, char const* z);				// Not followed by
		bool V_ByteNfbSeq   (ParseNode& p, byte c, Seq s, CaseMatch cm);		// Not followed by

		inline bool V_CrNfbLf    (ParseNode& p) { return V_ByteNfb(p, '\r', '\n'); }

		bool V_Utf8CharIf(ParseNode& p, CharPredicate pred);
		inline bool V_Utf8CharAny(ParseNode& p) { return V_Utf8CharIf(p, [] (uint) -> bool { return true; } ); }

		inline bool V_At         (ParseNode& p) { return V_ByteIs(p, '@' ); }
		inline bool V_Cr         (ParseNode& p) { return V_ByteIs(p, '\r'); }
		inline bool V_Eq         (ParseNode& p) { return V_ByteIs(p, '=' ); }
		inline bool V_Lf         (ParseNode& p) { return V_ByteIs(p, '\n'); }
		inline bool V_Amp        (ParseNode& p) { return V_ByteIs(p, '&' ); }
		inline bool V_Dot        (ParseNode& p) { return V_ByteIs(p, '.' ); }
		inline bool V_Apos       (ParseNode& p) { return V_ByteIs(p, '\''); }
		inline bool V_Bang       (ParseNode& p) { return V_ByteIs(p, '!' ); }
		inline bool V_Dash       (ParseNode& p) { return V_ByteIs(p, '-' ); }
		inline bool V_Less       (ParseNode& p) { return V_ByteIs(p, '<' ); }
		inline bool V_Grtr       (ParseNode& p) { return V_ByteIs(p, '>' ); }
		inline bool V_Hash       (ParseNode& p) { return V_ByteIs(p, '#' ); }
		inline bool V_Plus       (ParseNode& p) { return V_ByteIs(p, '+' ); }
		inline bool V_Star       (ParseNode& p) { return V_ByteIs(p, '*' ); }
		inline bool V_Comma      (ParseNode& p) { return V_ByteIs(p, ',' ); }
		inline bool V_Colon      (ParseNode& p) { return V_ByteIs(p, ':' ); }
		inline bool V_Slash      (ParseNode& p) { return V_ByteIs(p, '/' ); }
		inline bool V_Space      (ParseNode& p) { return V_ByteIs(p, ' ' ); }
		inline bool V_Tilde      (ParseNode& p) { return V_ByteIs(p, '~' ); }
		inline bool V_Percent    (ParseNode& p) { return V_ByteIs(p, '%' ); }
		inline bool V_Backtick   (ParseNode& p) { return V_ByteIs(p, '`' ); }
		inline bool V_DblQuote   (ParseNode& p) { return V_ByteIs(p, '"' ); }
		inline bool V_SglQuote   (ParseNode& p) { return V_ByteIs(p, '\''); }
		inline bool V_Backslash  (ParseNode& p) { return V_ByteIs(p, '\\'); }
		inline bool V_Semicolon  (ParseNode& p) { return V_ByteIs(p, ';' ); }
		inline bool V_CurlyOpen  (ParseNode& p) { return V_ByteIs(p, '{' ); }
		inline bool V_CurlyClose (ParseNode& p) { return V_ByteIs(p, '}' ); }
		inline bool V_OpenBr     (ParseNode& p) { return V_ByteIs(p, '(' ); }
		inline bool V_CloseBr    (ParseNode& p) { return V_ByteIs(p, ')' ); }
		inline bool V_QuestnMark (ParseNode& p) { return V_ByteIs(p, '?' ); }
		inline bool V_SqOpenBr   (ParseNode& p) { return V_ByteIs(p, '[' ); }
		inline bool V_SqCloseBr  (ParseNode& p) { return V_ByteIs(p, ']' ); }
		inline bool V_Underscore (ParseNode& p) { return V_ByteIs(p, '_' ); }

		bool V_Remaining(ParseNode& p);

		bool V_SeqMatch(ParseNode& p, CaseMatch cm, Seq s);		
		inline bool V_SeqMatchExact (ParseNode& p, Seq s) { return V_SeqMatch(p, CaseMatch::Exact,       s); }
		inline bool V_SeqMatchInsens(ParseNode& p, Seq s) { return V_SeqMatch(p, CaseMatch::Insensitive, s); }

		inline bool V_CRLF      (ParseNode& p) { return V_SeqMatchExact(p, "\r\n" ); }
		inline bool V_DblAnd    (ParseNode& p) { return V_SeqMatchExact(p, "&&" ); }
		inline bool V_DblPipe   (ParseNode& p) { return V_SeqMatchExact(p, "||" ); }
		inline bool V_DblEq     (ParseNode& p) { return V_SeqMatchExact(p, "==" ); }
		inline bool V_DblDash   (ParseNode& p) { return V_SeqMatchExact(p, "--" ); }
		inline bool V_DblSlash  (ParseNode& p) { return V_SeqMatchExact(p, "//" ); }
		inline bool V_SlashStar (ParseNode& p) { return V_SeqMatchExact(p, "/*" ); }
		inline bool V_StarSlash (ParseNode& p) { return V_SeqMatchExact(p, "*/" ); }
		inline bool V_Cmp       (ParseNode& p) { return V_SeqMatchExact(p, "<=>"); }
		inline bool V_Shl       (ParseNode& p) { return V_SeqMatchExact(p, "<<" ); }
		inline bool V_Shr       (ParseNode& p) { return V_SeqMatchExact(p, ">>" ); }
		inline bool V_LessEq    (ParseNode& p) { return V_SeqMatchExact(p, "<=" ); }
		inline bool V_GrtrEq    (ParseNode& p) { return V_SeqMatchExact(p, ">=" ); }

		bool V_HWs_Indent(ParseNode& p, sizet minNrCols, sizet maxNrCols);
		template <int Min, int Max> inline bool V_HWs_Indent(ParseNode& p) { return V_HWs_Indent(p, Min, Max); }

	
	
		// Constructed

		DECL_RUID(Ws)
		DECL_RUID(At)
		DECL_RUID(Eq)
		DECL_RUID(And)
		DECL_RUID(Dot)
		DECL_RUID(HWs)
		DECL_RUID(Apos)
		DECL_RUID(Bang)
		DECL_RUID(CRLF)
		DECL_RUID(Dash)
		DECL_RUID(Less)
		DECL_RUID(Grtr)
		DECL_RUID(Hash)
		DECL_RUID(Plus)
		DECL_RUID(Comma)
		DECL_RUID(Colon)
		DECL_RUID(Slash)
		DECL_RUID(Percent)
		DECL_RUID(Backtick)
		DECL_RUID(DblQuote)
		DECL_RUID(SglQuote)
		DECL_RUID(Backslash)
		DECL_RUID(Semicolon)
		DECL_RUID(CurlyOpen)
		DECL_RUID(CurlyClose)
		DECL_RUID(OpenBr)
		DECL_RUID(CloseBr)
		DECL_RUID(QuestnMark)
		DECL_RUID(SqOpenBr)
		DECL_RUID(SqCloseBr)
		DECL_RUID(Remaining)
	
		inline bool C_Ws         (ParseNode& p) { return G_Repeat (p, id_Ws,             V_AsciiWhitespace      ); }
		inline bool C_At         (ParseNode& p) { return G_Req    (p, id_At,             V_At                   ); }
		inline bool C_Eq         (ParseNode& p) { return G_Req    (p, id_Eq,             V_Eq                   ); }
		inline bool C_Amp        (ParseNode& p) { return G_Req    (p, id_And,            V_Amp                  ); }
		inline bool C_Dot        (ParseNode& p) { return G_Req    (p, id_Dot,            V_Dot                  ); }
		inline bool C_HWs        (ParseNode& p) { return G_Repeat (p, id_HWs,            V_AsciiHorizWhitespace ); }
		inline bool C_Apos       (ParseNode& p) { return G_Req    (p, id_Apos,           V_Apos                 ); }
		inline bool C_Bang       (ParseNode& p) { return G_Req    (p, id_Bang,           V_Bang                 ); }
		inline bool C_CRLF       (ParseNode& p) { return G_Req    (p, id_CRLF,           V_CRLF                 ); }
		inline bool C_Dash       (ParseNode& p) { return G_Req    (p, id_Dash,           V_Dash                 ); }
		inline bool C_Less       (ParseNode& p) { return G_Req    (p, id_Less,           V_Less                 ); }
		inline bool C_Grtr       (ParseNode& p) { return G_Req    (p, id_Grtr,           V_Grtr                 ); }
		inline bool C_Hash       (ParseNode& p) { return G_Req    (p, id_Hash,           V_Hash                 ); }
		inline bool C_Plus       (ParseNode& p) { return G_Req    (p, id_Plus,           V_Plus                 ); }
		inline bool C_Comma      (ParseNode& p) { return G_Req    (p, id_Comma,          V_Comma                ); }
		inline bool C_Colon      (ParseNode& p) { return G_Req    (p, id_Colon,          V_Colon                ); }
		inline bool C_Slash      (ParseNode& p) { return G_Req    (p, id_Slash,          V_Slash                ); }
		inline bool C_Percent    (ParseNode& p) { return G_Req    (p, id_Percent,        V_Percent              ); }
		inline bool C_Backtick   (ParseNode& p) { return G_Req    (p, id_Backtick,       V_Backtick             ); }
		inline bool C_DblQuote   (ParseNode& p) { return G_Req    (p, id_DblQuote,       V_DblQuote             ); }
		inline bool C_SglQuote   (ParseNode& p) { return G_Req    (p, id_SglQuote,       V_SglQuote             ); }
		inline bool C_Backslash  (ParseNode& p) { return G_Req    (p, id_Backslash,      V_Backslash            ); }
		inline bool C_Semicolon  (ParseNode& p) { return G_Req    (p, id_Semicolon,      V_Semicolon            ); }
		inline bool C_CurlyOpen  (ParseNode& p) { return G_Req    (p, id_CurlyOpen,      V_CurlyOpen            ); }
		inline bool C_CurlyClose (ParseNode& p) { return G_Req    (p, id_CurlyClose,     V_CurlyClose           ); }
		inline bool C_OpenBr     (ParseNode& p) { return G_Req    (p, id_OpenBr,         V_OpenBr               ); }
		inline bool C_CloseBr    (ParseNode& p) { return G_Req    (p, id_CloseBr,        V_CloseBr              ); }
		inline bool C_QuestnMark (ParseNode& p) { return G_Req    (p, id_QuestnMark,     V_QuestnMark           ); }
		inline bool C_SqOpenBr   (ParseNode& p) { return G_Req    (p, id_SqOpenBr,       V_SqOpenBr             ); }
		inline bool C_SqCloseBr  (ParseNode& p) { return G_Req    (p, id_SqCloseBr,      V_SqCloseBr            ); }
		inline bool C_Remaining  (ParseNode& p) { return G_Req    (p, id_Remaining,      V_Remaining            ); }

		bool C_UntilIncl(ParseNode& p, Ruid const& type, ParseFunc pfRepeat, Ruid const& untilType, sizet minCount = 1, sizet maxCount = SIZE_MAX);
		template <bool (*FR)(ParseNode&), Ruid const& U> inline bool C_UntilIncl(ParseNode& p) { return C_UntilIncl(p, id_Append, FR, U); }

		bool C_HWs_Indent(ParseNode& p, Ruid const& type, sizet minNrCols, sizet maxNrCols);
		template <int Min, int Max> inline bool C_HWs_Indent(ParseNode& p) { return C_HWs_Indent(p, id_HWs, Min, Max); }
	}
}
