#include "AtIncludes.h"
#include "AtHtmlBuilder.h"

#include "AtScripts.h"


namespace At
{

	HtmlBuilder::HtmlBuilder(sizet reserveHtml)
	{
		m_s.ReserveExact(reserveHtml).Add("<!DOCTYPE html><html");
		m_tags.Push("html");
	}


	HtmlBuilder& HtmlBuilder::AddCss(Seq cssBody)
	{
		if (!m_css.Any())
			m_css.ReserveExact(DefaultReserveCss);
		else
			m_css.SetEndExact(" ");
		
		m_css.Add(cssBody);
		return *this;
	}

	
	void HtmlBuilder::OnAddJs()
	{
		EnsureThrow(!m_jsDone);
		if (!m_js.Any())
		{
			m_jsScriptId = Token::Generate();
			m_js.ReserveExact(DefaultReserveJs).Add(c_js_AtHtmlBuilder_Intro);
			m_js.SetEndExact(" ").Add("const args = JSON.parse(Elem('").JsStrEncode(m_jsScriptId).Add("').dataset.args);");
			m_json.Object();
		}
	}


	void HtmlBuilder::AddJs(Seq jsBody)
	{
		OnAddJs();
		m_js.SetEndExact(" ").Add(jsBody);
	}


	JsonBuilder& HtmlBuilder::AddJs_Args(Seq jsBody)
	{
		OnAddJs();

		Str argsId;
		GenUniqueId(argsId);

		m_js.SetEndExact(" ").Add("{ const arg = args.").Add(argsId).Add("; ").Add(jsBody).SetEndExact(" ").Ch('}');
		m_json.Member(argsId).Object();
		return m_json;
	}


	JsonBuilder& HtmlBuilder::AddJs_ReloadElem_Call()
	{
		AddJs_ReloadElem();
		return AddJs_Args(c_js_AtHtmlBuilder_ReloadElem_Call);
	}


	void HtmlBuilder::AddJs_ReloadElem()
	{
		if (!m_jsHaveReloadElem)
		{
			AddJs(c_js_AtHtmlBuilder_ReloadElem);
			m_jsHaveReloadElem = true;
		}
	}


	void HtmlBuilder::AddJs_PopUpLinks()
	{
		if (!m_jsHavePopUpLinks)
		{
			AddJs(c_js_AtHtmlBuilder_PopUpLinks);
			m_jsHavePopUpLinks = true;
		}
	}


	HtmlBuilder& HtmlBuilder::InputOptStr(FormInputType const& t, Seq idAndName, Opt<Str> const& optStr, Enabled enabled)
	{
		Seq value = If(optStr.Any(), Seq, optStr.Ref(), Seq());

		if (enabled == Enabled::No)
		{
			if (optStr.Any())
				InputText(t).IdAndName(idAndName).Value(value).Disabled();
			else
				I("Not present");
		}
		else
		{
			Str checkboxId = Str::Join(idAndName, "_present");		// Must be kept in sync with HttpRequest::GetOptStrFromNvp
			
			InputCheckbox().IdAndName(checkboxId).CheckedIf(optStr.Any()).Span("cfmInput", "Present");
			InputText(t).IdAndName(idAndName).Value(value).DisabledIf(!optStr.Any());

			AddJs_Args(c_js_AtHtmlBuilder_CheckboxEnable)
					.Member("checkboxId").String(checkboxId)
					.Member("otherId").String(idAndName)
				.EndObject();
		}

		return *this;
	}


	HtmlBuilder& HtmlBuilder::TextAreaOptStr(TextAreaDims const& d, Seq idAndName, Opt<Str> const& optStr, Enabled enabled)
	{
		Seq value = If(optStr.Any(), Seq, optStr.Ref(), Seq());

		if (enabled == Enabled::No)
		{
			if (optStr.Any())
				TextArea(d).IdAndName(idAndName).Disabled().T(value).EndTextArea();
			else
				P().I("Not present").EndP();
		}
		else
		{
			Str checkboxId = Str::Join(idAndName, "_present");		// Must be kept in sync with HttpRequest::GetOptStrFromNvp

			P().InputCheckbox().IdAndName(checkboxId).CheckedIf(optStr.Any()).Span("cfmInput", "Present").EndP();
			TextArea(d).IdAndName(idAndName).DisabledIf(!optStr.Any()).T(value).EndTextArea();

			AddJs_Args(c_js_AtHtmlBuilder_CheckboxEnable)
					.Member("checkboxId").String(checkboxId)
					.Member("otherId").String(idAndName)
				.EndObject();
		}

		return *this;
	}


	HtmlBuilder& HtmlBuilder::ConfirmSubmit(Seq confirmText, Seq submitName, Seq submitText)
	{
		Str inputId, checkboxId;
		GenUniqueId(inputId);
		checkboxId.SetAdd(inputId, "Checkbox");

		InputCheckbox().Id(checkboxId).Name(Str("confirm_").Add(submitName)).Span("cfmInput", confirmText)
			.InputSubmit(submitName, submitText).Id(inputId).Disabled();

		AddJs_Args(c_js_AtHtmlBuilder_CheckboxEnable)
				.Member("checkboxId").String(checkboxId)
				.Member("otherId").String(inputId)
			.EndObject();

		return *this;
	}


	HtmlBuilder& HtmlBuilder::DefaultHead()
	{
		// The Referrer Policy spec defined the value "origin-when-crossorigin" in the First Public Working Draft (2014-08-07).
		// This was "fixed" in a following version of the spec, including in the current Editor's Draft (2015-10-01).
		// The "fixed" spelling is now implemented in Chrome and Firefox (in Firefox, since version 41).
		// For more info: http://denisbider.blogspot.com/2015/10/spec-author-fixes-mistake-wastes.html

		AddNonVoidElem("head");

		AddVoidElem("meta");
		AddAttr("charset", "utf-8");
		EndAttrs();

		AddVoidElem("meta");
		AddAttr("name", "viewport");
		AddAttr("content", "width=device-width, initial-scale=1.0");

		AddVoidElem("meta");
		AddAttr("name", "referrer");
		AddAttr("content", "origin-when-cross-origin");
		
		return *this;
	}


	HtmlBuilder& HtmlBuilder::NoSideEffectForm(Seq url, Seq name)
	{
		AddNonVoidElem("form");
		AddAttr("accept-charset", "utf-8");
		AddAttr("method", "get");
		AddAttr("action", url);
		if (name.n != 0)
			AddAttr("name", name);
		EndAttrs();
		return *this;
	}


	HtmlBuilder& HtmlBuilder::SideEffectForm(Seq url, Seq name, Seq encType)
	{
		AddNonVoidElem("form");
		AddAttr("accept-charset", "utf-8");
		AddAttr("method", "post");
		AddAttr("action", url);
		if (name.n)
			AddAttr("name", name);
		if (encType.n)
			AddAttr("enctype", encType);

		AddVoidElem("input");
		AddAttr("type", "hidden");
		AddAttr("name", "PostForm");
		AddAttr("value", PostFormSecurityField());
		EndAttrs();

		return *this;
	}


	HtmlBuilder& HtmlBuilder::MultipleUploadSection(Seq prefix)
	{
		Div().Id(Str(prefix).Add("Section")).EndDiv();

		AddJs_Args(c_js_AtHtmlBuilder_MultiUpload)
				.Member("idPrefix").String(prefix)
			.EndObject();

		return *this;
	}


	HtmlBuilder& HtmlBuilder::ToggleShowDiv(Seq prefix, Seq showText, Seq hideText)
	{
		Str crumbDivId  = Str::Join(prefix,     "Crumb" );
		Str crumbLinkId = Str::Join(crumbDivId, "Link"  );
		Str mainDivId   = Str::Join(prefix,     "Main"  );
		Str mainLinkId  = Str::Join(mainDivId,  "Link"  );

		Div().Id(crumbDivId).Class("toggleCrumb")
			.A().Id(crumbLinkId).Href("#").T(showText).EndA()
		.EndDiv()
		.Div().Id(mainDivId).Class("toggleMain")
			.Div().Class("toggleHide")
				.A().Id(mainLinkId).Href("#").T(hideText).EndA()
			.EndDiv();

		AddJs_Args(c_js_AtHtmlBuilder_ToggleShowDiv)
				.Member("crumbDivId").String(crumbDivId)
				.Member("crumbLinkId").String(crumbLinkId)
				.Member("mainDivId").String(mainDivId)
				.Member("mainLinkId").String(mainLinkId)
			.EndObject();

		return *this;
	}


	HtmlBuilder& HtmlBuilder::ToggleShowDiv_UniqueId(Seq showText, Seq hideText)
	{
		Str prefix;
		prefix.Set("toggle").UInt(m_nextUniqueId);
		++m_nextUniqueId;

		return ToggleShowDiv(prefix, showText, hideText);
	}


	HtmlBuilder& HtmlBuilder::EndHead()
	{
		if (m_css.Any())
		{
			Style().Type("text/css");
			AddScriptOrStyleContent(m_css);
			EndStyle();

			m_csp.AddStyleSrc(CspBuilder::Sha256(m_css));
		}

		m_cssDone = true;
		EndNonVoidElem("head");
		return *this;
	}


	HtmlBuilder& HtmlBuilder::EndBody()
	{
		if (m_js.Any())
		{
			m_json.EndObject();

			Script().Id(m_jsScriptId);
			AddAttr("data-args", m_json.Final());
			AddScriptOrStyleContent(m_js);
			EndScript();

			m_csp.AddScriptSrc(CspBuilder::Sha256(m_js));
		}

		m_jsDone = true;
		EndNonVoidElem("body");
		return *this;
	}


	void HtmlBuilder::AddAttr(Seq name)
	{
		EnsureThrow(m_state == HtmlState::Attrs);
		m_s.Ch(' ').Add(name);
	}


	void HtmlBuilder::AddAttr(Seq name, Seq value)
	{
		EnsureThrow(m_state == HtmlState::Attrs);
		m_s.Ch(' ').Add(name).Add("=\"").HtmlAttrValue(value, Html::CharRefs::Render).Ch('"');	
	}


	void HtmlBuilder::EndAttrs()
	{
		EnsureThrow(m_state == HtmlState::Attrs);
		m_s.Ch('>');

		Seq tag { m_tags.Last() };
		if (tag == "style" || tag == "script")
			m_state = HtmlState::ScriptOrStyle;
		else
			m_state = HtmlState::Elems;
	}


	void HtmlBuilder::GenUniqueId(Str& id)
	{
		id.Set("uq").UInt(m_nextUniqueId);
		++m_nextUniqueId;
	}


	HtmlBuilder& HtmlBuilder::T(Seq text, Html::CharRefs charRefs)
	{
		if (m_state == HtmlState::Attrs)
			EndAttrs();

		EnsureThrow(m_state == HtmlState::Elems);
		m_s.HtmlElemText(text, charRefs);
		return *this;
	}


	HtmlBuilder& HtmlBuilder::AddTextUnescaped(Seq text)
	{
		if (m_state == HtmlState::Attrs)
			EndAttrs();

		m_s.Add(text);
		return *this;
	}


	HtmlBuilder& HtmlBuilder::UIntBytes(uint64 v)
	{
		if (m_state == HtmlState::Attrs)
			EndAttrs();
		
		Span().TitleAttr(Str().UIntDecGrp(v).Add(" bytes"))
			.UIntUnits(v, Units::Bytes)
		.EndSpan();
		
		return *this;
	}


	HtmlBuilder& HtmlBuilder::UIntKb(uint64 v)
	{
		if (m_state == HtmlState::Attrs)
			EndAttrs();
		
		Span().TitleAttr(Str().UIntDecGrp(v).Add(" kB"))
			.UIntUnits(v, Units::kB)
		.EndSpan();
		
		return *this;
	}


	void HtmlBuilder::AddScriptOrStyleContent(Seq text)
	{
		if (m_state == HtmlState::Attrs)
			EndAttrs();

		EnsureThrow(m_state == HtmlState::ScriptOrStyle);
		m_s.Add(text);
	}


	void HtmlBuilder::AddCharRef(Seq name)
	{
		if (m_state == HtmlState::Attrs)
			EndAttrs();

		m_s.Ch('&').Add(name).Ch(';');
	}


	void HtmlBuilder::AddVoidElem(Seq tag)
	{
		if (m_state == HtmlState::Attrs)
			EndAttrs();
	
		m_s.Ch('<').Add(tag);
		m_state = HtmlState::Attrs;
	}


	void HtmlBuilder::AddNonVoidElem(Seq tag)
	{
		if (m_state == HtmlState::Attrs)
			EndAttrs();
	
		m_s.Ch('<').Add(tag);
		m_state = HtmlState::Attrs;
		m_tags.Push(tag);
	}


	void HtmlBuilder::EndNonVoidElem(Seq tag)
	{
		if (m_state == HtmlState::Attrs)
			EndAttrs();
		m_s.Add("</").Add(tag).Ch('>');
		m_tags.PopExact(tag);
		m_state = HtmlState::Elems;
	}

}
