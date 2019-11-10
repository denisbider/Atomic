#include "AtIncludes.h"
#include "AtDescEnum.h"

#include "AtHtmlBuilder.h"
#include "AtNumCvt.h"


namespace At
{

	bool DescEnum_NameToVal(ValueDesc const* values, Seq name, uint32& value)
	{
		while (values->name != nullptr)
		{
			if (name.EqualInsensitive(values->name))
			{
				value = values->value;
				return true;
			}
			++values;
		}
		value = INT_MAX;
		return false;
	}


	bool DescEnum_IsValid(ValueDesc const* values, uint64 value)
	{
		while (values->name != nullptr)
		{
			if (values->value == value)
				return true;
			++values;
		}
		return false;
	}


	char const* DescEnum_Name(ValueDesc const* values, uint64 value)
	{
		while (values->name != nullptr)
		{
			if (values->value == value)
				return values->name;
			++values;
		}
		return "(?)";
	}


	char const* DescEnum_Desc(ValueDesc const* values, uint64 value)
	{
		while (values->name != nullptr)
		{
			if (values->value == value)
				return values->desc;
			++values;
		}
		return "(?)";
	}


	HtmlBuilder& DescEnum_RenderOptions(HtmlBuilder& html, ValueDesc const* values, uint64 selectedValue, uint64 hideValuesGreaterOrEqualTo)
	{
		while (values->desc != nullptr)
		{
			if (values->value == selectedValue || values->value < hideValuesGreaterOrEqualTo)
			{
				html.Option()
						.Value(Str().UInt(values->value))
						.Cond(selectedValue == values->value, &HtmlBuilder::Selected)
						.T(values->desc)
					.EndOption();
			}

			++values;
		}

		return html;
	}

}
