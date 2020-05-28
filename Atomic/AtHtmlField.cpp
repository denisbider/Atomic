#include "AtIncludes.h"
#include "AtHtmlField.h"


namespace At
{

	// HtmlField

	HtmlField& HtmlField::SetNameAndId(HtmlForm& form, char const* fieldName, char const* friendlyName)
	{
		m_fieldName = fieldName;
		m_friendlyName = friendlyName;

		form.m_fields.Add(this);
		if (form.m_fieldIdPrefix.n)
			m_fieldId.SetAdd(form.m_fieldIdPrefix, m_fieldName);

		return *this;
	}


	HtmlField& HtmlField::SetHelpLit(char const* z)
	{
		m_helpFn = [z] (HtmlBuilder& html) { html.P().Class("help").T(z).EndP(); };
		return *this;
	}


	Seq HtmlField::FieldId() const
	{
		if (m_fieldId.Any()) return m_fieldId;
		return m_fieldName;
	}


	void HtmlField::RenderRow(HtmlBuilder& html) const
	{
		html.Tr()
				.Td().P().Label(FieldId(), m_friendlyName).EndTd()
				.Td();

		if (m_fieldErrs.Any())
			html.Class("err");

		RenderInput(html);
		for (Seq err : m_fieldErrs)
			html.P().Class("err").T(err).EndP();
		if (m_helpFn)
			m_helpFn(html);
		RenderHelp(html);

		html	.EndTd()
			.EndTr();
	}


	void HtmlField::RenderHelp(HtmlBuilder&) const {}


	
	// HtmlForm

	void HtmlForm::RenderForm(HtmlBuilder& html) const
	{
		if (m_headingLevel)
		{
			EnsureThrow(m_headingLevel <= 6);
			char zTag[2] = { 'h', '1' };
			zTag[1] = (char) ('0' + m_headingLevel);
			Seq tag { zTag, 2 };

			html.Advanced_AddNonVoidElem(tag);
			if (m_headingId.Any())
				html.Id(m_headingId);
			html.T(m_heading);
			html.Advanced_EndNonVoidElem(tag);
		}

		for (Seq err : m_formErrs)
			html.P().Class("err").T(err).EndP();

		if (m_url.Any())
			html.SideEffectForm(m_url);
		html.Table();

		for (HtmlField* field : m_fields)
			field->RenderRow(html);

		if (m_cmd.n)
		{
			html.Tr().Class("padTop")
					.Td().EndTd()
					.Td().P();
					
			if (m_confirmText.n)
				html.ConfirmSubmit(m_confirmText, "cmd", m_cmd);
			else
				html.InputSubmit("cmd", m_cmd);
					
			html.EndP();
			if (m_helpFn)
				m_helpFn(html);
			
			html	.EndTd()
				.EndTr();
		}

		html.EndTable();
		if (m_url.Any())
			html.EndForm();

		if (m_anyErrs && m_headingId.n)
		{
			Str js;
			js.ReserveExact(m_headingId.n + 50);
			js.Add("Elem('").JsStrEncode(m_headingId).Add("').scrollIntoView();");
			html.AddJs(js);
		}
	}


	bool HtmlForm::ReadFromPostRequest(HttpRequest const& req)
	{
		for (HtmlField* field : m_fields)
			if (!field->ReadFromPostRequest(req))
				m_anyErrs = true;

		return !m_anyErrs;
	}



	// HtmlFieldText

	void HtmlFieldText::RenderInput(HtmlBuilder& html) const
	{
		EnsureThrow(!!m_fit);
		html.InputText(*m_fit).Id(FieldId()).Name(m_fieldName).Value(m_value).DisabledIf(m_disabled);
	}


	void HtmlFieldText::RenderHelp(HtmlBuilder& html) const
	{
		if (!m_disabled)
		{
			EnsureThrow(!!m_fit);
			html.P().Class("help").T(Str::From(*m_fit)).EndP();
		}
	}


	bool HtmlFieldText::ReadFromPostRequest(HttpRequest const& req)
	{
		EnsureThrow(!!m_fit);
		m_oldValue.Init(std::move(m_value));
		m_value = req.PostNvp(m_fieldName).Trim();
		return m_fit->IsValid(m_value, m_fieldErrs, m_friendlyName);
	}



	// HtmlFieldCheckbox

	void HtmlFieldCheckbox::RenderInput(HtmlBuilder& html) const
	{
		html.InputCheckbox().Id(FieldId()).Name(m_fieldName).CheckedIf(m_value).DisabledIf(m_disabled);
	}


	bool HtmlFieldCheckbox::ReadFromPostRequest(HttpRequest const& req)
	{
		m_oldValue.Init(m_value);
		Seq valStr = req.PostNvp(m_fieldName);
		if (!valStr.n)     { m_value = false; return true; }
		if (valStr == "1") { m_value = true;  return true; }
		m_fieldErrs.Add(Str::Join(m_friendlyName, ": invalid checkbox value"));
		return false;
	}

}
