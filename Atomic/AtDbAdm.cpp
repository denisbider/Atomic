#include "AtIncludes.h"
#include "AtDbAdm.h"

#include "AtScripts.h"


namespace At
{

	HtmlBuilder& DbAdmPage::WHP_Head(HtmlBuilder& html) const
	{
		html.AddCss(c_css_AtDbAdm);
		return DAP_Env_Head(html);
	}

}
