#include "AtIncludes.h"
#include "AtJsPackGrammar.h"

// A vastly simplified JavaScript "grammar", for packing purposes only
// Implemented according to ECMAScript 2020 Language Specification, Draft April 12, 2019, at: https://tc39.github.io/ecma262/
// Note: This does NOT attempt to handle regular expressions correctly. Regular expressions may cause parsing errors or incorrect output.

namespace At
{
namespace JsPack
{

	using namespace Parse;

	DEF_RUID_B(WsNlComments)
	DEF_RUID_B(StrChars)
	DEF_RUID_X(SqStrChars, id_StrChars)
	DEF_RUID_X(DqStrChars, id_StrChars)
	DEF_RUID_B(StrEscSeq)
	DEF_RUID_B(StrLineCont)
	DEF_RUID_B(String)
	DEF_RUID_X(SqString, id_String)
	DEF_RUID_X(DqString, id_String)
	DEF_RUID_B(Other)
	DEF_RUID_B(TmplChars)
	DEF_RUID_B(TmplEsc)
	DEF_RUID_B(TmplInsertOpen)
	DEF_RUID_B(TmplInsertBody)
	DEF_RUID_B(TmplInsert)
	DEF_RUID_B(Template)

	inline bool IsJsWsChar        (uint c) { return c==9 || c==11 || c==12 || c==32 || c==0xFEFF || Unicode::IsCat_SepSpace(c); }
	inline bool IsLineTermChar    (uint c) { return c==10 || c==13 || c==0x2028 || c==0x2029; }

	bool V_JsWsChar          (ParseNode& p) { return V_Utf8CharIf    (p, IsJsWsChar                                                                                                        ); }
	bool V_NotLineTermChar   (ParseNode& p) { return V_Utf8CharIf    (p, [] (uint c) { return !IsLineTermChar(c); }                                                                        ); }
	bool V_LineTermNonCr     (ParseNode& p) { return V_Utf8CharIf    (p, [] (uint c) { return c==10 || c==0x2028 || c==0x2029; }                                                           ); }
	bool V_MlcNonStarChar    (ParseNode& p) { return V_Utf8CharIf    (p, [] (uint c) { return c!='*'; }                                                                                    ); }
	bool V_EscSingleChar     (ParseNode& p) { return V_Utf8CharIf    (p, [] (uint c) { return c!='x' && c!='u' && !Ascii::IsDecDigit(c) && !IsLineTermChar(c); }                           ); }
	bool V_SqStrChar         (ParseNode& p) { return V_Utf8CharIf    (p, [] (uint c) { return c!='\'' && c!='\\' && !IsLineTermChar(c); }                                                  ); }
	bool V_DqStrChar         (ParseNode& p) { return V_Utf8CharIf    (p, [] (uint c) { return c!='"'  && c!='\\' && !IsLineTermChar(c); }                                                  ); }
	bool V_OtherElemChar     (ParseNode& p) { return V_Utf8CharIf    (p, [] (uint c) { return !IsJsWsChar(c) && !IsLineTermChar(c) && !ZChr("/'\"`}", c); }                                ); }
	bool V_TmplCharNotDlr    (ParseNode& p) { return V_Utf8CharIf    (p, [] (uint c) { return c!='`'  && c!='\\' && c!='$'; }                                                              ); }
																																											               
	bool V_LineTermSingle    (ParseNode& p) { return G_Choice        (p, id_Append, V_CRLF, V_LineTermNonCr, V_CrNfbLf                                                                     ); }
	bool V_SingleLineComment (ParseNode& p) { return G_Req<1,0>      (p, id_Append, V_DblSlash, G_Repeat<V_NotLineTermChar>                                                                ); }
	bool V_MlcStarSeq        (ParseNode& p) { return G_NotFollowedBy (p, id_Append, G_Repeat<V_Star>, V_Slash                                                                              ); }
	bool V_MlcChars          (ParseNode& p) { return G_Repeat        (p, id_Append, G_Choice<V_MlcNonStarChar, V_MlcStarSeq>                                                               ); }
	bool V_MultiLineComment  (ParseNode& p) { return G_Req<1,0,1>    (p, id_Append, V_SlashStar, V_MlcChars, V_StarSlash                                                                   ); }
																																											               
	bool V_EscNull           (ParseNode& p) { return V_ByteNfbOneOf  (p, '0', "0123456789"                                                                                                 ); }
	bool V_EscHex            (ParseNode& p) { return G_Req<1,1,1>    (p, id_Append, V_ByteIs<'x'>, V_ByteIf<Ascii::IsHexDigit>, V_ByteIf<Ascii::IsHexDigit>                                ); }
	bool V_EscUniHex         (ParseNode& p) { return G_Req<1,1>      (p, id_Append, V_ByteIs<'u'>, G_Repeat<V_ByteIf<Ascii::IsHexDigit>, 4, 4>                                             ); }
	bool V_EscUniCP          (ParseNode& p) { return G_Req<1,1,1,1>  (p, id_Append, V_ByteIs<'u'>, V_CurlyOpen, G_Repeat<V_ByteIf<Ascii::IsHexDigit>, 1, 6>, V_CurlyClose                  ); }
	bool V_StrEscSeq         (ParseNode& p) { return G_Req<1,1>      (p, id_Append, V_Backslash, G_Choice<V_EscSingleChar, V_EscNull, V_EscHex, V_EscUniHex, V_EscUniCP>                   ); }
	bool V_StrLineCont       (ParseNode& p) { return G_Req<1,1>      (p, id_Append, V_Backslash, V_LineTermSingle                                                                          ); }

	bool V_SlashNonComment   (ParseNode& p) { return V_ByteNfbOneOf  (p, '/', "/*"                                                                                                         ); }
	bool V_OtherElem         (ParseNode& p) { return G_Choice        (p, id_Append, V_OtherElemChar, V_SlashNonComment                                                                     ); }
																																											               
	bool V_TmplCharDlrNfb    (ParseNode& p) { return V_ByteNfb       (p, '$', '{'                                                                                                          ); }
	bool V_TmplChar          (ParseNode& p) { return G_Choice        (p, id_Append, V_TmplCharNotDlr, V_TmplCharDlrNfb                                                                     ); }
	bool V_TmplEsc           (ParseNode& p) { return G_Req<1,1>      (p, id_Append, V_Backslash, G_Choice<V_NotLineTermChar, V_LineTermSingle>                                             ); }
	bool V_TmplInsertOpen    (ParseNode& p) { return V_SeqMatchExact (p, "${"                                                                                                              ); }
																																											               
	bool C_WsNlComments      (ParseNode& p) { return G_Repeat        (p, id_WsNlComments,    G_Choice<V_JsWsChar, V_LineTermSingle, V_SingleLineComment, V_MultiLineComment>               ); }
																							 																				               
	bool C_SqStrChars        (ParseNode& p) { return G_Repeat        (p, id_SqStrChars,      V_SqStrChar                                                                                   ); }
	bool C_DqStrChars        (ParseNode& p) { return G_Repeat        (p, id_DqStrChars,      V_DqStrChar                                                                                   ); }
	bool C_StrEscSeq         (ParseNode& p) { return G_Req           (p, id_StrEscSeq,       V_StrEscSeq                                                                                   ); }
	bool C_StrLineCont       (ParseNode& p) { return G_Req           (p, id_StrLineCont,     V_StrLineCont                                                                                 ); }
	bool C_SqStrElem         (ParseNode& p) { return G_Choice        (p, id_Append,          C_SqStrChars, C_StrEscSeq, C_StrLineCont                                                      ); }
	bool C_DqStrElem         (ParseNode& p) { return G_Choice        (p, id_Append,          C_DqStrChars, C_StrEscSeq, C_StrLineCont                                                      ); }
	bool C_SqString          (ParseNode& p) { return G_Req<1,0,1>    (p, id_SqString,        C_Apos, G_Repeat<C_SqStrElem>, C_Apos                                                         ); }
	bool C_DqString          (ParseNode& p) { return G_Req<1,0,1>    (p, id_DqString,        C_DblQuote, G_Repeat<C_DqStrElem>, C_DblQuote                                                 ); }
																							 																				               
	bool C_Other             (ParseNode& p) { return G_Repeat        (p, id_Other,           V_OtherElem                                                                                   ); }
																							 
	bool C_Template          (ParseNode& p);												 																															               
	bool C_TmplChars         (ParseNode& p) { return G_Repeat        (p, id_TmplChars,       V_TmplChar                                                                                    ); }
	bool C_TmplEsc           (ParseNode& p) { return G_Req           (p, id_TmplEsc,         V_TmplEsc                                                                                     ); }
	bool C_TmplInsertOpen    (ParseNode& p) { return G_Req           (p, id_TmplInsertOpen,  V_TmplInsertOpen                                                                              ); }
	bool C_TmplInsElem       (ParseNode& p) { return G_Choice        (p, id_Append,          C_WsNlComments, C_SqString, C_DqString, C_Template, C_Other                                   ); }
	bool C_TmplInsertBody    (ParseNode& p) { return G_Repeat        (p, id_TmplInsertBody,  C_TmplInsElem                                                                                 ); }
	bool C_TmplInsert        (ParseNode& p) { return G_Req<1,0,1>    (p, id_TmplInsert,      C_TmplInsertOpen, C_TmplInsertBody, C_CurlyClose                                              ); }
	bool C_TmplElem          (ParseNode& p) { return G_Choice        (p, id_Append,          C_TmplChars, C_TmplEsc, C_TmplInsert                                                          ); }
	bool C_Template          (ParseNode& p) { return G_Req<1,0,1>    (p, id_Template,        C_Backtick, G_Repeat<C_TmplElem>, C_Backtick                                                  ); }
																							 																							   
	bool C_ScriptElem        (ParseNode& p) { return G_Choice        (p, id_Append,          C_WsNlComments, C_SqString, C_DqString, C_Template, C_Other, C_CurlyClose                     ); }
	bool C_Script            (ParseNode& p) { return G_Repeat        (p, id_Append,          C_ScriptElem                                                                                  ); }

}
}
