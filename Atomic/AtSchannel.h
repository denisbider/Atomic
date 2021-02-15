#pragma once

#include "AtCert.h"
#include "AtReader.h"
#include "AtWriter.h"
#include "AtWinStr.h"


namespace At
{

	struct SecBufs : NoCopy
	{
		struct Owner { enum E { Internal, Sspi }; };

		SecBufs(Owner::E owner, unsigned long t0)                                                       : m_owner(owner) { SetSize(1); InitBufAt(0, t0); }
		SecBufs(Owner::E owner, unsigned long t0, unsigned long t1)                                     : m_owner(owner) { SetSize(2); InitBufAt(0, t0); InitBufAt(1, t1); }
		SecBufs(Owner::E owner, unsigned long t0, unsigned long t1, unsigned long t2)                   : m_owner(owner) { SetSize(3); InitBufAt(0, t0); InitBufAt(1, t1); InitBufAt(2, t2); }
		SecBufs(Owner::E owner, unsigned long t0, unsigned long t1, unsigned long t2, unsigned long t3) : m_owner(owner) { SetSize(4); InitBufAt(0, t0); InitBufAt(1, t1); InitBufAt(2, t2); InitBufAt(3, t3); }

		~SecBufs();

		void SetBufAt(sizet i, void* p, unsigned long n) { SecBuffer& sb = m_bufs[i]; sb.pvBuffer = p; sb.cbBuffer = n; }
		SecBuffer const* FindNonFirstBuf(unsigned long t) const { for (SecBuffer const& sb : m_bufs.GetSlice(1)) { if (sb.BufferType == t) return &sb; } return nullptr; }
		sizet FindExtraBuf_CalcBytesRead(Seq orig) const;
		Seq FindAlertContent() const;

		Owner::E m_owner;
		SecBufferDesc m_sbd;
		Vec<SecBuffer> m_bufs;

	private:
		void SetSize(unsigned long n) { m_bufs.ResizeExact(n); m_sbd.ulVersion = SECBUFFER_VERSION; m_sbd.cBuffers = n; m_sbd.pBuffers = m_bufs.Ptr(); }
		void InitBufAt(sizet i, unsigned long t) { SecBuffer& sb { m_bufs[i] }; sb.BufferType = t; sb.cbBuffer = 0; sb.pvBuffer = nullptr; }
	};


	struct TlsInfo
	{
		bool                         m_tlsStarted       {};
		SECURITY_STATUS              m_connInfoStatus   {};
		SecPkgContext_ConnectionInfo m_connInfo         {};
		SECURITY_STATUS              m_cipherInfoStatus {};
		SecPkgContext_CipherInfo     m_cipherInfo       {};
	};


	class Schannel : virtual public Reader, virtual public Writer
	{
	public:
		struct Err : CommunicationErr { Err(Seq msg) : CommunicationErr(msg) {} };

		enum class ErrLoc { ACH, ISC_Init, IASC_Cont, IASC_Reneg, IASC_Shutdown, QCA, Decrypt, Encrypt, ACT };

		struct SspiErr : WinErr<Err>
		{
			ErrLoc m_errLoc;
			Str    m_alertSent;

			SspiErr(int64 rc, ErrLoc errLoc, Seq alertSent, Seq msg)
				: WinErr<Err>(rc, Str("Schannel: ").Add(msg))
				, m_errLoc(errLoc)
				, m_alertSent(alertSent) {}
		};

		struct SspiErr_Acquire : SspiErr
		{
			SspiErr_Acquire(int64 rc, ErrLoc errLoc, Seq alertSent, Seq msg)
				: SspiErr(rc, errLoc, alertSent, msg) {}
		};
		
		struct SspiErr_LikelyDhIssue_TryRestart : SspiErr
		{
			SspiErr_LikelyDhIssue_TryRestart(int64 rc, ErrLoc errLoc, Seq alertSent, Seq msg)
				: SspiErr(rc, errLoc, alertSent, Str(msg).Add(" - likely DH issue, try restart")) {}
		};

		Schannel(Reader* reader = 0, Writer* writer = 0) : m_reader(reader), m_writer(writer) {} 
		~Schannel();

		PCtxtHandle GetCtxt() { return &m_ctxt; }

		void SetReader(Reader* reader) { EnsureThrow(!m_reader); m_reader = reader; }
		void SetWriter(Writer* writer) { EnsureThrow(!m_writer); m_writer = writer; }

		void SetStopCtl(Rp<StopCtl> const& sc) override final { EnsureThrow(m_reader); EnsureThrow(m_writer); m_reader->SetStopCtl(sc); m_writer->SetStopCtl(sc); }
		void SetStopCtl(Abortable& x) { SetStopCtl(x.GetStopCtl()); }

		void SetExpireTickCount (uint64 tc) override final { EnsureThrow(m_reader); EnsureThrow(m_writer); m_reader->SetExpireTickCount(tc);    m_writer->SetExpireTickCount(tc); }

		// Acts as a transparent Reader/Writer until StartTls is called
		void Read(std::function<ReadInstr(Seq&)> process) override final;
		void Write(Seq data) override final;
		void Shutdown() override final;

		bool TlsStarted() const { return m_state != State::NotStarted; }

		void AddCert(Cert const& cert);
		void InitCred(ProtoSide side);
		void SetServerName(Seq serverName);		// For client use
		void SetManualCredValidation(bool v);
		void SetWeakCiphersOk(bool v);
		void StartTls();

		void GetTlsInfo(TlsInfo& tlsInfo);

	private:
		Reader* m_reader {};
		Writer* m_writer {};
		sizet m_totalPlaintextBytesProcessed {};
		
		struct State { enum E { NotStarted, Started, Ended }; };
		State::E m_state { State::NotStarted };

		ProtoSide                 m_side           { ProtoSide::None };
		Vec<Cert>                 m_certs;
		Vec<PCCERT_CONTEXT>       m_certPtrs;
		bool                      m_haveCredHandle {};
		SCHANNEL_CRED             m_cred;
		CredHandle                m_credHandle;
		Str                       m_serverName;
		WinStr                    m_wsServerName;
		wchar_t*                  m_wzServerName   {};
		bool                      m_manualCredVal  {};
		bool                      m_weakCiphersOk  {};
		unsigned long             m_reqFlags       {};
		bool                      m_haveCtxt       {};
		CtxtHandle                m_ctxt;
		SecPkgContext_StreamSizes m_streamSizes;
		SECURITY_STATUS           m_lastDecryptSt  { SEC_E_OK };
		Str                       m_encryptBuf;

		void SendTokenBufs(SecBufs const& bufs);

		struct HandshakeType { enum E { Initial, Renegotiate, Shutdown }; };
		void CompleteHandshake(HandshakeType::E handshakeType);
	};

}
