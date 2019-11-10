#pragma once

#include "AtIncludes.h"
#include "AtPinStore.h"


namespace At
{

	// Builds a Content-Security-Policy
	class CspBuilder
	{
	public:
		CspBuilder() : m_store(1000) {}

		// If directives are already present, parsed values will be CONCATENATED to existing values for same directives.
		// Allows for newlines and sequences of whitespace in input, but will not produce them in Final().
		void AdditiveParse(Seq policy);

		bool Any() const { return m_directives.Any(); }

		Seq Final();

		CspBuilder& AddDefaultSrc (Seq s) { AddDirectiveValue_Lower("default-src", s); return *this; }
		CspBuilder& AddScriptSrc  (Seq s) { AddDirectiveValue_Lower("script-src",  s); return *this; }
		CspBuilder& AddStyleSrc   (Seq s) { AddDirectiveValue_Lower("style-src",   s); return *this; }
		CspBuilder& AddFrameSrc   (Seq s) { AddDirectiveValue_Lower("frame-src",   s); return *this; }

		static Str Sha256(Seq s) { return CspHash(s, "sha256-", CALG_SHA_256); }
		static Str Sha384(Seq s) { return CspHash(s, "sha384-", CALG_SHA_384); }
		static Str Sha512(Seq s) { return CspHash(s, "sha512-", CALG_SHA_512); }

	private:
		struct Directive
		{
			Seq      m_dirNameLower;
			Vec<Seq> m_values;
		};

		PinStore       m_store;
		Vec<Directive> m_directives;

		sizet    m_estSizeBytes {};
		bool     m_finalized {};
		Str      m_final;

		void AddDirectiveValue_Lower(Seq dirNameLower, Seq value);

		static Str CspHash(Seq s, Seq algPrefix, ALG_ID algId);
	};

}
