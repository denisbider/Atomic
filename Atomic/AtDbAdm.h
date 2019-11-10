#pragma once

#include "AtHtmlBuilder.h"
#include "AtWebEntities.h"
#include "AtWebHtmlPage.h"

namespace At
{
	// DbAdmPage

	class DbAdmPage : virtual public WebHtmlPage
	{
	public:
		HtmlBuilder& WHP_Title (HtmlBuilder& html) const override final { return DAP_Title(html).T(" | Atomic Database Administration"); }
		HtmlBuilder& WHP_Head  (HtmlBuilder& html) const override final;

		virtual HtmlBuilder& DAP_Title         (HtmlBuilder& html) const = 0;

		// Implemented by a final derived class for customization
		virtual HtmlBuilder& DAP_Env_Head      (HtmlBuilder& html) const = 0;
		virtual Seq          DAP_Env_MountPath () const = 0;
	};



	// DbAdm_Browse

	class DbAdm_Browse : virtual public DbAdmPage
	{
	public:
		bool		 WP_ValidateMethod (HttpRequest& req)                     const override final { return req.IsGetOrHead() || req.IsPost(); }
		ReqResult    WP_Process        (EntityStore& store, HttpRequest& req)       override final;
		HtmlBuilder& DAP_Title         (HtmlBuilder& html)                    const override final { return html.T("Browse"); }
		HtmlBuilder& WHP_Body          (HtmlBuilder& html, HttpRequest& req)  const override final;

	private:
		sizet                m_nrEntitiesRemoved  {};
		sizet                m_nrEntitiesInserted {};

		Rp<Entity>           m_entity;
		Str                  m_editedEntityJson;
		Vec<EntityChildInfo> m_children;

		void ProcessImportScript(EntityStore& store, Seq content);
	};



	// DbAdm_InitRequestHandlerCreator

	template <class PageEnv>
	Rp<WebRequestHandlerCreator> DbAdm_InitRequestHandlerCreator(HttpRequest& req)
	{
		Seq pathReader = req.AbsPath();
		if (pathReader.StripPrefixExact(PageEnv::DbAdmMountPath))
		{
			if (!pathReader.Any())		return new WebRequestHandler_Redirect::Creator(HttpStatus::SeeOther, Str(PageEnv::DbAdmMountPath).Add("browse"));
			if (pathReader == "browse") return new PageEnv::DbAdmWrapper<DbAdm_Browse>::Creator();
		}
		return new WebPage_Error::Creator(HttpStatus::NotFound);
	}
}
