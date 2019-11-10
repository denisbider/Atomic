#pragma once

#include "AtCrypt.h"
#include "AtOpt.h"
#include "AtParse.h"
#include "AtTime.h"


namespace At
{
	namespace Dkim
	{
		// DKIM signature signing and verification implemented according to RFC 6376.



		// Signing

		// Parses the message and produces a DKIM signature using the provided signing key and Signing Domain ID and selector.
		// Produces a string containing a DKIM-Signature field terminated by CRLF. This should be prepended to the message.
		// Uses rsa-sha256.
		//
		// The following behaviors were chosen because they appear to be used by major email originators (e.g. Google, Yahoo, MS)
		// as of August 2016:
		// - Canonicalization is relaxed/relaxed.
		// - Do not set an AUID parameter ("i=").
		// - Header field absence is not signed.

		void SignMessage(RsaSigner& signer, Seq sdid, Seq selector, Seq message, Str& sigField);



		// SigVerifier

		struct SigState
		{
			enum E
			{
				None,
				Failed,
				Parsed,
				Verified,
			};

			static char const* GetName(uint n);
		};


		enum class SigAlg
		{
			None,
			RsaSha1,
			RsaSha256,
		};


		enum class SigCanon
		{
			Simple,
			Relaxed,
		};


		enum class PubKeyLoadResult
		{
			PermFail,
			TempFail,
			Available,
		};


		struct SigInfo
		{
			SigState::E           m_state              { SigState::None };
			Seq                   m_failReason;

			SigAlg                m_alg                { SigAlg::None };
			Seq                   m_sdid;
			Opt<Seq>              m_auid;
			Seq                   m_selector;
			SigCanon              m_hdrCanon           { SigCanon::Simple };
			SigCanon              m_bodyCanon          { SigCanon::Simple };
			Opt<Time>             m_timestamp;
			Opt<Time>             m_expiration;
			Vec<Seq>              m_hdrsSigned;
			Opt<uint64>           m_bodyLenSigned;
			sizet                 m_bodyLenUnsigned    {};
			Str                   m_bodyHashEncoded;							// Binary
			Str                   m_signatureEncoded;							// Binary

			Str                   m_bodyHashCalculated;							// Binary
			Str                   m_sigInputToVerify;							// Constructed signature input to verify

			PubKeyLoadResult      m_lastPubKeyLoadResult;
			Str                   m_lastPubKeyLoadErr;
			Opt<RsaVerifier>      m_pubKey;

			bool SetFail(char const* z)
			{
				m_state = SigState::Failed;
				m_failReason = z;
				return false;
			};
		};


		class SigVerifier
		{
		public:
			SigVerifier(ParseTree const& ptMessage) : m_pt(ptMessage) {}

			// Parses message, calculates hashes, makes signatures available. Throws on failure.
			void ReadSignatures();

			// Becomes available (initialized) if LoadMessage() is successful.
			// User should walk the signatures and set a verifier for signatures to be verified.
			Vec<SigInfo>& Sigs() { return m_sigs; }

			// Verifies signatures that have not yet been verified and have verifiers available.
			// Does not throw on verification failure. Results are obtained by walking signatures.
			void VerifySignatures();

		private:
			ParseTree const& m_pt;
			Vec<SigInfo>     m_sigs;

			bool ReadSignature     (ParseNode const& msgFieldsNode, ParseNode const& sigNode);
			bool CalculateBodyHash (SigInfo& sig);
			void ConstructSigInput (ParseNode const& msgFieldsNode, ParseNode const& sigNode, SigInfo& sig, Seq encodedSigValue);
			void AddSigInputField  (SigInfo& sig, Seq field);
		};


		class PubKeyLocator
		{
		public:
			// Updates sig.m_lastPubKeyLoadResult, sig.m_lastPubKeyLoadErr, and sig.m_pubKey if successful.
			// If an error occurs, a description may be stored in sig.m_lastVrfLoadErr.
			PubKeyLoadResult TryLoadSigPubKey(SigInfo& sig);

		private:
		};

	}
}
