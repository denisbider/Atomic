#include "AtIncludes.h"
#include "AtJsonGrammar.h"

namespace At
{
	namespace Json
	{
		using namespace Parse;

		bool V_CharNonEsc    (ParseNode& p) { return V_Utf8CharIf         (p,                  [] (uint c) -> bool { return c >= 32 && c != '"' && c != '\\'; }   ); }
		bool V_CharsNonEsc   (ParseNode& p) { return G_Repeat             (p, id_Append,       V_CharNonEsc                                                       ); }
		bool V_EscSpecChar   (ParseNode& p) { return V_ByteIf             (p,                  [] (uint c) -> bool { return ZChr("\"\\/bfnrt", c) != nullptr; }   ); }
		bool V_EscSpecial    (ParseNode& p) { return G_Req<1,1>           (p, id_Append,       V_Backslash, V_EscSpecChar                                         ); }
		bool V_EscUniPrefix  (ParseNode& p) { return V_SeqMatchExact      (p,                  "\\u"                                                              ); }
		bool V_EscUniDigit   (ParseNode& p) { return V_ByteIf             (p,                  Ascii::IsHexDigit                                                  ); }
		bool V_EscUniDigits  (ParseNode& p) { return G_Repeat             (p, id_Append,       V_EscUniDigit, 4, 4                                                ); }
		bool V_EscUnicode    (ParseNode& p) { return G_Req<1,1>           (p, id_Append,       V_EscUniPrefix, V_EscUniDigits                                     ); }
		bool V_True          (ParseNode& p) { return V_SeqMatchExact      (p,                  "true"                                                             ); }
		bool V_False         (ParseNode& p) { return V_SeqMatchExact      (p,                  "false"                                                            ); }
		bool V_Null          (ParseNode& p) { return V_SeqMatchExact      (p,                  "null"                                                             ); }

		bool V_NrZero        (ParseNode& p) { return V_ByteIs             (p,                  '0'                                                                ); }
		bool V_NrDigitNZ     (ParseNode& p) { return V_ByteIf             (p,                  [] (uint c) -> bool { return c >= '1' && c <= '9'; }               ); }
		bool V_NrDigits      (ParseNode& p) { return G_Repeat             (p, id_Append,       V_AsciiDecDigit                                                    ); }
		bool V_NrNonZero     (ParseNode& p) { return G_Req<1,0>           (p, id_Append,       V_NrDigitNZ, V_NrDigits                                            ); }
		bool V_NrIntPart     (ParseNode& p) { return G_Choice             (p, id_Append,       V_NrZero, V_NrNonZero                                              ); }
		bool V_NrFracPart    (ParseNode& p) { return G_Req<1,1>           (p, id_Append,       V_Dot, V_NrDigits                                                  ); }
		bool V_NrExpEe       (ParseNode& p) { return V_ByteIsOf           (p,                  "Ee"                                                               ); }
		bool V_NrExpPlMin    (ParseNode& p) { return V_ByteIsOf           (p,                  "+-"                                                               ); }
		bool V_NrExpPart     (ParseNode& p) { return G_Req<1,1,1>         (p, id_Append,       V_NrExpEe, V_NrExpPlMin, V_NrDigits                                ); }
		bool V_Number        (ParseNode& p) { return G_Req<0,1,0,0>       (p, id_Append,       V_Dash, V_NrIntPart, V_NrFracPart, V_NrExpPart                     ); }

		DEF_RUID_B(CharsNonEsc)
		DEF_RUID_B(EscSpecial)
		DEF_RUID_B(EscUnicode)
		DEF_RUID_B(Value)
		DEF_RUID_X(String, id_Value)
		DEF_RUID_X(True,   id_Value)
		DEF_RUID_X(False,  id_Value)
		DEF_RUID_X(Null,   id_Value)
		DEF_RUID_X(Number, id_Value)
		DEF_RUID_X(Array,  id_Value)
		DEF_RUID_X(Pair,   id_Value)
		DEF_RUID_X(Object, id_Value)

		bool C_CharsNonEsc   (ParseNode& p) { return G_Req               (p, id_CharsNonEsc,  V_CharsNonEsc                                                       ); }
		bool C_EscSpecial    (ParseNode& p) { return G_Req               (p, id_EscSpecial,   V_EscSpecial                                                        ); }
		bool C_EscUnicode    (ParseNode& p) { return G_Req               (p, id_EscUnicode,   V_EscUnicode                                                        ); }
		bool C_CharSeq       (ParseNode& p) { return G_Choice            (p, id_Append,       C_CharsNonEsc, C_EscSpecial, C_EscUnicode                           ); }
		bool C_CharSeqs      (ParseNode& p) { return G_Repeat            (p, id_Append,       C_CharSeq                                                           ); }
		bool C_String        (ParseNode& p) { return G_Req<1,0,1>        (p, id_String,       C_DblQuote, C_CharSeqs, C_DblQuote                                  ); }
		bool C_True          (ParseNode& p) { return G_Req               (p, id_True,         V_True                                                              ); }
		bool C_False         (ParseNode& p) { return G_Req               (p, id_False,        V_False                                                             ); }
		bool C_Null          (ParseNode& p) { return G_Req               (p, id_Null,         V_Null                                                              ); }
		bool C_Number        (ParseNode& p) { return G_Req               (p, id_Number,       V_Number                                                            ); }
		bool C_CommaWsValWs  (ParseNode& p) { return G_Req<1,0,1,0>      (p, id_Append,       C_Comma, C_Ws, C_Value, C_Ws                                        ); }
		bool C_MoreValues    (ParseNode& p) { return G_Repeat            (p, id_Append,       C_CommaWsValWs                                                      ); }
		bool C_ArrayValues   (ParseNode& p) { return G_Req<1,0,0>        (p, id_Append,       C_Value, C_Ws, C_MoreValues                                         ); }
		bool C_Array         (ParseNode& p) { return G_Req<1,0,0,1>      (p, id_Array,        C_SqOpenBr, C_Ws, C_ArrayValues, C_SqCloseBr                        ); }
		bool C_Value         (ParseNode& p) { return G_Choice            (p, id_Append,       C_String, C_Number, C_Object, C_Array, C_True, C_False, C_Null      ); }
		bool C_Pair          (ParseNode& p) { return G_Req<1,0,1,0,1>    (p, id_Pair,         C_String, C_Ws, C_Colon, C_Ws, C_Value                              ); }
		bool C_CommaWsPairWs (ParseNode& p) { return G_Req<1,0,1,0>      (p, id_Append,       C_Comma, C_Ws, C_Pair, C_Ws                                         ); }
		bool C_MorePairs     (ParseNode& p) { return G_Repeat            (p, id_Append,       C_CommaWsPairWs                                                     ); }
		bool C_Members       (ParseNode& p) { return G_Req<1,0,0>        (p, id_Append,       C_Pair, C_Ws, C_MorePairs                                           ); }
		bool C_Object        (ParseNode& p) { return G_Req<1,0,0,1>      (p, id_Object,       C_CurlyOpen, C_Ws, C_Members, C_CurlyClose                          ); }
		bool C_ObjectOrArray (ParseNode& p) { return G_Choice            (p, id_Append,       C_Object, C_Array                                                   ); }
		bool C_Json          (ParseNode& p) { return G_Req<0,1,0>        (p, id_Append,       C_Ws, C_ObjectOrArray, C_Ws                                         ); }
	}
}
