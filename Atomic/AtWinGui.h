#pragma once

#include "AtEnsure.h"


namespace At
{

	struct ClientRect : RECT
	{
		int width  {};
		int height {};

		ClientRect() { left = top = right = bottom = 0; }

		ClientRect(HWND hWnd)
		{
			EnsureThrow(GetClientRect(hWnd, this));
			width = right - left;
			height = bottom - top;
		}
	};


	struct Dims
	{
		int left   {};
		int top    {};
		int width  {};
		int height {};
	};


	struct MetaKeys
	{
		enum : uint { Ctrl = 1, Alt = 2, Shift = 4 };

		static uint GetState();
	};


	template <class T, LRESULT (T::*Func)(WPARAM, LPARAM), LRESULT DefResult=0>
	inline LRESULT InvokeWindow(HWND hWnd, WPARAM wParam, LPARAM lParam)
	{
		T* obj = (T*) GetWindowLongPtrW(hWnd, GWLP_USERDATA);
		if (!obj)
			return DefResult;

		return (obj->*Func)(wParam, lParam);
	};

}
