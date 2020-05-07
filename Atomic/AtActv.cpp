#include "AtIncludes.h"
#include "AtActv.h"

#include "AtEncode.h"
#include "AtNum.h"
#include "AtReconstruct.h"
#include "AtToken.h"
#include "AtUtf8.h"

namespace At
{
	namespace Actv
	{

		// ActvCodeData

		bool ActvCodeData::operator== (ActvCodeData const& x) const
		{
			if (m_activationCodeFlags  != x.m_activationCodeFlags  ) return false;
			if (m_licenseGroupToken    != x.m_licenseGroupToken    ) return false;
			if (m_licenseTypeIdNr      != x.m_licenseTypeIdNr      ) return false;
			if (m_licensedToName       != x.m_licensedToName       ) return false;
			if (m_licenseGroupName     != x.m_licenseGroupName     ) return false;
			if (m_groupLicUnitHdsTotal != x.m_groupLicUnitHdsTotal ) return false;
			if (m_signatureKeyIdNr     != x.m_signatureKeyIdNr     ) return false;

			if (m_generatedTime.ToUnixTime() != x.m_generatedTime.ToUnixTime())
				return false;

			if ((  m_upgradeExpiryTime.ToUnixTime() / Time::SecondsPerDay) !=
				(x.m_upgradeExpiryTime.ToUnixTime() / Time::SecondsPerDay))
				return false;

			return true;
		}


		ActvCodeData::DecodeResult ActvCodeData::Decode(Seq actvCodeBinary)
		{
			Seq reader = actvCodeBinary;
			if (!DecodeUInt16(reader, m_activationCodeFlags))
				return DecodeResult::TooShort;

			Seq licenseGroupTokenRaw = reader.ReadBytes(Token::RawLen);
			if (licenseGroupTokenRaw.n != Token::RawLen)
				return DecodeResult::TooShort;
			m_licenseGroupToken = Token::ConvertBackFromBinary(licenseGroupTokenRaw);

			if (!DecodeUInt16(reader, m_licenseTypeIdNr))
				return DecodeResult::TooShort;

			uint32 generatedUnixTime {};
			if (!DecodeUInt32(reader, generatedUnixTime))
				return DecodeResult::TooShort;
			m_generatedTime = Time::FromUnixTime(generatedUnixTime);

			uint licensedToNameLen = reader.ReadByte();
			if (licensedToNameLen > 255)
				return DecodeResult::TooShort;

			Seq licensedToName = reader.ReadBytes(licensedToNameLen);
			if (licensedToName.n != licensedToNameLen)
				return DecodeResult::TooShort;
			m_licensedToName = licensedToName;

			uint licenseGroupNameLen = reader.ReadByte();
			if (licenseGroupNameLen > 255)
				return DecodeResult::TooShort;

			Seq licenseGroupName = reader.ReadBytes(licenseGroupNameLen);
			if (licenseGroupName.n != licenseGroupNameLen)
				return DecodeResult::TooShort;
			m_licenseGroupName = licenseGroupName;

			if (!DecodeUInt32(reader, m_groupLicUnitHdsTotal))
				return DecodeResult::TooShort;

			unsigned int upgradeExpiryUnixTimeDays;
			if (!DecodeUInt16(reader, upgradeExpiryUnixTimeDays))
				return DecodeResult::TooShort;
			int64 upgradeExpiryUnixTime = upgradeExpiryUnixTimeDays;
			upgradeExpiryUnixTime *= Time::SecondsPerDay;
			upgradeExpiryUnixTime += Time::SecondsPerDay - 1;	// Use maximum possible value, which is 23:59:59 on the indicated day
			m_upgradeExpiryTime = Time::FromUnixTime(upgradeExpiryUnixTime);

			if (!DecodeUInt16(reader, m_signatureKeyIdNr))
				return DecodeResult::TooShort;

			uint extensionDataBytes = reader.ReadByte();
			if (extensionDataBytes > 255)
				return DecodeResult::TooShort;

			Seq extensionData = reader.ReadBytes(extensionDataBytes);
			if (extensionData.n != extensionDataBytes)
				return DecodeResult::TooShort;

			if (reader.n < ActvCode_SigLen)
				return DecodeResult::TooShort;
			if (reader.n > ActvCode_SigLen)
				return DecodeResult::TooLong;

			return DecodeResult::OK;
		}


		ActvCodeData::CheckResult ActvCodeData::CheckFields() const
		{
			if (m_activationCodeFlags > std::numeric_limits<uint16>::max()) return CheckResult::ActivationCodeFlags;
			if (m_licenseGroupToken.Len() != Token::Len)                    return CheckResult::LicenseGroupToken;
			if (m_licenseTypeIdNr > std::numeric_limits<uint16>::max())     return CheckResult::LicenseTypeIdNr;
			if (!IsTimeValid(m_generatedTime))                              return CheckResult::GeneratedTime;
			if (m_licensedToName.Len() > MaxStrBytes)                       return CheckResult::LicensedToNameLen;
			if (!Utf8::IsValid(m_licensedToName))                           return CheckResult::LicensedToNameUtf8;
			if (m_licenseGroupName.Len() > MaxStrBytes)                     return CheckResult::LicenseLocatorLen;
			if (!Utf8::IsValid(m_licenseGroupName))                         return CheckResult::LicenseLocatorUtf8;
			if (!IsTimeValid(m_upgradeExpiryTime))                          return CheckResult::UpgradeExpiryTime;
			if (m_signatureKeyIdNr > MaxSignatureKeyIdNr)                   return CheckResult::SignatureKeyIdNr;
			return CheckResult::OK;
		}


		void ActvCodeData::EnsureValid() const
		{
			CheckResult result = CheckFields();
			EnsureThrowWithNr(result == CheckResult::OK, (int64) result);
		}


		bool ActvCodeData::IsTimeValid(Time t)
		{
			int64 unixTime = t.ToUnixTime();
			if (unixTime < 0) return false;
			if (unixTime > std::numeric_limits<uint32>::max()) return false;
			return true;
		}



		// ActvCode_BestLineLen

		sizet ActvCode_BestLineLen(sizet actvCodeLen)
		{
			sizet lineLen = ActvCode_MaxLineLen;
			sizet nrLines = (actvCodeLen + lineLen - 1) / lineLen;
			
			while (true)
			{
				sizet lastLineLen = actvCodeLen % lineLen;
				if (lastLineLen == 0)
					break;

				sizet newLineLen = lineLen - 1;
				sizet newNrLines = (actvCodeLen + newLineLen - 1) / newLineLen;
				if (newNrLines > nrLines)
					break;

				lineLen = newLineLen;
				if (lineLen == ActvCode_MinLineLen)
					break;
			}

			return lineLen;
		}



		// ActvCodeSigner

		void ActvCodeSigner::GenerateKeypair(Str& privKey, Str& pubKey)
		{
			BCrypt::Key key;
			key.GenerateKeyPair(m_provEcdsaP256.Ref(), 0, 0);
			key.FinalizeKeyPair();
			key.Export(privKey, BCRYPT_ECCPRIVATE_BLOB);
			key.Export(pubKey, BCRYPT_ECCPUBLIC_BLOB);
		}


		void ActvCodeSigner::Sign(ActvCodeData const& data, Str& actvCodeBinary)
		{
			data.EnsureValid();

			Str licenseGroupTokenBinary = Token::ToBinary(data.m_licenseGroupToken);
			EnsureThrow(licenseGroupTokenBinary.Len() == Token::RawLen);

			sizet encodedDataLen =
				2 + Token::RawLen + 2 + 4 +
				1 + data.m_licensedToName.Len() +
				1 + data.m_licenseGroupName.Len() +
				4 + 2 + 2 + 1;

			Enc::Meter meter = actvCodeBinary.IncMeter(encodedDataLen + ActvCode_SigLen);
			EncodeUInt16   (actvCodeBinary, data.m_activationCodeFlags);
			EncodeVerbatim (actvCodeBinary, licenseGroupTokenBinary);
			EncodeUInt16   (actvCodeBinary, data.m_licenseTypeIdNr);
			EncodeUInt32   (actvCodeBinary, NumCast<uint32>(data.m_generatedTime.ToUnixTime()));
			EncodeByte     (actvCodeBinary, NumCast<byte>(data.m_licensedToName.Len()));
			EncodeVerbatim (actvCodeBinary, data.m_licensedToName);
			EncodeByte     (actvCodeBinary, NumCast<byte>(data.m_licenseGroupName.Len()));
			EncodeVerbatim (actvCodeBinary, data.m_licenseGroupName);
			EncodeUInt32   (actvCodeBinary, data.m_groupLicUnitHdsTotal);
			EncodeUInt16   (actvCodeBinary, NumCast<uint32>(data.m_upgradeExpiryTime.ToUnixTime()) / Time::SecondsPerDay);
			EncodeUInt16   (actvCodeBinary, data.m_signatureKeyIdNr);
			EncodeByte     (actvCodeBinary, 0);
			EnsureThrow(meter.WrittenLen() == encodedDataLen);

			Str digest;
			m_sha512.Begin(m_provSha512.Ref());
			m_sha512.Update(actvCodeBinary);
			m_sha512.Final(digest);
			EnsureThrow(64 == digest.Len());
			digest.Resize(32);

			Str sig;
			m_key.Sign(nullptr, digest, sig, 0);
			EnsureThrow(ActvCode_SigLen == sig.Len());

			actvCodeBinary.Add(sig);
		}



		// ActvCodeVerifier

		bool ActvCodeVerifier::Verify(Seq actvCodeBinary)
		{
			Seq reader = actvCodeBinary;
			if (reader.n < ActvCode_SigLen)
				return false;

			sizet actvCodeDataLen = reader.n - ActvCode_SigLen;
			Seq actvCodeData = reader.ReadBytes(actvCodeDataLen);
			Seq sig = reader;

			Str digest;
			m_sha512.Begin(m_provSha512.Ref());
			m_sha512.Update(actvCodeData);
			m_sha512.Final(digest);
			EnsureThrow(64 == digest.Len());
			digest.Resize(32);

			return m_key.Verify(nullptr, digest, sig, 0);
		}

	}
}
