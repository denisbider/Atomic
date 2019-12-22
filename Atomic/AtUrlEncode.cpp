#include "AtIncludes.h"
#include "AtUrlEncode.h"

#include "AtNumCvt.h"
#include "AtUtfWin.h"

namespace At
{
	// ParseUrlEncodedNvp

	void ParseUrlEncodedNvp(InsensitiveNameValuePairsWithStore& nvp, Seq encoded, UINT inCp)
	{
		if (encoded.n && (encoded.p[0] == '?' || encoded.p[0] == '&'))
			encoded.DropByte();
	
		Str nameCp, valueCp, nameUtf8, valueUtf8;
		Vec<wchar_t> convertBuf1, convertBuf2;

		while (encoded.n)
		{
			Seq name = encoded.ReadToFirstByteOf("=&");
			Seq value;
			if (encoded.n && encoded.p[0] == '=')
				value = encoded.DropByte().ReadToByte('&');
		
			nameCp .Clear().UrlDecode(name);
			valueCp.Clear().UrlDecode(value);
			ToUtf8Norm(nameCp,  nameUtf8,  inCp, convertBuf1, convertBuf2);
			ToUtf8Norm(valueCp, valueUtf8, inCp, convertBuf1, convertBuf2);

			nvp.Add(nameUtf8, valueUtf8);
		
			if (encoded.n)
				encoded.DropByte();
		}
	}
}
