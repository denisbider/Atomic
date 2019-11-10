#pragma once

#include "AtSeq.h"


namespace At
{

	class HtmlBuilder;


	struct ValueDesc
	{
		uint32 value;
		char const* name;
		char const* desc;
	};


	bool         DescEnum_NameToVal     (ValueDesc const* values, Seq name, uint32& value);
	bool         DescEnum_IsValid       (ValueDesc const* values, uint64 value);
	char const*  DescEnum_Name          (ValueDesc const* values, uint64 value);
	char const*  DescEnum_Desc          (ValueDesc const* values, uint64 value);
	HtmlBuilder& DescEnum_RenderOptions (HtmlBuilder& html, ValueDesc const* values, uint64 selectedValue, uint64 hideValuesGreaterOrEqualTo = UINT64_MAX);


	enum { DescEnum_MaxTypeNameLen = 100, DescEnum_MaxDescLen = 100 };
	typedef char const* (*DescEnumToStrFunc)(uint32);

	template <typename T>
	struct DescEnum
	{
		static char const*   TypeName      ()                    { return T::sc_typeName; }
		static bool          NameToValUInt (Seq name, uint32& v) { return DescEnum_NameToVal (T::sc_values, name, v); }
		static bool          IsValid       (uint32 v)            { return DescEnum_IsValid   (T::sc_values, v); }
		static char const*   Name          (uint32 v)            { return DescEnum_Name      (T::sc_values, v); }
		static char const*   Desc          (uint32 v)            { return DescEnum_Desc      (T::sc_values, v); }

		static HtmlBuilder& RenderOptions(HtmlBuilder& html, uint64 selectedValue)
			{ return DescEnum_RenderOptions(html, T::sc_values, selectedValue, UINT64_MAX); }

		static HtmlBuilder& RenderOptionsWithHide(HtmlBuilder& html, uint64 selectedValue, uint64 hideValuesGreaterOrEqualTo)
			{ return DescEnum_RenderOptions(html, T::sc_values, selectedValue, hideValuesGreaterOrEqualTo); }

	protected:
		static bool Base_ReadNrAndVerify(Seq s, uint32& v)
		{
			Seq reader = s.Trim();
			uint32 n = reader.ReadNrUInt32Dec();
			if (reader.n || !IsValid(n))
				return false;

			v = n;
			return true;
		}
	};


	#define DESCENUM_DECL_BEGIN(NAME)				struct NAME : public DescEnum<NAME> { static char const* const sc_typeName; static ValueDesc const sc_values[]; enum E : uint32 {
	#define DESCENUM_DECL_VALUE(VALNAME, VALUE)		VALNAME = VALUE,
	#define DESCENUM_DECL_NEXT(VALNAME)				VALNAME,
	#define DESCENUM_DECL_CLOSE()					};	static bool ReadNrAndVerify(Seq s, E& v) { return Base_ReadNrAndVerify(s, (uint32&) v); } \
														static bool NameToValE(Seq name, E& v) { return DescEnum_NameToVal(sc_values, name, (uint32&) v); } };

	#define DESCENUM_DEF_BEGIN(NAME)				char const* const NAME::sc_typeName = #NAME; ValueDesc const NAME::sc_values[] = {
	#define DESCENUM_DEF_VALUE(VALNAME)				{ VALNAME, #VALNAME, #VALNAME },
	#define DESCENUM_DEF_VAL_X(VALNAME, DESC)	    { VALNAME, #VALNAME, DESC },
	#define DESCENUM_DEF_CLOSE()					{ INT_MAX, nullptr, nullptr } };

}
