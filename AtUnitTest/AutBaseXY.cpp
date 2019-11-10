#include "AutIncludes.h"
#include "AutMain.h"


sizet BaseXYTest(std::function<void(Seq, Enc&)> encode, std::function<void(Seq, Enc&)> decode)
{
	sizet nrBytesEncoded {};

	BCrypt::Rng rng;
	Str src, encoded, decoded;

	for (sizet i=0; i!=1000; ++i)
		for (sizet len=100; len!=0; )
		{
			--len;

			encoded.Clear();
			decoded.Clear();

			src.Resize(len);
			rng.GenRandom(src.Ptr(), src.Len());

			encode(src, encoded);
			decode(encoded, decoded);

			if (decoded != src)
				throw ZLitErr("Unexpected: decoded != src");

			nrBytesEncoded += encoded.Len();
		}

	return nrBytesEncoded;
}


void BaseXYTests(int argc, char** argv)
{
	(void) argc;
	(void) argv;

	try
	{
		sizet n;
		n = BaseXYTest( [] (Seq s, Enc& w) { Base64::UrlFriendlyEncode(s, w, Base64::Padding::No); },
		                [] (Seq s, Enc& w) { Base64::UrlFriendlyDecode(s, w); } );
		Console::Out(Str("Base64::UrlFriendly: ").UInt(n).Add(" bytes encoded\r\n"));

		n = BaseXYTest( [] (Seq s, Enc& w) { Base64::MimeEncode(s, w, Base64::Padding::Yes, Base64::NewLines::Mime()); },
		                [] (Seq s, Enc& w) { Base64::MimeDecode(s, w); } );
		Console::Out(Str("Base64::Mime: ").UInt(n).Add(" bytes encoded\r\n"));

		n = BaseXYTest( [] (Seq s, Enc& w) { Base32::Encode(s, w, Base32::NewLines::None()); },
		                [] (Seq s, Enc& w) { Base32::Decode(s, w); } );
		Console::Out(Str("Base32 - NewLines::None: ").UInt(n).Add(" bytes encoded\r\n"));

		n = BaseXYTest( [] (Seq s, Enc& w) { Base32::Encode(s, w, Base32::NewLines::Mime()); },
		                [] (Seq s, Enc& w) { Base32::Decode(s, w); } );
		Console::Out(Str("Base32 - NewLines::Mime: ").UInt(n).Add(" bytes encoded\r\n"));
	}
	catch (Exception const& e)
	{
		Str msg = "BaseXYTests terminated by exception:\r\n";
		msg.Add(e.what()).Add("\r\n");
		Console::Out(msg);
	}
}
