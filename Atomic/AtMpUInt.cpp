#include "AtIncludes.h"
#include "AtMpUInt.h"

namespace At
{

	void MpUInt::StripLeadingZeros()
	{
		while (m_words.Any() && !m_words.Last())
			m_words.PopLast();
	}


	sizet MpUInt::NrSignificantBytes(uint32 word)
	{
		sizet n = 0;
		while (word)
		{
			++n;
			word >>= 8;
		}
		return n;
	}


	MpUInt& MpUInt::Set(uint32 v)
	{
		if (!v)
			m_words.Clear();
		else
		{
			m_words.ResizeExact(1);
			m_words.First() = v;
		}

		return *this;
	}


	sizet MpUInt::NrBits() const
	{
		if (IsZero())
			return 1;

		return (WordBits * (NrWords() - 1)) + NrBitsUsed(m_words.Last());
	}


	sizet MpUInt::NrBytes() const
	{
		if (IsZero())
			return 1;

		// Assumes leading zeros have been stripped
		return (WordBytes * (NrWords() - 1)) + NrSignificantBytes(m_words.Last());
	}


	byte MpUInt::GetByte(sizet i) const
	{
		if (i >= WordBytes * NrWords())
			return 0;

		return (byte) ((m_words[i / WordBytes] >> (8 * (i % WordBytes))) & 0xFF);
	}


	MpUInt& MpUInt::ReadBytes(Seq bin)
	{
		bin.DropToFirstByteNotOfType(Ascii::IsNull);

		m_words.Clear();
		m_words.ReserveExact((bin.n + WordBytes - 1) / WordBytes);

		uint32 word {};
		uint shift {};
		
		for (sizet i=bin.n; i!=0; )
		{
			word |= (((uint32) bin.p[--i]) << shift);
			if (shift != 24)
				shift += 8;
			else
			{
				m_words.Add(word);
				word = 0;
				shift = 0;
			}
		}
		
		if (shift)
			m_words.Add(word);

		return *this;
	}
	
	
	MpUInt const& MpUInt::WriteBytes(Enc& enc, sizet padToMinBytes) const
	{
		sizet nrSigBytes = NrBytes();
		sizet nrOutBytes = nrSigBytes;
		sizet nrZeros {};

		if (padToMinBytes > nrSigBytes)
		{
			nrOutBytes = padToMinBytes;
			nrZeros = padToMinBytes - nrSigBytes;
		}

		Enc::Write write = enc.IncWrite(nrOutBytes);
		byte* p = write.Ptr();

		if (nrZeros)
		{
			memset(p, 0, nrZeros);
			p += nrZeros;
		}

		for (sizet i=nrSigBytes; i!=0; )
			*p++ = (byte) GetByte(--i);
		
		write.Add(nrOutBytes);
		return *this;
	}


	int MpUInt::CmpS(uint64 x) const
	{
		uint32 w[2] = { LoWord(x), HiWord(x) };
		if (!w[1])	return CmpS(w[0]);
		else		return Cmp_Impl(w, 2);
	}
	
	
	int MpUInt::CmpS(uint32 x) const
	{
		if (!x) return Cmp_Impl(nullptr, 0);
		else	return Cmp_Impl(&x, 1);
	}


	int MpUInt::Cmp_Impl(uint32 const* words, sizet nrWords) const
	{
		if (NrWords() > nrWords) return 1;
		if (NrWords() < nrWords) return -1;

		for (sizet i=NrWords(); i!=0; )
		{
			--i;
			uint32 tVal = m_words[i];
			uint32 xVal = words[i];
			if (tVal > xVal) return 1;
			if (tVal < xVal) return -1;
		}

		return 0;
	}


	MpUInt& MpUInt::Dec()
	{
		EnsureThrow(NonZero());

		for (sizet i=0; i!=NrWords(); ++i)
			if (m_words[i]-- != 0)
				break;

		StripLeadingZeros();
		return *this;
	}


	MpUInt& MpUInt::Inc()
	{
		for (sizet i=0; i!=NrWords(); ++i)
			if (++m_words[i] != 0)
				return *this;

		m_words.Add(1U);
		return *this;
	}


	void MpUInt::Shr_Impl(uint32 x, MpUInt& r) const
	{
		if (x >= WordBits * NrWords())
		{
			r.SetZero();
			return;
		}

		uint32 shiftWords = x / WordBits;
		uint32 shiftBits  = x % WordBits;

		sizet resultWords = NrWords() - shiftWords;
		r.m_words.ResizeExact(resultWords);

		uint32 carry {};
		for (sizet i=r.NrWords(); i!=0; )
		{
			--i;
			uint32 v = m_words[i+shiftWords];
			r.m_words[i] = carry | (v >> shiftBits);
			if (shiftBits)
				carry = (v << (WordBits - shiftBits));
		}

		r.StripLeadingZeros();
	}


	void MpUInt::Shl_Impl(uint32 x, MpUInt& r) const
	{
		uint32 shiftWords = x / WordBits;
		uint32 shiftBits  = x % WordBits;

		sizet resultWords = NrWords() + shiftWords;
		r.m_words.ReserveExact(resultWords + 1);
		r.m_words.ResizeExact(resultWords);

		uint32 carry {};
		for (sizet i=0; i!=NrWords(); ++i)
		{
			uint32 v = m_words[i];
			r.m_words[i+shiftWords] = (v << shiftBits) | carry;
			if (shiftBits)
				carry = (v >> (WordBits - shiftBits));
		}

		if (carry)
			r.m_words.Add(carry);
	}


	void MpUInt::AddM_Impl(MpUInt const& x, MpUInt& r) const
	{
		// Ensure *this has at least as many words as x
		if (x.NrWords() > NrWords())
		{
			x.AddM_Impl(*this, r);
			return;
		}

		// If x is zero, return *this. If x is non-zero, then at this point, *this is non-zero as well.
		if (x.IsZero())
		{
			r = *this;
			return;
		}

		r.m_words.ReserveExact(NrWords() + 1);
		r.m_words.ResizeExact(NrWords());

		sizet i {};
		uint32 carry {};

		while (i != x.NrWords())
		{
			uint64 v = ((uint64) m_words[i]) + ((uint64) x.m_words[i]) + ((uint64) carry);
			r.m_words[i] = LoWord(v);
			carry = HiWord(v);
			++i;
		}

		while (i != NrWords())
		{
			uint64 v = ((uint64) m_words[i]) + ((uint64) carry);
			r.m_words[i] = LoWord(v);
			carry = HiWord(v);
			++i;
		}

		if (carry)
			r.m_words.Add(carry);
	}


	void MpUInt::SubM_Impl(MpUInt const& x, MpUInt& r) const
	{
		// Ensure the result is not negative
		int cmpResult = CmpM(x);
		if (!cmpResult)
		{
			r.SetZero();
			return;
		}

		EnsureThrow(cmpResult > 0);

		// If x is zero, return *this
		if (x.IsZero())
		{
			r = *this;
			return;
		}

		r.m_words.ResizeExact(NrWords());

		sizet i {};
		uint32 borrow {};

		while (i != x.NrWords())
		{
			uint64 v = JoinWords(2, m_words[i]);
			v -= x.m_words[i];
			v -= borrow;
			r.m_words[i] = LoWord(v);
			borrow = 2U - HiWord(v);
			++i;
		}

		while (i != NrWords())
		{
			uint64 v = JoinWords(1, m_words[i]);
			v -= borrow;
			r.m_words[i] = LoWord(v);
			borrow = 1 - HiWord(v);
			++i;
		}

		r.StripLeadingZeros();
	}


	void MpUInt::MulS_Impl(uint32 x, MpUInt& r) const
	{
		if (!x || IsZero())
		{
			r.SetZero();
			return;
		}

		r.m_words.ReserveExact(NrWords() + 1);
		r.m_words.ResizeExact(NrWords());

		uint32 carry {};
		sizet i {};

		for (; i!=NrWords(); ++i)
		{
			uint64 v = (((uint64) m_words[i]) * x) + carry;
			r.m_words[i] = LoWord(v);
			carry = HiWord(v);
		}

		if (carry)
			r.m_words.Add(carry);
	}


	void MpUInt::MulM_Impl(MpUInt const& x, MpUInt& r, MpUInt& temp1, MpUInt& temp2) const
	{
		r.m_words.Clear();
		r.m_words.ReserveExact(NrWords() + x.NrWords());

		temp1.m_words.ReserveExact(r.m_words.Cap());
		temp2.m_words.ReserveExact(r.m_words.Cap());

		for (sizet i=NrWords(); i!=0; )
		{
			--i;
			x.MulS_Impl(m_words[i], temp1);
			temp1.m_words.InsertN(0, i);
			r.AddM_Impl(temp1, temp2);
			r.Swap(temp2);
		}
	}


	void MpUInt::DivS(uint32 divisor, MpUInt& quotient, uint32& remainder) const
	{
		EnsureThrow(divisor != 0);
		EnsureThrow(&quotient != this);

		DivS_Impl(divisor, quotient, remainder);
	}


	void MpUInt::DivS_Impl(uint32 divisor, MpUInt& quotient, uint32& remainder) const
	{
		if (IsZero())
		{
			quotient.SetZero();
			remainder = 0;
		}
		else if ((divisor & (divisor - 1)) == 0)
		{
			// Divisor is a power of 2
			Shr_Impl(NrBitsUsed(divisor) - 1, quotient);
			remainder = (m_words.First() & (divisor - 1));
		}
		else
		{
			quotient.m_words.ResizeExact(NrWords());
			remainder = 0;

			for (sizet i=NrWords(); i!=0; )
			{
				--i;
				uint64 v = JoinWords(remainder, m_words[i]);
				quotient.m_words[i] = (uint32) (v / divisor);
				remainder = (uint32) (v % divisor);
			}

			quotient.StripLeadingZeros();
		}
	}

	
	struct MpUInt_LongDivide
	{
		MpUInt const*  m_divisor    {};
		uint32         m_dHead      {};
		unsigned int   m_dTailBits  {};

		void SetDivisor(MpUInt const& divisor);
		void Calculate(MpUInt const& n, MpUInt& quotient, MpUInt& remainder) const;

	private:
		// Reusable storage for data that does not need to last through recursion
		MpUInt mutable m_qTemp;
		MpUInt mutable m_product;
		MpUInt mutable m_mulTemp1;
		MpUInt mutable m_mulTemp2;
	};


	void MpUInt_LongDivide::SetDivisor(MpUInt const& divisor)
	{
		m_divisor = &divisor;
		m_dHead = divisor.m_words.Last();
		m_dTailBits = NumCast<unsigned int>(MpUInt::WordBits * (divisor.NrWords() - 1));
	}


	void MpUInt_LongDivide::Calculate(MpUInt const& n, MpUInt& quotient, MpUInt& remainder) const
	{
		// Let dHead be the most significant word of divisor.
		// Let dTruncated (not explicitly used as a variable) be an MpUInt of same size as divisor, but of the form [dHead, 0, ..., 0].
		// We can use the DivS and Shr primitives to calculate qEstimate = n / dTruncated.
		// Since dTruncated <= divisor, it follows that qEstimate >= trueQuotient.
		MpUInt qEstimate;
		{
			uint32 r;
			n.DivS_Impl(m_dHead, m_qTemp, r);
			m_qTemp.Shr_Impl(m_dTailBits, qEstimate);
		}

		// Calculate product = qEstimate * divisor.
		qEstimate.MulM_Impl(*m_divisor, m_product, m_mulTemp1, m_mulTemp2);

		if (m_product <= n)
		{
			// The product, which equals qEstimate * divisor, is less than or equal to n.
			// Let trueProduct be trueQuotient * divisor. Since qEstimate >= trueQuotient, it follows that trueProduct <= product <= n.
			// Since (n - trueProduct) < divisor, and both product and trueProduct are multiples of divisor, it must be that product == trueProduct.
			// It therefore must be that qEstimate == trueQuotient.
			quotient.Swap(qEstimate);
		}
		else
		{
			// Since dTruncated <= divisor (or since we're here, it must be strictly less), we overshot, and qEstimate * divisor > n.
			// Calculate nOvershoot = product - n.
			MpUInt nOvershoot;
			m_product.SubM_Impl(n, nOvershoot);

			// Using recursion, calculate qOvershoot and rOvershoot such that nOvershoot == (qOvershoot * divisor) + rOvershoot.
			// In other words, qOvershoot = nOvershoot / divisor.
			MpUInt qOvershoot;
			MpUInt rOvershoot;
			Calculate(nOvershoot, qOvershoot, rOvershoot);

			// Calculate quotient = qEstimate - qOvershoot. This might not still be the final quotient: qOvershoot might be slightly too small.
			qEstimate.SubM_Impl(qOvershoot, quotient);

			// Calculate product = quotient * divisor.
			quotient.MulM_Impl(*m_divisor, m_product, m_mulTemp1, m_mulTemp2);

			// Decrement quotient, adjusting product accordingly, until product <= n. Should only need to do this once or twice.
			while (m_product > n)
			{
				quotient.Dec();
				m_product.SubM_Impl(*m_divisor, m_qTemp);
				m_product.Swap(m_qTemp);
			}
		}

		// Calculate remainder = n - product.
		n.SubM_Impl(m_product, remainder);
	}


	void MpUInt::DivM(MpUInt const& divisor, MpUInt& quotient, MpUInt& remainder) const
	{
		EnsureThrow(divisor.NonZero());
		EnsureThrow(&quotient  != this      );
		EnsureThrow(&quotient  != &divisor  );
		EnsureThrow(&remainder != this      );
		EnsureThrow(&remainder != &divisor  );
		EnsureThrow(&remainder != &quotient );

		DivM_Impl(divisor, quotient, remainder);
	}


	void MpUInt::DivM_Impl(MpUInt const& divisor, MpUInt& quotient, MpUInt& remainder) const
	{
		if (*this < divisor)
		{
			quotient.SetZero();
			remainder = *this;
		}
		else if (divisor.NrWords() == 1)
		{
			uint32 r;
			DivS_Impl(divisor.m_words.First(), quotient, r);
			remainder.Set(r);
		}
		else
		{
			// Our long division algorithm is more exact in the first round, and thus requires fewer recursions,
			// and thus makes fewer uses of DivS and MulM, if the bits in dHead are fully used.
			unsigned int divisorHeadBits = NrBitsUsed(divisor.m_words.Last());
			unsigned int shiftBits = WordBits - divisorHeadBits;

			if (!shiftBits)
			{
				MpUInt_LongDivide ld;
				ld.SetDivisor(divisor);
				ld.Calculate(*this, quotient, remainder);
			}
			else
			{
				MpUInt nShifted, dShifted, r;
				Shl_Impl(shiftBits, nShifted);
				divisor.Shl_Impl(shiftBits, dShifted);

				MpUInt_LongDivide ld;
				ld.SetDivisor(dShifted);
				ld.Calculate(nShifted, quotient, r);

				r.Shr_Impl(shiftBits, remainder);
			}
		}
	}


	void MpUInt::PowModS_Impl(uint32 x, MpUInt const& m, MpUInt& result) const
	{
		if (IsZero())
		{
			if (!x)
				result.Set(1);
			else
				result.SetZero();
		}
		else
		{
			result.Set(1);

			if (x)
			{
				MpUInt base = *this;
				MpUInt product, quotient, mulTemp1, mulTemp2;

				do
				{
					if ((x & 1) == 1)
					{
						// Calculate result = (result * base) % m
						result.MulM_Impl(base, product, mulTemp1, mulTemp2);
						product.DivM_Impl(m, quotient, result);
					}

					x >>= 1;

					// Calculate base = (base * base) % m
					base.MulM_Impl(base, product, mulTemp1, mulTemp2);
					product.DivM_Impl(m, quotient, base);
				}
				while (x);
			}
		}
	}


	void MpUInt::PowModM_Impl(MpUInt const& x, MpUInt const& m, MpUInt& result) const
	{
		if (IsZero())
		{
			if (x.IsZero())
				result.Set(1);
			else
				result.SetZero();
		}
		else
		{
			result.Set(1);

			if (x.NonZero())
			{
				MpUInt base = *this;
				MpUInt product, quotient, mulTemp1, mulTemp2;

				sizet xBits = x.NrBits();
				uint32 const* pxCurWord = x.m_words.Ptr();
				uint32 xCurWord = *pxCurWord;

				for (sizet i=1; i<=xBits; ++i)
				{
					if ((xCurWord & 1) == 1)
					{
						// Calculate result = (result * base) % m
						result.MulM_Impl(base, product, mulTemp1, mulTemp2);
						product.DivM_Impl(m, quotient, result);
					}

					// Get next bit or word in x
					if ((i % WordBits) != 0)
						xCurWord >>= 1;
					else
						xCurWord = *++pxCurWord;

					// Calculate base = (base * base) % m
					base.MulM_Impl(base, product, mulTemp1, mulTemp2);
					product.DivM_Impl(m, quotient, base);
				}
			}
		}
	}

}
