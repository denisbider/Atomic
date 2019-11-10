#include "AtIncludes.h"
#include "AtCssGrammar.h"

namespace At
{
	namespace Css
	{
		// Implemented according to CSS Syntax Module Level 3, W3C Candidate Recommendation, 20 February 2014, at: https://www.w3.org/TR/css-syntax-3/
		// and/or CSS Syntax Module Level 3, Editor's Draft, 25 January 2019, at: https://drafts.csswg.org/css-syntax/

		using namespace Parse;

		DEF_RUID_B(WhitespaceTkn)
		DEF_RUID_B(Comment)
		DEF_RUID_B(WsComments)
		DEF_RUID_B(StrChars)
		DEF_RUID_X(SqStrChars, id_StrChars)
		DEF_RUID_X(DqStrChars, id_StrChars)
		DEF_RUID_B(StrEscSeq)
		DEF_RUID_B(StrLineCont)
		DEF_RUID_B(String)
		DEF_RUID_X(SqString, id_String)
		DEF_RUID_X(DqString, id_String)
		DEF_RUID_B(Name)
		DEF_RUID_B(Ident)
		DEF_RUID_B(HashTkn)
		DEF_RUID_X(HashTknUnr, id_HashTkn)
		DEF_RUID_X(HashTknId, id_HashTkn)
		DEF_RUID_B(Number)
		DEF_RUID_B(DimensionTkn)
		DEF_RUID_B(PercentageTkn)
		DEF_RUID_B(CDO)
		DEF_RUID_B(CDC)
		DEF_RUID_B(UrlInner)
		DEF_RUID_B(UrlTkn)
		DEF_RUID_B(FunctionTkn)
		DEF_RUID_B(AtKwTkn)
		DEF_RUID_B(DelimTkn)

		inline bool IsNewLineChar   (uint c) { return c==10 || c==12 || c==13; }
		inline bool IsCssWsChar     (uint c) { return c==9 || c==32 || IsNewLineChar(c); }
		inline bool IsNameStartChar (uint c) { return Ascii::IsAlpha(c) || c=='_' || c>=0x80 || c==0; }		// NUL is treated as non-ASCII because the spec converts it to 0xFFFD in preprocessing
		inline bool IsNameChar      (uint c) { return IsNameStartChar(c) || Ascii::IsDecDigit(c) || c=='-'; }
		inline bool IsDelimTknChar  (uint c) { return !IsCssWsChar(c) && !Ascii::IsDecDigit(c) && !IsNameStartChar(c) && !ZChr("\"'(),:;[]\\{}", c); }
		inline bool IsUrlInnerChar  (uint c) { return c>=32 && c!=127 && c!='"' && c!='\'' && c!='(' && c!=')' && c!='\\'; }

		bool V_CssWsChar          (ParseNode& p) { return V_ByteIf           (p, IsCssWsChar                                                                                                            ); }
		bool V_NewLineNonCr       (ParseNode& p) { return V_ByteIf           (p, [] (uint c) { return c==10 || c==12; }                                                                                 ); }
		bool V_NotNewLineOrHex    (ParseNode& p) { return V_Utf8CharIf       (p, [] (uint c) { return !IsNewLineChar(c) && !Ascii::IsHexDigit(c); }                                                     ); }
		bool V_CommentNonStarChar (ParseNode& p) { return V_Utf8CharIf       (p, [] (uint c) { return c!='*'; }                                                                                         ); }
		bool V_SqStrChar          (ParseNode& p) { return V_Utf8CharIf       (p, [] (uint c) { return c!='\'' && c!='\\' && !IsNewLineChar(c); }                                                        ); }
		bool V_DqStrChar          (ParseNode& p) { return V_Utf8CharIf       (p, [] (uint c) { return c!='"'  && c!='\\' && !IsNewLineChar(c); }                                                        ); }
		bool V_NameStartChar      (ParseNode& p) { return V_Utf8CharIf       (p, IsNameStartChar                                                                                                        ); }
		bool V_NameChar           (ParseNode& p) { return V_Utf8CharIf       (p, IsNameChar                                                                                                             ); }
		bool V_DelimTknChar       (ParseNode& p) { return V_Utf8CharIf       (p, IsDelimTknChar                                                                                                         ); }
		bool V_UrlInnerChar       (ParseNode& p) { return V_Utf8CharIf       (p, IsUrlInnerChar                                                                                                         ); }
		bool V_KwUrl              (ParseNode& p) { return V_SeqMatchInsens   (p, "url"                                                                                                                  ); }
																		     
		bool V_NewLineSingle      (ParseNode& p) { return G_Choice           (p, id_Append, V_CRLF, V_NewLineNonCr, V_CrNfbLf                                                                           ); }
		bool V_CommentStarSeq     (ParseNode& p) { return G_NotFollowedBy    (p, id_Append, G_Repeat<V_Star>, V_Slash                                                                                   ); }
		bool V_CommentChars       (ParseNode& p) { return G_Repeat           (p, id_Append, G_Choice<V_CommentNonStarChar, V_CommentStarSeq>                                                            ); }
		bool V_Comment            (ParseNode& p) { return G_Req<1,0,1>       (p, id_Append, V_SlashStar, V_CommentChars, V_StarSlash                                                                    ); }
																			   																												        
		bool V_EscHex             (ParseNode& p) { return G_Req<1,0>         (p, id_Append, G_Repeat<V_AsciiHexDigit, 1, 6>, V_CssWsChar                                                                ); }
		bool V_EscSeq             (ParseNode& p) { return G_Req<1,1>         (p, id_Append, V_Backslash, G_Choice<V_EscHex, V_NotNewLineOrHex>                                                          ); }
		bool V_StrLineCont        (ParseNode& p) { return G_Req<1,1>         (p, id_Append, V_Backslash, V_NewLineSingle                                                                                ); }
		bool V_Name               (ParseNode& p) { return G_Repeat           (p, id_Append, G_Choice<V_NameChar, V_EscSeq>                                                                              ); }
		bool V_IdentStart         (ParseNode& p) { return G_Choice           (p, id_Append, V_DblDash, G_Req<V_Dash, G_Choice<V_NameStartChar, V_EscSeq>>                                               ); }
		bool V_Ident              (ParseNode& p) { return G_Req<1,0>         (p, id_Append, V_IdentStart, V_Name                                                                                        ); }
																		     
		bool V_Digits             (ParseNode& p) { return G_Repeat           (p, id_Append, V_AsciiDecDigit                                                                                             ); }
		bool V_IntegerAndFrac     (ParseNode& p) { return G_Req<0,1,1>       (p, id_Append, V_Digits, V_Dot, V_Digits                                                                                   ); }
		bool V_Exponent           (ParseNode& p) { return G_Req<1,0,1>       (p, id_Append, G_Choice<V_ByteIs<'E'>, V_ByteIs<'e'>>, G_Choice<V_Plus, V_Dash>, V_Digits                                  ); }
		bool V_Number             (ParseNode& p) { return G_Req<0,1,0>       (p, id_Append, G_Choice<V_Plus, V_Dash>, G_Choice<V_IntegerAndFrac, V_Digits>, V_Exponent                                  ); }

		bool V_UrlInner           (ParseNode& p) { return G_Repeat           (p, id_Append, G_Choice<V_UrlInnerChar, V_EscSeq>                                                                          ); }

		bool C_WhitespaceTkn      (ParseNode& p) { return G_Repeat           (p, id_WhitespaceTkn,   V_CssWsChar                                                                                        ); }
		bool C_KwUrl              (ParseNode& p) { return G_Req              (p, id_Ident,           V_KwUrl                                                                                            ); }
		bool C_Comment            (ParseNode& p) { return G_Req              (p, id_Comment,         V_Comment                                                                                          ); }
		bool C_EscSeq             (ParseNode& p) { return G_Req              (p, id_StrEscSeq,       V_EscSeq                                                                                           ); }
		bool C_Name               (ParseNode& p) { return G_Req              (p, id_Name,            V_Name                                                                                             ); }
		bool C_Ident              (ParseNode& p) { return G_Req              (p, id_Ident,           V_Ident                                                                                            ); }
																			   				 																				               
		bool C_SqStrChars         (ParseNode& p) { return G_Repeat           (p, id_SqStrChars,      V_SqStrChar                                                                                        ); }
		bool C_DqStrChars         (ParseNode& p) { return G_Repeat           (p, id_DqStrChars,      V_DqStrChar                                                                                        ); }
		bool C_StrLineCont        (ParseNode& p) { return G_Req              (p, id_StrLineCont,     V_StrLineCont                                                                                      ); }
		bool C_SqStrElem          (ParseNode& p) { return G_Choice           (p, id_Append,          C_SqStrChars, C_EscSeq, C_StrLineCont                                                              ); }
		bool C_DqStrElem          (ParseNode& p) { return G_Choice           (p, id_Append,          C_DqStrChars, C_EscSeq, C_StrLineCont                                                              ); }
		bool C_SqString           (ParseNode& p) { return G_Req<1,0,1>       (p, id_SqString,        C_Apos, G_Repeat<C_SqStrElem>, C_Apos                                                              ); }
		bool C_DqString           (ParseNode& p) { return G_Req<1,0,1>       (p, id_DqString,        C_DblQuote, G_Repeat<C_DqStrElem>, C_DblQuote                                                      ); }
		bool C_String             (ParseNode& p) { return G_Choice           (p, id_Append,          C_SqString, C_DqString                                                                             ); }

		bool C_WsComments         (ParseNode& p) { return G_Repeat           (p, id_WsComments,      G_Choice<C_Comment, C_WhitespaceTkn>                                                               ); }
		bool C_CDO                (ParseNode& p) { return G_Req<1,1,1,1>     (p, id_CDO,             V_ByteIs<'<'>, V_ByteIs<'!'>, V_ByteIs<'-'>, V_ByteIs<'-'>                                         ); }
		bool C_CDC                (ParseNode& p) { return G_Req<1,1,1>       (p, id_CDC,             V_ByteIs<'-'>, V_ByteIs<'-'>, V_ByteIs<'>'>                                                        ); }
		bool C_Bracket            (ParseNode& p) { return G_Choice           (p, id_Append,          C_OpenBr, C_CloseBr, C_SqOpenBr, C_SqCloseBr, C_CurlyOpen, C_CurlyClose                            ); }
		bool C_DelimSpecial       (ParseNode& p) { return G_Choice           (p, id_Append,          C_CDO, C_CDC, C_Bracket, C_Comma, C_Colon, C_Semicolon                                             ); }
		bool C_DelimTkn           (ParseNode& p) { return G_Req              (p, id_DelimTkn,        V_DelimTknChar                                                                                     ); }
		bool C_HashTknId          (ParseNode& p) { return G_Req<1,1>         (p, id_HashTknId,       C_Hash, C_Ident                                                                                    ); }
		bool C_HashTknUnr         (ParseNode& p) { return G_Req<1,1>         (p, id_HashTknId,       C_Hash, C_Name                                                                                     ); }
		bool C_HashTkn            (ParseNode& p) { return G_Choice           (p, id_Append,          C_HashTknId, C_HashTknUnr                                                                          ); }
		bool C_Number             (ParseNode& p) { return G_Req              (p, id_Number,          V_Number                                                                                           ); }
		bool C_DimensionTkn       (ParseNode& p) { return G_Req<1,1>         (p, id_DimensionTkn,    C_Number, C_Ident                                                                                  ); }
		bool C_PercentageTkn      (ParseNode& p) { return G_Req<1,1>         (p, id_PercentageTkn,   C_Number, C_Percent                                                                                ); }
		bool C_Numeric            (ParseNode& p) { return G_Choice           (p, id_Append,          C_DimensionTkn, C_PercentageTkn, C_Number                                                          ); }
		bool C_UrlInner           (ParseNode& p) { return G_Req              (p, id_UrlInner,        V_UrlInner                                                                                         ); }
		bool C_UrlTkn             (ParseNode& p) { return G_Req<1,1,0,0,0,1> (p, id_UrlTkn,          C_KwUrl, C_OpenBr, C_WhitespaceTkn, C_UrlInner, C_WhitespaceTkn, C_CloseBr                         ); }
		bool C_FunctionTkn        (ParseNode& p) { return G_Req<1,1>         (p, id_FunctionTkn,     C_Name, C_OpenBr                                                                                   ); }
		bool C_IdentLike          (ParseNode& p) { return G_Choice           (p, id_Append,          C_UrlTkn, C_FunctionTkn, C_Name                                                                    ); }
		bool C_AtKwTkn            (ParseNode& p) { return G_Req<1,1>         (p, id_AtKwTkn,         C_At, C_Ident                                                                                      ); }
		bool C_Token              (ParseNode& p) { return G_Choice           (p, id_Append,          C_WsComments, C_String, C_HashTkn, C_Numeric, C_IdentLike, C_AtKwTkn, C_DelimSpecial, C_DelimTkn   ); }
		bool C_Tokens             (ParseNode& p) { return G_Repeat           (p, id_Append,          C_Token                                                                                            ); }
	}
}
