#include "AtIncludes.h"
#include "AtSchannel.h"


namespace At
{
	// SecBufs

	SecBufs::~SecBufs()
	{
		if (m_bufs.Any())
		{
			SECURITY_STATUS stLastErr = SEC_E_OK;
			if (m_owner != Owner::Internal)
			{
				for (SecBuffer& sb : m_bufs)
					if (sb.pvBuffer)
					{
						SECURITY_STATUS st = FreeContextBuffer(sb.pvBuffer);
						if (st != SEC_E_OK)
							stLastErr = st;

						sb.cbBuffer = 0;
						sb.pvBuffer = 0;
					}
			}

			m_sbd.cBuffers = 0;
			m_sbd.pBuffers = nullptr;
			m_bufs.Clear();

			EnsureReportWithCode(stLastErr == SEC_E_OK, stLastErr);
		}
	}


	sizet SecBufs::FindExtraBuf_CalcBytesRead(Seq orig) const
	{
		SecBuffer const* extraBuf = FindBuf(SECBUFFER_EXTRA);
		if (!extraBuf)
			return orig.n;

		if (extraBuf->pvBuffer)
		{
			EnsureThrow(((byte const*) extraBuf->pvBuffer) >= orig.p);
			EnsureThrow(((byte const*) extraBuf->pvBuffer) <  orig.p + orig.n);
		}

		EnsureThrow(extraBuf->cbBuffer <= orig.n);
		return orig.n - extraBuf->cbBuffer;
	}



	// Schannel

	namespace
	{
		// Context Requirements:
		// https://msdn.microsoft.com/en-us/library/windows/desktop/aa374770%28v=vs.85%29.aspx
		//
		// REPLAY_DETECT		The security package detects replayed packets and notifies the caller if a packet has been replayed.
		//						The use of this flag implies all of the conditions specified by the INTEGRITY flag.
		//
		// SEQUENCE_DETECT		The context must be allowed to detect out-of-order delivery of packets later through the message support functions.
		//						Use of this flag implies all of the conditions specified by the INTEGRITY flag.
		//
		// CONFIDENTIALITY		The context can protect data while in transit using the EncryptMessage (General) and DecryptMessage (General) functions.
		//						The CONFIDENTIALITY flag does not work if the generated context is for the Guest account.
		//
		// USE_SUPPLIED_CREDS	Package-specific credential information is available in the input buffer. The security package can use these credentials to authenticate the connection.
		//
		// ALLOCATE_MEMORY		The security package must allocate memory. The caller must eventually call the FreeContextBuffer function to free memory allocated by the security package.
		//
		// EXTENDED_ERROR		Error reply messages for the peer must be generated if the context fails.
		//
		// STREAM				Stream semantics must be used. For more information, see Stream Contexts.

		enum { IscReqFlags = ISC_REQ_REPLAY_DETECT | ISC_REQ_SEQUENCE_DETECT | ISC_REQ_CONFIDENTIALITY | ISC_REQ_ALLOCATE_MEMORY | ISC_REQ_EXTENDED_ERROR | ISC_REQ_STREAM | ISC_REQ_USE_SUPPLIED_CREDS };
		enum { AscReqFlags = ASC_REQ_REPLAY_DETECT | ASC_REQ_SEQUENCE_DETECT | ASC_REQ_CONFIDENTIALITY | ASC_REQ_ALLOCATE_MEMORY | ASC_REQ_EXTENDED_ERROR | ASC_REQ_STREAM };

		enum { IscRetFlags = ISC_RET_REPLAY_DETECT | ISC_RET_SEQUENCE_DETECT | ISC_RET_CONFIDENTIALITY | ISC_RET_STREAM };
		enum { AscRetFlags = ASC_RET_REPLAY_DETECT | ASC_RET_SEQUENCE_DETECT | ASC_RET_CONFIDENTIALITY | ASC_RET_STREAM };

		unsigned long ExpectedRetFlags(ProtoSide side) { if (side == ProtoSide::Client) return IscRetFlags; return AscRetFlags; }
		unsigned long RetExtendedError(ProtoSide side) { if (side == ProtoSide::Client) return ISC_RET_EXTENDED_ERROR; return ASC_RET_EXTENDED_ERROR; }
		bool HaveRetExtendedError(unsigned long retFlags, ProtoSide side) { return (retFlags & RetExtendedError(side)) != 0; }

	} // anon


	Schannel::~Schannel()
	{
		if (m_haveCredHandle)
		{
			SECURITY_STATUS st { FreeCredentialsHandle(&m_credHandle) };
			EnsureReportWithCode(st == SEC_E_OK, st);
		}

		for (PCCERT_CONTEXT cert : m_certs)
			CertFreeCertificateContext(cert);	// According to MSDN, "The function always returns nonzero."

		if (m_haveCtxt)
		{
			SECURITY_STATUS st { DeleteSecurityContext(&m_ctxt) };
			EnsureReportWithCode(st == SEC_E_OK, st);
		}
	}


	void Schannel::Read(std::function<Instr::E(Seq&)> process)
	{
		try
		{
			if (m_state == State::NotStarted)
			{
				EnsureThrow(m_reader != nullptr);
				m_reader->Read(process);
			}
			else
			{
				EnsureThrow(m_state == State::Started);

				while (true)
				{
					if (ProcessData(process))
						return;
				
					if (m_lastDecryptSt == SEC_I_CONTEXT_EXPIRED)
						throw Reader::ReachedEnd();

					m_reader->Read(
						[&] (Seq& reader) -> Reader::Instr::E
						{
							SecBufs bufs(SecBufs::Owner::Internal, SECBUFFER_DATA, SECBUFFER_EMPTY, SECBUFFER_EMPTY, SECBUFFER_EMPTY);
							bufs.SetBufAt(0, (void*) reader.p, NumCast<unsigned long>(reader.n));

							m_lastDecryptSt = DecryptMessage(&m_ctxt, &bufs.m_sbd, 0, nullptr);
							if (m_lastDecryptSt == SEC_E_INCOMPLETE_MESSAGE)
								return Reader::Instr::NeedMore;

							SecBuffer const* dataBuf = bufs.FindBuf(SECBUFFER_DATA);
							if (dataBuf != nullptr && dataBuf->cbBuffer != 0)
							{
								PrepareToRead(dataBuf->cbBuffer, dataBuf->cbBuffer);
								memmove(m_buf.Ptr() + m_bytesInBuf, dataBuf->pvBuffer, dataBuf->cbBuffer);
								m_bytesInBuf += dataBuf->cbBuffer;
								m_haveNewDataToProcess = true;
							}

							sizet readBytes = bufs.FindExtraBuf_CalcBytesRead(reader);
							reader.ReadBytes(readBytes);
							return Reader::Instr::Done;
						} );

					if (m_lastDecryptSt == SEC_I_RENEGOTIATE)
					{
						{
							SecBufs outBufs(SecBufs::Owner::Sspi, SECBUFFER_TOKEN, SECBUFFER_ALERT);
							unsigned long retFlags = 0;
							SECURITY_STATUS st;

							if (m_side == ProtoSide::Client)
								st = InitializeSecurityContext(&m_credHandle, &m_ctxt, (wchar_t*) m_wsServerName.Z(), m_reqFlags, 0, 0, nullptr, 0, nullptr, &outBufs.m_sbd, &retFlags, nullptr);
							else
								st = AcceptSecurityContext(&m_credHandle, &m_ctxt, nullptr, m_reqFlags, 0, nullptr, &outBufs.m_sbd, &retFlags, nullptr);
							
							if (st != SEC_I_CONTINUE_NEEDED)
								throw SspiErr(st, "Unexpected return value from Initialize or AcceptSecurityContext (renegotiate)");

							SendTokenBufs(outBufs);
						}

						CompleteHandshake(HandshakeType::Renegotiate);

						m_lastDecryptSt = SEC_E_OK;
					}
					else
					{
						if (m_lastDecryptSt != SEC_E_OK && m_lastDecryptSt != SEC_I_CONTEXT_EXPIRED)
							throw SspiErr(m_lastDecryptSt, "Error in DecryptMessage");
					}
				}
			}
		}
		catch (...)
		{
			m_state = State::Ended;
			throw;
		}
	}


	void Schannel::Write(Seq data)
	{
		try
		{
			if (m_state == State::NotStarted)
			{
				EnsureThrow(m_writer != nullptr);
				m_writer->Write(data);
			}
			else
			{
				EnsureThrow(m_state == State::Started);

				while (data.n)
				{
					Seq           chunk    { data.ReadBytes(m_streamSizes.cbMaximumMessage) };
					unsigned long chunkLen { NumCast<unsigned long>(chunk.n) };
				
					SecBufs bufs(SecBufs::Owner::Internal, SECBUFFER_STREAM_HEADER, SECBUFFER_DATA, SECBUFFER_STREAM_TRAILER, SECBUFFER_EMPTY);
					bufs.SetBufAt(0, m_encryptBuf.Ptr(),                                     m_streamSizes.cbHeader  );
					bufs.SetBufAt(1, m_encryptBuf.Ptr() + m_streamSizes.cbHeader,            chunkLen                );
					bufs.SetBufAt(2, m_encryptBuf.Ptr() + m_streamSizes.cbHeader + chunkLen, m_streamSizes.cbTrailer );

					memcpy_s(bufs.m_bufs[1].pvBuffer, bufs.m_bufs[1].cbBuffer, chunk.p, chunk.n);

					SECURITY_STATUS st { EncryptMessage(&m_ctxt, 0, &bufs.m_sbd, 0) };
					if (st != SEC_E_OK)
						throw SspiErr(st, "Error in EncryptMessage");

					Seq encrypted(bufs.m_bufs[0].pvBuffer, bufs.m_bufs[0].cbBuffer + bufs.m_bufs[1].cbBuffer + bufs.m_bufs[2].cbBuffer);
					m_writer->Write(encrypted);
				}
			}
		}
		catch (...)
		{
			m_state = State::Ended;
			throw;
		}
	}


	void Schannel::Shutdown()
	{
		EnsureThrow(m_state != State::Ended);

		OnExit setEnded( [&] () { m_state = State::Ended; } );

		if (m_state == State::NotStarted)
		{
			EnsureThrow(m_writer != nullptr);
			m_writer->Shutdown();
		}
		else
		{
			{
				DWORD type = SCHANNEL_SHUTDOWN;
				SecBufs bufs(SecBufs::Owner::Internal, SECBUFFER_TOKEN);
				bufs.SetBufAt(0, &type, sizeof(type));

				SECURITY_STATUS st = ApplyControlToken(&m_ctxt, &bufs.m_sbd);
				if (st != SEC_E_OK)
					throw SspiErr(st, "Error in ApplyControlToken");

				SecBufs outBufs(SecBufs::Owner::Sspi, SECBUFFER_TOKEN, SECBUFFER_ALERT);
				unsigned long retFlags = 0;

				if (m_side == ProtoSide::Client)
					st = InitializeSecurityContext(&m_credHandle, &m_ctxt, (wchar_t*) m_wsServerName.Z(), m_reqFlags, 0, 0, nullptr, 0, nullptr, &outBufs.m_sbd, &retFlags, nullptr);
				else
					st = AcceptSecurityContext(&m_credHandle, &m_ctxt, nullptr, m_reqFlags, 0, nullptr, &outBufs.m_sbd, &retFlags, nullptr);
				
				if (st != SEC_E_OK && st != SEC_I_CONTEXT_EXPIRED && st != SEC_I_CONTINUE_NEEDED)
					throw SspiErr(st, "Unexpected return value from Initialize or AcceptSecurityContext (shutdown)");

				SendTokenBufs(outBufs);

				if (st == SEC_I_CONTINUE_NEEDED)
					CompleteHandshake(HandshakeType::Shutdown);
			}

			m_writer->Shutdown();
		}
	}


	void Schannel::AddCert(PCCERT_CONTEXT certContext)
	{
		EnsureThrow(m_state == State::NotStarted);
		EnsureThrow(!m_haveCredHandle);
		EnsureThrow(certContext != nullptr);

		m_certs.Add(certContext);
	}


	void Schannel::InitCred(ProtoSide side)
	{
		EnsureThrow(m_state == State::NotStarted);
		EnsureThrow(!m_haveCredHandle);
		EnsureThrow(side == ProtoSide::Client || side == ProtoSide::Server);
		m_side = side;

		ZeroMemory(&m_cred, sizeof(m_cred));
		m_cred.dwVersion = SCHANNEL_CRED_VERSION;

		if (m_side == ProtoSide::Client)
			m_cred.grbitEnabledProtocols = SP_PROT_TLS1_0_CLIENT | SP_PROT_TLS1_1_CLIENT | SP_PROT_TLS1_2_CLIENT;
		else
			m_cred.grbitEnabledProtocols = SP_PROT_TLS1_0_SERVER | SP_PROT_TLS1_1_SERVER | SP_PROT_TLS1_2_SERVER;

		m_cred.dwMinimumCipherStrength = 128;
		m_cred.dwFlags = SCH_CRED_AUTO_CRED_VALIDATION | SCH_CRED_NO_DEFAULT_CREDS | SCH_SEND_AUX_RECORD | SCH_USE_STRONG_CRYPTO;
		
		m_cred.cCreds = NumCast<DWORD>(m_certs.Len());
		m_cred.paCred = m_certs.Ptr();

		SECURITY_STATUS st = AcquireCredentialsHandle(nullptr, UNISP_NAME, SECPKG_CRED_OUTBOUND, nullptr, &m_cred, nullptr, nullptr, &m_credHandle, nullptr);
		if (st != SEC_E_OK)
			throw SspiErr(st, "Error in AcquireCredentialsHandle");

		m_haveCredHandle = true;
	}


	void Schannel::ValidateServerName(Seq serverName)
	{
		EnsureThrow(m_state == State::NotStarted);
		m_serverName = serverName;
	}


	void Schannel::StartTls()
	{
		EnsureThrow(m_reader != nullptr);
		EnsureThrow(m_writer != nullptr);
		EnsureThrow(m_state == State::NotStarted);
		EnsureThrow(m_haveCredHandle);
		EnsureThrow(!m_haveCtxt);

		try
		{
			if (m_side == ProtoSide::Server)
				m_reqFlags = AscReqFlags;
			else
			{
				m_reqFlags = IscReqFlags;
				
				if (!m_serverName.Any())
				{
					m_reqFlags |= ISC_REQ_MANUAL_CRED_VALIDATION;
					m_wzServerName = nullptr;
				}
				else
				{
					m_wsServerName.Set(m_serverName);
					m_wzServerName = (wchar_t*) m_wsServerName.Z();
				}

				SecBufs outBufs(SecBufs::Owner::Sspi, SECBUFFER_TOKEN, SECBUFFER_ALERT);
				unsigned long retFlags = 0;
				SECURITY_STATUS st = InitializeSecurityContext(&m_credHandle, nullptr, m_wzServerName, m_reqFlags, 0, 0, nullptr, 0, &m_ctxt, &outBufs.m_sbd, &retFlags, nullptr);
				if (SUCCEEDED(st))
					m_haveCtxt = true;
				if (st != SEC_I_CONTINUE_NEEDED)
					throw SspiErr(st, "Unexpected return value from InitializeSecurityContext (initial)");

				SendTokenBufs(outBufs);
			}

			CompleteHandshake(HandshakeType::Initial);
			m_state = State::Started;
		}
		catch (...)
		{
			m_state = State::Ended;
			throw;
		}
	}


	void Schannel::GetTlsInfo(TlsInfo& tlsInfo)
	{
		tlsInfo.m_tlsStarted = TlsStarted();
		if (tlsInfo.m_tlsStarted)
		{
			tlsInfo.m_connInfoStatus   = QueryContextAttributesW(&m_ctxt, SECPKG_ATTR_CONNECTION_INFO, &tlsInfo.m_connInfo   );
			tlsInfo.m_cipherInfoStatus = QueryContextAttributesW(&m_ctxt, SECPKG_ATTR_CIPHER_INFO,     &tlsInfo.m_cipherInfo );		
		}
	}


	void Schannel::SendTokenBufs(SecBufs const& bufs)
	{
		for (SecBuffer const& sb : bufs.m_bufs)
			if (sb.cbBuffer != 0 && sb.pvBuffer != nullptr)
				m_writer->Write(Seq(sb.pvBuffer, sb.cbBuffer));
	}


	void Schannel::CompleteHandshake(HandshakeType::E handshakeType)
	{
		unsigned long retFlags = 0;
		while (true)
		{
			SECURITY_STATUS st = SEC_E_OK;
			m_reader->Read(
				[&] (Seq& reader) -> Reader::Instr::E
				{
					SecBufs inBufs(SecBufs::Owner::Internal, SECBUFFER_TOKEN, SECBUFFER_EMPTY);
					inBufs.SetBufAt(0, (void*) reader.p, NumCast<unsigned long>(reader.n));

					SecBufs outBufs(SecBufs::Owner::Sspi, SECBUFFER_TOKEN);

					if (m_side == ProtoSide::Client)
						st = InitializeSecurityContext(&m_credHandle, &m_ctxt, (wchar_t*) m_wsServerName.Z(), m_reqFlags, 0, 0, &inBufs.m_sbd, 0, nullptr, &outBufs.m_sbd, &retFlags, nullptr);
					else
					{
						PCtxtHandle existingCtxt = If(m_haveCtxt, PCtxtHandle, &m_ctxt, nullptr);
						PCtxtHandle createCtxt = If(m_haveCtxt, PCtxtHandle, nullptr, &m_ctxt);
						st = AcceptSecurityContext(&m_credHandle, existingCtxt, &inBufs.m_sbd, m_reqFlags, 0, createCtxt, &outBufs.m_sbd, &retFlags, nullptr);
						if (!m_haveCtxt && SUCCEEDED(st))
							m_haveCtxt = true;
					}

					if (st == SEC_E_INCOMPLETE_MESSAGE)
						return Reader::Instr::NeedMore;

					if (st == SEC_E_OK ||
						st == SEC_I_CONTINUE_NEEDED ||
						st == SEC_I_CONTEXT_EXPIRED ||
						FAILED(st) && HaveRetExtendedError(retFlags, m_side))
					{
						SendTokenBufs(outBufs);
					}

					sizet readBytes = inBufs.FindExtraBuf_CalcBytesRead(reader);
					reader.ReadBytes(readBytes);
					return Reader::Instr::Done;
				} );


			if (st == SEC_E_OK)
				break;

			if (handshakeType == HandshakeType::Shutdown && st == SEC_I_CONTEXT_EXPIRED)
				break;

			if (st == SEC_E_BUFFER_TOO_SMALL && m_side == ProtoSide::Client)
			{
				// From https://github.com/dblock/waffle/pull/128#issuecomment-163342222:
				// wbond: "I've tracked it down to the ServerKeyExchange handshake message that is sent from the server to the client for DHE_RSA cipher suites.
				// That is: TLS_DHE_RSA_WITH_AES_128_GCM_SHA256, TLS_DHE_RSA_WITH_AES_256_GCM_SHA384. Sometimes a server will send a value for dh_Ys from
				// https://tools.ietf.org/html/rfc5246#page-51 with a byte encoding that is one byte less than other times. This appears to be instances where
				// the public integer can be encoded in a shorter byte string. In these situations, SChannel returns a SEC_E_BUFFER_TOO_SMALL error.
				// I've confirmed this to happen on Windows 7 and Windows 8.1. I have not confirmed on Windows 10 yet."
				throw SspiErr_LikelyDhIssue_TryRestart(st, "Error in InitializeSecurityContext (subsequent)");
			}

			if (st != SEC_I_CONTINUE_NEEDED)
				throw SspiErr(st, "Error in Initialize or AcceptSecurityContext (subsequent)");
		}

		if (handshakeType != HandshakeType::Shutdown)
		{
			unsigned long expectedRetFlags = ExpectedRetFlags(m_side);
			if ((retFlags & expectedRetFlags) != expectedRetFlags)
				throw Err(Str("Initialize or AcceptSecurityContext succeeded, but not all expected security features are present. Expected: ").UInt(expectedRetFlags).Add(", actual: ").UInt(retFlags));

			SECURITY_STATUS st = QueryContextAttributes(&m_ctxt, SECPKG_ATTR_STREAM_SIZES, &m_streamSizes);
			if (st != SEC_E_OK)
				throw SspiErr(st, "Error in QueryContextAttributes (SECPKG_ATTR_STREAM_SIZES)");

			m_encryptBuf.Resize(m_streamSizes.cbHeader + m_streamSizes.cbMaximumMessage + m_streamSizes.cbTrailer);
		}
	}

}
