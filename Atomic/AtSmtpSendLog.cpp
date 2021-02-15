#include "AtIncludes.h"
#include "AtSmtpSendLog.h"


namespace At
{

	// SmtpEnhStatus

	SmtpEnhStatus SmtpEnhStatus::Read(Seq& reader)
	{
		if (Ascii::IsDecDigit(reader.FirstByte()))
		{
			uint32 enhClass = reader.ReadNrUInt32Dec(999);
			if (!!enhClass && enhClass != UINT_MAX && reader.StripPrefixExact(".") && Ascii::IsDecDigit(reader.FirstByte()))
			{
				uint32 enhSubject = reader.ReadNrUInt32Dec(999);
				if (enhSubject != UINT_MAX && reader.StripPrefixExact(".") && Ascii::IsDecDigit(reader.FirstByte()))
				{
					uint32 enhDetail = reader.ReadNrUInt32Dec(999);
					if (enhDetail != UINT_MAX && (!reader.n || Ascii::IsHorizWhitespace(reader.FirstByte())))
					{
						uint64 value =	(((uint64) enhClass   ) << 32) |
										(((uint64) enhSubject ) << 16) |
										(((uint64) enhDetail  )      );
						return FromUint(value);
					}
				}
			}
		}

		return None();
	}


	void SmtpEnhStatus::EncObj(Enc& s) const
	{
		if (Any())
			s.UInt(GetClass()).Ch('.').UInt(GetSubject()).Ch('.').UInt(GetDetail());
	}


	bool SmtpEnhStatus::Match(uint cls, uint subject, uint detail) const noexcept
	{
		if (!Any()) return false;
		if (cls     != UINT_MAX && cls     != GetClass   ()) return false;
		if (subject != UINT_MAX && subject != GetSubject ()) return false;
		if (detail  != UINT_MAX && detail  != GetDetail  ()) return false;
		return true;
	}



	// TextSmtpSendLog

	void TextSmtpSendLog::Enc_Msg(Enc& enc, SmtpMsgToSend const& msg)
	{
		enc.Add(" <msg id=\"")      .Obj  (msg.m_entityId)
		   .Add("\" next=\"")       .Obj  (msg.f_nextAttemptTime, TimeFmt::IsoMicroZ)
		   .Add("\" status=\"")     .Add  (SmtpMsgStatus::Name(msg.f_status))
		   .Add("\" from=\"")       .Add  (msg.f_fromAddress)
		   .Add("\" toDomain=\"")   .Add  (msg.f_toDomain)
		   .Add("\" lenPart1=\"")   .UInt (msg.f_contentPart1.Len())
		   .Add("\" tlsReq=\"")     .Add  (SmtpTlsAssurance::Name(msg.f_tlsRequirement))
		   .Add("\" baseSecs=\"")   .UInt (msg.f_baseSendSecondsMax)
		   .Add("\" nrBytes1Sec=\"").UInt (msg.f_nrBytesToAddOneSec)
		   .Add("\" retry=\"");

		bool needSpace {};
		for (uint64 rdm : msg.f_futureRetryDelayMinutes)
		{
			if (needSpace)
				enc.Ch(' ');
			else
				needSpace = true;

			enc.UInt(rdm);
		}

		enc.Add("\" mcCx=\"").HtmlAttrValue(msg.f_moreContentContext, Html::CharRefs::Escape)
		   .Add("\" dvCx=\"").HtmlAttrValue(msg.f_deliveryContext, Html::CharRefs::Escape).Add("\">\r\n");

		for (Str const& md : msg.f_additionalMatchDomains)
			enc.Add("  <match domain=\"").HtmlAttrValue(md, Html::CharRefs::Escape).Add("\" />\r\n");

		for (Str const& mbox : msg.f_pendingMailboxes)
			enc.Add("  <pending mbox=\"").HtmlAttrValue(mbox, Html::CharRefs::Escape).Add("\" />\r\n");

		enc.Add(" </msg>\r\n");
	}


	void TextSmtpSendLog::SmtpSendLog_OnReset(RpVec<SmtpMsgToSend> const& msgs)
	{
		Time now { Time::StrictNow() };
		TzInfo tzi;

		Str entry;
		entry.ReserveExact(msgs.Len() * 500)
			 .Add("<reset time=\"").Fun(Enc_EntryTime, now, tzi)
			 .Add("\">\r\n");

		for (Rp<SmtpMsgToSend> const& msg : msgs)
			entry.Fun(Enc_Msg, msg.Ref());

		entry.Add("</reset>\r\n");

		WriteEntry(now, tzi, entry);
	}


	void TextSmtpSendLog::SmtpSendLog_OnAttempt(SmtpMsgToSend const& msg)
	{
		Time now { Time::StrictNow() };
		TzInfo tzi;

		Str entry;
		entry.ReserveExact(500)
			 .Add("<attempt time=\"").Fun(Enc_EntryTime, now, tzi)
			 .Add("\">\r\n")
			 .Fun(Enc_Msg, msg)
			 .Add("</attempt>\r\n");

		WriteEntry(now, tzi, entry);
	}


	void TextSmtpSendLog::SmtpSendLog_OnResult(SmtpMsgToSend const& msg, Vec<MailboxResult> const& mailboxResults, SmtpTlsAssurance::E tlsAssuranceAchieved)
	{
		Time now { Time::StrictNow() };
		TzInfo tzi;

		Str entry;
		entry.ReserveExact(500)
			 .Add("<result time=\"").Fun(Enc_EntryTime, now, tzi)
			 .Add("\" tlsAchieved=\"").Add(SmtpTlsAssurance::Name(tlsAssuranceAchieved))
			 .Add("\">\r\n")
			 .Fun(Enc_Msg, msg);

		for (MailboxResult const& result : mailboxResults)
		{
			entry.Add(" <mbox name=\"").HtmlAttrValue(result.f_mailbox, Html::CharRefs::Escape)
				 .Add("\" state=\"").Add(SmtpDeliveryState::Name(result.f_state));

			if (result.f_state == SmtpDeliveryState::Success)
				entry.Add("\" mx=\"").HtmlAttrValue(result.f_successMx, Html::CharRefs::Escape)
					 .Add("\" localAddr=\"").HtmlAttrValue(result.f_successLocalAddr, Html::CharRefs::Escape)
					 .Add("\" />\r\n");
			else
			{
				entry.Add("\">\r\n");

				SmtpSendFailure const* failure = result.f_failure.Ptr();
				while (failure)
				{
					entry.Add("  <failure time=\"").Fun(Enc_EntryTime, failure->f_time, tzi)
						 .Add("\" stage=\"").Add(SmtpSendStage::Name(failure->f_stage))
						 .Add("\" detail=\"").Add(SmtpSendDetail::Name(failure->f_detail))
						 .Add("\" mx=\"").Add(failure->f_mx)
						 .Add("\" localAddr=\"").Add(failure->f_localAddr)
						 .Add("\" replyCode=\"").Obj(SmtpReplyCode(failure->f_replyCode))
						 .Add("\" enhStatus=\"").Obj(SmtpEnhStatus::FromUint(failure->f_enhStatus))
						 .Add("\" desc=\"").HtmlAttrValue(Seq(failure->f_desc).Trim(), Html::CharRefs::Escape);

					if (!failure->f_lines.Any())
						entry.Add("\" />\r\n");
					else
					{
						entry.Add("\">\r\n");
						for (Seq line : failure->f_lines)
							entry.Add("   <replyLine>").HtmlElemText(line, Html::CharRefs::Escape).Add("</replyLine>\r\n");
						entry.Add("  </failure>\r\n");
					}

					failure = failure->f_prevFailure.DynamicCast<SmtpSendFailure>();
				}

				entry.Add(" </mbox>\r\n");
			}
		}

		entry.Add("</result>\r\n");

		WriteEntry(now, tzi, entry);
	}

}
