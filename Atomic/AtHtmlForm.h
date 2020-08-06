#include "AtIncludes.h"
#include "AtHtmlBuilder.h"
#include "AtHttpRequest.h"
#include "AtRecaptcha.h"


namespace At
{

	// HtmlField

	struct HtmlForm;

	struct HtmlField
	{
		using HelpFn = std::function<void (HtmlBuilder&)>;

		HtmlForm*   m_form         {};
		Seq         m_fieldName    {};
		Seq         m_friendlyName {};		// Not set if field is hidden

		Str         m_fieldId;				// Not set if same as m_fieldName; not set if field is hidden
		HelpFn      m_helpFn       {};
		bool        m_autoFocus    {};
		bool        m_disabled     {};
		bool        m_hidden       {};

		Vec<Str>    m_fieldErrs;

		// The HtmlField instance must have a lifespan equal to or exceeding the lifespan of the HtmlForm to which it is being added
		HtmlField& SetNameAndId(HtmlForm& form, char const* fieldName, char const* friendlyName)
			{ SetNameAndIdEx(form, fieldName, friendlyName, Visibility::Visible); return *this; }

		HtmlField& SetHelpLit     (char const* z);
		HtmlField& SetHelpFn      (HelpFn f) { m_helpFn    = f;    return *this; }
		HtmlField& SetAutoFocus   ()         { m_autoFocus = true; return *this; }
		HtmlField& SetAutoFocusIf (bool v)   { m_autoFocus = v;    return *this; }
		HtmlField& SetDisabled    ()         { m_disabled  = true; return *this; }
		HtmlField& SetDisabledIf  (bool v)   { m_disabled  = v;    return *this; }

		Seq FieldId() const;

		void RenderRow(HtmlBuilder& html) const;

		virtual void RenderInput(HtmlBuilder& html) const = 0;
		virtual void RenderFieldTypeHelp(HtmlBuilder& html) const;
		virtual bool ReadFromRequest(HttpRequest const& req, InsensitiveNameValuePairsWithStore const& nvp) = 0;

		void AddFieldErr(Seq s);
		void AddFieldErr(char const* z);
		void AddFieldErr(Str&& s);

	protected:
		enum class Visibility { Visible, Hidden };
		void SetNameAndIdEx(HtmlForm& form, char const* fieldName, char const* friendlyName, Visibility visibility);
	};



	// HtmlForm

	struct HtmlForm
	{
		using HelpFn = std::function<void (HtmlBuilder&)>;

	    uint            m_headingLevel  {};
		Str             m_heading       {};
		Seq             m_headingId     {};
		Str             m_url           {};
		bool            m_noSideEffect  {};
		bool            m_multiUpload   {};
		Seq             m_fieldIdPrefix {};
		Seq             m_confirmText   {};
		Seq             m_cmd           {};
		HelpFn          m_helpFn        {};
		Opt<Recaptcha>  m_recaptcha     {};

		Vec<HtmlField*> m_fields;
		bool            m_anyHidden     {};
		Vec<Str>        m_formErrs;
		bool            m_anyErrs       {};
		Str             m_fieldsChanged;

		HtmlForm& SetHeading       (uint l, Seq s)         { m_headingLevel = l; m_heading = s;            return *this; }
		HtmlForm& SetHeading       (uint l, char const* z) { m_headingLevel = l; m_heading = z;            return *this; }
		HtmlForm& SetHeading       (uint l, Str&& s)       { m_headingLevel = l; m_heading = std::move(s); return *this; }

		HtmlForm& SetHeadingId     (char const* z) { m_headingId = z;       return *this; }
		HtmlForm& SetUrl           (Seq s)         { m_url = s;             return *this; }
		HtmlForm& SetNoSideEffect  ()              { m_noSideEffect = true; return *this; }
		HtmlForm& SetMultiUpload   ()              { m_multiUpload = true;  return *this; }
		HtmlForm& SetFieldIdPrefix (char const* z) { m_fieldIdPrefix = z;   return *this; }
		HtmlForm& SetConfirmText   (char const* z) { m_confirmText = z;     return *this; }
		HtmlForm& SetCmd           (char const* z) { m_cmd = z;             return *this; }
		HtmlForm& SetHelpFn        (HelpFn f)      { m_helpFn = f;          return *this; }

		HtmlForm& SetRecaptcha     (Seq siteKey, Seq secret) { m_recaptcha.Init().Init(siteKey, secret); return *this; }

		void RenderForm(HtmlBuilder& html) const;
		bool ReadFromRequest(HttpRequest const& req);

		void AddFormErr(Seq s)         { m_formErrs.Add(s);            m_anyErrs = true; }
		void AddFormErr(char const* z) { m_formErrs.Add(z);            m_anyErrs = true; }
		void AddFormErr(Str&& s)       { m_formErrs.Add(std::move(s)); m_anyErrs = true; }
	};


	inline void HtmlField::AddFieldErr(Seq s)         { m_fieldErrs.Add(s);            m_form->m_anyErrs = true; }
	inline void HtmlField::AddFieldErr(char const* z) { m_fieldErrs.Add(z);            m_form->m_anyErrs = true; }
	inline void HtmlField::AddFieldErr(Str&& s)       { m_fieldErrs.Add(std::move(s)); m_form->m_anyErrs = true; }



	// HtmlFieldText

	struct HtmlFieldText : HtmlField
	{
	    bool                 m_password {};
		FormInputType const* m_fit      {};
		Opt<Str>             m_oldValue;
		Str                  m_value;

		HtmlFieldText& SetFit      (FormInputType const& fit) { m_fit = &fit;           return *this; }
		HtmlFieldText& SetValue    (Seq v)                    { m_value = v;            return *this; }
		HtmlFieldText& SetValue    (char const* z)            { m_value = z;            return *this; }
		HtmlFieldText& SetValue    (Str&& v)                  { m_value = std::move(v); return *this; }

		void RenderInput(HtmlBuilder& html) const override;
		void RenderFieldTypeHelp(HtmlBuilder& html) const override;
		bool ReadFromRequest(HttpRequest const& req, InsensitiveNameValuePairsWithStore const& nvp) override;
	};



	// HtmlFieldPass
	
	struct HtmlFieldPass : HtmlFieldText
	{
		HtmlFieldPass() { m_password = true; }
	};



	// HtmlFieldHidden

	struct HtmlFieldHidden : HtmlField
	{
		Opt<Str> m_oldValue;
		Str      m_value;

		HtmlFieldHidden& SetName(HtmlForm& form, char const* fieldName)
			{ HtmlField::SetNameAndIdEx(form, fieldName, nullptr, Visibility::Hidden); return *this; }

		HtmlFieldHidden& SetValue(Seq v)         { m_value = v;            return *this; }
		HtmlFieldHidden& SetValue(char const* z) { m_value = z;            return *this; }
		HtmlFieldHidden& SetValue(Str&& v)       { m_value = std::move(v); return *this; }

		void RenderInput(HtmlBuilder& html) const override;
		bool ReadFromRequest(HttpRequest const& req, InsensitiveNameValuePairsWithStore const& nvp) override;

	private:
		HtmlField& SetNameAndId(HtmlForm& form, char const* fieldName, char const* friendlyName) = delete;
	};



	// HtmlFieldTextArea

	struct HtmlFieldTextArea : HtmlField
	{
		TextAreaDims const*  m_tad      {};
		Opt<Str>             m_oldValue;
		Str                  m_value;

		HtmlFieldTextArea& SetTad      (TextAreaDims const& tad) { m_tad = &tad;           return *this; }
		HtmlFieldTextArea& SetValue    (Seq v)                   { m_value = v;            return *this; }
		HtmlFieldTextArea& SetValue    (char const* z)           { m_value = z;            return *this; }
		HtmlFieldTextArea& SetValue    (Str&& v)                 { m_value = std::move(v); return *this; }

		void RenderInput(HtmlBuilder& html) const override;
		void RenderFieldTypeHelp(HtmlBuilder& html) const override;
		bool ReadFromRequest(HttpRequest const& req, InsensitiveNameValuePairsWithStore const& nvp) override;
	};



	// HtmlFieldCheckbox

	struct HtmlFieldCheckbox : HtmlField
	{
		Opt<bool> m_oldValue;
		bool      m_value {};

		HtmlFieldCheckbox& SetValue(bool v) { m_value = v; return *this; }

		void RenderInput(HtmlBuilder& html) const override;
		bool ReadFromRequest(HttpRequest const& req, InsensitiveNameValuePairsWithStore const& nvp) override;
	};



	// HtmlFieldSelectBase

	struct HtmlFieldSelectBase : HtmlField
	{
		void RenderInput(HtmlBuilder& html) const override;

		virtual void RenderOptions(HtmlBuilder& html) const = 0;
	};



	// HtmlFieldSelect

	struct SelectOptInfo
	{
		Seq m_label;
		Seq m_value;

		SelectOptInfo() {}
		SelectOptInfo(Seq label, Seq value) : m_label(label), m_value(value) {}
	};


	struct HtmlFieldSelect : HtmlFieldSelectBase
	{
		typedef void FnRenderOptions(HtmlBuilder&);
		typedef bool FnVerifyValue(Seq);

		std::function<FnRenderOptions> m_renderOptions;
		std::function<FnVerifyValue>   m_verifyValue;
		Opt<Str>                       m_oldValue;
		Str                            m_value {};

		HtmlFieldSelect& SetRenderOptions(std::function<FnRenderOptions> const& f) { m_renderOptions = f; return *this; }
		HtmlFieldSelect& SetVerifyValue(std::function<FnVerifyValue> const& f) { m_verifyValue = f; return *this; }

		// Can call SetOptions() as a replacement for SetRenderOptions() and SetVerifyValue().
		// optSelected can either be one of the options or nullptr if no option is to be initially selected.
		// The "opts" slice must remain valid at least as long as the last call to RenderInput() or ReadFromRequest().
		HtmlFieldSelect& SetOptions(Slice<SelectOptInfo> opts, sizet selectedIndex);
		HtmlFieldSelect& SetOptions(Slice<SelectOptInfo> opts, SelectOptInfo const* selectedPtr = nullptr);

		// Can call SetOptions_Dynamic() as a replacement for SetRenderOptions() and SetVerifyValue().
		// Renders no options, accepts all values. The page must implement JavaScript to render the options.
		HtmlFieldSelect& SetOptions_Dynamic();

		void RenderOptions(HtmlBuilder& html) const override;
		bool ReadFromRequest(HttpRequest const& req, InsensitiveNameValuePairsWithStore const& nvp) override;
	};



	// HtmlFieldSelectEnum

	template <class T>
	struct HtmlFieldSelectEnum : HtmlFieldSelectBase
	{
		Opt<typename T::E> m_oldValue;
		typename T::E      m_value {};
		uint64             m_hideValuesEqualOrGreaterThan { UINT_MAX };

		HtmlFieldSelectEnum<T>& SetValue(typename T::E v) { m_value = v; return *this; }
		HtmlFieldSelectEnum<T>& HideValuesEqualOrGreaterThan(uint64 n) { m_hideValuesEqualOrGreaterThan = n; return *this; }

		void RenderOptions(HtmlBuilder& html) const override
			{ DescEnum_RenderOptions(html, T::sc_values, m_value, m_hideValuesEqualOrGreaterThan); }

		bool ReadFromRequest(HttpRequest const&, InsensitiveNameValuePairsWithStore const& nvp) override
		{
			Seq reader = nvp.Get(m_fieldName);
			if (reader.n)
			{
				uint32 v = reader.ReadNrUInt32Dec();
				if (!reader.n && v < m_hideValuesEqualOrGreaterThan && DescEnum_IsValid(T::sc_values, v))
				{
					m_oldValue.Init(m_value);
					m_value = (typename T::E) v;
					return true;
				}
			}

			m_fieldErrs.Add(Str::Join(m_friendlyName, ": invalid select field value"));
			return false;
		}
	};

}
