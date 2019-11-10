#pragma once

#include "AtMpUInt.h"

namespace At
{

	struct EcParams
	{
		char const* m_prime_p;
		sizet       m_prime_n;

		char const* m_ident_p;
		sizet       m_ident_n;

		char const* m_a_p;
		sizet       m_a_n;

		char const* m_b_p;
		sizet       m_b_n;

		Seq Prime () const { return Seq(m_prime_p, m_prime_n ); }
		Seq Ident () const { return Seq(m_ident_p, m_ident_n ); }
		Seq A     () const { return Seq(m_a_p,     m_a_n     ); }
		Seq B     () const { return Seq(m_b_p,     m_b_n     ); }

		static EcParams const Nistp256;
	};


	struct EllipticCurve
	{
		MpUInt const mc_prime;
		MpUInt const mc_ident;
		MpUInt const mc_a;
		MpUInt const mc_b;

		EllipticCurve(EcParams const& params) : mc_prime(params.Prime()), mc_ident(params.Ident()), mc_a(params.A()), mc_b(params.B()) {}

		// In the current implementation using MpUInt, this is a relatively expensive operation: on the order of 6 ms using nistp256.
		void CalculateY(MpUInt const& x, bool yOdd, MpUInt& y);
	};

}
