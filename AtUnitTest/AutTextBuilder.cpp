#include "AutIncludes.h"
#include "AutMain.h"


void TextBuilderTest(Seq s)
{
	TextBuilder text;
	text.Add(s);
	Console::Out(Str(text.Final()).Add("\r\n"));
}


void TextBuilderTests()
{
	TextBuilderTest("Lorem ipsum dolor sit amet, consectetur adipiscing elit. Morbi interdum magna eu libero commodo, eu feugiat lacus rutrum. "
					"Curabitur a est et nisl maximus congue eu sit amet risus. Nunc egestas vitae erat sed finibus. Vestibulum laoreet nisl in "
					"tortor tincidunt iaculis. Sed congue id dui vitae hendrerit. Nulla nec commodo neque. In hac habitasse platea dictumst. "
					"Duis dolor ligula, pretium et tellus fringilla, feugiat eleifend magna. Vestibulum nulla ex, molestie in semper sit amet, "
					"rutrum porta ligula. Nullam tortor eros, malesuada quis malesuada vitae, tempor in erat. Pellentesque eget euismod tellus. "
					"Cras fringilla eget tortor eu porta. Duis feugiat tincidunt velit, in iaculis lectus vestibulum a. Cras at velit id magna "
					"consectetur bibendum eu nec augue. Aenean consequat ipsum et leo imperdiet, vel suscipit eros scelerisque.");

	TextBuilderTest(". 3 5 7 911 3 5 7 921 3 5 7 931 3 5 7 941 3 5 7 951 3 5 7 961 3 5 7 971 3 5 7 981 3 5 7 9\r\n"
					"a. 3 5 7 911 3 5 7 921 3 5 7 931 3 5 7 941 3 5 7 951 3 5 7 961 3 5 7 971 3 5 7 981 3 5 7 9\r\n"
					"\xc3\xa7. 3 5 7 911 3 5 7 921 3 5 7 931 3 5 7 941 3 5 7 951 3 5 7 961 3 5 7 971 3 5 7 981 3 5 7 9\r\n"			// Latin small leter c with cedilla (U+00E7) - width=1
					"a\xc3\xa7. 3 5 7 911 3 5 7 921 3 5 7 931 3 5 7 941 3 5 7 951 3 5 7 961 3 5 7 971 3 5 7 981 3 5 7 9\r\n"
					"\xE5\x97\x8A. 3 5 7 911 3 5 7 921 3 5 7 931 3 5 7 941 3 5 7 951 3 5 7 961 3 5 7 971 3 5 7 981 3 5 7 9\r\n"		// Unicode Han Character U+55CA (CJK) - width=2
					"a\xE5\x97\x8A. 3 5 7 911 3 5 7 921 3 5 7 931 3 5 7 941 3 5 7 951 3 5 7 961 3 5 7 971 3 5 7 981 3 5 7 9\r\n");
}
