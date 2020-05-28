#pragma once

#include "AtAuto.h"
#include "AtCspBuilder.h"
#include "AtFormInputType.h"
#include "AtJsonBuilder.h"
#include "AtPostFormSecurity.h"
#include "AtTagStack.h"


namespace At
{

	// TextAreaDims

	struct TextAreaDims
	{
		TextAreaDims(char const* zRows, char const* zCols)
			: mc_zRows(zRows), mc_zCols(zCols) {}

		char const* const mc_zRows {};
		char const* const mc_zCols {};
	};

	extern TextAreaDims const tad_policy;
	extern TextAreaDims const tad_centerpiece;



	// HtmlBuilder

	namespace Html { class Transform; }

	struct HtmlState { enum E {
		Attrs,
		Elems,
		ScriptOrStyle,
	}; };

	class HtmlBuilder : public PostFormSecurity
	{
	public:
		enum { DefaultReserveHtml = 8000, DefaultReserveCss = 4000, DefaultReserveJs = 4000 };
		enum EFragment { Fragment };

		// The reserve parameter can help reserve a large enough amount of memory upfront to avoid reallocations
		HtmlBuilder(sizet reserveHtml = DefaultReserveHtml);
		HtmlBuilder(EFragment, sizet reserveHtml = DefaultReserveHtml) { m_s.ReserveExact(reserveHtml); }

		Seq Final() const { EnsureThrow(!m_tags.Any()); return m_s; }

		// Any styles and scripts added via AddCss and AddJs will be concatenated, included inline just before
		// the end of the <head> element (for CSS) or the <body> element (for JS), and a hash of the resulting
		// concatenation will be included in the CSP automatically.
		CspBuilder&       Csp()       { return m_csp; }
		CspBuilder const& Csp() const { return m_csp; }

		// CSS and JS

		HtmlBuilder& AddCss(Seq cssBody);

		// Use this version when the JS takes no parameters. The passed script is NOT enclosed in a block, so any
		// let/const variables it declares will be visible to other scripts added.
		void AddJs(Seq jsBody);

		// Use this version when the JS takes parameters. The passed script is enclosed in a block,
		// so any let/const variables it declares will NOT be visible outside the script.
		// The function automatically adds an object to the JsonBuilder that's returned.
		// The caller must add any members to the JsonBuilder and close the object with EndObject.
		// The parameters will be available to the script in a local const object named "arg".
		JsonBuilder& AddJs_Args(Seq jsBody);
		JsonBuilder& AddJs_ReloadElem_Call();

		void AddJs_ReloadElem();
		void AddJs_PopUpLinks();

		// Generics

		template <class F, typename... Args>
		HtmlBuilder& Fun(F f, Args&&... args) { return f(*this, std::forward<Args>(args)...); }

		template <class T, class M, typename... Args>
		HtmlBuilder& Method(T& t, M m, Args&&... args) { return (t.*m)(*this, std::forward<Args>(args)...); }

		template <class T, class M, typename... Args>
		HtmlBuilder& Method(T const& t, M m, Args&&... args) { return (t.*m)(*this, std::forward<Args>(args)...); }
	
		template <typename... Args>
		HtmlBuilder& Cond(bool cond, HtmlBuilder& (HtmlBuilder::*onTrue)(Args...), Args&&... args)
			{ if (cond) (this->*onTrue)(std::forward<Args>(args)...); return *this; }

		// Attributes

		// Policy: When a supported attribute has the same name as a supported element, the attribute receives an "Attr" suffix.
		
		HtmlBuilder& AutoFocus    ()          { AddAttr("autofocus"          ); return *this; }
		HtmlBuilder& Checked      ()          { AddAttr("checked"            ); return *this; }
		HtmlBuilder& Disabled     ()          { AddAttr("disabled"           ); return *this; }
		HtmlBuilder& Multiple     ()          { AddAttr("multiple"           ); return *this; }
		HtmlBuilder& ReadOnly     ()          { AddAttr("readonly"           ); return *this; }
		HtmlBuilder& Sandbox      ()          { AddAttr("sandbox"            ); return *this; }
		HtmlBuilder& Selected     ()          { AddAttr("selected"           ); return *this; }
								 									  
		HtmlBuilder& AccessKey    (Seq value) { AddAttr("accesskey",    value); return *this; }
		HtmlBuilder& AutoComplete (Seq value) { AddAttr("autocomplete", value); return *this; }
		HtmlBuilder& Class        (Seq value) { AddAttr("class",        value); return *this; }
		HtmlBuilder& Cols         (Seq value) { AddAttr("cols",         value); return *this; }
		HtmlBuilder& ColSpan      (Seq value) { AddAttr("colspan",      value); return *this; }
		HtmlBuilder& Data_SiteKey (Seq value) { AddAttr("data-sitekey", value); return *this; }
		HtmlBuilder& For          (Seq value) { AddAttr("for",          value); return *this; }
		HtmlBuilder& Href         (Seq value) { AddAttr("href",         value); return *this; }
		HtmlBuilder& Id           (Seq value) { AddAttr("id",           value); return *this; }
		HtmlBuilder& InputMode    (Seq value) { AddAttr("inputmode",    value); return *this; }
		HtmlBuilder& LabelAttr    (Seq value) { AddAttr("label",        value); return *this; }
		HtmlBuilder& List         (Seq value) { AddAttr("list",         value); return *this; }
		HtmlBuilder& MinAttr      (Seq value) { AddAttr("min",          value); return *this; }
		HtmlBuilder& MaxAttr      (Seq value) { AddAttr("max",          value); return *this; }
		HtmlBuilder& MaxLength    (Seq value) { AddAttr("maxlength",    value); return *this; }
		HtmlBuilder& Media        (Seq value) { AddAttr("media",        value); return *this; }
		HtmlBuilder& Name         (Seq value) { AddAttr("name",         value); return *this; }
		HtmlBuilder& Placeholder  (Seq value) { AddAttr("placeholder",  value); return *this; }
		HtmlBuilder& Rel          (Seq value) { AddAttr("rel",          value); return *this; }
		HtmlBuilder& Rows         (Seq value) { AddAttr("rows",         value); return *this; }
		HtmlBuilder& RowSpan      (Seq value) { AddAttr("rowspan",      value); return *this; }
		HtmlBuilder& Sandbox      (Seq value) { AddAttr("sandbox",      value); return *this; }
		HtmlBuilder& Size         (Seq value) { AddAttr("size",         value); return *this; }
		HtmlBuilder& SpellCheck   (Seq value) { AddAttr("spellcheck",   value); return *this; }
		HtmlBuilder& Src          (Seq value) { AddAttr("src",          value); return *this; }
		HtmlBuilder& SrcDoc       (Seq value) { AddAttr("srcdoc",       value); return *this; }
		HtmlBuilder& Start        (Seq value) { AddAttr("start",        value); return *this; }
		HtmlBuilder& Target       (Seq value) { AddAttr("target",       value); return *this; }
		HtmlBuilder& TitleAttr    (Seq value) { AddAttr("title",        value); return *this; }
		HtmlBuilder& Type         (Seq value) { AddAttr("type",         value); return *this; }
		HtmlBuilder& Url          (Seq value) { AddAttr("url",          value); return *this; }
		HtmlBuilder& Value        (Seq value) { AddAttr("value",        value); return *this; }
								 
		HtmlBuilder& CheckedIf    (bool cond) { if (cond) Checked(); return *this; }
		HtmlBuilder& DisabledIf   (bool cond) { if (cond) Disabled(); return *this; }
		HtmlBuilder& ReadOnlyIf   (bool cond) { if (cond) ReadOnly(); return *this; }
		HtmlBuilder& IdAndName    (Seq value) { AddAttr("id", value); AddAttr("name", value); return *this; }

		HtmlBuilder& UniqueId(Str& id) { GenUniqueId(id); return Id(id); }

		HtmlBuilder& T(Seq text) { return T(text, Html::CharRefs::Render); }
		HtmlBuilder& T(Seq text, Html::CharRefs charRefs);
		HtmlBuilder& AddTextUnescaped(Seq text);
		HtmlBuilder& B(Seq text)          { return B().T(text).EndB(); }
		HtmlBuilder& I(Seq text)          { return I().T(text).EndI(); }
		HtmlBuilder& H1(Seq text)         { return H1().T(text).EndH1(); }
		HtmlBuilder& H2(Seq text)         { return H2().T(text).EndH2(); }
		HtmlBuilder& Code(Seq text)       { return Code().T(text).EndCode(); }

		HtmlBuilder& A(Seq url, Seq text) { return A().Href(url).T(text).EndA(); }
		
		// Creates a pop-up link that uses two separate mechanisms to prevent the new tab from navigating the current tab (which could be used to e.g. navigate our tab to a phishing page):
		// - If JavaScript is enabled, uses window.open() and sets .opener=null. This prevents the new tab from navigating the opener, but does not suppress referrer information.
		// - If JavaScript is disabled, uses target="_blank" rel="noopener noreferrer". In this case, referrer will not be sent either. This is necessary because Firefox does not support "noopener".
		// This probably does not mitigate all possible avenues for this type of manipulation. As of August 25, 2016, Google's position is that this issue is inherent in current browser design;
		// that there are other ways to do this that nullifying the opener property does not suppress; and that no single website can mitigate this issue. 
		HtmlBuilder& PopUpLink(Seq url)
			{ AddJs_PopUpLinks(); return A().Class("popUpLink").Href(url).Target("_blank").Rel("noopener noreferrer"); }

		HtmlBuilder& Nbsp  () { AddCharRef("nbsp"  ); return *this; }
		HtmlBuilder& Ndash () { AddCharRef("ndash" ); return *this; }

		HtmlBuilder& UInt(uint64 v,                               uint b=10, uint w=0, CharCase cc=CharCase::Upper) { if (m_state == HtmlState::Attrs) EndAttrs(); m_s.UInt(v,     b, w, cc); return *this; }
		HtmlBuilder& SInt(int64  v, AddSign::E as=AddSign::IfNeg, uint b=10, uint w=0, CharCase cc=CharCase::Upper) { if (m_state == HtmlState::Attrs) EndAttrs(); m_s.SInt(v, as, b, w, cc); return *this; }
		HtmlBuilder& Dbl (double v, uint prec=4)                                                                    { if (m_state == HtmlState::Attrs) EndAttrs(); m_s.Dbl (v, prec        ); return *this; }
		HtmlBuilder& TzOffset(int64 v)                                                                              { if (m_state == HtmlState::Attrs) EndAttrs(); m_s.TzOffset(v);           return *this; }

		// Void elements
		HtmlBuilder& Area()    { AddVoidElem("area");  return *this; }
		HtmlBuilder& Base()    { AddVoidElem("base");  return *this; }
		HtmlBuilder& Br()      { AddVoidElem("br");    return *this; }
		HtmlBuilder& Col()     { AddVoidElem("col");   return *this; }
		HtmlBuilder& Hr()      { AddVoidElem("hr");    return *this; }
		HtmlBuilder& Img()     { AddVoidElem("img");   return *this; }
		HtmlBuilder& Input()   { AddVoidElem("input"); return *this; }
		HtmlBuilder& Link()    { AddVoidElem("link");  return *this; }
		HtmlBuilder& Meta()    { AddVoidElem("meta");  return *this; }
		HtmlBuilder& Param()   { AddVoidElem("param"); return *this; }

		HtmlBuilder& InputPassword(FormInputType const& t) { return Input().Type("password").Size(t.mc_zWidth).MaxLength(t.mc_zMaxLen); }

		HtmlBuilder& InputText(FormInputType const& t) { return Input().Type("text").Size(t.mc_zWidth).MaxLength(t.mc_zMaxLen); }
		HtmlBuilder& InputIp4         (Seq ph) { return InputText(fit_ip4         ).Placeholder(ph); }
		HtmlBuilder& InputIp6         (Seq ph) { return InputText(fit_ip6         ).Placeholder(ph); }
		HtmlBuilder& InputIp4Or6      (Seq ph) { return InputText(fit_ip6         ).Placeholder(ph); }
		HtmlBuilder& InputDnsName     (Seq ph) { return InputText(fit_dnsName     ).Placeholder(ph); }
		HtmlBuilder& InputEmail       (Seq ph) { return InputText(fit_email       ).Placeholder(ph); }
		HtmlBuilder& InputUrl         (Seq ph) { return InputText(fit_url         ).Placeholder(ph); }
		HtmlBuilder& InputPostalCode  ()       { return InputText(fit_postalCode  ); }
		HtmlBuilder& InputName        ()       { return InputText(fit_name        ); }
		HtmlBuilder& InputNameOrEmail ()       { return InputText(fit_nameOrEmail ); }
		HtmlBuilder& InputStreet      ()       { return InputText(fit_street      ); }
		HtmlBuilder& InputDesc        ()       { return InputText(fit_desc        ); }
		HtmlBuilder& InputKeywords    ()       { return InputText(fit_keywords    ); }
		HtmlBuilder& InputOrigin      ()       { return InputText(fit_origin      ); }
		HtmlBuilder& InputNameList    ()       { return InputText(fit_nameList    ); }
		HtmlBuilder& InputLocalPath   ()       { return InputText(fit_localPath   ); }

		HtmlBuilder& InputNumber() { return Input().Type("text").InputMode("numeric").Size(fit_number.mc_zWidth).MaxLength(fit_number.mc_zMaxLen); }

		HtmlBuilder& InputIp4     () { return InputIp4     ("127.0.0.1"); }
		HtmlBuilder& InputIp6     () { return InputIp6     ("::1"); }
		HtmlBuilder& InputIp4Or6  () { return InputIp4Or6  ("127.0.0.1 or ::1"); }
		HtmlBuilder& InputDnsName () { return InputDnsName ("example.com"); }
		HtmlBuilder& InputEmail   () { return InputEmail   ("email@example.com"); }
		HtmlBuilder& InputUrl     () { return InputUrl     ("https://www.example.com/"); }

		HtmlBuilder& InputOptStr(FormInputType const& t, Seq idAndName, Opt<Str> const& optStr, Enabled enabled = Enabled::Yes);

		HtmlBuilder& InputDate() { return Input().Type("date").Size("10").MaxLength("10").Placeholder("yyyy-mm-dd"); }

		HtmlBuilder& InputCheckbox() { return Input().Type("checkbox").Value("1"); }

		HtmlBuilder& InputFile() { return Input().Type("file"); }

		HtmlBuilder& InputHidden(Seq name, Seq value) { return Input().Type("hidden").Name(name).Value(value); }

		HtmlBuilder& InputSubmit(Seq value) { return Input().Type("submit").Value(value); }
		HtmlBuilder& InputSubmit(Seq name, Seq value) { return Input().Type("submit").Name(name).Value(value); }

		HtmlBuilder& ConfirmSubmit(Seq confirmText, Seq submitName, Seq submitText);

		// Non-void elements
		HtmlBuilder& B()          { AddNonVoidElem("b");      EndAttrs(); return *this; }
		HtmlBuilder& Strong()     { AddNonVoidElem("strong"); EndAttrs(); return *this; }
		HtmlBuilder& I()          { AddNonVoidElem("i");      EndAttrs(); return *this; }
		HtmlBuilder& Em()         { AddNonVoidElem("em");     EndAttrs(); return *this; }

		HtmlBuilder& A()          { AddNonVoidElem("a");           return *this; }
		HtmlBuilder& Blockquote() { AddNonVoidElem("blockquote");  return *this; }
		HtmlBuilder& Body()       { AddNonVoidElem("body");        return *this; }
		HtmlBuilder& Button()     { AddNonVoidElem("button");      return *this; }
		HtmlBuilder& Code()       { AddNonVoidElem("code");        return *this; }
		HtmlBuilder& ColGroup()   { AddNonVoidElem("colgroup");    return *this; }
		HtmlBuilder& Datalist()   { AddNonVoidElem("datalist");    return *this; }
		HtmlBuilder& Div()        { AddNonVoidElem("div");         return *this; }
		HtmlBuilder& H1()         { AddNonVoidElem("h1");          return *this; }
		HtmlBuilder& H2()         { AddNonVoidElem("h2");          return *this; }
		HtmlBuilder& H3()         { AddNonVoidElem("h3");          return *this; }
		HtmlBuilder& H4()         { AddNonVoidElem("h4");          return *this; }
		HtmlBuilder& Iframe()     { AddNonVoidElem("iframe");      return *this; }
		HtmlBuilder& Label()      { AddNonVoidElem("label");       return *this; }
		HtmlBuilder& Li()         { AddNonVoidElem("li");          return *this; }
		HtmlBuilder& Ol()         { AddNonVoidElem("ol");          return *this; }
		HtmlBuilder& OptGroup()   { AddNonVoidElem("optgroup");    return *this; }
		HtmlBuilder& Option()     { AddNonVoidElem("option");      return *this; }
		HtmlBuilder& P()          { AddNonVoidElem("p");           return *this; }
		HtmlBuilder& Pre()        { AddNonVoidElem("pre");         return *this; }
		HtmlBuilder& S()          { AddNonVoidElem("s");           return *this; }
		HtmlBuilder& Script()     { AddNonVoidElem("script");      return *this; }
		HtmlBuilder& Select()     { AddNonVoidElem("select");      return *this; }
		HtmlBuilder& Span()       { AddNonVoidElem("span");        return *this; }
		HtmlBuilder& Style()      { AddNonVoidElem("style");       return *this; }
		HtmlBuilder& Table()      { AddNonVoidElem("table");       return *this; }
		HtmlBuilder& Td()         { AddNonVoidElem("td");          return *this; }
		HtmlBuilder& TextArea()   { AddNonVoidElem("textarea");    return *this; }
		HtmlBuilder& Th()         { AddNonVoidElem("th");          return *this; }
		HtmlBuilder& Title()      { AddNonVoidElem("title");       return *this; }
		HtmlBuilder& Tr()         { AddNonVoidElem("tr");          return *this; }
		HtmlBuilder& Ul()         { AddNonVoidElem("ul");          return *this; }

		HtmlBuilder& TextArea(TextAreaDims const& d) { return TextArea().Rows(d.mc_zRows).Cols(d.mc_zCols); }

		// Block-level element. Do not enclose in P/EndP.
		HtmlBuilder& TextAreaOptStr(TextAreaDims const& d, Seq idAndName, Opt<Str> const& optStr, Enabled enabled = Enabled::Yes);

		// Special method. Not only ends head but inserts CSS scripts.
		// No further AddCss calls can be made after this.
		HtmlBuilder& EndHead();

		// Special method. Not only ends body but inserts JS scripts and their arguments.
		// No further AddJs calls can be made after this.
		HtmlBuilder& EndBody();

		HtmlBuilder& EndA()            { EndNonVoidElem("a");          return *this; }
		HtmlBuilder& EndB()            { EndNonVoidElem("b");          return *this; }
		HtmlBuilder& EndBlockquote()   { EndNonVoidElem("blockquote"); return *this; }
		HtmlBuilder& EndButton()       { EndNonVoidElem("button");     return *this; }
		HtmlBuilder& EndCode()         { EndNonVoidElem("code");       return *this; }
		HtmlBuilder& EndColGroup()     { EndNonVoidElem("colgroup");   return *this; }
		HtmlBuilder& EndDatalist()     { EndNonVoidElem("datalist");   return *this; }
		HtmlBuilder& EndDiv()          { EndNonVoidElem("div");        return *this; }
		HtmlBuilder& EndEm()           { EndNonVoidElem("em");         return *this; }
		HtmlBuilder& EndForm()         { EndNonVoidElem("form");       return *this; }
		HtmlBuilder& EndH1()           { EndNonVoidElem("h1");         return *this; }
		HtmlBuilder& EndH2()           { EndNonVoidElem("h2");         return *this; }
		HtmlBuilder& EndH3()           { EndNonVoidElem("h3");         return *this; }
		HtmlBuilder& EndH4()           { EndNonVoidElem("h4");         return *this; }
		HtmlBuilder& EndHtml()         { EndNonVoidElem("html");       return *this; }
		HtmlBuilder& EndI()            { EndNonVoidElem("i");          return *this; }
		HtmlBuilder& EndIframe()       { EndNonVoidElem("iframe");     return *this; }
		HtmlBuilder& EndLabel()        { EndNonVoidElem("label");      return *this; }
		HtmlBuilder& EndLi()           { EndNonVoidElem("li");         return *this; }
		HtmlBuilder& EndOl()           { EndNonVoidElem("ol");         return *this; }
		HtmlBuilder& EndOptGroup()     { EndNonVoidElem("optgroup");   return *this; }
		HtmlBuilder& EndOption()       { EndNonVoidElem("option");     return *this; }
		HtmlBuilder& EndP()            { EndNonVoidElem("p");          return *this; }
		HtmlBuilder& EndPre()          { EndNonVoidElem("pre");        return *this; }
		HtmlBuilder& EndS()            { EndNonVoidElem("s");          return *this; }
		HtmlBuilder& EndScript()       { EndNonVoidElem("script");     return *this; }
		HtmlBuilder& EndSelect()       { EndNonVoidElem("select");     return *this; }
		HtmlBuilder& EndSpan()         { EndNonVoidElem("span");       return *this; }
		HtmlBuilder& EndStrong()       { EndNonVoidElem("strong");     return *this; }
		HtmlBuilder& EndStyle()        { EndNonVoidElem("style");      return *this; }
		HtmlBuilder& EndTable()        { EndNonVoidElem("table");      return *this; }
		HtmlBuilder& EndTd()           { EndNonVoidElem("td");         return *this; }
		HtmlBuilder& EndTextArea()     { EndNonVoidElem("textarea");   return *this; }
		HtmlBuilder& EndTh()           { EndNonVoidElem("th");         return *this; }
		HtmlBuilder& EndTitle()        { EndNonVoidElem("title");      return *this; }
		HtmlBuilder& EndTr()           { EndNonVoidElem("tr");         return *this; }
		HtmlBuilder& EndUl()           { EndNonVoidElem("ul");         return *this; }

		HtmlBuilder& Label(Seq forId, Seq text) { return Label().For(forId).T(text).EndLabel(); }

		HtmlBuilder& OptGroup(Seq label) { return OptGroup().LabelAttr(label); }
		
		HtmlBuilder& Option(Seq value) { return Option().T(value).EndOption(); }
		HtmlBuilder& Option(Seq value, bool selected) { return Option().Cond(selected, &HtmlBuilder::Selected).T(value).EndOption(); }
		HtmlBuilder& Option(Seq label, Seq value) { return Option().LabelAttr(label).T(value).EndOption(); }
		HtmlBuilder& Option(Seq label, Seq value, bool selected) { return Option().LabelAttr(label).Cond(selected, &HtmlBuilder::Selected).T(value).EndOption(); }

		HtmlBuilder& ScriptSrc(Seq src) { return Script().Type("text/javascript").Src(src).EndScript(); }

		HtmlBuilder& Span(Seq cls, Seq text) { return Span().Class(cls).T(text).EndSpan(); }

		HtmlBuilder& Td   (Seq text) { return Td().T(text).EndTd(); }
		HtmlBuilder& Td_T (Seq text) { return Td().T(text).EndTd(); }
		HtmlBuilder& Th   (Seq text) { return Th().T(text).EndTh(); }
		HtmlBuilder& Th_T (Seq text) { return Th().T(text).EndTh(); }

		HtmlBuilder& DefaultHead();
		HtmlBuilder& RawHead() { AddNonVoidElem("head"); }

		HtmlBuilder& NoSideEffectForm(Seq url, Seq name = Seq());
		HtmlBuilder& SideEffectForm(Seq url, Seq name = Seq(), Seq enctype = Seq());
		HtmlBuilder& MultipartForm(Seq url, Seq name = Seq()) { return SideEffectForm(url, name, "multipart/form-data"); }
		HtmlBuilder& RawForm() { AddNonVoidElem("form"); }

		HtmlBuilder& MultipleUploadSection(Seq prefix);

		HtmlBuilder& ToggleShowDiv(Seq prefix, Seq showText, Seq hideText);			// Starts a div which must be closed by the user. Id of main div is [prefix + "Main"]
		HtmlBuilder& ToggleShowDiv_UniqueId(Seq showText, Seq hideText);
	
		void Advanced_AddNonVoidElem(Seq tag) { return AddNonVoidElem(tag); }
		void Advanced_EndNonVoidElem(Seq tag) { return EndNonVoidElem(tag); }

	protected:
		Str             m_s;
		HtmlState::E    m_state             { HtmlState::Attrs };
		TagStack        m_tags;
		sizet           m_nextUniqueId      { 1 };

		CspBuilder      m_csp;

		bool            m_cssDone           {};
		Str             m_css;

		bool            m_jsDone            {};
		Str             m_js;
		Str             m_jsScriptId;
		JsonBuilder     m_json;
		bool            m_jsHaveReloadElem  {};
		bool            m_jsHavePopUpLinks  {};

		void OnAddJs();
		void AddScriptOrStyleContent(Seq text);

		void AddAttr(Seq name);
		void AddAttr(Seq name, Seq value);
		void EndAttrs();

		void AddCharRef(Seq name);

		void AddVoidElem(Seq tag);
		void AddNonVoidElem(Seq tag);
		void EndNonVoidElem(Seq tag);

		void GenUniqueId(Str& id);

		friend class Html::Transform;
	};

}
