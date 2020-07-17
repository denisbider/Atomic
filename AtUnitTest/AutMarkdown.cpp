#include "AutIncludes.h"
#include "AutMain.h"


void MarkdownTest(Seq srcText)
{
	Console::Out(Str("\r\n"
					 "----- Markdown -----\r\n")
				.Add(srcText).Add("\r\n"));

	Markdown::Transform xform { srcText };
	xform.Tree().RecordBestToStack();
	if (!xform.Parse())
	{
		Console::Out(Str("----- Parsing error -----\r\n")
					.Obj(xform.Tree(), ParseTree::BestAttempt).Add("\r\n"
					     "----- End -----\r\n"
					     "\r\n"));

		throw "Terminating due to error";
	}
	else
	{
		HtmlBuilder html;
		html.Body();
		xform.ToHtml(html);
		html.EndBody().EndHtml();

		Console::Out(Str("----- HTML -----\r\n")
					.Add(html.Final()).Add("\r\n"));

		TextBuilder text;
		xform.ToText(text);

		Console::Out(Str("----- Text -----\r\n")
					.Add(text.Final()).Add("\r\n"
						 "----- End -----\r\n"
						 "\r\n"));
	}
}


void MarkdownTests(Slice<Seq> args)
{
	if (args.Len() > 2)
	{
		Str mkdn;

		WinStr filePath { args[2] };
		File().Open(filePath.Z(), File::OpenArgs::DefaultRead())
			.ReadAllInto(mkdn);

		MarkdownTest(mkdn);
	}
	else
	{
		MarkdownTest("");
		MarkdownTest(" ");
		MarkdownTest("a");
		MarkdownTest(" a");
		MarkdownTest("    a");
		MarkdownTest("    a\r\n"
						"    b");
		MarkdownTest("* a");
		MarkdownTest(" + a");
		MarkdownTest("  - a");
		MarkdownTest("1. a");
		MarkdownTest(" 1) a");
		MarkdownTest("  2. a");
		MarkdownTest("   2) a");
		MarkdownTest("1. a\r\n"
						"2. b");
		MarkdownTest("1. a\r\n"
						"2. b\r\n");
		MarkdownTest("1. a\r\n"
						"\r\n"
						"2. b");
		MarkdownTest("1. a\r\n"
						"\r\n"
						"2. b\r\n");
		MarkdownTest("> 1. a\r\n"
						"> 2. b");
		MarkdownTest("> 1. a\r\n"
						"> 2. b\r\n");
		MarkdownTest("> 1. a\r\n"
						">\r\n"
						"> 2. b\r\n");
		MarkdownTest("> 2. * a\r\n"
						">    * b\r\n"
						"> 3. c\r\n"
						"d");
		MarkdownTest("a\r\n"
						"> b\r\n"
						"> > 1. c\r\n"
						"> > 2. * d\r\n"
						"> >    * e\r\n"
						"> 3. f\r\n"
						"g");
		MarkdownTest("\\> a\r\n");
		MarkdownTest("&lt;");
		MarkdownTest("&#123;");
		MarkdownTest("&#x69;");
		MarkdownTest("a&lt;b");
		MarkdownTest("a&#123;b");
		MarkdownTest("a&#x69;b");
		MarkdownTest("a\\&lt;b");
		MarkdownTest("a\\&#123;b");
		MarkdownTest("a\\&#x69;b");
		MarkdownTest("a&\\#123;b");
		MarkdownTest("a&\\#x69;b");
		MarkdownTest("*a __c__ b* &lt; `code`");
		MarkdownTest("*a __c__ b* &lt; `&lt;code`");
		MarkdownTest("*a __c__ b* &lt; `co&lt;de`");
		MarkdownTest("*a __c__ b* &lt; `co&lt;de`&lt;");
		MarkdownTest("\\");
		MarkdownTest("\\\\");
		MarkdownTest("\\a");
		MarkdownTest("This line\r\n"
						"should be in paragraph with this one");
		MarkdownTest("This line\r\n"
						"should be in paragraph with this one\r\n");
		MarkdownTest("This line\r\n"
						"should be in paragraph with this one\r\n"
						"  but this line should not");
		MarkdownTest("This line\r\n"
						"should be in paragraph with this one\r\n"
						"  but this line should not\r\n");
		MarkdownTest("---");
		MarkdownTest("Paragraph\r\n"
						"---");
		MarkdownTest("Paragraph\r\n"
						"---\r\n");
		MarkdownTest("Paragraph\r\n"
						"---\r\n"
						"Below horizontal rule\r\n");
		MarkdownTest("*a*");
		MarkdownTest("*a*\r\n");
		MarkdownTest("__a__");
		MarkdownTest("__a__\r\n");
		MarkdownTest("a*b");
		MarkdownTest("a*b*");
		MarkdownTest("a__b");
		MarkdownTest("a__b__");
		MarkdownTest("a__b__c");
		MarkdownTest("a__b__c\r\n");
		MarkdownTest("a__b__ \r\n");
		MarkdownTest("__a __b\r\n");
		MarkdownTest("__a *c* b__ c\r\n");
		MarkdownTest("*a __c__ b* c\r\n");
		MarkdownTest("*a __c__ b* `code`");
		MarkdownTest("*a __c__ b* `code`\r\n");
		MarkdownTest("*a __c__ b* `code` c\r\n");
		MarkdownTest("*a __c__ b* ``co`de`` c\r\n");
		MarkdownTest("*a __c__ b* ``co`de\r\n");
		MarkdownTest("***a*b**\r\n");
		MarkdownTest("***a**b*\r\n");
		MarkdownTest("***a**b\r\n");
		MarkdownTest("***a*b\r\n");
		MarkdownTest("*a**b***\r\n");
		MarkdownTest("**a*b***\r\n");
		MarkdownTest("a**b***\r\n");
		MarkdownTest("a*b***\r\n");
		MarkdownTest("This is a paragraph\r\n"
						"\tThis is code\r\n"
						"    This continues to be code\r\n"
						"  \tStill code\r\n"
						">\tNo longer code, but instead a quote\r\n"
						">\t\tIs this code, or a paragraph? Code block boundary is in middle of second tab\r\n"
						"> 1. This is a list\r\n"
						">    Still part of list\r\n"
						"> \t Also part of list\r\n"
						">\t More list\r\n"
						"> \t   New list item paragraph\r\n"
						"> \tNo longer in list");
	}
}
