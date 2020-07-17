#include "AtIncludes.h"
#include "AtMimeReadWrite.h"

#include "AtImfReadWrite.h"
#include "AtMimeGrammar.h"
#include "AtMimeQuotedPrintable.h"


namespace At
{
	namespace Mime
	{

		// FieldWithParams

		void FieldWithParams::ReadParam(ParseNode const& paramNode, PinStore& store)
		{
			// Name
			ParseNode const& nameTextNode = paramNode.FirstChild().DeepFindRef(id_token_text);
			Seq name = nameTextNode.SrcText();
			Seq value;
						
			// Value
			ParseNode const* valueQsNode = paramNode.LastChild().DeepFind(Imf::id_quoted_string);
			if (valueQsNode != nullptr)
				value = Imf::ReadQuotedString(*valueQsNode, store);
			else
			{
				ParseNode const& valueTextNode = paramNode.LastChild().DeepFindRef(id_token_text);
				value = valueTextNode.SrcText();
			}

			m_params.Add(name, value);
		}


		void FieldWithParams::WriteParams(MsgWriter& writer) const
		{
			if (m_params.Any())
			{
				Seq name, value;
				bool first { true };
				for (NameValuePair const& pair : m_params)
				{
					if (first)
						first = false;
					else
					{
						writer.Add(name);
						writer.Add("=");
						Imf::WriteQuotedString(writer, value, "; ");
					}

					name = pair.m_name;
					value = pair.m_value;
				}

				writer.Add(name);
				writer.Add("=");
				Imf::WriteQuotedString(writer, value);
			}
		}



		// ContentType

		void ContentType::Read(ParseNode const& fieldNode, PinStore& store)
		{
			for (ParseNode const& c : fieldNode)
			{
				if (c.IsType(id_ct_type) ||
					c.IsType(id_ct_subtype))
				{
					ParseNode const& tokenTextNode = c.DeepFindRef(id_token_text);

					if (c.IsType(id_ct_type))
						m_type = tokenTextNode.SrcText();
					else
						m_subType = tokenTextNode.SrcText();
				}
				else if (c.IsType(id_parameter))
					ReadParam(c, store);
			}
		}


		void ContentType::Write(MsgWriter& writer) const
		{
			writer.Add("Content-Type: ");
			WriteContent(writer);
			writer.Add("\r\n");
		}


		void ContentType::WriteContent(MsgWriter& writer) const
		{
			{
				MsgSectionScope section { writer };
				writer.Add(m_type).Add("/").Add(m_subType);
				if (m_params.Any())
					writer.Add("; ");
			}

			WriteParams(writer);
		}


		bool ContentType::IsSafeAsUntrustedInlineContent(Seq content) const
		{
			// To determine if a type is likely safe, we follow the MIME Sniffing Standard as implemented by browsers:
			// https://mimesniff.spec.whatwg.org/#no-sniff-flag

			// May want to expand this with other content types besides images (e.g. audio, video) if it becomes necessary.
			// At the time of this writing, audio/video seems unlikely (and poor form) to be embedded in email, where this is currently used.

			if (EqualType("image"))
			{
				if (m_subType.EqualInsensitive("bmp"  )) { return content.StartsWithExact("BM"); }
				if (m_subType.EqualInsensitive("gif"  )) { return content.StartsWithExact("GIF87a") || content.StartsWithExact("GIF89a"); }
				if (m_subType.EqualInsensitive("webp" )) { return content.StripPrefixExact("RIFF") && content.n>=10 && content.DropBytes(4).StartsWithExact("WEBPVP"); }
				if (m_subType.EqualInsensitive("png"  )) { return content.StartsWithExact("PNG\r\n\x1A\n"); }
				if (m_subType.EqualInsensitive("jpeg" )) { return content.StartsWithExact("\xFF\xD8\xFF"); }
			}

			return false;
		}



		// ContentEnc

		void ContentEnc::Read(ParseNode const& fieldNode, PinStore&)
		{
			ParseNode const& tokenTextNode = fieldNode.FlatFindRef(id_token_text);
			m_value = tokenTextNode.SrcText();
		}


		void ContentEnc::Write(MsgWriter& writer) const
		{
			writer.Add("Content-Transfer-Encoding: ").Add(m_value).Add("\r\n");
		}



		// ContentId

		void ContentId::Read(ParseNode const& fieldNode, PinStore&)
		{
			// Obsolete syntax allows for quoted strings and CFWS within msg-id,
			// but this is troublesome to remove in-depth, and the benefits are unconvincing.
			m_id = fieldNode.FlatFindRef(Imf::id_msg_id).SrcText();
		}


		void ContentId::Write(MsgWriter& writer) const
		{
			writer.Add("Content-ID: ");

			MsgSectionScope { writer };
			writer.Add("<").Add(m_id).Add(">\r\n");
		}



		// ContentDesc

		void ContentDesc::Read(ParseNode const& fieldNode, PinStore&)
		{
			ParseNode const& unstructuredNode = fieldNode.FlatFindRef(Imf::id_unstructured);
			m_desc = unstructuredNode.SrcText();
		}


		void ContentDesc::Write(MsgWriter& writer) const
		{
			writer.Add("Content-Description: ");
			Imf::WriteUnstructured(writer, m_desc);
			writer.Add("\r\n");
		}



		// ContentDisp

		void ContentDisp::Read(ParseNode const& fieldNode, PinStore& store)
		{
			for (ParseNode const& c : fieldNode)
			{
				if (c.IsType(id_disposition_type))
				{
					ParseNode const& tokenTextNode = c.DeepFindRef(id_token_text);
					m_type = tokenTextNode.SrcText();
				}
				else if (c.IsType(id_parameter))
					ReadParam(c, store);
			}
		}


		void ContentDisp::Write(MsgWriter& writer) const
		{
			writer.Add("Content-Disposition: ");
			WriteContent(writer);
			writer.Add("\r\n");
		}


		void ContentDisp::WriteContent(MsgWriter& writer) const
		{
			{
				MsgSectionScope section { writer };
				writer.Add(m_type);
				if (m_params.Any())
					writer.Add("; ");
			}

			WriteParams(writer);
		}



		// ExtensionField

		void ExtensionField::Read(ParseNode const& fieldNode, PinStore&)
		{
			EnsureThrow(fieldNode.FirstChild().IsType(id_extension_field_name));
			m_name = fieldNode.FirstChild().SrcText();

			ParseNode const* valueNode = fieldNode.FlatFind(Imf::id_unstructured);
			if (!valueNode)
				m_value = Seq();
			else
				m_value = valueNode->SrcText();
		}


		void ExtensionField::Write(MsgWriter& writer) const
		{
			writer.Add(m_name).Add(": ");
			Imf::WriteUnstructured(writer, m_value);
			writer.Add("\r\n");
		}



		// MimeVersion

		void MimeVersion::Read(ParseNode const& fieldNode, PinStore&)
		{
			ParseNode const& majorNode = fieldNode.FlatFindRef(id_version_major);
			m_major = majorNode.SrcText().ReadNrUInt16Dec();

			ParseNode const& minorNode = fieldNode.FlatFindRef(id_version_minor);
			m_minor = minorNode.SrcText().ReadNrUInt16Dec();
		}


		void MimeVersion::Write(MsgWriter& writer) const
		{
			AuxStr version { writer };
			version.ReserveAtLeast(10);
			version.UInt(m_major).Ch('.').UInt(m_minor).Add("\r\n");

			writer.Add("MIME-Version: ").Add(version);
		}



		// PartReadErr

		void PartReadErr::EncObj(Enc& enc) const
		{
			enc.Add("MIME part ");

			bool first {};
			for (sizet n : m_errPartPath)
			{
				if (first) first = false; else enc.Ch('.');
				enc.UInt(n);
			}

			enc.Add(": ").Add(m_errDesc);
		}



		// PartReadCx

		void PartReadCx::EncObj(Enc& enc) const
		{
			for (PartReadErr const& err : m_errs)
				enc.Obj(err).Add("\r\n");
		}



		// Part

		void Part::EncLoc(Enc& e, Slice<sizet> loc)
		{
			e.Ch('/');

			if (loc.Any())
			{
				while (true)
				{
					e.UInt(loc.First());
					loc.PopFirst();
					if (!loc.Any()) break;
					e.Ch('/');
				}
			}
		}


		bool Part::ReadLoc(Vec<sizet>& loc, Seq& reader)
		{
			if (!reader.StripPrefixExact("/"))
				return false;

			if (reader.n)
			{
				do
				{
					uint c = reader.FirstByte();
					if (UINT_MAX == c) return false;
					if (!Ascii::IsDecDigit(c)) return false;

					uint64 n = reader.ReadNrUInt64Dec();
					if (UINT64_MAX == n) return false;
					if (SIZE_MAX < n) return false;

					loc.Add((sizet) n);
				}
				while (reader.StripPrefixExact("/"));
			}

			return true;
		}


		Part const* Part::GetPartByLoc(Slice<sizet> loc) const
		{
			Part const* p = this;
			while (loc.Any())
			{
				sizet i = loc.First();
				if (p->m_parts.Len() <= i)
					return nullptr;

				p = &(p->m_parts[i]);
				loc.PopFirst();
			}
			return p;
		}


		bool Part::TryReadMimeField(ParseNode const& fieldNode, PinStore& store)
		{
			if (fieldNode.IsType(id_content_type))    { m_contentType  .ReInit().Read(fieldNode, store); return true; }
			if (fieldNode.IsType(id_content_enc))     { m_contentEnc   .ReInit().Read(fieldNode, store); return true; }
			if (fieldNode.IsType(id_content_id))      { m_contentId    .ReInit().Read(fieldNode, store); return true; }
			if (fieldNode.IsType(id_content_desc))    { m_contentDesc  .ReInit().Read(fieldNode, store); return true; }
			if (fieldNode.IsType(id_content_disp))    { m_contentDisp  .ReInit().Read(fieldNode, store); return true; }
			if (fieldNode.IsType(id_mime_version))    { m_mimeVersion  .ReInit().Read(fieldNode, store); return true; }
			if (fieldNode.IsType(id_extension_field)) { m_extensionFields .Add().Read(fieldNode, store); return true; }
			return false;
		}


		void Part::WriteMimeFields(MsgWriter& writer) const
		{
			if (IsMultipart())	// Implies m_contentType.Any()
			{
				Seq boundary { m_contentType->m_params.Get("boundary") };
				if (!boundary.n)
				{
					// RFC 2045: "A good strategy is to choose a boundary that includes a character
					// sequence such as "=_" which can never appear in a quoted-printable body"
					m_boundaryStorage.ReserveExact(2 + Token::Len);
					m_boundaryStorage.Set("=_");
					Token::Generate(m_boundaryStorage);

					m_contentType->m_params.Add("boundary", m_boundaryStorage);
				}
			}

			if (m_mimeVersion.Any()) m_mimeVersion->Write(writer);
			if (m_contentType.Any()) m_contentType->Write(writer);
			if (m_contentEnc .Any()) m_contentEnc ->Write(writer);
			if (m_contentId  .Any()) m_contentId  ->Write(writer);
			if (m_contentDesc.Any()) m_contentDesc->Write(writer);
			if (m_contentDisp.Any()) m_contentDisp->Write(writer);

			for (ExtensionField const& xf : m_extensionFields)
				xf.Write(writer);
		}


		void Part::ReadPartHeader(ParseNode const& partHeaderNode, PinStore& store)
		{
			for (ParseNode const& fieldNode : partHeaderNode)
				TryReadMimeField(fieldNode, store);
		}


		bool Part::ReadMultipartBody(Seq& encoded, PinStore& store, PartReadCx& prcx)
		{
			ParseTree pt { encoded };
			if (prcx.m_verboseParseErrs)
				pt.RecordBestToStack();
			if (!pt.Parse(Mime::C_multipart_body))
				return prcx.AddParseErr(pt);

			prcx.m_curPartPath.Add(0U);
			OnExit popPartPath = [&prcx] { prcx.m_curPartPath.PopLast(); };

			for (ParseNode const& c : pt.Root().FlatFindRef(Mime::id_multipart_body))
				if (c.IsType(Mime::id_body_part))
				{
					++prcx.m_curPartPath.Last();
					if (!AddChildPart().ReadPart(c, store, prcx))
						if (prcx.m_stopOnNestedErr)
							return false;
				}

			return true;
		}


		void Part::WriteMultipartBody(MsgWriter& writer, Seq boundary) const
		{
			EnsureThrow(m_parts.Any());

			for (Part const& part : m_parts)
			{
				// Part boundary
				{
					MsgSectionScope section { writer };
					writer.Add("--").Add(boundary).Add("\r\n");
				}

				part.WritePart(writer);
			}

			// End boundary
			{
				MsgSectionScope section { writer };
				writer.Add("--").Add(boundary).Add("--\r\n");
			}
		}


		bool Part::ReadPart(ParseNode const& partNode, PinStore& store, PartReadCx& prcx)
		{
			m_srcText = partNode.SrcText();

			for (ParseNode const& c : partNode)
				if (c.IsType(Mime::id_part_header))
					ReadPartHeader(c, store);
				else if (c.IsType(Mime::id_part_content))
					m_contentEncoded = c.SrcText();

			if (IsMultipart())
			{
				if (prcx.m_curPartPath.Len() >= prcx.m_decodeDepth)
					return prcx.AddDecodeDepthErr();

				if (!ReadMultipartBody(m_contentEncoded, store, prcx))
					return false;
			}

			return true;
		}


		void Part::WritePart(MsgWriter& writer) const
		{
			WriteMimeFields(writer);
			writer.Add("\r\n");

			if (!IsMultipart())
				writer.AddVerbatim(m_contentEncoded).AddVerbatim("\r\n");
			else
			{
				// If not set previously by caller, boundary is set in WriteMimeFields()
				Seq boundary { m_contentType->m_params.Get("boundary") };
				EnsureThrow(boundary.n);
				WriteMultipartBody(writer, boundary);
			}
		}


		bool Part::DecodeContent(Seq& decoded, PinStore& store) const
		{
			// Parts of type "multipart" must use an identity encoding ("7bit", "8bit" or "binary")
			EnsureThrow(!IsMultipart());

			if (!m_contentEnc.Any())
				{ decoded = m_contentEncoded; return true; }
				
			Seq encType = m_contentEnc->m_value;
			if (encType.EqualInsensitive("7bit") ||
			    encType.EqualInsensitive("8bit") ||
				encType.EqualInsensitive("binary"))
				{ decoded = m_contentEncoded; return true; }

			if (encType.EqualInsensitive("quoted-printable")) { decoded = DecodeContent_QP     (store); return true; }
			if (encType.EqualInsensitive("base64"))           { decoded = DecodeContent_Base64 (store); return true; }
			return false;
		}


		Seq Part::DecodeContent_QP(PinStore& store) const
		{
			Seq reader = m_contentEncoded;
			Enc& enc = store.GetEnc(QuotedPrintableDecode_MaxLen(reader.n));
			return QuotedPrintableDecode(reader, enc);
		}


		Seq Part::DecodeContent_Base64(PinStore& store) const
		{
			Seq reader = m_contentEncoded;
			Enc& enc = store.GetEnc(Base64::DecodeMaxLen(reader.n));
			return Base64::MimeDecode(reader, enc);
		}


		void Part::EncodeContent_Base64(Seq content, PinStore& store)
		{
			m_contentEnc.ReInit().m_value = "base64";

			if (!content.n)
				m_contentEncoded = Seq();
			else
			{
				Base64::NewLines nl = Base64::NewLines::Mime();
				Enc& enc = store.GetEnc(Base64::EncodeMaxOutLen(content.n, nl));
				m_contentEncoded = Base64::MimeEncode(content, enc, Base64::Padding::Yes, nl);
			}
		}


		void Part::EncodeContent_QP(Seq content, PinStore& store)
		{
			m_contentEnc.ReInit().m_value = "quoted-printable";

			if (!content.n)
				m_contentEncoded = Seq();
			else
			{
				Enc& enc = store.GetEnc(QuotedPrintableEncode_MaxLen(content.n));
				m_contentEncoded = QuotedPrintableEncode(content, enc);
			}
		}


	}
}
