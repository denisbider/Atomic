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
						Imf::WriteQuotedString(writer, value, "\"; ");
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

			{
				MsgSectionScope section { writer };
				writer.Add(m_type).Add("/").Add(m_subType);
				if (m_params.Any())
					writer.Add("; ");
			}

			WriteParams(writer);
			writer.Add("\r\n");
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

			{
				MsgSectionScope section { writer };
				writer.Add(m_type);
				if (m_params.Any())
					writer.Add("; ");
			}

			WriteParams(writer);
			writer.Add("\r\n");
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



		// MultipartBody

		bool MultipartBody::Read(Seq& encoded, PinStore& store, sizet depth)
		{
			ParseTree pt(encoded);
			if (!pt.Parse(Mime::C_multipart_body))
				return false;

			for (ParseNode const& c : pt.Root().FlatFindRef(Mime::id_multipart_body))
				if (c.IsType(Mime::id_body_part))
					m_parts.Add().ReadPart(c, store, depth);

			return true;
		}


		void MultipartBody::Read(ParseTree const& ptBody, PinStore& store, sizet depth)
		{
			for (ParseNode const& c : ptBody.Root().FlatFindRef(Mime::id_multipart_body))
				if (c.IsType(Mime::id_body_part))
					m_parts.Add().ReadPart(c, store, depth);
		}


		void MultipartBody::Write(MsgWriter& writer, Seq boundary) const
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



		// Part

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


		void Part::ReadPart(ParseNode const& partNode, PinStore& store, sizet depth)
		{
			m_srcText = partNode.SrcText();

			for (ParseNode const& c : partNode)
				if (c.IsType(Mime::id_part_header))
					ReadPartHeader(c, store);
				else if (c.IsType(Mime::id_part_content))
					m_contentEncoded = c.SrcText();

			if (IsMultipart() && depth < MaxAutoDecodeDepth)
				m_multipartBody.Read(m_contentEncoded, store, depth + 1);
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
				m_multipartBody.Write(writer, boundary);
			}
		}


		bool Part::DecodeContent(Seq& decoded, Str& storage) const
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

			if (encType.EqualInsensitive("quoted-printable")) { storage.Clear(); DecodeContent_QP     (storage); decoded = storage; return true; }
			if (encType.EqualInsensitive("base64"))           { storage.Clear(); DecodeContent_Base64 (storage); decoded = storage; return true; }
			return false;
		}


		void Part::DecodeContent_QP(Enc& enc) const
		{
			Seq reader = m_contentEncoded;
			QuotedPrintableDecode(reader, enc);
		}


		void Part::DecodeContent_Base64(Enc& enc) const
		{
			Seq reader = m_contentEncoded;
			Base64::MimeDecode(reader, enc);
		}


	}
}
