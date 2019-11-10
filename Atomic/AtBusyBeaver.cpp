#include "AtIncludes.h"
#include "AtBusyBeaver.h"

namespace At
{
	namespace
	{
		static inline uint GetUInt16BE(byte const* p)
		{
			uint v;
			v  = p[0]; v <<= 8;
			v |= p[1];
			return v;
		}

		static inline uint GetUInt16LE(byte const* p)
		{
		#if defined(_M_X86) || defined(_M_X64)
			return ((ushort*) p)[0];
		#else
			return (((uint) p[1]) << 8) | p[0];
		#endif
		}

		static inline uint GetUInt16(byte const* p, bool bigEndian) { if (bigEndian) return GetUInt16BE(p); return GetUInt16LE(p); }


		static inline uint32 GetUInt32BE(byte const* p)
		{
			uint32 v;
			v  = p[0]; v <<= 8;
			v |= p[1]; v <<= 8;
			v |= p[2]; v <<= 8;
			v |= p[3];
			return v;
		}

		static inline uint32 GetUInt32LE(byte const* p)
		{
		#if defined(_M_X86) || defined(_M_X64)
			return ((uint32*) p)[0];
		#else
			return	(((uint32) p[0])      ) |
					(((uint32) p[1]) <<  8) |
					(((uint32) p[2]) << 16) |
					(((uint32) p[3]) << 24);
		#endif
		}

		static inline uint32 GetUInt32(byte const* p, bool bigEndian) { if (bigEndian) return GetUInt32BE(p); return GetUInt32LE(p); }


		static inline uint64 GetUInt64BE(byte const* p)
		{
			uint64 v;
			v  = p[0]; v <<= 8;
			v |= p[1]; v <<= 8;
			v |= p[2]; v <<= 8;
			v |= p[3]; v <<= 8;
			v |= p[4]; v <<= 8;
			v |= p[5]; v <<= 8;
			v |= p[6]; v <<= 8;
			v |= p[7];
			return v;
		}

		static inline uint64 GetUInt64LE(byte const* p)
		{
		#if defined(_M_X86) || defined(_M_X64)
			return ((uint64*) p)[0];
		#else
			return	(((uint64) p[0])      ) |
					(((uint64) p[1]) <<  8) |
					(((uint64) p[2]) << 16) |
					(((uint64) p[3]) << 24) |
					(((uint64) p[4]) << 32) |
					(((uint64) p[5]) << 40) |
					(((uint64) p[6]) << 48) |
					(((uint64) p[7]) << 56);
		#endif
		}

		static inline uint64 GetUInt64(byte const* p, bool bigEndian) { if (bigEndian) return GetUInt64BE(p); return GetUInt64LE(p); }
	
	
		static inline void PutUInt64BE(byte* p, uint64 v)
		{
			p[0] = (byte) ((v >> 56) & 0xFF);
			p[1] = (byte) ((v >> 48) & 0xFF);
			p[2] = (byte) ((v >> 40) & 0xFF);
			p[3] = (byte) ((v >> 32) & 0xFF);
			p[4] = (byte) ((v >> 24) & 0xFF);
			p[5] = (byte) ((v >> 16) & 0xFF);
			p[6] = (byte) ((v >>  8) & 0xFF);
			p[7] = (byte) ((v      ) & 0xFF);
		}

		static inline void PutUInt64LE(byte* p, uint64 v)
		{
		#if defined(_M_X86) || defined(_M_X64)
			((uint64*) p)[0] = v;
		#else
			p[0] = (byte) ((v      ) & 0xFF);
			p[1] = (byte) ((v >>  8) & 0xFF);
			p[2] = (byte) ((v >> 16) & 0xFF);
			p[3] = (byte) ((v >> 24) & 0xFF);
			p[4] = (byte) ((v >> 32) & 0xFF);
			p[5] = (byte) ((v >> 40) & 0xFF);
			p[6] = (byte) ((v >> 48) & 0xFF);
			p[7] = (byte) ((v >> 56) & 0xFF);
		#endif
		}

		static inline void PutUInt64(byte* p, uint64 v, bool bigEndian) { if (bigEndian) PutUInt64BE(p, v); else PutUInt64LE(p, v); }
	
		static inline uint64 MulModOp32(uint32 a, uint32 b)
		{
			// Multiplication of 32x32 into 64, modulo largest 32-bit prime for a 32-bit result.
			// Right shifts multiplication result by 4 before modulo to use a portion that preserves more entropy.

		#if defined(_MSC_VER) && !defined(_DEBUG)
			return (__emulu(a, b) >> 4) % 0xFFFFFFFB;
		#else
			return ((((uint64) a) * ((uint64) b)) >> 4) % 0xFFFFFFFB;
		#endif
		}
	
		static inline uint64 MulModOp64(uint64 a, uint64 b)
		{
			// Would prefer to use multiplication of 64x64 into 128 (_umul64), but then we would need to modulo 128 into 64.
			// The x64 platform can do this, but it's expensive for x86, and VS compiler support for this is poor even on x64.
			uint32 aHi = (uint32) (a >> 32);
			uint32 aLo = (uint32) (a & 0xFFFFFFFF);
			uint32 bHi = (uint32) (b >> 32);
			uint32 bLo = (uint32) (b & 0xFFFFFFFF);
			return (MulModOp32(aHi, bLo) << 32) | MulModOp32(aLo, bHi);
		}
	}	// anonymous namespace


	void BusyBeaverMachine(void* pvSaltBlock, void* pvPwBlock, uint nrOps, uint swapBlockThreshold)
	{
		byte* saltBlock = (byte*) pvSaltBlock;
		byte* pwBlock = (byte*) pvPwBlock;
		uint instrOffset = 0;
		uint64 registerValue = 0;

		uint opNr = 0;
		while (opNr != nrOps)
		{
			if (instrOffset + 3 > BusyBeaver_BlockSize)
			{
				instrOffset = 0;
				++(saltBlock[0]);
			}

			uint saltByte = saltBlock[instrOffset];
			uint saltByteHi = (saltByte >> 4);
			uint saltByteLo = (saltByte & 7);
		
			uint instrCount = 1 + (saltByte & 0x0F);
			uint paramSize = instrCount * 8;

			bool loadBigEndian = ((saltByte & 0x10) != 0);
			bool storeBigEndian = !loadBigEndian;

			bool relativeOffset = ((saltByte & 0x20) != 0);
			uint paramOffset = GetUInt16(saltBlock + instrOffset + 1, loadBigEndian);
			if (relativeOffset)
				paramOffset = ((instrOffset + paramOffset) & 0xFFFF);

			++(saltBlock[instrOffset + 1]);		// Vary offset to cover more ground
		
			if (saltByteHi == saltByteLo)
			{
				// Perform jump
				++(saltBlock[instrOffset]);
				instrOffset = paramOffset;

				if (opNr >= swapBlockThreshold)
					std::swap(saltBlock, pwBlock);
			}
			else
			{
				// Perform an instruction or swap
				if (instrOffset + 4 + paramSize > BusyBeaver_BlockSize)
				{
					instrOffset = 0;
					++(saltBlock[0]);
					continue;
				}

				if (paramOffset + paramSize > BusyBeaver_BlockSize)
					paramOffset = (((paramOffset + paramSize) - 1) & 0xFFFF);
			
				// Are the two operands the same operand?
				if (paramOffset == instrOffset + 4)
				{
					// Perform bitwise negate instead
					do
					{
						uint64 val = GetUInt64(pwBlock + paramOffset, loadBigEndian);
						val = ~val;
						PutUInt64(pwBlock + paramOffset, val, storeBigEndian);
					
						paramOffset += 8;
						--instrCount;
					}
					while (instrCount != 0);
				}
				else
				{
					uint instructions = GetUInt32(pwBlock + instrOffset, loadBigEndian);
					uint localOffset = instrOffset + 4;
			
					do
					{
						// Load parameters
						uint64 localVal = GetUInt64(pwBlock + localOffset, loadBigEndian);
						uint64 paramVal = GetUInt64(pwBlock + paramOffset, loadBigEndian);

						if (saltByteHi == saltByteLo + 1)
						{
							if (opNr < swapBlockThreshold)
							{
								// Perform partial bitwise negate and swap within pwBlock
								PutUInt64(pwBlock + paramOffset, ~localVal, storeBigEndian);
								PutUInt64(pwBlock + localOffset, paramVal, storeBigEndian);
							}
							else
							{
								// Swap data between blocks
								uint64 saltLocalVal = GetUInt64(saltBlock + localOffset, loadBigEndian);
								uint64 saltParamVal = GetUInt64(saltBlock + paramOffset, loadBigEndian);

								PutUInt64(pwBlock   + paramOffset, saltLocalVal, storeBigEndian);
								PutUInt64(pwBlock   + localOffset, saltParamVal, storeBigEndian);
								PutUInt64(saltBlock + paramOffset, localVal,     storeBigEndian);
								PutUInt64(saltBlock + localOffset, paramVal,     storeBigEndian);
							}
						}
						else
						{
							// This way is nearly side-channel free, and actually FASTER on x64 than branching.
							registerValue += MulModOp64(localVal, paramVal);

							uint64 result[4];
							result[0] = localVal + paramVal;
						
							int rotBits { (int) (registerValue & 63) };
							if (rotBits != 0)
								result[1] = RotateRight64(localVal, rotBits);
							else
								result[1] = ~localVal;

							result[2] = (localVal ^ paramVal);
							result[3] = registerValue;
						
							PutUInt64(pwBlock + localOffset, result[instructions & 3], storeBigEndian);
						}
					
						instructions >>= 2;
						localOffset += 8;
						paramOffset += 8;
						--instrCount;
					}
					while (instrCount != 0);
				}
			
				++instrOffset;
			}

			++opNr;
		}
	}


	using namespace BusyBeaverHelpers;


	template <class H>
	BusyBeaver<H>::BusyBeaver(uint nrOps) : m_nrOps(nrOps)
	{
		m_frontGuard = m_blocksAndGuards;
		m_pwBlock    = m_frontGuard + 8;
		m_midGuard   = m_pwBlock + BusyBeaver_BlockSize;
		m_saltBlock  = m_midGuard + 8;
		m_backGuard  = m_saltBlock + BusyBeaver_BlockSize;

		PutUInt64LE(m_frontGuard, GuardValue);
		PutUInt64LE(m_midGuard,   GuardValue);
		PutUInt64LE(m_backGuard,  GuardValue);
	}


	template <class H>
	void BusyBeaver<H>::Process_Inner(void const* salt, uint saltLen, void const* pw, uint pwLen, byte* out, uint swapBlockThreshold)
	{
		if (saltLen > H::OutputSize || pwLen > H::OutputSize)
			throw "BusyBeaver: input too large for Process_Inner(); use Process()";

		EraseOnExit erasePwBlock(m_pwBlock, BusyBeaver_BlockSize);
		EraseOnExit eraseSaltBlock(m_saltBlock, BusyBeaver_BlockSize);

		// Initialize SaltBlock, PwBlock
		{
			uint maxOfSaltAndPwLen = saltLen;
			if (pwLen > saltLen)
				maxOfSaltAndPwLen = pwLen;

			byte buf[2 * H::OutputSize];
			EraseOnExit eraseBuf(buf, 2 * H::OutputSize);
		
			// Initialize SaltBlock
			byte* p = m_saltBlock + (BusyBeaver_BlockSize - H::OutputSize);
			H::Hash(salt, saltLen, p);
		
			do
			{
				memcpy(buf, p, H::OutputSize);
				memcpy(buf + H::OutputSize, salt, saltLen);

				p -= H::OutputSize;
				H::Hash(buf, H::OutputSize + saltLen, p);
			}
			while (p != m_saltBlock);

			// Generate seed key for PwBlock
			memcpy(buf, m_saltBlock, H::OutputSize);
			memcpy(buf + H::OutputSize, salt, saltLen);

			byte pwBlockSeedKey[H::OutputSize];
			H::Hash(buf, H::OutputSize + saltLen, pwBlockSeedKey);
		
			// Initialize PwBlock
			p = m_pwBlock + (BusyBeaver_BlockSize - H::OutputSize);
			H::HMac(pwBlockSeedKey, H::OutputSize, pw, pwLen, p);

			do
			{
				memcpy(buf, p, H::OutputSize);
				memcpy(buf + H::OutputSize, pw, pwLen);
		
				p -= H::OutputSize;
				H::Hash(buf, H::OutputSize + pwLen, p);
			}
			while (p != m_pwBlock);
		}
	
		// Run BusyBeaverMachine
		BusyBeaverMachine(m_saltBlock, m_pwBlock, m_nrOps, swapBlockThreshold);

		if (GetUInt64LE(m_frontGuard) != GuardValue) throw "BusyBeaver: front guard modified";
		if (GetUInt64LE(m_midGuard  ) != GuardValue) throw "BusyBeaver: mid guard modified";
		if (GetUInt64LE(m_backGuard ) != GuardValue) throw "BusyBeaver: back guard modified";

		// Hash everything to produce output
		byte finalKey[2 * H::OutputSize];
		EraseOnExit eraseFinalKey(finalKey, 2 * H::OutputSize);

		H::Hash(m_blocksAndGuards, BlocksAndGuardsSize, finalKey);
		H::Hash(salt, saltLen, finalKey + H::OutputSize);
		H::HMac(finalKey, 2 * H::OutputSize, pw, pwLen, out);
	}


	template <class H>
	void BusyBeaver<H>::Process(void const* salt, uint saltLen, void const* pw, uint pwLen, byte* out, std::function<void()> destroyInputs)
	{
		byte saltHash[H::OutputSize];
		EraseOnExit eraseSaltHash(saltHash, H::OutputSize);

		H::Hash(salt, saltLen, saltHash);

		byte saltPwHmac[H::OutputSize];
		EraseOnExit eraseSaltPwHmac(saltPwHmac, H::OutputSize);

		H::HMac(salt, saltLen, pw, pwLen, saltPwHmac);
	
		if (destroyInputs)
			destroyInputs();

		Process_Inner(saltHash, H::OutputSize, saltPwHmac, H::OutputSize, out, m_nrOps / 4);
	}


	template class BusyBeaver<BB_SHA512>;
}
