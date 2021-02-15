#include "AtIncludes.h"
#include "AtTranscriber.h"


namespace At
{

	// TranscriptEvent

	DESCENUM_DEF_BEGIN(TranscriptEvent)
	DESCENUM_DEF_VALUE(Read)
	DESCENUM_DEF_VALUE(Decrypted)
	DESCENUM_DEF_VALUE(Written)
	DESCENUM_DEF_VALUE(Encrypted)
	DESCENUM_DEF_CLOSE();



	// TextFileTranscriber

	void TextFileTranscriber::Init(Seq facility, Socket const& sk)
	{
		TextLog::Init("Transcript", TextLog::Flags::CreateUnique);

		Time now { Time::StrictNow() };
		TzInfo tzi;

		Str entry;
		entry.ReserveExact(500)
			.Add("<transcript facility=\"").HtmlAttrValue(facility, Html::CharRefs::Escape)
			.Add("\" localAddr=\"").Obj(sk.LocalAddr(), SockAddr::AddrPort)
			.Add("\" remoteAddr=\"").Obj(sk.RemoteAddr(), SockAddr::AddrPort)
			.Add("\" startTime=\"").Fun(Enc_EntryTime, now, tzi)
			.Add("\" />\r\n");

		WriteEntry(now, tzi, entry);
	}


	void TextFileTranscriber::Transcribe(TranscriptEvent::E te, Seq data)
	{
		Time now { Time::StrictNow() };
		TzInfo tzi;

		Str entry;
		entry.ReserveExact(500 + ((data.n / 16) * 80))
			.Add("<io type=\"").HtmlAttrValue(TranscriptEvent::Desc(te), Html::CharRefs::Escape)
			.Add("\" time=\"").Fun(Enc_EntryTime, now, tzi)
			.Add("\" bytes=\"").UInt(data.n)
			.Add("\">").CDataEncode(Str("\r\n").HexDump(data))
			.Add("</io>\r\n");

		WriteEntry(now, tzi, entry);
	}

}
