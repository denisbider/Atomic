#pragma once

#include "AtNumCvt.h"
#include "AtUtfWin.h"

namespace At
{
	class WinStr
	{
	public:
		WinStr() = default;
		WinStr(Seq            s) { Set(s); }
		WinStr(wchar_t const* z) { Set(z); }
		WinStr(WinStr const&  x) : m_ws(x.m_ws)            {}
		WinStr(WinStr&&       x) : m_ws(std::move(x.m_ws)) {}

		WinStr& Set(Seq            s) { if (!s.n) m_ws.Clear(); else ToUtf16(s, m_ws, CP_UTF8, NullTerm::Yes);                                        return *this; }
		WinStr& Set(wchar_t const* z) { if (!z)   m_ws.Clear(); else { m_ws.ResizeExact(ZLen(z)+1); memcpy(m_ws.Ptr(), z, m_ws.Len() * sizeof(wchar_t)); } return *this; }
		WinStr& Set(WinStr const&  x) { m_ws = x.m_ws;            return *this; }
		WinStr& Set(WinStr&&       x) { m_ws = std::move(x.m_ws); return *this; }

		wchar_t const* Z() const { if (!m_ws.Any()) return L""; return m_ws.Ptr(); }

		bool Any() const { return Len() > 0; }
		sizet Len() const { if (!m_ws.Any()) return 0; return m_ws.Len() - 1; }

		void EncObj(Enc& enc) const { FromUtf16(m_ws.Ptr(), NumCast<USHORT>(Len()), enc, CP_UTF8); }

	private:
		Vec<wchar_t> m_ws;
	};
}
