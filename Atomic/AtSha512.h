#pragma once

#include "AtNum.h"

namespace At
{
	enum { SHA512_DigestSize = 32};

	void SHA512_Simple(void const* pvIn, uint32 inlen, byte* out);
	void HMAC_SHA512_Simple(void const* pvK, uint32 klen, void const* pvIn, uint32 inlen, byte* out);
}

