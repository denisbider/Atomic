#pragma once

#include "AtDescEnum.h"
#include "AtRp.h"
#include "AtSeq.h"
#include "AtSocket.h"
#include "AtTextLog.h"


namespace At
{
	
	// TranscriptEvent

	DESCENUM_DECL_BEGIN(TranscriptEvent)
	DESCENUM_DECL_VALUE(Read,      1000)
	DESCENUM_DECL_VALUE(Decrypted, 1100)
	DESCENUM_DECL_VALUE(Written,   2000)
	DESCENUM_DECL_VALUE(Encrypted, 2100)
	DESCENUM_DECL_CLOSE();


	// Transcriber

	class Transcriber : public RefCountable, NoCopy
	{
	public:
		virtual void Transcribe(TranscriptEvent::E te, Seq data) = 0;
	};



	// Transcribable

	class Transcribable : NoCopy
	{
	public:
		void SetTranscriber(Rp<Transcriber> const& transcriber) { m_transcriber = transcriber; }

	protected:
		Rp<Transcriber> m_transcriber;
	};



	// TextFileTranscriber
	
	class TextFileTranscriber : public Transcriber, private TextLog
	{
	public:
		static Rp<TextFileTranscriber> Start(Seq facility, Socket const& sk)
			{ Rp<TextFileTranscriber> t = new TextFileTranscriber; t->Init(facility, sk); return t; }

	private:
		void Init(Seq facility, Socket const& sk);
		void Transcribe(TranscriptEvent::E te, Seq data) override final;
	};

}
