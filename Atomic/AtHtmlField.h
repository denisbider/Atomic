#include "AtIncludes.h"
#include "AtHtmlBuilder.h"
#include "AtHttpRequest.h"


namespace At
{

	struct HtmlForm;

	struct HtmlField
	{
		using HelpFn = std::function<void (HtmlBuilder&)>;

		Seq         m_fieldName    {};
		Seq         m_friendlyName {};

		Str         m_fieldId;
		HelpFn      m_helpFn       {};
		bool        m_disabled     {};

		Vec<Str>    m_fieldErrs;

		// The HtmlField instance must have a lifespan equal to or exceeding the lifespan of the HtmlForm to which it is being added
		HtmlField& SetNameAndId  (HtmlForm& form, char const* fieldName, char const* friendlyName);
		HtmlField& SetHelpLit    (char const* z);
		HtmlField& SetHelpFn     (HelpFn f) { m_helpFn   = f;    return *this; }
		HtmlField& SetDisabled   ()         { m_disabled = true; return *this; }
		HtmlField& SetDisabledIf (bool v)   { m_disabled = v;    return *this; }

		Seq FieldId() const;

		void RenderRow(HtmlBuilder& html) const;

		virtual void RenderInput(HtmlBuilder& html) const = 0;
		virtual void RenderHelp(HtmlBuilder& html) const;
		virtual bool ReadFromPostRequest(HttpRequest const& req) = 0;
	};


	struct HtmlForm
	{
		using HelpFn = std::function<void (HtmlBuilder&)>;

	    uint            m_headingLevel  {};
		Str             m_heading       {};
		Seq             m_headingId     {};
		Str             m_url           {};
		Seq             m_fieldIdPrefix {};
		Seq             m_confirmText   {};
		Seq             m_cmd           {};
		HelpFn          m_helpFn        {};

		Vec<HtmlField*> m_fields;
		Vec<Str>        m_formErrs;
		bool            m_anyErrs       {};
		Str             m_fieldsChanged;

		HtmlForm& SetHeading       (uint l, Seq s)         { m_headingLevel = l; m_heading = s;            return *this; }
		HtmlForm& SetHeading       (uint l, char const* z) { m_headingLevel = l; m_heading = z;            return *this; }
		HtmlForm& SetHeading       (uint l, Str&& s)       { m_headingLevel = l; m_heading = std::move(s); return *this; }

		HtmlForm& SetHeadingId     (char const* z) { m_headingId = z;     return *this; }
		HtmlForm& SetUrl           (Seq s)         { m_url = s;           return *this; }
		HtmlForm& SetFieldIdPrefix (char const* z) { m_fieldIdPrefix = z; return *this; }
		HtmlForm& SetConfirmText   (char const* z) { m_confirmText = z;   return *this; }
		HtmlForm& SetCmd           (char const* z) { m_cmd = z;           return *this; }
		HtmlForm& SetHelpFn        (HelpFn f)      { m_helpFn = f;        return *this; }

		void RenderForm(HtmlBuilder& html) const;
		bool ReadFromPostRequest(HttpRequest const& req);

		void AddFormErr(Seq s)         { m_formErrs.Add(s);            m_anyErrs = true; }
		void AddFormErr(char const* z) { m_formErrs.Add(z);            m_anyErrs = true; }
		void AddFormErr(Str&& s)       { m_formErrs.Add(std::move(s)); m_anyErrs = true; }
	};


	struct HtmlFieldText : HtmlField
	{
		FormInputType const* m_fit {};
		Opt<Str>             m_oldValue;
		Str                  m_value;

		HtmlFieldText& SetFit   (FormInputType const& fit) { m_fit = &fit;           return *this; }
		HtmlFieldText& SetValue (Seq v)                    { m_value = v;            return *this; }
		HtmlFieldText& SetValue (char const* z)            { m_value = z;            return *this; }
		HtmlFieldText& SetValue (Str&& v)                  { m_value = std::move(v); return *this; }

		void RenderInput(HtmlBuilder& html) const override;
		void RenderHelp(HtmlBuilder& html) const override;
		bool ReadFromPostRequest(HttpRequest const& req) override;
	};


	struct HtmlFieldCheckbox : HtmlField
	{
		Opt<bool>            m_oldValue;
		bool                 m_value;

		HtmlFieldCheckbox& SetValue(bool v) { m_value = v; return *this; }

		void RenderInput(HtmlBuilder& html) const override;
		bool ReadFromPostRequest(HttpRequest const& req) override;
	};

}
