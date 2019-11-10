#pragma once

#include "AtBCrypt.h"
#include "AtStr.h"
#include "AtTime.h"

// These activation primitives provide support for implementing activation functionality:
//
// - In deployed applications of various types, possibly using parallel implementations of this functionality.
//
// - In an activation server - intended to be Integral.

namespace At
{
	namespace Actv
	{
		// Reasons for the chosen algorithms:
		//
		// - The worst-case supported target platform for products that communicate with the activation server (not the activation server itself)
		//   is Windows XP SP3 or Windows Server 2003 with FIPS mode activated. This means restricting cryptography to what is provided by Windows,
		//   which means no AES-GCM (requires Windows Vista or higher), no Curve25519 (requires Windows 10 and not FIPS validated), etc.
		//
		// - CBC because it may have better security properties in this circumstance than CTR. Also, CTR is not provided by Windows BCrypt.
		//   ECB is provided, so CTR is not difficult to implement, but is some work.
		//
		// - nistp256, rather than nistp384, because both appear to be sufficient against classic cryptanalysis for the foreseeable future,
		//   and neither against quantum. However, use of nistp256 saves 16 bytes in the activation ping, where size limits are low.


		// An activation code:
		//
		// - Is issued by an activation server, and applied by a license holder to a product instance, to associate the instance with a license group.
		//
		// - Contains initial license information such as licensed-to name, and license locator.
		//
		// - For instances that are connected to the internet, the activation code may be updated through a DNS or HTTPS activation ping.
		//
		// - For instances that are not connected to the internet, the activation code might not be updated, except manually.
		//
		// An activation code may be Base32-encoded. Before Base32-encoding, it is signed (not encrypted), and has the following format:
		//
		//   word16     activationCodeFlags
		//   byte[18]   licenseGroupTokenBinary
		//   word16     licenseTypeId
		//   word32     generatedUnixTime
		//   byte       licensedToNameBytes
		//   byte[...]  licensedToName
		//   byte       licenseGroupNameBytes
		//   byte[...]  licenseGroupName
		//   word32     groupLicUnitHdsTotal
		//   word16     upgradeExpiryUnixTimeDays
		//   word16     signatureKeyId
		//   byte       extensionDataBytes
		//   byte[...]  extensionData
		//   byte[64]   signatureNistp256
		//
		// The meanings of the fields are:
		//
		// - activationCodeFlags encodes a combination of bits. The following flags are defined:
		//
		//     0x0001 - This license type grants an infinite quantity of license units.
		//     0x0002 - Product instances to which this activation code is applied MAY send activation pings over HTTPS.
		//     0x0004 - Product instances to which this activation code is applied MAY send activation pings over DNS.
		//
		//   This field is intended to act as a versioning mechanism if necessary in the future.
		//
		// - licenseGroupTokenBinary encodes the At::Token that identifies the license group. This is an ID used for a DNS activation ping.
		//   Encoding this as first thing after activationCodeFlags allows the token to be extracted with a greater probability if only a partial
		//   activation code is available. This can be important when a user sends e.g. only the first line of an activation code.
		//
		// - licenseTypeId identifies the license type, defined locally between the activation server and the product being activated.
		//
		// - generatedUnixTime is the UNIX timestamp when the activation code was generated, unsigned, in the UTC time zone.
		//
		// - licensedToName is a UTF-8 string encoding the name of the person or entity to which the product is licensed.
		//
		// - licenseGroupName is a UTF-8 string encoding the name of the license group.
		//
		// - groupLicUnitHdsTotal is the total number of license unit hundredths in the license group. This can be used to prevent activation of
		//   functionality which requires more license units than the license group contains.
		//
		// - upgradeExpiryUnixTimeDays is the Unix timestamp when upgrade access for the license group expires, unsigned, in the UTC time zone, divided by 86,400.
		//
		// - signatureKeyId identifies the activation server's public key used to sign the activation code. This allows older product versions to
		//   display a useful diagnostic when keys are rotated over time. It also avoids retrying verification a number of times if multiple keys are trusted.
		//
		// - extensionData allows for the activation code format to be extended in a backward compatible way. Implementations that accept and verify activation
		//   codes MUST ignore any length of unrecognized extension data, allowing for any format of content, regardless of value.
		//
		// - signatureNistp256 is an ECDSA/nistp256 signature of the SHA-512/256 hash of the preceding fields as encoded in binary. The signature is done
		//   with a well-known key specific to the activation server. This key must be different from the well-known ECDH key used for pings.

		enum { ActvCode_SigLen = 64 };

		struct ActvCodeFlags { enum E {
			InfiniteLicenseUnits   = 0x0001,
			AllowActvPings_Https = 0x0002,
			AllowActvPings_Dns   = 0x0004,
			AllowActvPings_All   = AllowActvPings_Https | AllowActvPings_Dns,
		}; };

		struct ActvCodeData
		{
			enum { MaxStrBytes = 255, MaxSignatureKeyIdNr = UINT16_MAX };

			uint m_activationCodeFlags  {};		// Combination of ActvCodeFlags::E
			Str  m_licenseGroupToken;			// In token form - not binary
			uint m_licenseTypeIdNr      {};
			Time m_generatedTime;
			Str  m_licensedToName;				// Must NOT be longer than MaxStrBytes
			Str  m_licenseGroupName;			// Must NOT be longer than MaxStrBytes
			uint m_groupLicUnitHdsTotal {};
			Time m_upgradeExpiryTime;
			uint m_signatureKeyIdNr     {};

			bool operator== (ActvCodeData const& x) const;
			bool operator!= (ActvCodeData const& x) const { return !operator==(x); }

			enum class DecodeResult { OK, TooShort, TooLong };

			// Does not check field values or signature. Call CheckFields to check field values. Use ActvCodeVerifier to check signature
			DecodeResult Decode(Seq actvCodeBinary);

			enum class CheckResult { OK, ActivationCodeFlags, LicenseGroupToken, LicenseTypeIdNr, GeneratedTime, LicensedToNameLen, LicensedToNameUtf8,
				LicenseLocatorLen, LicenseLocatorUtf8, GroupLicUnitHdsTotal, UpgradeExpiryTime, SignatureKeyIdNr };

			CheckResult CheckFields() const;
			void EnsureValid() const;

			static bool IsTimeValid(Time t);
		};


		// Determines a line length between ActvCode_MinLineLen and MaxLineLen such that there will be the smallest number of lines,
		// while the last line is as filled out as possible.
		enum { ActvCode_MinLineLen = 58, ActvCode_MaxLineLen = 64 };
		sizet ActvCode_BestLineLen(sizet actvCodeLen);


		class ActvCodeSigner : public RefCountable
		{
		public:
			ActvCodeSigner(Rp<BCrypt::Provider> const& provEcdsaP256, Rp<BCrypt::Provider> const& provSha512)
				: m_provEcdsaP256(provEcdsaP256), m_provSha512(provSha512)
				{ m_sha512.Init(m_provSha512.Ref()); }

			// Does not affect loaded key, if any
			void GenerateKeypair(Str& privKey, Str& pubKey);

			void LoadPrivateKey(Seq privKey) { m_key.Import(m_provEcdsaP256.Ref(), privKey, BCRYPT_ECCPRIVATE_BLOB); }
			void Sign(ActvCodeData const& data, Str& actvCodeBinary);

		private:
			Rp<BCrypt::Provider> m_provEcdsaP256;
			Rp<BCrypt::Provider> m_provSha512;
			BCrypt::Hash         m_sha512;
			BCrypt::Key          m_key;
		};


		class ActvCodeVerifier : public RefCountable
		{
		public:
			// Provider must have been opened using Provider::OpenEcdsaP256()
			ActvCodeVerifier(Rp<BCrypt::Provider> const& provEcdsaP256, Rp<BCrypt::Provider> const& provSha512)
				: m_provEcdsaP256(provEcdsaP256), m_provSha512(provSha512)
				{ m_sha512.Init(m_provSha512.Ref()); }

			void LoadPublicKey(Seq pubKey) { m_key.Import(m_provEcdsaP256.Ref(), pubKey, BCRYPT_ECCPUBLIC_BLOB); }
			bool Verify(Seq actvCodeBinary);

		private:
			Rp<BCrypt::Provider> m_provEcdsaP256;
			Rp<BCrypt::Provider> m_provSha512;
			BCrypt::Hash         m_sha512;
			BCrypt::Key          m_key;
		};



		// A DNS activation ping:
		//
		// - Is sent as a ping for a TXT record associated with a long uniquely generated DNS name.
		//
		// - The DNS name being queried has the form <base32Payload1>.<...>.<base32PayloadN>._actv.example.com.
		//
		// - On receipt by the activation DNS server, the payloads are concatenated (the periods between them are dropped) and Base32-decoded.
		//
		// The decoded ping payload is encrypted, and has the following format:
		//
		//   byte[4]    randomPingNonceOuter
		//   byte[4]    randomPingNonceInner
		//   word16     pingFlagsUnencrypted
		//   word16     activationServerKeyId
		//   byte[32]   publicKeyNistp256
		//   byte[...]  ciphertextAes256Cbc
		//   byte[20]   truncatedHmacSha512
		//
		// These fields are generated as follows:
		//
		// - randomPingNonceOuter is chosen such that the SHA-512 hash of the entire payload (binary, without Base32-encoding) has its last 20 bits set to 0.
		//   This serves as proof of work by the ping sender, and increases the amount of work needed to mount a denial-of-service attack on the activation server
		//   that would exploit the CPU work the server needs to do to perform point decompression and ECDH agreement before it can fully decode the ping.
		//   Since the ping sender has 32 bits to control 20 bits, it will need to re-generate the ping in the 1:4096 chance an appropriate nonce can't be found.
		//
		// - randomPingNonceInner is chosen such that the SHA-512 HMAC encoded in the payload, before truncation, has its last 20 bits set to 0.
		//   This serves as further proof of work by the ping sender, and increases the amount of work needed to mount a denial-of-service attack on the activation
		//   server that would exploit non-cryptographic work and storage the server needs to spend on activation pings after they are successfully decrypted.
		//   Since the ping sender has 32 bits to control 20 bits, it will need to re-generate the ping in the 1:4096 chance an appropriate nonce can't be found.
		//
		// - pingFlagsUnencrypted encodes a combination of bits. The following flags are defined:
		//
		//     0x0001 - If this flag is present, the Y coordinate corresponding to publicKeyNistp256 is odd. If not present, the Y coordinate is even.
		//
		//   This field is intended to act as a versioning mechanism if necessary in the future.
		//
		// - activationServerKeyId identifies the ECDH/nistp256 public key of the activation server that the ping sender is using.
		//   This allows the activation server to migrate to new public keys without losing compatibility with old ping senders,
		//   and without having to perform trial decryption of pings with multiple keys.
		//
		// - publicKeyNistp256 is the X coordinate of an ECDH/nistp256 public key generated randomly by the ping sender for each unique (not retried) ping.
		//   The Y coordinate is generated from the X coordinate, with parity as specified using pingFlagsUnencrypted. This key is used to generate an agreed
		//   value by the ping sender and activation server. The other key used for key agreement is a well-known ECDH public key specific to the activation server.
		//
		//   The agreed value is used to derive cryptographic parameters as follows. Note lowercase suffixes:
		//
		//     AES-256 key     = bytes  0..31 of SHA-512(agreed value | "e")
		//     AES-256 CBC IV  = bytes 32..47 of SHA-512(agreed value | "e")
		//     HMAC-SHA512 key =                 SHA-512(agreed value | "m")
		//
		// - ciphertextAes256Cbc is the ping content encrypted using AES-256 in CBC mode, with key and IV generated as described above.
		//   It does not have to be block size aligned. Size is inferred from total payload size.
		//
		// - truncatedHmacSha512 is a truncated HMAC-SHA-512, with key as described above, calculated over the fields:
		//
		//     randomPingNonceInner
		//     activationServerKeyId
		//     publicKeyNistp256
		//     ciphertextAes256Cbc
		//
		// The total encryption overhead of the activation ping is 64 bytes. Given a reasonable size of the domain suffix (e.g. "._actv.example.com"),
		// this permits the unencrypted ping content to be in the range of 76+ bytes. The unencrypted ping content is as follows:
		//
		//   word16       pingFlagsEncrypted
		//   word32       pingUnixTime
		//   byte[18]     licenseGroupTokenBinary
		//   byte[4]      prevPingSha512Start
		//   word16       licenseTypeId
		//   word16       productVersionId
		//   byte         nrOsDescriptors
		//   word16[...]  osDescriptorIds
		//   word16       nrLicenseUnitsHundredths
		//   word16       lastInstalledUnixTimeDays
		//   byte         nrComputerNameBytes
		//   byte[...]    computerName
		//   byte[...]    pingPadding
		//
		// The total length of fixed-size fields (including the lengths of variable fields) is 38 bytes. The meanings of the fields are:
		//
		// - pingFlagsEncrypted is currently 0. This field is intended to act as a versioning mechanism if necessary in the future.
		//
		// - pingUnixTime is the UNIX timestap when the ping was generated, unsigned, in the UTC time zone.
		//
		// - licenseGroupTokenBinary assumes that the license is in a license group, and the license group is identified by an At::Token.
		//   This may be all zeros if the instance is using a free license (e.g. a personal edition), or is under evaluation (not associated with a paid license).
		//
		// - prevPingSha512Start is the first 4 bytes of the SHA-512 hash (outer hash, not the encoded HMAC) of the last activation ping sent by this instance of the product.
		//   The activation server uses this field, in combination with licenseGroupTokenBinary and computerName, to properly count pings sent by the same instance.
		//   The computerName field alone is not sufficient, because the product may run on a number of cloned virtual machines that share the same computer name.
		//   The product needs to store at least these 4 bytes in permanent storage, otherwise the number of instances may be double counted if the product restarts.
		//
		// - licenseTypeId and productVersionId identify the license type, product (implied in license type), and product version.
		//   The identifiers are specific to the relationship between the ping sender and the activation server.
		//
		// - osDescriptorIds encode the OS, its version, and edition information. These are specific to the relationship between the ping sender and activation server.
		//
		// - nrLicenseUnitsHundredths is the number of license units consumed by this product instance, times 100. A test instance might consume 0.25 units,
		//   a standard production instance 1 unit, and an advanced production instance 10 units.
		//
		// - lastInstalledUnixTimeDays is the last time an installation of this product was performed on this instance, expressed as Unix time divided by 86,400.
		//
		// - computerName is a UTF-8 string encoding the name of the computer where the product is installed. This should be truncated if necessary to fit size limits.
		//
		// - pingPadding is any number of bytes, with any value, to mask the size of the foregoing information.


		// A DNS activation response:
		//
		// - Is sent as a TXT record in response to a DNS activation ping.
		//
		// - Has the form "R=<base64Payload>", without the quotes.
		//
		// After Base64-decoding, the response payload is encrypted, and has the following format:
		//
		//   byte[8]    randomResponseNonce
		//   byte[...]  ciphertextAes256Cbc
		//   byte[20]   truncatedHmacSha512
		//
		// The cryptographic parameters are generated as follows, based on the same agreed value as the corresponding ping. Note uppercase suffixes:
		//
		//     AES-256 key     = bytes  0..31 of SHA-512(agreed value | "E" | randomResponseNonce)
		//     AES-256 CBC IV  = bytes 32..47 of SHA-512(agreed value | "E" | randomResponseNonce)
		//     HMAC-SHA512 key =                 SHA-512(agreed value | "M")
		//
		// The HMAC-SHA512 is generated over both the nonce and the ciphertext.
		//
		// The unencrypted response content is as follows:
		//
		//   word16     activationResponseFlags
		//   byte       nrActivationCodeBytes
		//   byte[...]  activationCodeBinary
		//   byte[...]  responsePadding
		//   
		// The meanings of the fields are:
		//
		// - activationResponseFlags encodes a combination of states for the ping sender's license. Bits have the following meanings:
		//
		//     0x0001 - Internal server error. The ping sender should wait for a period of time, and try again.
		//     0x0002 - License group not found. If this flag is set, all other bits and fields are still present, but have undefined values.
		//     0x0004 - License limits exceeded by this instance in particular. This instance itself is consuming more license units than available.
		//     0x0008 - License limits exceeded for this license type, by the group in general. There are too many product instances consuming too many license units.
		//     0x0010 - License limits exceeded for another license type in the group. There are too many product instances of the other license type, consuming too many license units.
		//     0x0020 - Upgrade access exceeded by this instance in particular. This instance itself is using a newer version than group upgrade access permits.
		//     0x0040 - Upgrade access exceeded for this license type, by the group in general. Other instances of this license type are using a newer version than upgrade access permits.
		//     0x0080 - Upgrade access exceeded for another license type in the group. Instances of another license type in the group are using a newer version than upgrade access permits.
		//
		// - activationCodeBinary is the current activation code associated with the license group, encoding up-to-date information about the group.
		//
		// - responsePadding is any number of bytes, with any value, to mask the size of the foregoing information.

	}
}
