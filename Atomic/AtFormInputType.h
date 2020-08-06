#pragma once

#include "AtStr.h"
#include "AtVec.h"


namespace At
{

	// MaxLenField

	struct MaxLenField
	{
		char const* const mc_zMaxLen {};
		uint const        mc_nMaxLen {};

		MaxLenField(char const* zMaxLen, uint nMaxLen)
			: mc_zMaxLen(zMaxLen), mc_nMaxLen(nMaxLen) {}

		virtual void EncObj(Enc& s) const;
		virtual bool IsValidEx(Seq value, Vec<Str>* errs, Seq friendlyName) const;

		bool IsValid(Seq value, Vec<Str>& errs, Seq friendlyName) const { return IsValidEx(value, &errs, friendlyName); }
		bool IsValid(Seq value) const { return IsValidEx(value, nullptr, Seq()); }
	};



	// FormInputType

	struct FormInputType : MaxLenField
	{
		char const* const mc_zWidth  {};

		FormInputType(char const* zMaxLen, uint nMaxLen, char const* zWidth)
			: MaxLenField(zMaxLen, nMaxLen), mc_zWidth(zWidth) {}
	};


	extern FormInputType const fit_pw_noMinLen;
	extern FormInputType const fit_ip4;
	extern FormInputType const fit_ip6;
	extern FormInputType const fit_dnsName;
	extern FormInputType const fit_email;
	extern FormInputType const fit_url;
	extern FormInputType const fit_number;
	extern FormInputType const fit_postalCode;
	extern FormInputType const fit_salutation;
	extern FormInputType const fit_name;
	extern FormInputType const fit_nameOrEmail;
	extern FormInputType const fit_street;
	extern FormInputType const fit_desc;
	extern FormInputType const fit_keywords;
	extern FormInputType const fit_origin;
	extern FormInputType const fit_nameList;
	extern FormInputType const fit_localPath;



	// FormInputTypeWithMin

	struct FormInputTypeWithMin : FormInputType
	{
		char const* const mc_zMinLen {};
		uint const        mc_nMinLen {};

		FormInputTypeWithMin(char const* zMaxLen, uint nMaxLen, char const* zWidth, char const* zMinLen, uint nMinLen)
			: FormInputType(zMaxLen, nMaxLen, zWidth), mc_zMinLen(zMinLen), mc_nMinLen(nMinLen) {}

		void EncObj(Enc& s) const override;
		bool IsValidEx(Seq value, Vec<Str>* errs, Seq friendlyName) const override;
	};



	// FormInputTypeWithChars

	struct FormInputTypeWithChars : FormInputTypeWithMin
	{
		FormInputTypeWithChars(char const* zMaxLen, uint nMaxLen, char const* zWidth, char const* zMinLen, uint nMinLen, CharCriterion charCrit, char const* charDesc)
			: FormInputTypeWithMin(zMaxLen, nMaxLen, zWidth, zMinLen, nMinLen), mc_charCrit(charCrit), mc_charDesc(charDesc) {}

		CharCriterion const mc_charCrit;
		char const*   const mc_charDesc;

		void EncObj(Enc& s) const override;
		bool IsValidEx(Seq value, Vec<Str>* errs, Seq friendlyName) const override;
	};



	// TextAreaDims

	struct TextAreaDims : MaxLenField
	{
		TextAreaDims(char const* zMaxLen, uint nMaxLen, char const* zRows, char const* zCols)
			: MaxLenField(zMaxLen, nMaxLen), mc_zRows(zRows), mc_zCols(zCols) {}

		char const* const mc_zRows   {};
		char const* const mc_zCols   {};
	};

	extern TextAreaDims const tad_policy;
	extern TextAreaDims const tad_centerpiece;

}
