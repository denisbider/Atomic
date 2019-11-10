#include "AtIncludes.h"
#include "AtWinGui.h"


namespace At
{

	uint MetaKeys::GetState()
	{
		uint v {};
		if (!!(GetKeyState(VK_CONTROL ) & 0x8000)) v |= Ctrl;
		if (!!(GetKeyState(VK_MENU    ) & 0x8000)) v |= Alt;
		if (!!(GetKeyState(VK_SHIFT   ) & 0x8000)) v |= Shift;
		return v;
	}

}