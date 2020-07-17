#include "AtIncludes.h"
#include "AtHtmlGrammar.h"

#include "AtUnicode.h"

namespace At
{
	using namespace Parse;

	namespace Html
	{
		// This is not a validating grammar. It is a permissive grammar, meant to accept nearly any kind of malformed document,
		// without producing a parsing error, and still extracting a good amount of meaning out of it.
		// 
		// Because malformed documents are not necessarily properly structured, this is a flat grammar, rather than hierarchical.
		// The input is interpreted as flat text interspersed with insertions like comments, whitespace, start tags, and end tags,
		// and elements with special content rules that we try to recognize. Other than the elements with special content rules,
		// everything is flat. A start tag does not have to be followed by an end tag, elements can overlap without proper nesting,
		// there can be end tags without start tags, and so on.
		//
		// This grammar concerns itself with extracting only the first layer of meaning from the document: where the whitespace is,
		// what the comments and tags are; and elements like style, script, and textarea with special content. Anything else is
		// up to a higher layer to interpret.
		//
		// This grammar assumes input is UTF-8; it should be converted to UTF-8 if it's received in a different encoding. Invalid
		// UTF-8 is NOT accepted because it would complicate the grammar considerably, and make use of the parse tree dangerous.
		// If invalid UTF-8 should be tolerated, excise it from the input before passing the input to the grammar.

		DEF_RUID_B(HtmlWs)
		DEF_RUID_B(Text)
		DEF_RUID_B(CharRef)
		DEF_RUID_X(CharRefName, id_CharRef)
		DEF_RUID_X(CharRefDec,  id_CharRef)
		DEF_RUID_X(CharRefHex,  id_CharRef)
		DEF_RUID_B(Comment)

		DEF_RUID_B(CDataStart)
		DEF_RUID_B(CDataContent)
		DEF_RUID_B(CDataEnd)
		DEF_RUID_B(CData)

		DEF_RUID_B(QStr)

		DEF_RUID_B(AttrName)
		DEF_RUID_B(AttrValUnq)
		DEF_RUID_B(Attr)

		DEF_RUID_B(Tag)
		DEF_RUID_B(StartTag)
		DEF_RUID_B(LessSlash)
		DEF_RUID_B(Ws_Grtr)
		DEF_RUID_B(EndTag)

		DEF_RUID_B(RawText)
		DEF_RUID_B(SpecialElem)
		DEF_RUID_X(RawTextElem, id_SpecialElem)
		DEF_RUID_X(Script,      id_RawTextElem)
		DEF_RUID_X(Style,       id_RawTextElem)
		DEF_RUID_X(RcdataElem,  id_SpecialElem)
		DEF_RUID_X(Title,       id_RcdataElem)
		DEF_RUID_X(TextArea,    id_RcdataElem)

		DEF_RUID_B(TrashTag)
		DEF_RUID_B(SuspectChar)

		char const* const c_htmlWsChars { " \t\n\f\r" };
		bool IsHtmlWsChar (uint c) { return !!ZChr(c_htmlWsChars, c); }

		bool V_HtmlWsChar            (ParseNode& p) { return V_Utf8CharIf          (p,                     IsHtmlWsChar                                                                                  ); }
		bool V_HtmlWs                (ParseNode& p) { return G_Repeat              (p, id_Append,          V_HtmlWsChar                                                                                  ); }
		bool C_HtmlWs                (ParseNode& p) { return G_Req                 (p, id_HtmlWs,          V_HtmlWs                                                                                      ); }
																																																		 
		bool V_TextCharNoWs          (ParseNode& p) { return V_Utf8CharIf          (p,                     [] (uint c) -> bool { return c!='<' && c!='&' && !IsHtmlWsChar(c); }                          ); }
		bool V_SpNfbWs               (ParseNode& p) { return V_ByteNfbOneOf        (p,                     ' ', "\t\n\f\r "                                                                              ); }
		bool C_Text                  (ParseNode& p) { return G_Repeat              (p, id_Text,            G_Choice<V_TextCharNoWs, V_SpNfbWs>                                                           ); }
																																																		 
		bool V_AmpHash               (ParseNode& p) { return V_SeqMatchInsens      (p,                     "&#"                                                                                          ); }
		bool V_AmpHashX              (ParseNode& p) { return V_SeqMatchInsens      (p,                     "&#x"                                                                                         ); }
		bool C_CharRefName           (ParseNode& p) { return G_Req<1,1,1>          (p, id_CharRefName,     V_Amp, G_Repeat<V_AsciiAlphaNum, MinCharRefNameLen, MaxCharRefNameLen>, V_Semicolon           ); }
		bool C_CharRefDec            (ParseNode& p) { return G_Req<1,1,1>          (p, id_CharRefDec,      V_AmpHash, G_Repeat<V_AsciiDecDigit, 1, 7>, V_Semicolon                                       ); }
		bool C_CharRefHex            (ParseNode& p) { return G_Req<1,1,1>          (p, id_CharRefHex,      V_AmpHashX, G_Repeat<V_AsciiHexDigit, 1, 6>, V_Semicolon                                      ); }
		bool C_CharRef               (ParseNode& p) { return G_Choice              (p, id_Append,          C_CharRefName, C_CharRefDec, C_CharRefHex                                                     ); }
																																																		 
		bool V_CommentStart          (ParseNode& p) { return V_SeqMatchExact       (p,                     "<!--"                                                                                        ); }
		bool V_NonDashChar           (ParseNode& p) { return V_Utf8CharIf          (p,                     [] (uint c) -> bool { return c!='-'; }                                                        ); }
		bool V_DashNfbEnd            (ParseNode& p) { return V_ByteNfbSeq          (p,                     '-', "->", CaseMatch::Exact                                                                   ); }
		bool V_CommentEnd            (ParseNode& p) { return V_SeqMatchExact       (p,                     "-->"                                                                                         ); }
		bool C_Comment               (ParseNode& p) { return G_Req<1,0,1>          (p, id_Comment,         V_CommentStart, G_Repeat<G_Choice<V_NonDashChar, V_DashNfbEnd>>, G_OptIfEnd<V_CommentEnd>     ); }
																																																		 
		bool C_CDataStart            (ParseNode& p) { return G_SeqMatchExact       (p, id_CDataStart,      "<![CDATA["                                                                                   ); }
		bool V_NonSqBrCloseChar      (ParseNode& p) { return V_Utf8CharIf          (p,                     [] (uint c) -> bool { return c!=']'; }                                                        ); }
		bool V_SqBrCloseNfbEnd       (ParseNode& p) { return V_ByteNfbSeq          (p,                     ']', "]>", CaseMatch::Exact                                                                   ); }
		bool C_CDataContent          (ParseNode& p) { return G_Repeat              (p, id_CDataContent,    G_Choice<V_NonSqBrCloseChar, V_SqBrCloseNfbEnd>                                               ); }
		bool C_CDataEnd              (ParseNode& p) { return G_SeqMatchExact       (p, id_CDataEnd,        "]]>"                                                                                         ); }
		bool C_CData                 (ParseNode& p) { return G_Req<1,0,1>          (p, id_CData,           C_CDataStart, C_CDataContent, G_OptIfEnd<C_CDataEnd>                                          ); }
																																																		 
		bool V_NonAposChar           (ParseNode& p) { return V_Utf8CharIf          (p,                     [] (uint c) -> bool { return c != '\''; }                                                     ); }
		bool V_NonDblQuoteChar       (ParseNode& p) { return V_Utf8CharIf          (p,                     [] (uint c) -> bool { return c != '"'; }                                                      ); }
		bool V_SQStr                 (ParseNode& p) { return G_Req<1,0,1>          (p, id_Append,          V_Apos, G_Repeat<V_NonAposChar>, G_OptIfEnd<V_Apos>                                           ); }
		bool V_DQStr                 (ParseNode& p) { return G_Req<1,0,1>          (p, id_Append,          V_DblQuote, G_Repeat<V_NonDblQuoteChar>, G_OptIfEnd<V_DblQuote>                               ); }
		bool V_QStr                  (ParseNode& p) { return G_Choice              (p, id_Append,          V_SQStr, V_DQStr                                                                              ); }
		bool C_QStr                  (ParseNode& p) { return G_Req                 (p, id_QStr,            V_QStr                                                                                        ); }
																																																		 
		bool V_AttrNameChar          (ParseNode& p) { return V_Utf8CharIf          (p,                     [] (uint c) -> bool { return !Unicode::IsControl((uint) c) && !ZChr(" \"'/=>", c); }          ); }
		bool V_AttrValUnqChar        (ParseNode& p) { return V_Utf8CharIf          (p,                     [] (uint c) -> bool { return !ZChr("\t\n\f\r \"'<=>`", c); }                                  ); }
		bool C_AttrName              (ParseNode& p) { return G_Repeat              (p, id_AttrName,        V_AttrNameChar                                                                                ); }
		bool C_AttrValUnq            (ParseNode& p) { return G_Repeat              (p, id_AttrValUnq,      V_AttrValUnqChar                                                                              ); }
		bool C_AttrVal               (ParseNode& p) { return G_Choice              (p, id_Append,          C_AttrValUnq, C_QStr                                                                          ); }
		bool C_Eq_AttrVal            (ParseNode& p) { return G_Req<0,1,0,1>        (p, id_Append,          C_HtmlWs, C_Eq, C_HtmlWs, G_OptIfEnd<C_AttrVal>                                               ); }
		bool C_Attr                  (ParseNode& p) { return G_Req<1,0>            (p, id_Attr,            C_AttrName, C_Eq_AttrVal                                                                      ); }
		bool C_Ws_Attr               (ParseNode& p) { return G_Req<1,1>            (p, id_Append,          C_HtmlWs, C_Attr                                                                              ); }
																																																		 
		template <bool (*VTag)(ParseNode&)>																																								 
		bool C_Tag                   (ParseNode& p) { return G_Req                 (p, id_Tag,             VTag                                                                                          ); }
																																																		 
		bool V_GenericTag            (ParseNode& p) { return G_Req<1,0>            (p, id_Append,          V_AsciiAlpha, G_Repeat<V_AsciiAlphaNum>                                                       ); }
		bool V_LessSlash             (ParseNode& p) { return V_SeqMatchExact       (p,                     "</"                                                                                          ); }
		bool C_LessSlash             (ParseNode& p) { return G_SeqMatchExact       (p, id_LessSlash,       "</"                                                                                          ); }
		bool C_Ws_Grtr               (ParseNode& p) { return G_Req<0,1>            (p, id_Ws_Grtr,         V_HtmlWs, G_OptIfEnd<V_Grtr>                                                                  ); }
																																																		 
		template <bool (*VTag)(ParseNode&)>																																								 
		bool C_StartTag              (ParseNode& p) { return G_Req<1,1,0,0,0,1>    (p, id_StartTag,        C_Less, G_OptIfEnd<C_Tag<VTag>>, G_Repeat<C_Ws_Attr>, C_HtmlWs, C_Slash, G_OptIfEnd<C_Grtr>   ); }
																																																		 
		template <bool (*VTag)(ParseNode&)>																																								 
		bool C_EndTag                (ParseNode& p) { return G_Req<1,1,1>          (p, id_EndTag,          C_LessSlash, C_Tag<VTag>, C_Ws_Grtr                                                           ); }
																																																		 
		template <bool (*VTag)(ParseNode&)>																																								 
		bool V_EndTagStart           (ParseNode& p) { return G_Req<1,1>            (p, id_Append,          V_LessSlash, VTag                                                                             ); }
																																																		 
		template <bool (*VTag)(ParseNode&)>																																								 
		bool V_RawTextCharExcept     (ParseNode& p) { return G_Except              (p, id_Append,          V_Utf8CharAny, V_EndTagStart<VTag>                                                            ); }
																																																		 
		template <bool (*VTag)(ParseNode&)>																																								 
		bool C_RawText               (ParseNode& p) { return G_Repeat              (p, id_RawText,         V_RawTextCharExcept<VTag>                                                                     ); }
																																																		 
		template <bool (*VTag)(ParseNode&)>																																								 
		bool V_RcdataCharExcept      (ParseNode& p) { return G_Except              (p, id_Append,          V_Utf8CharAny, G_Choice<C_CharRef, V_EndTagStart<VTag>>                                       ); }
																																																		 
		template <bool (*VTag)(ParseNode&)>																																								 
		bool C_RcdataExcept          (ParseNode& p) { return G_Repeat              (p, id_RawText,         V_RcdataCharExcept<VTag>                                                                      ); }
																																																		 
		template <bool (*VTag)(ParseNode&)>																																								 
		bool C_Rcdata                (ParseNode& p) { return G_Repeat              (p, id_Append,          G_Choice<C_RcdataExcept<VTag>, C_CharRef>                                                     ); }
																																																		 
		bool V_ScriptTag             (ParseNode& p) { return V_SeqMatchInsens      (p,                     "script"                                                                                      ); }
		bool V_StyleTag              (ParseNode& p) { return V_SeqMatchInsens      (p,                     "style"                                                                                       ); }
		bool V_TitleTag              (ParseNode& p) { return V_SeqMatchInsens      (p,                     "title"                                                                                       ); }
		bool V_TextAreaTag           (ParseNode& p) { return V_SeqMatchInsens      (p,                     "textarea"                                                                                    ); }
																																																		 
		bool C_Script                (ParseNode& p) { return G_Req<1,0,1>          (p, id_Script,          C_StartTag<V_ScriptTag>, C_RawText<V_ScriptTag>, G_Choice<C_EndTag<V_ScriptTag>, N_End>       ); }
		bool C_Style                 (ParseNode& p) { return G_Req<1,0,1>          (p, id_Style,           C_StartTag<V_StyleTag>, C_RawText<V_StyleTag>, G_Choice<C_EndTag<V_StyleTag>, N_End>          ); }
																																																		 
		bool C_Title                 (ParseNode& p) { return G_Req<1,0,1>          (p, id_Title,           C_StartTag<V_TitleTag>, C_Rcdata<V_TitleTag>, G_Choice<C_EndTag<V_TitleTag>, N_End>           ); }
		bool C_TextArea              (ParseNode& p) { return G_Req<1,0,1>          (p, id_TextArea,        C_StartTag<V_TextAreaTag>, C_Rcdata<V_TextAreaTag>, G_Choice<C_EndTag<V_TextAreaTag>, N_End>  ); }
																																																		 
		bool V_TrashTagStartChar     (ParseNode& p) { return V_ByteIf              (p,                     [] (uint c) -> bool { return Ascii::IsAlpha(c) || c=='!' || c=='/'; }                         ); }
		bool V_TrashTagTextChar      (ParseNode& p) { return V_Utf8CharIf          (p,                     [] (uint c) -> bool { return c!='>'; }                                                        ); }
		bool C_TrashTag              (ParseNode& p) { return G_Req<1,1,0,1>        (p, id_TrashTag,        V_Less, V_TrashTagStartChar, G_Repeat<V_TrashTagTextChar>, V_Grtr                             ); }
																																																		 
		bool V_SuspectChar           (ParseNode& p) { return V_ByteIf              (p,                     [] (uint c) -> bool { return c=='<' || c=='&'; }                                              ); }
		bool C_SuspectChar           (ParseNode& p) { return G_Req                 (p, id_SuspectChar,     V_SuspectChar                                                                                 ); }
																																																		 
		bool C_TextParticle          (ParseNode& p) { return G_Choice              (p, id_Append,          C_HtmlWs, C_Text, C_CharRef                                                                   ); }
		bool C_TagParticle           (ParseNode& p) { return G_Choice              (p, id_Append,          C_StartTag<V_GenericTag>, C_EndTag<V_GenericTag>, C_TrashTag                                  ); }
		bool C_SpecialElem           (ParseNode& p) { return G_Choice              (p, id_Append,          C_Script, C_Style, C_Title, C_TextArea                                                        ); }
		bool C_Particle              (ParseNode& p) { return G_Choice              (p, id_Append,          C_TextParticle, C_Comment, C_CData, C_SpecialElem, C_TagParticle, C_SuspectChar               ); }
		bool C_Document              (ParseNode& p) { return G_Repeat              (p, id_Append,          C_Particle                                                                                    ); }
	}
}
