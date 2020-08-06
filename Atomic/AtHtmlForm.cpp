#include "AtIncludes.h"
#include "AtHtmlForm.h"


namespace At
{

	// HtmlField

	void HtmlField::SetNameAndIdEx(HtmlForm& form, char const* fieldName, char const* friendlyName, Visibility visibility)
	{
		m_form = &form;
		m_fieldName = fieldName;

		if (visibility == Visibility::Hidden)
		{
			m_hidden = true;
			form.m_anyHidden = true;
		}
		else
		{
			m_friendlyName = friendlyName;
			if (form.m_fieldIdPrefix.n)
				m_fieldId.SetAdd(form.m_fieldIdPrefix, m_fieldName);
		}

		form.m_fields.Add(this);
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
				.Td().P().Label(FieldId(), m_friendlyName).EndP().EndTd()
				.Td();

		if (m_fieldErrs.Any())
			html.Class("err");

		RenderInput(html);
		for (Seq err : m_fieldErrs)
			html.P().Class("err").T(err).EndP();
		if (m_helpFn)
			m_helpFn(html);
		if (!m_disabled)
			RenderFieldTypeHelp(html);

		html	.EndTd()
			.EndTr();
	}


	void HtmlField::RenderFieldTypeHelp(HtmlBuilder&) const {}


	
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
		{
			if (m_noSideEffect)
			{
				EnsureThrow(!m_multiUpload);
				html.NoSideEffectForm(m_url);
			}
			else if (m_multiUpload)
				html.MultipartForm(m_url);
			else
				html.SideEffectForm(m_url);
		}

		if (m_anyHidden)
			for (HtmlField* field : m_fields)
				if (field->m_hidden)
					field->RenderInput(html);

		html.Table();

		for (HtmlField* field : m_fields)
			if (!field->m_hidden)
				field->RenderRow(html);

		if (m_multiUpload)
			html.Tr()
					.Td().EndTd()
					.Td().MultipleUploadSection(m_fieldIdPrefix).EndTd()
				.EndTr();

		if (m_recaptcha.Any())
			html.Tr()
					.Td().EndTd()
					.Td().Method(m_recaptcha.Ref(), &Recaptcha::Render).EndTd()
				.EndTr();

		if (m_cmd.n)
		{
			html.Tr()
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


	bool HtmlForm::ReadFromRequest(HttpRequest const& req)
	{
		InsensitiveNameValuePairsWithStore const& nvp = (m_noSideEffect ? req.QueryNvp() : req.PostNvp());

		for (HtmlField* field : m_fields)
			if (!field->m_disabled)
				if (!field->ReadFromRequest(req, nvp))
					m_anyErrs = true;

		if (m_recaptcha.Any())
			if (!m_recaptcha->ProcessPost(req))
				AddFormErr("Sorry! Please re-enter the CAPTCHA");

		return !m_anyErrs;
	}



	// HtmlFieldText

	void HtmlFieldText::RenderInput(HtmlBuilder& html) const
	{
		EnsureThrow(!!m_fit);
		if (m_password)
			html.InputPassword(*m_fit);
		else
			html.InputText(*m_fit);

		html.Id(FieldId()).Name(m_fieldName).Value(m_value);
		if (m_autoFocus) html.AutoFocus();
		if (m_disabled) html.Disabled();
	}


	void HtmlFieldText::RenderFieldTypeHelp(HtmlBuilder& html) const
	{
		EnsureThrow(!!m_fit);
		html.P().Class("help").T(Str::From(*m_fit)).EndP();
	}


	bool HtmlFieldText::ReadFromRequest(HttpRequest const&, InsensitiveNameValuePairsWithStore const& nvp)
	{
		EnsureThrow(!!m_fit);
		m_oldValue.Init(std::move(m_value));
		m_value = nvp.Get(m_fieldName).Trim();

		if (!m_fit->IsValid(m_value, m_fieldErrs, m_friendlyName))
		{
			if (m_password)
				m_value.Clear();
			return false;
		}

		return true;
	}



	// HtmlFieldHidden

	void HtmlFieldHidden::RenderInput(HtmlBuilder& html) const
	{
		html.InputHidden(m_fieldName, m_value).Id(FieldId());
	}


	bool HtmlFieldHidden::ReadFromRequest(HttpRequest const&, InsensitiveNameValuePairsWithStore const& nvp)
	{
		m_oldValue.Init(std::move(m_value));
		m_value = nvp.Get(m_fieldName);
		return true;
	}



	// HtmlFieldTextArea

	void HtmlFieldTextArea::RenderInput(HtmlBuilder& html) const
	{
		EnsureThrow(!!m_tad);
		html.TextArea(*m_tad).Id(FieldId()).Name(m_fieldName);
		if (m_autoFocus) html.AutoFocus();
		if (m_disabled) html.Disabled();

		html.T(m_value).EndTextArea();
	}


	void HtmlFieldTextArea::RenderFieldTypeHelp(HtmlBuilder& html) const
	{
		EnsureThrow(!!m_tad);
		html.P().Class("help").T(Str::From(*m_tad)).EndP();
	}


	bool HtmlFieldTextArea::ReadFromRequest(HttpRequest const&, InsensitiveNameValuePairsWithStore const& nvp)
	{
		EnsureThrow(!!m_tad);
		m_oldValue.Init(std::move(m_value));

		Seq reader = nvp.Get(m_fieldName);

		// Skip over leading lines that are empty or consist entirely of whitespace,
		// but don't skip over indentation in the first line that contains non-whitespace characters
		while (reader.n)
		{
			Seq lineReader = reader;
			lineReader.DropToFirstByteNotOf(" \t\f");
			if (!lineReader.ReadToFirstByteNotOf("\n\r").n)
				break;

			reader = lineReader;
		}

		// Skip over trailing lines that are empty or consist entirely of whitespace,
		// but don't skip over trailing whitespace in the last line that contains non-whitespace characters
		while (reader.n)
		{
			Seq lineReader = reader;
			lineReader.RevDropToFirstByteNotOf(" \t\f");
			if (!lineReader.RevReadToFirstByteNotOf("\n\r").n)
				break;

			reader = lineReader;
		}

		// "reader" is now either empty, or contains one or more non-empty lines
		if (!reader.n)
			m_value.Clear();
		else
		{
			// Concatenate lines while canonicalizing newlines to \n
			// After our trimming, last line is non-terminated, so we will append a newline
			m_value.ReserveExact(reader.n + 1);

			while (true)
			{
				Seq line = reader.ReadToFirstByteOf("\n\r");
				m_value.Add(line).Ch(10);
				if (!reader.n)
					break;

				if (!reader.StripPrefixExact("\r\n"))
					reader.DropByte();
			}
		}

		// Validate the processed value
		if (!m_tad->IsValid(m_value, m_fieldErrs, m_friendlyName))
			return false;

		return true;
	}



	// HtmlFieldCheckbox

	void HtmlFieldCheckbox::RenderInput(HtmlBuilder& html) const
	{
		html.InputCheckbox().Id(FieldId()).Name(m_fieldName).CheckedIf(m_value);
		if (m_autoFocus) html.AutoFocus();
		if (m_disabled) html.Disabled();
	}


	bool HtmlFieldCheckbox::ReadFromRequest(HttpRequest const&, InsensitiveNameValuePairsWithStore const& nvp)
	{
		m_oldValue.Init(m_value);
		Seq valStr = nvp.Get(m_fieldName);
		if (!valStr.n)     { m_value = false; return true; }
		if (valStr == "1") { m_value = true;  return true; }
		m_fieldErrs.Add(Str::Join(m_friendlyName, ": invalid checkbox value"));
		return false;
	}



	// HtmlFieldSelectBase

	void HtmlFieldSelectBase::RenderInput(HtmlBuilder& html) const
	{
		html.Select().Id(FieldId()).Name(m_fieldName);
		if (m_autoFocus) html.AutoFocus();
		if (m_disabled) html.Disabled();

		RenderOptions(html);

		html.EndSelect();
	}



	// HtmlFieldSelect

	HtmlFieldSelect& HtmlFieldSelect::SetOptions(Slice<SelectOptInfo> opts, sizet selectedIndex = SIZE_MAX)
	{
		if (selectedIndex == SIZE_MAX)
			return SetOptions(opts, (SelectOptInfo const*) nullptr);
		else
			return SetOptions(opts, &opts[selectedIndex]);
	}


	HtmlFieldSelect& HtmlFieldSelect::SetOptions(Slice<SelectOptInfo> opts, SelectOptInfo const* selectedPtr)
	{
		if (selectedPtr)
			m_value = selectedPtr->m_value;

		m_renderOptions = [this, opts] (HtmlBuilder& html)
			{
				for (SelectOptInfo const& opt : opts)
					html.Option(opt.m_label, opt.m_value, opt.m_value.EqualExact(m_value));
			};

		m_verifyValue = [opts] (Seq value) -> bool
			{
				for (SelectOptInfo const& opt : opts)
					if (value.EqualExact(opt.m_value))
						return true;

				return false;
			};

		return *this;
	}


	HtmlFieldSelect& HtmlFieldSelect::SetOptions_Dynamic()
	{
		m_renderOptions = [] (HtmlBuilder&) {};
		m_verifyValue = [] (Seq) -> bool { return true; };
		return *this;
	}


	void HtmlFieldSelect::RenderOptions(HtmlBuilder& html) const
	{
		if (!!m_renderOptions)
			m_renderOptions(html);
	}


	bool HtmlFieldSelect::ReadFromRequest(HttpRequest const&, InsensitiveNameValuePairsWithStore const& nvp)
	{
		EnsureThrow(!!m_verifyValue);
		m_oldValue.Init(m_value);
		m_value = nvp.Get(m_fieldName);
		if (m_verifyValue(m_value)) return true;
		m_fieldErrs.Add(Str::Join(m_friendlyName, ": invalid select field value"));
		return false;
	}

}
