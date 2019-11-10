#pragma once

#include "AtSha512.h"

namespace At
{
	// Takes pointers to two 64 kB block of (presumably random) data as input.
	// Initial values in SaltBlock should be derived from information that may be known to the attacker.
	// Initial values in PwBlock should be derived from private information NOT known to the attacker.
	// Interprets combined blocks as byte code for a virtual machine where all inputs are valid as instructions and parameters.
	// The instructions perform calculations and changes on PwBlock, with jumps and offsets controlled by values in SaltBlock,
	// using operations that preserve entropy. Processing stops when the requested number of operations have been processed.
	//
	// When the number of operations executed reaches swapBlockThreshold, the algorithm will begin to swap the roles of
	// SaltBlock and PwBlock at every jump, so that both blocks begin to control jumps and offsets, and both blocks are
	// read from and written to. This provides the ability to choose a tradeoff between side channel resistance and the
	// ability of an attacker to rearrange the order in which instructions are executed so as to better fit into the
	// attacker's cache. Suggested value for swapBlockThreshold is 1/4 of nrOps. Any value is valid, including zero and
	// UINT_MAX. Any value larger than nrOps will have the same effect: there will be no block swapping, and the final
	// contents of SaltBlock will be entirely predictable based on salt information only.

	enum { BusyBeaver_BlockSize = 0x10000 };

	void BusyBeaverMachine(void* pvSaltBlock, void* pvPwBlock, uint nrOps, uint swapBlockThreshold);


	// Hash function interface for BusyBeaver

	struct BB_SHA512
	{
		enum { OutputSize = 64 };

		static inline void Hash(void const* in, uint32 inlen, byte* out)
			{ SHA512_Simple(in, inlen, out); }

		static inline void HMac(void const* k, uint32 klen, void const* in, uint32 inlen, byte* out)
			{ HMAC_SHA512_Simple(k, klen, in, inlen, out); }
	};


	namespace BusyBeaverHelpers
	{
		struct NoCopy
		{
			NoCopy() {}
		private:
			NoCopy(NoCopy const&);
			void operator=(NoCopy const&);
		};

		class EraseOnExit : NoCopy
		{
		public:
			EraseOnExit(void* p, uint n) : m_p(p), m_n(n) {}
			~EraseOnExit() { memset(m_p, 0, m_n); }

		private:
			void* const m_p;
			uint m_n;
		};
	}


	// Uses provided hash+HMAC and BusyBeaver to deterministically derive a digest based on key (= salt) and password.

	template <class H = BB_SHA512>
	class BusyBeaver : BusyBeaverHelpers::NoCopy
	{
	public:
		static_assert(H::OutputSize == 16 || H::OutputSize == 32 || H::OutputSize == 64, "Hash output size must be a power of 2");

		enum { OutputSize = H::OutputSize };
	
		BusyBeaver(uint nrOps);
	
		void Process_Inner(void const* salt, uint saltLen, void const* pw, uint pwLen, byte* out, uint swapBlockThreshold);
		void Process(void const* salt, uint saltLen, void const* pw, uint pwLen, byte* out, std::function<void()> destroyInputs);

	private:
		uint const m_nrOps;

		enum { BlocksAndGuardsSize = (2 * BusyBeaver_BlockSize) + (3 * 8) };
		static uint64 const GuardValue = 0xFDFDFDFDFDFDFDFDULL;

		// PwBlock is before SaltBlock so that the final hash of salt block cannot be precomputed when swapBlockThreshold >= nrOps
		byte m_blocksAndGuards[BlocksAndGuardsSize];
		byte* m_frontGuard;
		byte* m_pwBlock;
		byte* m_midGuard;
		byte* m_saltBlock;
		byte* m_backGuard;
	};

}
