#include "AtIncludes.h"
#include "AtBaseXY.h"

#include "AtEnc.h"


namespace At
{
	// Base64::Encode

	char const* const Base64::BaseAlphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";

	byte* Base64::Encode(Seq plain, byte* pWrite, Padding padding, char const* last3, NewLines const& nl)
	{
		EnsureThrow(ZLen(last3) == 3);

		byte alphabet[65];
		memcpy(alphabet,    BaseAlphabet, 62);
		memcpy(alphabet+62, last3,        3);

		Encoder enc { pWrite, nl };
	
		while (plain.n >= 3)
		{
			enc.Add(alphabet[                     (plain.p[0] >> 2)  & 63U]);
			enc.Add(alphabet[((plain.p[0] << 4) | (plain.p[1] >> 4)) & 63U]);
			enc.Add(alphabet[((plain.p[1] << 2) | (plain.p[2] >> 6)) & 63U]);
			enc.Add(alphabet[  plain.p[2]                            & 63U]);
			plain.DropBytes(3);
		}

		switch (plain.n)
		{
		case 2:
			enc.Add(alphabet[                     (plain.p[0] >> 2)  & 63U]);
			enc.Add(alphabet[((plain.p[0] << 4) | (plain.p[1] >> 4)) & 63U]);
			enc.Add(alphabet[ (plain.p[1] << 2)                      & 63U]);
			if (padding != Padding::No)
				enc.Add((byte) last3[2]);
			break;

		case 1:
			enc.Add(alphabet[                     (plain.p[0] >> 2)  & 63U]);
			enc.Add(alphabet[ (plain.p[0] << 4)                      & 63U]);
			if (padding != Padding::No)
			{
				enc.Add((byte) last3[2]);
				enc.Add((byte) last3[2]);
			}
			break;
		}

		return enc.Done();
	}


	Seq Base64::Encode(Seq plain, Enc& out, Padding padding, char const* last3, NewLines const& nl)
	{
		sizet maxOutLen = nl.MaxOutLen(EncodeBaseLen(plain.n));
		return out.InvokeWriter(maxOutLen, [&] (byte* pStart) -> byte*
			{ return Encode(plain, pStart, padding, last3, nl); } );
	}



	// Base64::Decode

	namespace
	{
		byte Base64DigToNr(byte dig, char const* last3)
		{
			if (dig >= 'A' && dig <= 'Z')	return (byte) (dig - 'A');
			if (dig >= 'a' && dig <= 'z')	return (byte) (26 + (dig - 'a'));
			if (dig >= '0' && dig <= '9')	return (byte) (52 + (dig - '0'));
			if (dig == last3[0])			return 62;
			if (dig == last3[1])			return 63;
			if (dig == last3[2])			return 64;
											return 255;
		}

		byte* Base64DecodeChunk(byte const* chunk, byte* pWrite)
		{
			if (chunk[0] > 63) return pWrite;
			if (chunk[1] > 63) return pWrite; *pWrite++ = (byte) ((chunk[0] << 2) | (chunk[1] >> 4));
			if (chunk[2] > 63) return pWrite; *pWrite++ = (byte) ((chunk[1] << 4) | (chunk[2] >> 2));
			if (chunk[3] > 63) return pWrite; *pWrite++ = (byte) ((chunk[2] << 6) |  chunk[3]      );
			                   return pWrite;
		}

	} // anon


	byte* Base64::Decode(Seq& encoded, byte* pWrite, char const* last3)
	{
		EnsureThrow(ZLen(last3) == 3);

		byte chunk[4];
		sizet chunkChars = 0;
		memset(chunk, 255, 4);
	
		while (true)
		{
			encoded.ReadToFirstByteNotOf(" \t\r\n");
			if (!encoded.n)
				break;
		
			byte nr = Base64DigToNr(encoded.p[0], last3);
			if (nr == 255)
				break;

			encoded.DropByte();
			chunk[chunkChars++] = nr;
		
			if (chunkChars == 4)
			{
				pWrite = Base64DecodeChunk(chunk, pWrite);
				chunkChars = 0;
				memset(chunk, 255, 4);
			}
		}
	
		return Base64DecodeChunk(chunk, pWrite);
	}


	Seq Base64::Decode(Seq& encoded, Enc& out, char const* last3)
	{
		sizet maxOutLen = DecodeMaxLen(encoded.n);
		return out.InvokeWriter(maxOutLen, [&] (byte* pStart) -> byte*
			{ return Decode(encoded, pStart, last3); } );
	}



	// Base32::Encode

	byte const* const Base32::EncodeAlphabet = (byte const*) "0123456789ABCDEFGHJKLMNPQRTUVWXY";

	byte* Base32::Encode(Seq plain, byte* pWrite, NewLines const& nl)
	{
		Encoder enc { pWrite, nl };

		while (plain.n >= 5)
		{
			enc.Add(EncodeAlphabet[                     (plain.p[0] >> 3)  & 31U]);
			enc.Add(EncodeAlphabet[((plain.p[0] << 2) | (plain.p[1] >> 6)) & 31U]);
			enc.Add(EncodeAlphabet[                     (plain.p[1] >> 1)  & 31U]);
			enc.Add(EncodeAlphabet[((plain.p[1] << 4) | (plain.p[2] >> 4)) & 31U]);
			enc.Add(EncodeAlphabet[((plain.p[2] << 1) | (plain.p[3] >> 7)) & 31U]);
			enc.Add(EncodeAlphabet[                     (plain.p[3] >> 2)  & 31U]);
			enc.Add(EncodeAlphabet[((plain.p[3] << 3) | (plain.p[4] >> 5)) & 31U]);
			enc.Add(EncodeAlphabet[  plain.p[4]                            & 31U]);
			plain.DropBytes(5);
		}

		switch (plain.n)
		{
		case 4:
			enc.Add(EncodeAlphabet[                     (plain.p[0] >> 3)  & 31U]);
			enc.Add(EncodeAlphabet[((plain.p[0] << 2) | (plain.p[1] >> 6)) & 31U]);
			enc.Add(EncodeAlphabet[                     (plain.p[1] >> 1)  & 31U]);
			enc.Add(EncodeAlphabet[((plain.p[1] << 4) | (plain.p[2] >> 4)) & 31U]);
			enc.Add(EncodeAlphabet[((plain.p[2] << 1) | (plain.p[3] >> 7)) & 31U]);
			enc.Add(EncodeAlphabet[                     (plain.p[3] >> 2)  & 31U]);
			enc.Add(EncodeAlphabet[ (plain.p[3] << 3)                      & 31U]);
			break;

		case 3:
			enc.Add(EncodeAlphabet[                     (plain.p[0] >> 3)  & 31U]);
			enc.Add(EncodeAlphabet[((plain.p[0] << 2) | (plain.p[1] >> 6)) & 31U]);
			enc.Add(EncodeAlphabet[                     (plain.p[1] >> 1)  & 31U]);
			enc.Add(EncodeAlphabet[((plain.p[1] << 4) | (plain.p[2] >> 4)) & 31U]);
			enc.Add(EncodeAlphabet[ (plain.p[2] << 1)                      & 31U]);
			break;

		case 2:
			enc.Add(EncodeAlphabet[                     (plain.p[0] >> 3)  & 31U]);
			enc.Add(EncodeAlphabet[((plain.p[0] << 2) | (plain.p[1] >> 6)) & 31U]);
			enc.Add(EncodeAlphabet[                     (plain.p[1] >> 1)  & 31U]);
			enc.Add(EncodeAlphabet[ (plain.p[1] << 4)                      & 31U]);
			break;

		case 1:
			enc.Add(EncodeAlphabet[                     (plain.p[0] >> 3)  & 31U]);
			enc.Add(EncodeAlphabet[ (plain.p[0] << 2)                      & 31U]);
			break;
		}

		return enc.Done();
	}


	Seq Base32::Encode(Seq plain, Enc& out, NewLines const& nl)
	{
		sizet maxOutLen = nl.MaxOutLen(EncodeBaseLen(plain.n));
		return out.InvokeWriter(maxOutLen, [&] (byte* pStart) -> byte*
			{ return Encode(plain, pStart, nl); } );
	}



	// Base32::Decode

	byte const* const Base32::DecodeTable = (byte const*)
	//    '0' '1' '2' '3'    '4' '5' '6' '7'    '8' '9'
		"\x00\x01\x02\x03" "\x04\x05\x06\x07" "\x08\x09\xFF\xFF" "\xFF\xFF\xFF\xFF"
	//        'A' 'B' 'C'    'D' 'E' 'F' 'G'    'H' 'I' 'J' 'K'    'L' 'M' 'N' 'O'
		"\xFF\x0A\x0B\x0C" "\x0D\x0E\x0F\x10" "\x11\x01\x12\x13" "\x14\x15\x16\x00"
	//    'P' 'Q' 'R' 'S'    'T' 'U' 'V' 'W'    'X' 'Y' 'Z'
		"\x17\x18\x19\x05" "\x1A\x1B\x1C\x1D" "\x1E\x1F\x02\xFF" "\xFF\xFF\xFF\xFF"
	//        'a' 'b' 'c'    'd' 'e' 'f' 'g'    'h' 'i' 'j' 'k'    'l' 'm' 'n' 'o'
		"\xFF\xFF\xFF\x0C" "\xFF\xFF\x0F\xFF" "\xFF\x01\x12\x13" "\x01\x15\xFF\x00"
	//    'p' 'q' 'r' 's'    't' 'u' 'v' 'w'    'x' 'y' 'z'
		"\x17\xFF\xFF\x05" "\x1A\x1B\x1C\x1D" "\x1E\x1F\x02\xFF" "\xFF\xFF\xFF\xFF";

	namespace
	{
		byte Base32DigToNr(byte dig)
		{
			if (dig < '0' || dig > 'z') return 255;
			return Base32::DecodeTable[dig - '0'];
		}

		byte* Base32DecodeChunk(byte const* chunk, byte* pWrite)
		{
			if (chunk[0] > 31) return pWrite;
			if (chunk[1] > 31) return pWrite; *pWrite++ = (byte) ((chunk[0] << 3) | (chunk[1] >> 2)                  );
			if (chunk[2] > 31) return pWrite;
			if (chunk[3] > 31) return pWrite; *pWrite++ = (byte) ((chunk[1] << 6) | (chunk[2] << 1) | (chunk[3] >> 4));
			if (chunk[4] > 31) return pWrite; *pWrite++ = (byte) ((chunk[3] << 4) | (chunk[4] >> 1)                  );
			if (chunk[5] > 31) return pWrite;
			if (chunk[6] > 31) return pWrite; *pWrite++ = (byte) ((chunk[4] << 7) | (chunk[5] << 2) | (chunk[6] >> 3));
			if (chunk[7] > 31) return pWrite; *pWrite++ = (byte) ((chunk[6] << 5) |  chunk[7]                        );
			                   return pWrite;
		}

	} // anon


	byte* Base32::Decode(Seq& encoded, byte* pWrite)
	{
		byte chunk[8];
		sizet chunkChars = 0;
		memset(chunk, 255, 8);
	
		while (true)
		{
			encoded.ReadToFirstByteNotOf(" \t\r\n");
			if (!encoded.n)
				break;
		
			byte nr = Base32DigToNr(encoded.p[0]);
			if (nr == 255)
				break;

			encoded.DropByte();
			chunk[chunkChars++] = nr;
		
			if (chunkChars == 8)
			{
				pWrite = Base32DecodeChunk(chunk, pWrite);
				chunkChars = 0;
				memset(chunk, 255, 8);
			}
		}
	
		return Base32DecodeChunk(chunk, pWrite);
	}


	Seq Base32::Decode(Seq& encoded, Enc& out)
	{
		sizet maxOutLen = DecodeMaxLen(encoded.n);
		return out.InvokeWriter(maxOutLen, [&] (byte* pStart) -> byte*
			{ return Decode(encoded, pStart); } );
	}

}
