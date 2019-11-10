#include "AtIncludes.h"
#include "AtJsonPretty.h"

namespace At
{
	namespace Json
	{
		namespace
		{
			struct LineState { enum E { Empty, Indented, Pair }; };
			struct InlineDisp { enum E { Check, Inline, NoInline }; };

			bool CanInlineArray(ParseNode const& p);

			bool CanInlineValue(ParseNode const& p)
			{
				if (p.IsType(id_Object))
					return false;
				if (p.IsType(id_Array))
					return CanInlineArray(p);
				return true;
			}

			bool CanInlinePair(ParseNode const& p)
			{
				return CanInlineValue(p.LastChild());
			}

			bool CanInlineArray(ParseNode const& p)
			{
				sizet nrValues = 0;
				for (ParseNode const& c : p)
				{
					if (c.IsType(id_Value))
						if (!CanInlineValue(c) || ++nrValues >= 2)
							return false;
				}
				return true;
			}

			void PrettyPrintArray(Str& out, ParseNode const& p, sizet indent, LineState::E lineState, InlineDisp::E inlineDisp);
			void PrettyPrintObject(Str& out, ParseNode const& p, sizet indent, LineState::E lineState);

			void PrettyPrintValue(Str& out, ParseNode const& p, sizet indent, LineState::E lineState, InlineDisp::E inlineDisp)
			{
				if (p.IsType(id_Object))
					PrettyPrintObject(out, p, indent, lineState);
				else if (p.IsType(id_Array))
					PrettyPrintArray(out, p, indent, lineState, inlineDisp);
				else
				{
					EnsureThrow(p.IsType(id_Value));
					out.Add(p.SrcText());
				}
			}

			void PrettyPrintPair(Str& out, ParseNode const& p, sizet indent, InlineDisp::E inlineDisp)
			{
				out.Add(p.FirstChild().SrcText()).Add(": ");
				PrettyPrintValue(out, p.LastChild(), indent + 2, LineState::Pair, inlineDisp);
			}

			void PrettyPrintArray(Str& out, ParseNode const& p, sizet indent, LineState::E lineState, InlineDisp::E inlineDisp)
			{
				if (inlineDisp == InlineDisp::Check)
					if (CanInlineArray(p))
						inlineDisp = InlineDisp::Inline;
					else
					{
						if (lineState == LineState::Pair)
						{
							out.Add("\r\n");
							lineState = LineState::Empty;
						}

						inlineDisp = InlineDisp::NoInline;
					}

				if (inlineDisp == InlineDisp::Inline)
				{
					out.Add("[ ");

					if (lineState == LineState::Empty)
						lineState = LineState::Indented;

					for (ParseNode const& c : p)
						if (c.IsType(id_Value))
						{
							PrettyPrintValue(out, c, indent, lineState, inlineDisp);
							break;
						}

					if (out[out.Len() - 1] == ' ')
						out.Add("]");
					else
						out.Add(" ]");
				}
				else
				{
					if (lineState != LineState::Empty)
						out.Add("[ ");
					else
					{
						out.Chars(indent, ' ').Add("[ ");
						lineState = LineState::Indented;
					}

					bool needComma = false;
					for (ParseNode const& c : p)
						if (c.IsType(id_Value))
						{
							if (needComma)
							{
								out.Add(",\r\n").Chars(indent + 2, ' ');
								lineState = LineState::Indented;
							}
							else
								needComma = true;

							PrettyPrintValue(out, c, indent + 2, lineState, InlineDisp::Check);
						}

					if (out[out.Len() - 1] == ' ')
						out.Add("]");
					else
						out.Add(" ]");
				}
			}

			void PrettyPrintObject(Str& out, ParseNode const& p, sizet indent, LineState::E lineState)
			{
				if (!p.IsType(id_Object))
					throw Json::DecodeErr(p, "Expected object");

				if (lineState == LineState::Pair)
				{
					out.Add("\r\n");
					lineState = LineState::Empty;
				}

				if (lineState == LineState::Empty)
					out.Chars(indent, ' ');

				out.Add("{ ");

				bool needComma = false;
				for (ParseNode const& c : p)
					if (c.IsType(id_Pair))
					{
						if (needComma)
							out.Add(",\r\n").Chars(indent + 2, ' ');
						else
							needComma = true;

						PrettyPrintPair(out, c, indent + 2, InlineDisp::Check);
					}

				if (out[out.Len() - 1] == ' ')
					out.Add("}");
				else
					out.Add(" }");
			}
		}

		Str PrettyPrint(ParseNode const& p)
		{
			Str out;

			ParseNode const* v = p.DeepFindFirstOf(id_Object, id_Array);
			if (!v)
				throw Json::DecodeErr(p, "Expected object or array");
			
			if (v->IsType(id_Object))
				PrettyPrintObject(out, *v, 0, LineState::Empty);
			else
				PrettyPrintArray(out, *v, 0, LineState::Empty, InlineDisp::Check);

			return out;
		}
	}
}
